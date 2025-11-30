#include "prism_drv.h"

/* * GEM 对象的释放 
 * 注意：我们不在这里 kfree，而是减少 TTM 的引用计数。
 * 当 TTM 计数为 0 时，会自动调用 prism_bo_destroy。
 */
static void prism_gem_free_object(struct drm_gem_object *obj)
{
    struct prism_bo *bo = to_prism_bo(obj);
    ttm_bo_put(&bo->tbo);
}

/* TTM 的 mmap 入口 */
static int prism_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct drm_gem_object *obj = filp->private_data; // 这里 DRM 核心可能处理方式不同，看具体 fops
    struct prism_device *pdev;
    
    /* 通常我们通过 drm_gem_mmap 进来，然后调用 ttm_bo_mmap */
    /* 需要从 file_priv 获取 device */
    struct drm_file *priv = filp->private_data;
    struct drm_device *dev = priv->minor->dev;
    pdev = to_prism(dev);

    return ttm_bo_mmap(filp, vma, &pdev->ttm);
}

/* GEM 函数表 */
static const struct drm_gem_object_funcs prism_gem_funcs = {
    .free = prism_gem_free_object,
    .print_info = drm_gem_print_info,
    /* 这里的 mmap 通常不需要，因为我们会在 fops 里覆盖 */
};

/* Dumb Create: 用户请求分配显存 */
int prism_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
                          struct drm_mode_create_dumb *args)
{
    struct prism_device *pdev = to_prism(dev);
    struct prism_bo *bo;
    int ret;

    args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
    args->size = args->pitch * args->height;

    /* * 初始创建在 SYSTEM 域。
     * 只有当它被挂到 Plane 上准备显示时，我们才 Pin 到 VRAM。
     * 这样可以节省宝贵的 64MB VRAM。
     */
    ret = prism_bo_create(pdev, args->size, false, TTM_PL_SYSTEM, &bo);
    if (ret) return ret;
    
    /* 关键：设置 GEM 函数表 */
    bo->gem.funcs = &prism_gem_funcs;

    ret = drm_gem_handle_create(file, &bo->gem, &args->handle);
    
    /* handle 持有引用，我们释放掉手中的 */
    drm_gem_object_put(&bo->gem);
    
    return ret;
}