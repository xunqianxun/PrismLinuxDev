#ifndef COMMON_H
#define COMMON_H

#include <linux/module.h>
#include <linux/pci.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/ttm/ttm_device.h>
#include <drm/ttm/ttm_range_manager.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <linux/types.h>
#include <drm/drm_vblank.h>
#include <drm/drm_probe_helper.h>

#include <drm/drm_print.h>
#include <drm/ttm/ttm_resource.h>
#include <linux/dma-buf-map.h>
#include <linux/dma-fence.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_ttm_helper.h>
#include <drm/qxl_drm.h>
#include <drm/ttm/ttm_execbuf_util.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>   
#include <drm/ttm/ttm_bo_api.h>


#define PRISM_VRAM_BAR_IDX  0
#define PRISM_MMIO_BAR_IDX  2
#define PRISM_VRAM_SIZE     (64 * 1024 * 1024) // 64MB


#define PRISM_GEM_DOMAIN_CPU 0
#define PRISM_GEM_DOMAIN_VRAM 1
#define PRISM_GEM_DOMAIN_SURFACE 2


/* 支持的像素格式：这里只支持标准的 32位 XRGB */
static const uint32_t prism_formats[] = {
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ARGB8888,
};

/* 假设 Prism 硬件在 BAR2 有如下控制寄存器用于显示 */
/* 注意：你需要确保 struct prism_head 定义里有这些字段，或者用 offset 操作 */
#define PRISM_REG_WIDTH     0x08
#define PRISM_REG_HEIGHT    0x0C
#define PRISM_REG_ENABLE    0x10  // 状态/启用位
#define PRISM_REG_STRIDE    0x14  // [新增] 跨距 (一行多少字节)
#define PRISM_REG_START     0x28  // [新增] 显存起始偏移量 (告诉硬件去哪读图)


// /* Prism TTM 上下文结构体 */
// struct prism_ttm_domain {
//     struct ttm_device bdev;
//     struct drm_device *ddev;
//     u64 vram_size;
// };


/* ========================================================
 * 1. 硬件寄存器定义 (BAR 2)
 * ======================================================== */
struct prism_head {
    uint32_t signature;      // 0x00: 设备签名
    uint32_t version;        // 0x04: 版本号
    uint32_t width;          // 0x08: 屏幕宽度
    uint32_t height;         // 0x0C: 屏幕高度
    uint32_t status;         // 0x10: 状态寄存器
    uint32_t stride;        // 0x14: 命令寄存器
    uint32_t frambufferoffset;     // 0x18: VRAM 物理基地址 (供调试)
};

/* ========================================================
 * 2. 设备与 BO 结构体
 * ======================================================== */

struct prism_device {
    struct drm_device drm;
    struct ttm_device pttm;

    struct pci_dev *pdev;

    /* VRAM (BAR 0) 资源信息 */
    resource_size_t vram_base_phys; // VRAM 物理总线地址
    resource_size_t vram_size;      // VRAM 大小
    void __iomem *vram_virt;        // CPU 访问 VRAM 的虚拟地址 (映射后)

    /* MMIO (BAR 2) 资源信息 */
    resource_size_t mmio_base_phys;
    resource_size_t mmio_size;
    struct prism_head __iomem *mmio_virt; // 映射后的寄存器结构体指针

    struct drm_plane primary_plane;
    struct drm_crtc crtc;
    struct drm_encoder encoder;
    struct drm_connector connector;
};

struct prism_bo {
    struct ttm_buffer_object tbo;
    struct ttm_placement placement;
    struct ttm_place placements[2];
};

#define to_prism_dev(dev) container_of(dev, struct prism_device, drm)
#define to_prism_ttm(dev) container_of(dev, struct prism_device, pttm)
#define to_prism_bo(p) container_of(p, struct prism_bo, tbo)
#define to_ttm_bo(gem) container_of(gem, struct ttm_buffer_object, base)

void prism_write_reg(struct prism_device *pdev, u32 offset, u32 val);
uint32_t prism_read_reg(struct prism_device *pdev, u32 offset);

/***********************************************************/
/*              ttm define some function                   */
/***********************************************************/
int prism_ttm_init(struct prism_device *pdev, uint64_t vram_size);
void prism_ttm_fini(struct prism_device *pdev);

/***********************************************************/
/*              object define some function                */
/***********************************************************/
int prism_plane_prepare_fb(struct drm_plane *plane, struct drm_plane_state *new_state);
void prism_plane_cleanup_fb(struct drm_plane *plane, struct drm_plane_state *old_state);
struct prism_bo *prism_ttm_bo_create(struct prism_device *pdev, size_t size, uint32_t domain_flags);

/***********************************************************/
/*              gem define some function                   */
/***********************************************************/
extern const struct drm_gem_object_funcs prism_gem_object_funcs;
int prism_gem_dumb_create(struct drm_file *file, struct drm_device *dev, struct drm_mode_create_dumb *args);
int prism_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
                              uint32_t handle, uint64_t *offset);
/***********************************************************/
/*              kms define some function                   */
/***********************************************************/
int prism_modeset_init(struct prism_device *pdev);

#endif 