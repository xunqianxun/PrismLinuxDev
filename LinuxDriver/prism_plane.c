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

static const struct drm_plane_helper_funcs prism_primary_helper_funcs = {
    .prepare_fb = drm_gem_vram_plane_helper_prepare_fb,
    .cleanup_fb = drm_gem_vram_plane_helper_cleanup_fb,
    .atomic_check = prism_plane_atomic_check,
    .atomic_update = prism_primary_atomic_update,
};

static const struct drm_plane_funcs prism_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};


static void vkms_plane_atomic_update(struct drm_plane *plane,
				     struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct vkms_plane_state *vkms_plane_state;
	struct drm_shadow_plane_state *shadow_plane_state;
	struct drm_framebuffer *fb = new_state->fb;
	struct vkms_composer *composer;

	if (!new_state->crtc || !fb)
		return;

	vkms_plane_state = to_vkms_plane_state(new_state);
	shadow_plane_state = &vkms_plane_state->base;

	composer = vkms_plane_state->composer;
	memcpy(&composer->src, &new_state->src, sizeof(struct drm_rect));
	memcpy(&composer->dst, &new_state->dst, sizeof(struct drm_rect));
	memcpy(&composer->fb, fb, sizeof(struct drm_framebuffer));
	memcpy(&composer->map, &shadow_plane_state->data, sizeof(composer->map));
	drm_framebuffer_get(&composer->fb);
	composer->offset = fb->offsets[0];
	composer->pitch = fb->pitches[0];
	composer->cpp = fb->format->cpp[0];
}

static int vkms_plane_atomic_check(struct drm_plane *plane,
				   struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct drm_crtc_state *crtc_state;
	bool can_position = false;
	int ret;

	if (!new_plane_state->fb || WARN_ON(!new_plane_state->crtc))
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state,
					       new_plane_state->crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	if (plane->type != DRM_PLANE_TYPE_PRIMARY)
		can_position = true;

	ret = drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
						  DRM_PLANE_HELPER_NO_SCALING,
						  DRM_PLANE_HELPER_NO_SCALING,
						  can_position, true);
	if (ret != 0)
		return ret;

	/* for now primary plane must be visible and full screen */
	if (!new_plane_state->visible && !can_position)
		return -EINVAL;

	return 0;
}

static const struct drm_plane_helper_funcs vkms_primary_helper_funcs = {
	.atomic_update		= vkms_plane_atomic_update,
	.atomic_check		= vkms_plane_atomic_check,
	DRM_GEM_SHADOW_PLANE_HELPER_FUNCS,
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
