#include <drm/ttm/ttm_device.h>
#include <drm/ttm/ttm_range_manager.h> 
#include "prism_drv.h"

/* 内存移动回调 */
static int prism_bo_move(struct ttm_buffer_object *bo,
                         bool evict,
                         struct ttm_operation_ctx *ctx,
                         struct ttm_resource *new_mem,
                         struct ttm_place *hop)
{
    struct ttm_resource *old_mem = bo->resource;
    struct prism_device *pdev = ttm_to_prism(bo->bdev);
    
    /* 情况 1: 如果只是在系统内存之间移动 (TTM 内部处理) */
    if (old_mem->mem_type == TTM_PL_SYSTEM && new_mem->mem_type == TTM_PL_SYSTEM)
        goto out_move;

    /* 情况 2: System -> VRAM (上传) */
    if (old_mem->mem_type == TTM_PL_SYSTEM && new_mem->mem_type == TTM_PL_VRAM) {
        // 1. 分配 VRAM 空间 (ttm_bo_move_memcpy 会帮我们调用 man->func->get_node)
        // 2. 执行 memcpy (CPU 拷贝)
        // ttm_bo_move_memcpy 是内核提供的通用 helper，非常适合虚拟显卡！
        return ttm_bo_move_memcpy(bo, ctx, new_mem); 
    }

    /* 情况 3: VRAM -> System (逐出) */
    if (old_mem->mem_type == TTM_PL_VRAM && new_mem->mem_type == TTM_PL_SYSTEM) {
        return ttm_bo_move_memcpy(bo, ctx, new_mem);
    }

out_move:
    // 如果我们处理不了，交给 TTM 默认处理
    return ttm_bo_move_memcpy(bo, ctx, new_mem);
}

static void prism_ttm_backend_destroy(struct ttm_device *bdev, struct ttm_tt *ttm)
{
	ttm_tt_fini(ttm);
	kfree(ttm);
}

static struct ttm_tt *prism_ttm_tt_create(struct ttm_buffer_object *bo,
					uint32_t page_flags)
{
	struct ttm_tt *ttm;

	ttm = kzalloc(sizeof(struct ttm_tt), GFP_KERNEL);
	if (ttm == NULL)
		return NULL;
	if (ttm_tt_init(ttm, bo, page_flags, ttm_cached)) {
		kfree(ttm);
		return NULL;
	}
	return ttm;
}

/******************************************************/
/*               Eviction Flags segment               */
/******************************************************/

/* 定义两个静态的“位置”描述符 */

/* 1. 系统内存位置 */
static const struct ttm_place prism_sys_placement_flags = {
    .fpfn = 0, // First Page Frame Number (0 表示不限制起始位置)
    .lpfn = 0, // Last Page Frame Number (0 表示不限制结束位置)
    .mem_type = TTM_PL_SYSTEM, // <--- 关键：放到系统内存
    .flags = 0, // 默认缓存策略 (Cached)
};

/* 2. VRAM 显存位置 */
static const struct ttm_place prism_vram_placement_flags = {
    .fpfn = 0,
    .lpfn = 0,
    .mem_type = TTM_PL_VRAM,   // <--- 关键：放到显存
    .flags = 0, // 通常需要 TTM_PL_FLAG_WC (Write Combined) 用于显存
};

/* 辅助函数：将 BO 的 placement 设置为系统内存 */
void prism_bo_placement_system(struct prism_bo *pbo)
{
    pbo->placements[0] = prism_sys_placement_flags;
    
    pbo->placement.num_placement = 1;
    pbo->placement.placement = &pbo->placements[0];
    
    // busy_placement 是指如果首选位置忙碌时的备选
    // 这里我们简单处理，首选和备选一样
    pbo->placement.num_busy_placement = 1;
    pbo->placement.busy_placement = &pbo->placements[0];
}

/* 辅助函数：将 BO 的 placement 设置为 VRAM */
void prism_bo_placement_vram(struct prism_bo *pbo)
{
    pbo->placements[0] = prism_vram_placement_flags;
    pbo->placement.num_placement = 1;
    pbo->placement.placement = &pbo->placements[0];
    pbo->placement.num_busy_placement = 1;
    pbo->placement.busy_placement = &pbo->placements[0];
}

/*
* for evict flage if one bo need out vram 
*/
static void prism_evict_flags(struct ttm_buffer_object *bo,
                              struct ttm_placement *placement)
{
    struct prism_bo *pbo = to_prism_bo(bo);

    prism_bo_placement_system(pbo);
    
    /* 将 BO 内部更新好的 placement 赋值给 TTM 传入的指针 */
    *placement = pbo->placement;
}

/******************************************************/
/*      Eviction IO mem reserve segment               */
/******************************************************/

static int prism_io_mem_reserve(struct ttm_device *bdev,
                                struct ttm_resource *mem)
{
    struct prism_device *pdev = ttm_to_prism(bdev);

    switch (mem->mem_type) {
    case TTM_PL_SYSTEM:
        return 0;

    case TTM_PL_VRAM:
        mem->bus.is_iomem = true;
        mem->bus.offset = (mem->start << PAGE_SHIFT) + pdev->vram_base;
        mem->bus.caching = ttm_write_combined;
        /* 如果你已经有了全局映射的虚拟地址
         * 这一步非常重要！如果你设置了 mem->bus.addr，
         * ttm_bo_move_memcpy 就会直接用这个指针，而不去 ioremap。
         */
        if (pdev->vram_virt) {
             // 虚拟基地址 + 页偏移
             mem->bus.addr = (u8 *)pdev->vram_virt + (mem->start << PAGE_SHIFT);
        }
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static struct ttm_device_funcs prism_ttm_funcs = {
    .ttm_tt_create = prism_ttm_tt_create, // 创建系统页表，使用默认的
    .ttm_tt_populate = ttm_pool_alloc,   // 分配系统页面
    .ttm_tt_unpopulate = ttm_pool_free,  // 释放系统页面
    .ttm_tt_destroy = prism_ttm_backend_destroy,
    .eviction_valuable = ttm_bo_eviction_valuable,
    .evict_flags = prism_evict_flags, //显存不足时退回策略
    
    .move = prism_bo_move,               // <--- 核心：搬运数据

    .io_mem_reserve = prism_io_mem_reserve,
};

int prism_ttm_init(struct prism_device *pdev)
{
    int ret;

    /* 1. 初始化 TTM 设备 */
    // false: use_dma_alloc (虚拟设备通常不需要 DMA API 分配，除非模拟了 DMA 引擎)
    // true:  use_dma32 (是否限制在 4GB 寻址空间，根据你的 QEMU PCI 设置决定)
    ret = ttm_device_init(&pdev->ttm, &prism_ttm_funcs, 
                          pdev->drm.dev, pdev->drm.anon_inode->i_mapping,
                          pdev->drm.vma_offset_manager,
                          false, true);
    if (ret) return ret;

    /* 2. 注册 VRAM 管理器 (64MB) */
    ret = ttm_range_man_init(&pdev->ttm, TTM_PL_VRAM, false,
                             PRISM_MEM_VRAM_SIZE >> PAGE_SHIFT);
    if (ret) return ret;
    
    return 0;
}

void prism_ttm_fini(struct prism_device *pdev)
{
    ttm_range_man_fini(&pdev->ttm, TTM_PL_VRAM);
    ttm_device_fini(&pdev->ttm);
}