#include "test_drv.h"

/* ========================================================
 * 1. MMAP 缺页中断处理 (VM Operations)
 * ======================================================== 
 * 当用户 mmap 显存并尝试访问时，CPU 会触发缺页中断。
 * 这里负责把中断转发给 TTM，由 TTM 建立页表。
 */

static vm_fault_t prism_gem_fault(struct vm_fault *vmf)
{
    struct ttm_buffer_object *tbo = vmf->vma->vm_private_data;
    
    /* 转发给 TTM 核心处理 */
    return ttm_bo_vm_fault_reserved(vmf, vmf->vma->vm_page_prot, 
                                    TTM_BO_VM_NUM_PREFAULT);
}

static void prism_gem_vm_open(struct vm_area_struct *vma)
{
    struct drm_gem_object *obj = vma->vm_private_data;
    struct prism_bo *bo = to_prism_bo(container_of(obj, struct ttm_buffer_object, base));
    
    /* 这里可以添加 Prism 特定的引用逻辑，通常留空即可 */
    /* DRM 核心已经处理了 obj 的引用计数 */
}

static void prism_gem_vm_close(struct vm_area_struct *vma)
{

}

static const struct vm_operations_struct prism_gem_vm_ops = {
    .fault = prism_gem_fault,
    .open = prism_gem_vm_open,
    .close = prism_gem_vm_close,
};

/* ========================================================
 * 2. GEM Object Functions (对象行为表)
 * ======================================================== 
 */

static void prism_gem_free_object(struct drm_gem_object *obj)
{
    struct ttm_buffer_object *tbo = container_of(obj, struct ttm_buffer_object, base);
    
    /* 减少 TTM 引用。当归零时，会触发 prism_bo_delete_notify */
    ttm_bo_put(tbo);
}

static void prism_gem_print_info(struct drm_printer *p, unsigned int indent,
                                 const struct drm_gem_object *obj)
{
    /* 1. 获取 TTM BO */
    struct ttm_buffer_object *tbo = container_of(obj, struct ttm_buffer_object, base);
    
    /* 2. 获取当前 BO 所在的资源区域 (VRAM 或 SYSTEM) */
    /* 注意：tbo->resource 可能为空（如果尚未分配） */
    const char *placement_name = "UNKNOWN";
    
    if (tbo->resource) {
        switch (tbo->resource->mem_type) {
        case TTM_PL_VRAM:
            placement_name = "VRAM";
            break;
        case TTM_PL_SYSTEM:
            placement_name = "SYSTEM";
            break;
        default:
            placement_name = "OTHER";
            break;
        }
    }

    /* 3. 打印信息 */
    drm_printf_indent(p, indent, "placement: %s\n", placement_name);
    drm_printf_indent(p, indent, "size: %zu\n", tbo->base.size);
    drm_printf_indent(p, indent, "pin_count: %d\n", tbo->pin_count);
}

/* * 这是一个全局常量结构体。
 * 我们需要在 prism_ttm_bo_create 时把它赋值给 bo->base.funcs
 */
const struct drm_gem_object_funcs prism_gem_object_funcs = {
    .free = prism_gem_free_object,
    .print_info = prism_gem_print_info,
    .vm_ops = &prism_gem_vm_ops,      /* 关键：关联 mmap 处理函数 */
};

/* ========================================================
 * 3. Dumb Buffer Create (分配入口)
 * ======================================================== 
 */

int prism_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
                          struct drm_mode_create_dumb *args)
{
    struct prism_device *pdev = to_prism_dev(dev);
    struct prism_bo *bo;
    int ret;

    /* 1. 计算大小与步长 */
    args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
    args->size = args->pitch * args->height;
    args->size = PAGE_ALIGN(args->size);

    /* 2. 调用 TTM 后端分配 (默认放在 VRAM) 
     * 这里的 PRISM_PL_VRAM 来自 prism_ttm.h
     */
    bo = prism_ttm_bo_create(pdev, args->size, PRISM_GEM_DOMAIN_VRAM);
    if (IS_ERR(bo))
        return PTR_ERR(bo);

    /* 3. 创建 GEM Handle */
    ret = drm_gem_handle_create(file, &bo->tbo.base, &args->handle);

    /* 4. 释放多余引用 (handle_create 持有一份，bo_create 持有一份) */
    drm_gem_object_put(&bo->tbo.base);

    return ret;
}

/* ========================================================
 * 4. MMAP Offset 查找
 * ======================================================== 
 */

int prism_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
                              uint32_t handle, uint64_t *offset)
{
    struct drm_gem_object *obj;
    int ret;

    obj = drm_gem_object_lookup(file, handle);
    if (!obj)
        return -ENOENT;

    /* 这是一个 lazy 操作：只有用户第一次请求 map 时才创建 offset 节点 */
    ret = drm_gem_create_mmap_offset(obj);
    if (ret == 0)
        *offset = drm_vma_node_offset_addr(&obj->vma_node);

    drm_gem_object_put(obj);
    return ret;
}