#include "test_drv.h"

/* ========================================================
 * 3. 硬件寄存器操作辅助函数
 * ======================================================== */

void prism_write_reg(struct prism_device *pdev, u32 offset, u32 val)
{
    /* 注意：mmio_virt 是 prism_head 类型，这里为了通用演示简化了写入 */
    iowrite32(val, (void __iomem *)pdev->mmio_virt + offset);
}

uint32_t prism_read_reg(struct prism_device *pdev, u32 offset)
{
    return ioread32((void __iomem *)pdev->mmio_virt + offset);
}

/* * mmap 映射：这是真实驱动的关键。
 * 当用户 mmap GEM handle 时，我们需要把 VRAM 映射给用户。
 * DRM 核心会自动调用 ttm_bo_mmap。
 * 我们只需要确保 ttm_device_init 时传入了正确的 mapping。
 */
static const struct file_operations prism_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .mmap = drm_gem_mmap, /* 标准 GEM mmap，底层会调 TTM */
    .llseek = noop_llseek,
};

static struct drm_driver prism_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
    .fops = &prism_fops,

    .dumb_create = prism_gem_dumb_create,
    .dumb_map_offset = prism_gem_dumb_map_offset,

    .prime_handle_to_fd = drm_gem_prime_handle_to_fd,
    .prime_fd_to_handle = drm_gem_prime_fd_to_handle,

    .name = "prism",
    .desc = "Driver for Prism Educational GPU",
    .date = "20251122",
    .major = 1,
    .minor = 0,
};

/* ========================================================
 * 6. PCI Probe - 真实硬件映射核心逻辑
 * ======================================================== */

static int prism_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct prism_device *prism;
    struct drm_device *ddev;
    int ret;

    /* 1. 启用 PCI 设备 */
    ret = pci_enable_device(pdev);
    if (ret) return ret;

    /* 设置 DMA 掩码 (假设设备支持 64位 DMA) */
    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));

    /* 2. 分配 DRM 设备结构 */
    prism = devm_drm_dev_alloc(&pdev->dev, &prism_driver, struct prism_device, drm);
    if (IS_ERR(prism)) return PTR_ERR(prism);

    ddev = &prism->drm;
    prism->pdev = pdev;
    pci_set_drvdata(pdev, ddev);

    /* 3. 请求 PCI BAR 区域 (防止冲突) */
    ret = pci_request_regions(pdev, "prism-drv");
    if (ret) return ret;

    /* ---------------------------------------------------
     * 真实驱动关键步骤：BAR 映射
     * --------------------------------------------------- */

    /* --- 映射 BAR 0: VRAM (64MB) --- */
    prism->vram_base_phys = pci_resource_start(pdev, PRISM_VRAM_BAR_IDX);
    prism->vram_size = pci_resource_len(pdev, PRISM_VRAM_BAR_IDX);

    if (prism->vram_size < PRISM_VRAM_SIZE) {
        dev_warn(&pdev->dev, "VRAM BAR too small: %llx\n", prism->vram_size);
        // 这里可以报错退出，或者只使用实际大小
    }

    /* 使用 ioremap_wc (Write Combine) 以获得最佳 VRAM 写性能 */
    prism->vram_virt = pci_iomap_wc(pdev, PRISM_VRAM_BAR_IDX, 0);
    if (!prism->vram_virt) {
        dev_err(&pdev->dev, "Failed to map VRAM BAR\n");
        ret = -ENOMEM;
        goto err_release_regions;
    }

    /* --- 映射 BAR 2: MMIO Registers (配置寄存器) --- */
    prism->mmio_base_phys = pci_resource_start(pdev, PRISM_MMIO_BAR_IDX);
    prism->mmio_size = pci_resource_len(pdev, PRISM_MMIO_BAR_IDX);

    /* MMIO 必须使用不可缓存映射 (ioremap / pci_iomap) */
    prism->mmio_virt = pci_iomap(pdev, PRISM_MMIO_BAR_IDX, 0);
    if (!prism->mmio_virt) {
        dev_err(&pdev->dev, "Failed to map MMIO BAR\n");
        ret = -ENOMEM;
        goto err_unmap_vram;
    }

    dev_info(&pdev->dev, "Mapped VRAM at %p (phys %llx), MMIO at %p\n",
             prism->vram_virt, prism->vram_base_phys, prism->mmio_virt);

    /* 读取一下 Hardware Signature 验证映射是否成功 */
    dev_info(&pdev->dev, "Prism HW Signature: 0x%08x\n", 
             ioread32(&prism->mmio_virt->signature));

    /* ---------------------------------------------------
     * TTM 初始化
     * --------------------------------------------------- */
    
    prism_ttm_init(prism, prism->vram_size);
    if (ret) goto err_unmap_mmio;

    /* * 初始化 VRAM 管理器。
     * 注意：ttm_range_man_init 管理的是“从 0 开始的偏移量”。
     * TTM 自身不绑定物理地址。物理地址在 DRM 用户空间 mmap 时通过 
     * vm_pgoff 结合 vram_base_phys 计算得出。
     */
    ret = ttm_range_man_init(&prism->pttm, TTM_PL_VRAM, false, prism->vram_size >> PAGE_SHIFT);
    if (ret) goto err_ttm_fini;

    ret = ttm_range_man_init(&prism->pttm, TTM_PL_SYSTEM, true, 0);
    if (ret) goto err_man_fini;

    /* 注册 DRM 设备 */
    ret = drm_dev_register(ddev, 0);
    if (ret) goto err_man_fini;

    return 0;

err_man_fini:
    ttm_range_man_fini(&prism->pttm, TTM_PL_VRAM);
err_ttm_fini:
    ttm_device_fini(&prism->pttm);
err_unmap_mmio:
    pci_iounmap(pdev, prism->mmio_virt);
err_unmap_vram:
    pci_iounmap(pdev, prism->vram_virt);
err_release_regions:
    pci_release_regions(pdev);
    return ret;
}

static void prism_pci_remove(struct pci_dev *pdev)
{
    struct drm_device *ddev = pci_get_drvdata(pdev);
    struct prism_device *prism = to_prism_dev(ddev);

    drm_dev_unregister(ddev);
    
    ttm_range_man_fini(&prism->pttm, TTM_PL_VRAM);
    ttm_range_man_fini(&prism->pttm, TTM_PL_SYSTEM);
    ttm_device_fini(&prism->pttm);

    pci_iounmap(pdev, prism->mmio_virt);
    pci_iounmap(pdev, prism->vram_virt);
    pci_release_regions(pdev);
}

/* 假设 Prism 设备的 Vendor ID 为 0x1234, Device ID 为 0x9999 */
static const struct pci_device_id prism_pci_table[] = {
    { PCI_DEVICE(0x1a03, 0x2000) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, prism_pci_table);

static struct pci_driver prism_pci_driver = {
    .name = "prism-drv",
    .id_table = prism_pci_table,
    .probe = prism_pci_probe,
    .remove = prism_pci_remove,
};

module_pci_driver(prism_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Prism Dev Team");
MODULE_DESCRIPTION("Prism GPU Driver for Kernel 5.15");