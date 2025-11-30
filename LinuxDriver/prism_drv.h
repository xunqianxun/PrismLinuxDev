#ifndef __PRISM_DRV_H__
#define __PRISM_DRV_H__
#include <linux/hrtimer.h>

#include <drm/drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_writeback.h>

#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_device.h>


#define PRISM_MEM_VRAM_SIZE (64 * 1024 * 1024) // 64MB

#define PRISM_PL_FLAG_SYSTEM  (1 << 0) // 系统内存
#define PRISM_PL_FLAG_VRAM    (1 << 1) // 你的 64MB 显存

static const u32 prism_plane_formats[] = {
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ARGB8888,
};

static const u32 prism_formats[] = {
	DRM_FORMAT_XRGB8888,
};

struct prism_bo {
    struct ttm_buffer_object tbo; // TTM 后端对象
    struct drm_gem_object gem;    // GEM 前端对象
    
    struct ttm_placement placement;
    struct ttm_place placements[3];
};


struct prism_device {
    struct drm_device drm;
    void __iomem *mmio;
    void __iomem *vram_virt
    resource_size_t vram_base;
    struct ttm_device ttm;
};

struct prism_plane {
    struct drm_plane base;
};


#define to_prism(dev) container_of(dev, struct prism_device, drm)
#define to_prism_bo(obj) container_of(obj, struct prism_bo, gem)
#define ttm_to_prism_bo(tbo) container_of(tbo, struct prism_bo, tbo)


/*
    * Prism_plane define 
*/
struct prism_plane *prism_plane_init(struct prism_device *pdev,
				   enum drm_plane_type type, int index);

#endif 