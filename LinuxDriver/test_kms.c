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
    uint32_t hw_format_val;
    struct drm_framebuffer *fb = new_state->fb;
    uint32_t drm_fmt = fb->format->format;

    if (!new_state->fb) return;

    switch (drm_fmt) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        /* 对应 QEMU/Pixman 的 PIXMAN_x8r8g8b8 (32bpp) */
        /* 如果你不知道具体值，通常是 0x20028888，或者查看你的硬件文档 */
        hw_format_val = 0x20028888; 
        break;

    case DRM_FORMAT_RGB565:
        /* 对应 QEMU/Pixman 的 PIXMAN_r5g6b5 (16bpp) */
        hw_format_val = 0x20020888;
        break;

    default:
        /* 理论上不应该走到这里，因为 init 时我们限制了支持列表 */
        dev_warn(pdev->drm.dev, "Unsupported format: 0x%x\n", drm_fmt);
        return;
    }

    /* 获取当前要显示的 BO */
    bo = to_prism_bo(to_ttm_bo(new_state->fb->obj[0]));

    /* 计算 VRAM 内的偏移量：
     * tbo.resource->start 是 Page Frame Number (页号)
     * 必须左移 PAGE_SHIFT 转换为字节偏移
     */
    offset = bo->tbo.resource->start << PAGE_SHIFT;

    /* 0x00: FORMAT */
    prism_write_reg(pdev, PRISM_REG_FORMAT, hw_format_val);
    
    /* 0x04: BYTEPP (Bytes Per Pixel) */
    /* cpp[0] 是 DRM 计算好的每像素字节数，XRGB8888 是 4 */
    prism_write_reg(pdev, PRISM_REG_BYTEPP, fb->format->cpp[0]);
    
    /* 0x08: WIDTH */
    prism_write_reg(pdev, PRISM_REG_WIDTH, fb->width);
    
    /* 0x0C: HEIGHT */
    prism_write_reg(pdev, PRISM_REG_HEIGHT, fb->height);
    
    /* 0x10: STRIDE (Pitch) */
    /* 一行像素占用的字节数，包含对齐填充 */
    prism_write_reg(pdev, PRISM_REG_STRIDE, fb->pitches[0]);
    
    /* 0x14: OFFSET */
    /* 告诉硬件从 VRAM 的哪里开始读图 */
    prism_write_reg(pdev, PRISM_REG_OFFSET, offset);

    /* 0x28: SIZE */
    /* 整个 Buffer 的大小，用于硬件边界检查 */
    prism_write_reg(pdev, PRISM_REG_SIZE, bo->tbo.base.size);
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

}

static void prism_crtc_atomic_disable(struct drm_crtc *crtc,
                                      struct drm_atomic_state *state)
{
    struct prism_device *pdev = to_prism_dev(crtc->dev);
    
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

static const struct drm_mode_config_funcs prism_mode_config_funcs = {
    .fb_create = drm_gem_fb_create,      
    .atomic_check = drm_atomic_helper_check,
    .atomic_commit = drm_atomic_helper_commit,
};

int prism_modeset_init(struct prism_device *pdev)
{
    struct drm_device *dev = &pdev->drm;
    int ret;

    /* 必须设置，否则 DRM 不知道显卡支持多大的分辨率 */
    dev->mode_config.min_width = 64;
    dev->mode_config.min_height = 64;
    dev->mode_config.max_width = 4096;
    dev->mode_config.max_height = 4096;
    
    /* 需要一个简单的 helper funcs */
    dev->mode_config.funcs = &prism_mode_config_funcs;

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
