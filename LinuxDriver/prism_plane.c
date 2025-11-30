// SPDX-License-Identifier: GPL-2.0+

#include <linux/dma-buf-map.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_vram_helper.h> 

#include "prism_drv.h"

/* ------------------------------------------------------------------
 * 1. 硬件寄存器定义 (BAR 2)
 * ------------------------------------------------------------------ */
#define PRISM_REG_FORMAT    0x00
#define PRISM_REG_BYTEPP    0x04
#define PRISM_REG_WIDTH     0x08
#define PRISM_REG_HEIGHT    0x0C
#define PRISM_REG_STRIDE    0x10
#define PRISM_REG_OFFSET    0x14
#define PRISM_REG_SIZE      0x18
#define PRISM_REG_START     0x1c

#define PRISM_FMT_XRGB8888  0x20020888
#define PRISM_FMT_ARGB8888  0x20028888

static void prism_primary_atomic_update(struct drm_plane *plane,
                                        struct drm_atomic_state *state)
{
    struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
    struct prism_device *pdev = to_prism(plane->dev);
    struct drm_framebuffer *fb = new_state->fb;
    struct drm_gem_vram_object *gbo;
    u64 vram_offset;
    u32 hw_fmt;

    if (!fb)  
		return;

    gbo = drm_gem_vram_of_gem(fb->obj[0]);
    vram_offset = drm_gem_vram_offset(gbo);
    
    if ((s64)vram_offset < 0) { /* [FIXED] 显式转换类型以进行负数检查 */
        return;
    }

    switch (fb->format->format) {
    case DRM_FORMAT_XRGB8888: hw_fmt = PRISM_FMT_XRGB8888; break;
    case DRM_FORMAT_ARGB8888: hw_fmt = PRISM_FMT_ARGB8888; break;
    default: hw_fmt = PRISM_FMT_XRGB8888; break;
    }

    iowrite32(hw_fmt,           pdev->mmio + PRISM_REG_FORMAT);
    iowrite32(fb->format->cpp[0], pdev->mmio + PRISM_REG_BYTEPP);
    iowrite32(fb->width,        pdev->mmio + PRISM_REG_WIDTH);
    iowrite32(fb->height,       pdev->mmio + PRISM_REG_HEIGHT);
    iowrite32(fb->pitches[0],   pdev->mmio + PRISM_REG_STRIDE);
    iowrite32((u32)vram_offset, pdev->mmio + PRISM_REG_OFFSET);
    iowrite32(gbo->bo.base.size, pdev->mmio + PRISM_REG_SIZE);
    iowrite32(1,   pdev->mmio + PRISM_REG_START);
}

static int prism_plane_atomic_check(struct drm_plane *plane,
                                    struct drm_atomic_state *state)
{
    struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
    struct drm_crtc_state *new_crtc_state;

    if (!new_plane_state->crtc)
        return 0;

    new_crtc_state = drm_atomic_get_new_crtc_state(state, new_plane_state->crtc);
    
    return drm_atomic_helper_check_plane_state(new_plane_state, new_crtc_state,
                                                  (1 << 16), 
                                                  (1 << 16),
                                                  false, true);
}

static int prism_plane_prepare_fb(struct drm_plane *plane,
                                  struct drm_plane_state *new_state)
{
    struct drm_gem_object *obj = new_state->fb->obj[0];
    struct prism_bo *bo = to_prism_bo(obj);
    int ret;

    if (!bo) return 0;

    /* 核心魔法：把数据从系统内存搬运到 64MB VRAM 中！ */
    ret = prism_bo_pin(bo, PRISM_PL_FLAG_VRAM);
    if (ret) return ret;

    /* 记录 VRAM 里的偏移量，方便 atomic_update 写寄存器 */
    // bo->tbo.resource->start 是 page index，需要左移转换为地址
    // 假设 VRAM 物理基地址是 pdev->vram_base
    u64 vram_offset = bo->tbo.resource->start << PAGE_SHIFT;
    
    // 你可以在 plane_state 里加个字段存这个地址
    // 或者直接在这里算好物理地址
    // dma_addr_t paddr = pdev->vram_base + vram_offset;
    
    return 0;
}


static const struct drm_plane_helper_funcs prism_primary_helper_funcs = {
    .prepare_fb = drm_gem_vram_plane_helper_prepare_fb,
    .cleanup_fb = drm_gem_vram_plane_helper_cleanup_fb,
    .atomic_check = prism_plane_atomic_check,
    .atomic_update = prism_primary_atomic_update,
};

static const struct drm_plane_funcs prism_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_mode_config_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};



struct prism_plane *prism_plane_init(struct prism_device *pdev,
				   enum drm_plane_type type, int index)
{
	struct drm_device *dev = &pdev->drm;
	const struct drm_plane_helper_funcs *funcs;
	struct prism_plane *plane;
	const u32 *formats;
	int nformats;

	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
		formats = prism_formats;
		nformats = ARRAY_SIZE(prism_formats);
		funcs = &prism_primary_helper_funcs;
		break;
	case DRM_PLANE_TYPE_CURSOR:
	case DRM_PLANE_TYPE_OVERLAY:
		formats = prism_plane_formats;
		nformats = ARRAY_SIZE(prism_plane_formats);
		funcs = &prism_primary_helper_funcs;
		break;
	default:
		formats = prism_formats;
		nformats = ARRAY_SIZE(prism_formats);
		funcs = &prism_primary_helper_funcs;
		break;
	}

	plane = drmm_universal_plane_alloc(dev, struct prism_plane, base, 1 << index,
					   &prism_plane_funcs,
					   formats, nformats,
					   NULL, type, NULL);
	if (IS_ERR(plane))
		return plane;

	drm_plane_helper_add(&plane->base, funcs);

	return plane;
}
