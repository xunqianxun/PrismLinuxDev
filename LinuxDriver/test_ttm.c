#include "test_drv.h"

/* * ------------------------------------------------------------------
 * 1. TTM Backend Helpers (回调函数)
 * ------------------------------------------------------------------
 */

static struct ttm_tt *prism_tt_create(struct ttm_buffer_object *bo,
                                      uint32_t page_flags)
{
    struct ttm_tt *tt = kzalloc(sizeof(*tt), GFP_KERNEL);
    if (!tt) return NULL;
    
    /* * Prism 是物理 PCIe 设备。
     * 使用 ttm_write_combined 可以利用 CPU 的写合并缓冲区加速 VRAM 写操作。
     */
    if (ttm_tt_init(tt, bo, page_flags, ttm_write_combined)) {
        kfree(tt);
        return NULL;
    }
    return tt;
}

static void prism_tt_destroy(struct ttm_device *bdev, struct ttm_tt *tt)
{
    ttm_tt_fini(tt);
    kfree(tt);
}

static bool prism_ttm_eviction_valuable(struct ttm_buffer_object *bo,
                                        const struct ttm_place *place)
{
    /* 默认允许驱逐 */
    return ttm_bo_eviction_valuable(bo, place);
}

static struct ttm_device_funcs prism_ttm_funcs = {
    .ttm_tt_create = prism_tt_create,
    .ttm_tt_destroy = prism_tt_destroy,
    .eviction_valuable = prism_ttm_eviction_valuable,
};

int prism_ttm_init(struct prism_device *pdev, 
                   uint64_t vram_size)
{
    int ret;

    /* * 初始化 TTM 设备
     * use_dma_alloc = true: 因为 Prism 是真实设备，系统内存的分配可能涉及 DMA
     */
    ret = ttm_device_init(&pdev->pttm, &prism_ttm_funcs, pdev->drm.dev,
                          pdev->drm.anon_inode->i_mapping, pdev->drm.vma_offset_manager,
                          true, true);
    if (ret) return ret;

    /* * 初始化 VRAM 管理器
     * 让 TTM 管理 0 ~ vram_size 的地址偏移量
     */
    ret = ttm_range_man_init(&pdev->pttm, TTM_PL_VRAM, false, 
                             vram_size >> PAGE_SHIFT);
    if (ret) return ret;

    /* * 初始化 SYSTEM 管理器 (作为 VRAM 满时的备选)
     */
    ret = ttm_range_man_init(&pdev->pttm, TTM_PL_SYSTEM, true, 0);
    if (ret) {
        ttm_range_man_fini(&pdev->pttm, TTM_PL_VRAM);
        return ret;
    }

    return 0;
}

void prism_ttm_fini(struct prism_device *pdev)
{
    ttm_range_man_fini(&pdev->pttm, TTM_PL_VRAM);
    ttm_range_man_fini(&pdev->pttm, TTM_PL_SYSTEM);
    ttm_device_fini(&pdev->pttm);
}