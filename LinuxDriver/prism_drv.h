#ifndef __PRISM_DRV_H__
#define __PRISM_DRV_H__
#include <linux/hrtimer.h>

#include <drm/drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_writeback.h>


static const u32 prism_plane_formats[] = {
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ARGB8888,
};

static const u32 prism_formats[] = {
	DRM_FORMAT_XRGB8888,
};

struct prism_device {
    struct drm_device drm;
    void __iomem *mmio;
};

struct prism_plane {
    struct drm_plane base;
};


#define to_prism(dev) container_of(dev, struct prism_device, drm)

/*
    * Prism_plane define 
*/
struct prism_plane *prism_plane_init(struct prism_device *pdev,
				   enum drm_plane_type type, int index);

#endif 