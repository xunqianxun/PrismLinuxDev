#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/bug.h> 
#include <linux/pci.h>
#include <linux/slab.h>

#include <drm/drm_drv.h>

#define PRISMDEBUG

#define DRV_NAME "PrismGPU-V1"
#define PRISM_PCI_VENDOR_ID 0x1A03
#define PRISM_PCI_DEVICE_ID 0x2000
//for prism_private reg
#define PRISM_PCI_REG_SIZE 4 * 16

static const struct drm_driver drm_prism_sim_driver ;

struct prism_private prism_private;

struct prism_private {
    volatile uint32_t __iomem* regs;
    struct drm_connector connector;
    struct drm_encoder encoder;
    struct drm_crtc crtc;
}

static const struct drm_mode_config_funcs prism_drv_mode_funcs = {
    
}

static struct drm_device *prism_drm_init(struct device *dev, prism_private *private){
    struct drm_device *drm_dev = drm_dev_alloc(&drm_prism_sim_driver, dev)
    int rc = drmm_mode_config_init(drm_dev);
    if(ret){
        goto error;
    }

	drm_dev->dev_private = private;

	drm_dev->mode_config.funcs = &sphaero_drv_mode_funcs;
	drm_dev->mode_config.min_width = 32;
	drm_dev->mode_config.min_height = 32;
	drm_dev->mode_config.max_width = 1024;
	drm_dev->mode_config.max_height = 768;

    drm_connector_init(drm_dev, )

}

static int prism_pci_init(struct pci_dev *pdev, prism_private *priv){

    int ret = 0;

    ret = pci_enable_device(pdev);
    if(ret){
        return ret;
    }
    ret = pci_request_region(pdev);
    if(ret){
        pci_disable_device(pdev);
    }
   resource_size_t pciaddr = pci_resource_start(pdev,0); 
   prism_private->regs = ioremap(pciaddr, PRISM_PCI_REG_SIZE);
   return ret;

}
int prism_probe (struct pci_dev *pdev, const struct pci_device_id *id){
    int ret = 0 ;

    prism_private* private = kzalloc(sizeof(prism_private), GFP_KERNEL);

    ret = prism_pci_init(pdev, private);
    if(ret){
        goto err_pci_init;    
    }

    struct drm_device *drm_dev = prism_drm_init()

}

static const struct pci_device_id pci_table[] = {
        { PCI_DEVICE(PRISM_PCI_VENDOR_ID,     PRISM_PCI_DEVICE_ID), },
        { },
};

static struct pci_driver prism_driver = {
    .name         = DRV_NAME,
	.id_table     = pci_table,
    .probe        = prism_probe,
    .remove       = prism_remove,
};


static struct drm_driver drm_prism_sim_driver = {
    .driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_RENDER | DRIVER_ATOMIC
                          DRIVER_SYNCOBJ,
    .name = "prism",
    .desc = "Prism Simple KMS DRM Driver",
    .date = "20251116",
    .major = 1,
    .minor = 0
};

//打印函数调用栈dump_stack()；
static int __init prism_init(void)
{
	#ifdef PRISMDEBUG
    	printk(KERN_INFO "加载函数成功!\n");
    #endif 
	return pci_register_driver(&prism_driver);
}

static void __exit prism_exit(void)
{
    #ifdef PRISMDEBUG
        printk(KERN_INFO "卸载函数成功!\n");
    #endif 
	pci_unregister_driver(&prism_driver);
}

module_init(prism_init);
module_exit(prism_exit);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guoqi Li");
MODULE_DESCRIPTION("For PrismGPU develop kernel module.");