#include "prism_drv.h"

/* * BO 销毁回调 
 * 当 TTM 引用计数归零时，TTM 会调用这个函数来释放内存。
 */
static void prism_bo_destroy(struct ttm_buffer_object *tbo)
{
    struct prism_bo *bo = ttm_to_prism_bo(tbo);

    drm_gem_object_release(&bo->gem);
    kfree(bo);
}

static void prism_bo_placement_init(struct prism_bo *bo, u32 domain)
{
    unsigned i = 0;

    bo->placements[i].fpfn = 0;
    bo->placements[i].lpfn = 0;
    bo->placements[i].flags = 0;
    bo->placements[i].mem_type = domain; // TTM_PL_VRAM 或 TTM_PL_SYSTEM
    i++;

    /* 如果首选是 VRAM，添加 System 作为备选 (忙碌时回退) */
    if (domain == TTM_PL_VRAM) {
        bo->placements[i].fpfn = 0;
        bo->placements[i].lpfn = 0;
        bo->placements[i].flags = 0;
        bo->placements[i].mem_type = TTM_PL_SYSTEM;
        i++;
    }

    bo->placement.placement = bo->placements;
    bo->placement.num_placement = i;
    
    bo->placement.busy_placement = bo->placements;
    bo->placement.num_busy_placement = i;
}

/*
 * 核心创建函数
 * @domain: 初始放在哪里？(通常是 TTM_PL_SYSTEM，显示时再 pin 到 VRAM)
 */
int prism_bo_create(struct prism_device *pdev, size_t size,
                    bool kernel, u32 domain,
                    struct prism_bo **pbo_out)
{
    struct prism_bo *bo;
    int ret;

    bo = kzalloc(sizeof(*bo), GFP_KERNEL);
    if (!bo) return -ENOMEM;

    /* 1. 初始化 GEM 部分 */
    size = PAGE_ALIGN(size);
    /* prism_gem_funcs 稍后定义 */
    // bo->gem.funcs = &prism_gem_funcs; 
    drm_gem_private_object_init(&pdev->drm, &bo->gem, size);

    /* 2. 初始化 Placement */
    prism_bo_placement_init(bo, domain);

    /* 3. 初始化 TTM 部分 (这会触发 TTM 分配内存) */
    /* * ttm_bo_type_device: 普通用户对象
     * ttm_bo_type_kernel: 内核专用对象
     */
    ret = ttm_bo_init(&pdev->ttm, 
                      &bo->tbo,
                      size,
                      kernel ? ttm_bo_type_kernel : ttm_bo_type_device,
                      &bo->placement,
                      PAGE_SIZE, 
                      false, /* interruptible */
                      NULL,  /* sg */
                      NULL,  /* resv */
                      prism_bo_destroy);
    if (ret) {
        return ret;
    }

    *pbo_out = bo;
    return 0;
}

/* Pin: 把 BO 强行移动到 VRAM 并锁定 */
int prism_bo_pin(struct prism_bo *bo, u32 domain)
{
    struct ttm_operation_ctx ctx = { false, false };
    int ret;

    /* 修改策略：只允许在指定 domain (VRAM) */
    prism_bo_placement_init(bo, domain);
    
    /* 触发搬运 (System -> VRAM) */
    ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
    if (ret) return ret;

    ttm_bo_pin(&bo->tbo); //TTM 维护着 LRU 链表（用于决定踢谁）。pin 操作通常会把这个 BO 从 LRU 链表中移除，或者打上特殊标记，这样 TTM 的内存回收机制就会忽略它
    return 0;
}

void prism_bo_unpin(struct prism_bo *bo)
{
    ttm_bo_unpin(&bo->tbo);
    /* Unpin 后，我们把策略改回允许回退到 System，
     * 这样下次内存不足时 TTM 就可以把它踢出去了 
     */
    prism_bo_placement_init(bo, TTM_PL_SYSTEM); 
    ttm_bo_validate(&bo->tbo, &bo->placement, NULL);
}