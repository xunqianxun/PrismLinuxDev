#include "test_drv.h"

/* 定义一个专门用于 Pin 的 Placement 策略 */
static void prism_ttm_placement(struct prism_bo *bo, int domain)
{
    uint32_t pflag = 0 ;
    int placementnum = 0;

    if(bo->tbo.base.size <= PAGE_SIZE){
        pflag |= TTM_PL_FLAG_TOPDOWN;
    }
    if(domain == PRISM_GEM_DOMAIN_VRAM){
        bo->placements[0].fpfn = 0;
        bo->placements[0].lpfn = 0; //表示无限制
        bo->placements[0].mem_type = TTM_PL_VRAM;
        bo->placements[0].flags = pflag;
        placementnum = 1;
    }
    if(domain == PRISM_GEM_DOMAIN_SURFACE){
        bo->placements[0].fpfn = 0;
        bo->placements[0].lpfn = 0; 
        bo->placements[0].mem_type = TTM_PL_PRIV;
        bo->placements[0].flags = pflag;
        bo->placements[1].fpfn = 0;
        bo->placements[1].lpfn = 0; 
        bo->placements[1].mem_type = TTM_PL_VRAM;
        bo->placements[1].flags = pflag;
        placementnum = 2;
    }
    if(domain == PRISM_GEM_DOMAIN_CPU){
        bo->placements[0].fpfn = 0;
        bo->placements[0].lpfn = 0; 
        bo->placements[0].mem_type = TTM_PL_SYSTEM;
        bo->placements[0].flags = pflag;
        placementnum = 1;
    }

    bo->placement.num_placement = placementnum;
    bo->placement.placement = bo->placements;
    bo->placement.num_busy_placement = placementnum;
    bo->placement.busy_placement = bo->placements;
}

/* Pin 函数实现 */
int prism_bo_pin(struct prism_bo *bo, int mem_type)
{
    struct ttm_buffer_object *tbo = &bo->tbo;
    struct ttm_operation_ctx ctx = { false, false };
    int ret;

    /* 1. 上锁 (Reserve)
     * 修改 BO 状态前必须加锁
     */
    ret = ttm_bo_reserve(tbo, true, false, NULL);
    if (ret)
        return ret;

    /* 2. 增加 Pin 计数器
     * 一个 BO 可能被多个使用者 Pin 住 (比如既做屏幕又做光标)
     * 只有计数器 > 0，它才是被钉住的。
     */
    if (tbo->pin_count) {
        tbo->pin_count++;
        goto out_unreserve;
    }

    /* 3. 修改策略，设置为“不可驱逐” */
    prism_ttm_placement(bo, mem_type);

    /* 4. 执行 Validate
     * TTM 会检查当前 BO 是否在这个位置。
     * 如果不在，它会立刻搬运过来。
     * 然后给它打上 NO_EVICT 标签。
     */
    ret = ttm_bo_validate(tbo, &bo->placement, &ctx);
    if (ret) {
        dev_err(tbo->base.dev->dev, "Failed to pin bo\n");
        goto out_unreserve;
        /* 失败回滚策略 (通常恢复成默认的可驱逐策略) */
        // prism_ttm_placement_default(bo); 
    }

    tbo->pin_count = 1;

out_unreserve:
    /* 5. 统一解锁 */
    ttm_bo_unreserve(tbo);
    return ret;
}

/* Unpin 函数实现 */
int prism_bo_unpin(struct prism_bo *bo)
{
    struct ttm_buffer_object *tbo = &bo->tbo;
    struct ttm_operation_ctx ctx = { false, false };
    int ret;

    ret = ttm_bo_reserve(tbo, true, false, NULL);
    if (ret) return ret;

    if (tbo->pin_count) {
        tbo->pin_count--;
        if (tbo->pin_count == 0) {
            /* * 计数归零，拔掉钉子。
             * 恢复成默认策略：首选 VRAM，允许回退到 SYSTEM，允许驱逐
             */
            prism_ttm_placement(bo, PRISM_GEM_DOMAIN_VRAM);
            
            /* 重新 validate 一下以应用新策略 */
            ttm_bo_validate(tbo, &bo->placement, &ctx);
        }
    }

    ttm_bo_unreserve(tbo);
    return 0;
}

int prism_plane_prepare_fb(struct drm_plane *plane,
                                  struct drm_plane_state *new_state)
{
    struct drm_framebuffer *fb = new_state->fb;
    struct drm_gem_object *obj;
    struct prism_bo *bo;
    int ret;

    /* 如果这一帧没有 Framebuffer (比如关闭了这个 Plane)，直接返回 */
    if (!fb)
        return 0;

    /* 获取 GEM 对象 (Prism 驱动只支持单平面格式，取 obj[0]) */
    obj = fb->obj[0];
    bo = to_prism_bo(container_of(obj, struct ttm_buffer_object, base));

    /* * 调用底层的 Pin 函数。
     * TTM_PL_VRAM: 告诉它显示用的 Buffer 必须在显存里 
     */
    ret = prism_bo_pin(bo, PRISM_GEM_DOMAIN_VRAM);
    if (ret)
        return ret;

    return 0;
}

/* * 钩子 2: cleanup_fb 
 * DRM 核心会在“画面切换完成后”调用这个函数。
 * 任务：拿到“上一帧”显示的 BO，给它解锁。
 */
void prism_plane_cleanup_fb(struct drm_plane *plane,
                                   struct drm_plane_state *old_state)
{
    struct drm_framebuffer *fb = old_state->fb;
    struct drm_gem_object *obj;
    struct prism_bo *bo;

    if (!fb)
        return;

    obj = fb->obj[0];
    bo = to_prism_bo(container_of(obj, struct ttm_buffer_object, base));

    /* 调用底层的 Unpin 函数 */
    prism_bo_unpin(bo);
}

static void prism_bo_delete_notify(struct ttm_buffer_object *tbo)
{
    struct prism_bo *bo = to_prism_bo(tbo);
    
    /* 释放 GEM 对象资源 */
    drm_gem_object_release(&tbo->base);
    /* 释放结构体内存 */
    kfree(bo);
}

struct prism_bo *prism_ttm_bo_create(struct prism_device *pdev,
                                     size_t size,
                                     uint32_t domain_flags)
{
    struct ttm_operation_ctx ctx = { false, false };
    struct prism_bo *bo;
    int ret;

    bo = kzalloc(sizeof(*bo), GFP_KERNEL);
    if (!bo) return ERR_PTR(-ENOMEM);

    size = PAGE_ALIGN(size);

    /* 初始化 GEM Base */
    bo->tbo.base.funcs = &prism_gem_object_funcs; 
    ret = drm_gem_object_init(&pdev->drm, &bo->tbo.base, size);
    if (ret) {
        kfree(bo);
        return ERR_PTR(ret);
    }

    /* 配置策略 */
    prism_ttm_placement(bo, domain_flags);

    /* TTM 初始化与分配 */
    ret = ttm_bo_init_reserved(&pdev->pttm,     // 1. TTM 设备指针
                               &bo->tbo,             // 2. BO 对象
                               size,                 // 3. 【关键补丁】 显存大小
                               ttm_bo_type_device,   // 4. BO 类型 (注意用小写)
                               &bo->placement,       // 5. 放置策略
                               0,                    // 6. 页对齐
                               &ctx,                 // 7. 上下文
                               NULL,                 // 8. SG Table (散列表)
                               NULL,                 // 9. Resv (预留锁对象，传 NULL 会自动分配)
                               prism_bo_delete_notify); // 10. 析构回调函数
    if (ret) {
        drm_gem_object_release(&bo->tbo.base);
        return ERR_PTR(ret);
    }

    /* 成功后解锁 */
    ttm_bo_unreserve(&bo->tbo);
    return bo;
}