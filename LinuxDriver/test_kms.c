#include "test_drv.h"


/* 1. 检查配置是否合法 (Atomic Check) */
static int prism_plane_atomic_check(struct drm_plane *plane,
                                    struct drm_atomic_state *state)
{
    struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
    struct drm_crtc_state *crtc_state;

    if (!new_plane_state->crtc)
        return 0; // 图层被禁用了，无需检查

    crtc_state = drm_atomic_get_new_crtc_state(state, new_plane_state->crtc);
    
    /* 使用通用的检查函数，确保坐标、裁剪没问题 */
    return drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
                                               0x10000,
                                               0x10000,
                                               false, true);
}

/* 2. 更新图层 (Atomic Update) - 写寄存器的地方！ */
static void prism_plane_atomic_update(struct drm_plane *plane,
                                      struct drm_atomic_state *state)
{
    struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
    struct prism_device *pdev = to_prism_dev(plane->dev);
    struct prism_bo *bo;
    uint32_t offset;

    if (!new_state->fb) return;

    /* 获取当前要显示的 BO */
    bo = to_prism_bo(to_ttm_bo(new_state->fb->obj[0]));

    /* 计算 VRAM 内的偏移量：
     * tbo.resource->start 是 Page Frame Number (页号)
     * 必须左移 PAGE_SHIFT 转换为字节偏移
     */
    offset = bo->tbo.resource->start << PAGE_SHIFT;

    /* 真正的硬件操作：告诉显卡去哪里读数据 */
    /* 写入 VRAM 偏移量 */
    prism_write_reg(pdev, PRISM_REG_START, offset);
    /* 写入跨距 (Stride) */
    prism_write_reg(pdev, PRISM_REG_STRIDE, new_state->fb->pitches[0]);
    
    /* 可选：如果支持硬件缩放，还要写 src_w, src_h, crtc_w, crtc_h 等 */
}


/* 3. 绑定 Helper (包含我们在上一个问题里写的 Pin/Unpin) */
static const struct drm_plane_helper_funcs prism_plane_helper_funcs = {
    .prepare_fb = prism_plane_prepare_fb, 
    .cleanup_fb = prism_plane_cleanup_fb, 
    .atomic_check = prism_plane_atomic_check,
    .atomic_update = prism_plane_atomic_update,
};

static const struct drm_plane_funcs prism_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static void prism_crtc_atomic_enable(struct drm_crtc *crtc,
                                     struct drm_atomic_state *state)
{
    struct prism_device *pdev = to_prism_dev(crtc->dev);
    
    /* 写寄存器：开启显示引擎 */
    prism_write_reg(pdev, PRISM_REG_ENABLE, 1);
}

static void prism_crtc_atomic_disable(struct drm_crtc *crtc,
                                      struct drm_atomic_state *state)
{
    struct prism_device *pdev = to_prism_dev(crtc->dev);
    
    /* 写寄存器：关闭显示引擎 */
    prism_write_reg(pdev, PRISM_REG_ENABLE, 0);
    
    /* 必须调用，通知 DRM 这一帧结束了，否则会卡死 */
    drm_crtc_vblank_off(crtc);
}

static const struct drm_crtc_helper_funcs prism_crtc_helper_funcs = {
    .atomic_enable = prism_crtc_atomic_enable,
    .atomic_disable = prism_crtc_atomic_disable,
};

static const struct drm_crtc_funcs prism_crtc_funcs = {
    .reset = drm_atomic_helper_crtc_reset,
    .destroy = drm_crtc_cleanup,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

/* === Encoder (Dummy) === */
static const struct drm_encoder_funcs prism_encoder_funcs = {
    .destroy = drm_encoder_cleanup,
};

/* === Connector (Dummy) === */
/* 告诉用户空间我们支持什么分辨率 */
static int prism_conn_get_modes(struct drm_connector *connector)
{
    /* 添加一个固定的 1024x768 模式 */
    struct drm_display_mode *mode;

    mode = drm_cvt_mode(connector->dev, 1024, 768, 60, false, false, false);
    if (!mode) return 0;

    /* 设置为首选模式 */
    mode->type |= DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, mode);
    return 1;
}

static const struct drm_connector_helper_funcs prism_conn_helper_funcs = {
    .get_modes = prism_conn_get_modes,
};

static const struct drm_connector_funcs prism_conn_funcs = {
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

int prism_modeset_init(struct prism_device *pdev)
{
    struct drm_device *dev = &pdev->drm;
    int ret;

    /* 1. 初始化 Primary Plane */
    ret = drm_universal_plane_init(dev, &pdev->primary_plane,
                                   1, /* possible_crtcs: 只能被第0个CRTC使用 */
                                   &prism_plane_funcs,
                                   prism_formats, ARRAY_SIZE(prism_formats),
                                   NULL,
                                   DRM_PLANE_TYPE_PRIMARY, NULL);
    if (ret) return ret;
    drm_plane_helper_add(&pdev->primary_plane, &prism_plane_helper_funcs);

    /* 2. 初始化 CRTC */
    ret = drm_crtc_init_with_planes(dev, &pdev->crtc,
                                    &pdev->primary_plane, NULL, /* Cursor Plane 暂空 */
                                    &prism_crtc_funcs, NULL);
    if (ret) return ret;
    drm_crtc_helper_add(&pdev->crtc, &prism_crtc_helper_funcs);

    /* 3. 初始化 Encoder */
    ret = drm_encoder_init(dev, &pdev->encoder, &prism_encoder_funcs,
                           DRM_MODE_ENCODER_VIRTUAL, NULL);
    if (ret) return ret;
    /* possible_crtcs: Encoder 可以连接哪个 CRTC？这里选第0个 */
    pdev->encoder.possible_crtcs = 1;

    /* 4. 初始化 Connector */
    ret = drm_connector_init(dev, &pdev->connector, &prism_conn_funcs,
                             DRM_MODE_CONNECTOR_VIRTUAL);
    if (ret) return ret;
    drm_connector_helper_add(&pdev->connector, &prism_conn_helper_funcs);

    /* 5. 连接 Encoder 和 Connector */
    ret = drm_connector_attach_encoder(&pdev->connector, &pdev->encoder);
    if (ret) return ret;

    /* 6. 初始化 vblank 处理 (必须，否则 atomic commit 会超时) */
    ret = drm_vblank_init(dev, 1);
    if (ret) return ret;

    /* 7. 重置模式设置状态 */
    drm_mode_config_reset(dev);

    return 0;
}
