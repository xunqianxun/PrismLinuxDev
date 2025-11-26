#include <linux/module.h>
#include <linux/pci.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_vram_helper.h> 
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_managed.h>
#include <drm/drm_atomic.h>
#include <drm/drm_connector.h> 
#include <drm/drm_edid.h>
#include <drm/drm_plane.h>
#include <drm/drm_aperture.h>
#include <drm/drm_fb_helper.h>


static const struct drm_plane_funcs prism_primary_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const struct drm_crtc_funcs prism_crtc_funcs = {
    .reset = drm_atomic_helper_crtc_reset,
    .destroy = drm_crtc_cleanup,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static const struct drm_crtc_helper_funcs prism_crtc_helper_funcs = {
};

static const struct drm_mode_config_funcs prism_mode_config_funcs = {
    .fb_create = drm_gem_fb_create,
    .atomic_check = drm_atomic_helper_check,
    .atomic_commit = drm_atomic_helper_commit,
};

/* ------------------------------------------------------------------
 * 3. [FIXED] Connector 实现 (关键修复)
 * ------------------------------------------------------------------
 */

/* * 这个函数非常关键！它告诉内核你的虚拟显示器支持什么分辨率。
 * 如果没有这个函数，Modsetting 会失败，驱动加载时会报错。
 */
static int prism_conn_get_modes(struct drm_connector *connector)
{
    /* 创建一个固定的 1024x768 @ 60Hz 模式 */
    struct drm_display_mode *mode = drm_cvt_mode(connector->dev, 1024, 768, 60, 0, 0, 0);
    
    if (!mode)
        return 0;

    /* 标记为首选模式 */
    mode->type |= DRM_MODE_TYPE_PREFERRED;
    
    /* 添加到 connector 的模式列表 */
    drm_mode_probed_add(connector, mode);

    /* 设置显示的物理尺寸 (毫米)，这里随便写，如果不写有些桌面环境会显示异常 */
    connector->display_info.width_mm = 400;
    connector->display_info.height_mm = 300;

    return 1; /* 返回检测到的模式数量 */
}

/* 必须定义 Helper Funcs 才能使用 atomic 框架 */
static const struct drm_connector_helper_funcs prism_conn_helper_funcs = {
    .get_modes = prism_conn_get_modes,
};

static enum drm_connector_status prism_conn_detect(struct drm_connector *connector,
                                                   bool force)
{
    return connector_status_connected;
}

static const struct drm_connector_funcs prism_conn_funcs = {
    .detect = prism_conn_detect,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup, /* drmm_connector_init 会自动处理内存，这里只需清理内部 */
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};




DEFINE_DRM_GEM_FOPS(prism_fops);

static struct drm_driver prism_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_RENDER,
    .fops = &prism_fops,
    .name = "prism-drm",
    .desc = "Prism Educational GPU",
    .date = "20251123",
    .major = 1, 
    .minor = 0,
    .dumb_create = drm_gem_vram_driver_dumb_create,
};


static int prism_modeset_init(struct prism_device *pdev)
{
    struct drm_device *dev = &pdev->drm;
    struct drm_plane *primary, *cursor;
    struct prism_plane *priplane, *overplane = NULL, *cursorplane = NULL;
    struct drm_crtc *crtc;
    struct drm_encoder *encoder;
    struct drm_connector *connector;
    int ret;

    /* 1. 初始化 Mode Config */
    ret = drmm_mode_config_init(dev);
    if (ret) return ret;

    dev->mode_config.min_width = 0;
    dev->mode_config.min_height = 0;
    dev->mode_config.max_width = 4096;
    dev->mode_config.max_height = 4096;
    dev->mode_config.funcs = &prism_mode_config_funcs;

    ret = drm_vblank_init(dev, 1);
    if (ret) {
        DRM_ERROR("Failed to init vblank\n");
        return ret;
    }

    priplane = prism_plane_init(pdev, DRM_PLANE_TYPE_PRIMARY, 0); // this index set zero because only one CRTC
	if (IS_ERR(priplane))
		return PTR_ERR(priplane);
    primary = &priplane->base;

	overplane = prism_plane_init(pdev, DRM_PLANE_TYPE_OVERLAY, 0);
	if (IS_ERR(overplane))
		return PTR_ERR(overplane);
    
	cursorplane = prism_plane_init(pdev, DRM_PLANE_TYPE_CURSOR, 0);
	if (IS_ERR(cursorplane))
		return PTR_ERR(cursorplane);
    cursor = &cursorplane->base;



    crtc = devm_kzalloc(dev->dev, sizeof(*crtc), GFP_KERNEL);
    if (!crtc) return -ENOMEM;

    /* 使用基础初始化函数，显式传递名称 "crtc-0" 防止 NULL 崩溃 */
    ret = drm_crtc_init_with_planes(dev, crtc,
                                    primary, cursor, 
                                    &prism_crtc_funcs,
                                    "crtc-0");
    if (ret) {
        DRM_ERROR("Failed to init CRTC\n");
        return ret;
    }
    drm_crtc_helper_add(crtc, &prism_crtc_helper_funcs);


    if (!overplane->base.possible_crtcs)
		overplane->base.possible_crtcs = drm_crtc_mask(crtc);


    /* 4. 初始化 Encoder */
    encoder = drmm_encoder_alloc(dev, struct drm_encoder, dev,
                                 NULL, DRM_MODE_ENCODER_VIRTUAL, NULL);
    if (IS_ERR(encoder)) return PTR_ERR(encoder);
    encoder->possible_crtcs = 1;

    /* * 5. [FIX] 手动分配并初始化 Connector 
     * 替代 drmm_connector_init
     */
    connector = devm_kzalloc(dev->dev, sizeof(*connector), GFP_KERNEL);
    if (!connector) return -ENOMEM;

    /* 经典的 connector 初始化 */
    ret = drm_connector_init(dev, connector, &prism_conn_funcs, DRM_MODE_CONNECTOR_VIRTUAL);
    if (ret) return ret;

    /* 注册 helper (这一步必须有) */
    drm_connector_helper_add(connector, &prism_conn_helper_funcs);

    /* 6. 连接 Connector 和 Encoder */
    ret = drm_connector_attach_encoder(connector, encoder);
    if (ret) return ret;

    drm_mode_config_reset(&pdev->drm);

    return 0;
}

static int prism_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct prism_device *prism;
    struct drm_device *ddev;
    int ret;

    ret = drm_aperture_remove_conflicting_pci_framebuffers(pdev, &prism_driver);
	if (ret)
	return ret;

    /* 1. Alloc Device */
    prism = devm_drm_dev_alloc(&pdev->dev, &prism_driver, struct prism_device, drm);
    if (IS_ERR(prism)) return PTR_ERR(prism);
    ddev = &prism->drm;

    pci_set_drvdata(pdev, ddev);
    ret = pcim_enable_device(pdev);
    if (ret) return ret;

    /* 2. Map BAR 2 (MMIO) */
    prism->mmio = pcim_iomap(pdev, 2, 0);
    if (!prism->mmio) {
        DRM_ERROR("Failed to map BAR 2\n");
        return -ENOMEM;
    }

    /* 3. Init VRAM (BAR 0) */
    resource_size_t vram_base = pci_resource_start(pdev, 0);
    resource_size_t vram_len = pci_resource_len(pdev, 0);

    /* [FIXED] 增加基本的校验，防止 BAR 0 没配置导致恐慌 */
    if (vram_len == 0) {
        DRM_ERROR("VRAM size is 0, check QEMU device definition\n");
        return -ENODEV;
    }

    ret = drmm_vram_helper_init(ddev, vram_base, vram_len);
    if (ret) {
        DRM_ERROR("Failed to init VRAM MM: %d\n", ret);
        return ret;
    }

    /* 4. Modeset Init */
    ret = prism_modeset_init(prism);
    if (ret) return ret;

    /* 5. Register */
    ret = drm_dev_register(ddev, 0);
    if (ret) return ret;

    DRM_INFO("Prism DRM driver initialized\n");
    return 0;
}

static void prism_pci_remove(struct pci_dev *pdev)
{
    struct drm_device *dev = pci_get_drvdata(pdev);
    drm_dev_unregister(dev);
    /* drmm_ managed resources automatically cleaned up */
}

/* 请确保这个 ID 和你的 QEMU 代码一致 */
static const struct pci_device_id prism_pci_table[] = {
    { PCI_DEVICE(0x1a03, 0x2000) }, 
    { 0, }
};
MODULE_DEVICE_TABLE(pci, prism_pci_table);

static struct pci_driver prism_pci_driver = {
    .name = "prism-drm",
    .id_table = prism_pci_table,
    .probe = prism_pci_probe,
    .remove = prism_pci_remove,
};

module_pci_driver(prism_pci_driver);
MODULE_LICENSE("GPL");