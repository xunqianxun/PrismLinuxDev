#include "prism_sim.h"


OBJECT_DECLARE_TYPE(PrismSimState, PrismSimClass, PRISM_SIM)

/*
 * prism display mode get
 *
 * get the current display mode from registers
 */
static int prism_display_get_mode(PrismSimState *s, PrismDisplayMode *mode){
    uint32_t *reg = s->prism_reg;

    mode->format = reg[PRISM_SIM_MODE_REG_FORMATE]; //暂时先这样简单来
    mode->bytepp = reg[PRISM_SIM_MODE_REG_BYREPP];
    mode->width  = reg[PRISM_SIM_MODE_REG_WIDTH];
    mode->height = reg[PRISM_SIM_MODE_REG_HEIGHT];
    mode->stride = reg[PRISM_SIM_MODE_REG_STRIDE];
    mode->offset = reg[PRISM_SIM_MODE_REG_OFFSET];
    mode->size   = reg[PRISM_SIM_MODE_REG_SIZE];

    return 0;
}


/*
 * qemu com update 
 *
 * update function 
 */
static void prism_sim_display_update(void *opaque)
{
    PrismSimState *s = opaque ;
    DirtyBitmapSnapshot *snap = NULL;
    bool full_update = false ;
    PrismDisplayMode mode;
    DisplaySurface *ds ;
    uint8_t *ptr ;
    bool dirty ;
    int y, ys, ret ;

    ret = prism_display_get_mode(s, &mode);

    if(ret < 0){
        return;
    }

    if(memcmp(&s->mode, &mode, sizeof(mode)) != 0){
        s->mode = mode;
        ptr = memory_region_get_ram_ptr(&s->vram);
        ds = qemu_create_displaysurface_from(mode.width,
                                             mode.height,
                                             mode.format,
                                             mode.stride,
                                             ptr + mode.offset);
        dpy_gfx_replace_surface(s->con, ds);
        full_update = true;
    }

    if(full_update){
        dpy_gfx_update_full(s->con);
    }
    else {
        snap = memory_region_snapshot_and_clear_dirty(&s->vram,
                                                      mode.offset, mode.size,
                                                      DIRTY_MEMORY_VGA);
        ys = -1;
        for (y = 0; y < mode.height; y++) {
            dirty = memory_region_snapshot_get_dirty(&s->vram, snap,
                                                     mode.offset + mode.stride * y,
                                                     mode.stride);
            if (dirty && ys < 0) {
                ys = y;
            }
            if (!dirty && ys >= 0) {
                dpy_gfx_update(s->con, 0, ys,
                               mode.width, y - ys);
                ys = -1;
            }
        }
        if (ys >= 0) {
            dpy_gfx_update(s->con, 0, ys,
                           mode.width, y - ys);
        }
    }

    g_free(snap);
}


/*
 * graphic ops 
 *
 * only realize update function now
 */
static GraphicHwOps prism_sim_gfx_ops = {
    .gfx_update = prism_sim_display_update,
};


/*
 * mode reg  read 
 *
 * read function for prism sim mode reg
 */
static uint64_t prism_sim_mode_reg_read(void *opaque,
                                     hwaddr addr,
                                     unsigned size)
{
    PrismSimState *s = opaque;

    unsigned int index = addr >> 2;
    return s->prism_reg[index];
}

/*
 * mode reg write
 *
 * write function for prism sim mode reg
 */
static void prism_sim_mode_reg_write(void *opaque,
                                      hwaddr addr,
                                      uint64_t val,
                                      unsigned size)
{
    PrismSimState *s = opaque;

    unsigned int index = addr >> 2;
    s->prism_reg[index] = val;
}


/*
 * prism display mode ctrl reg 
 *
 * realize the operation of prism sim mode reg
 */
static const MemoryRegionOps prism_sim_mode_reg_ops = {
    .read = prism_sim_mode_reg_read,
    .write = prism_sim_mode_reg_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};


/*
 * class realize
 *
 * Class for Prism Simulator PCI Device realize function
 */
static void prism_sim_realize(PCIDevice *dev, Error **errp)
{
    PrismSimState *s = PRISM_SIM(dev);
    Object *obj = OBJECT(dev);
    int ret;

    s->con = graphic_console_init(DEVICE(dev), 0, &prism_sim_gfx_ops, s);

    /* vram */
    memory_region_init_ram(&s->vram, obj, "prism-sim.vram",
                           s->vgamem, errp);
    if (*errp) {
        return;
    }
    pci_register_bar(&s->pci, 0, PCI_BASE_ADDRESS_SPACE_MEMORY 
                                | PCI_BASE_ADDRESS_MEM_TYPE_32 
                                | PCI_BASE_ADDRESS_MEM_PREFETCH, &s->vram);


    /* mmio */
    memory_region_init_io(&s->mmio, obj, &unassigned_io_ops,
                          s, "prism-sim.mmio", PCI_PRISM_MMIO_SIZE);

    memory_region_init_io(&s->preg, obj, &prism_sim_mode_reg_ops,
                          s, "prism-sim.mode-reg", PRISM_REGISTER_SIZE);
    memory_region_add_subregion(&s->mmio, PRISM_SIM_REG_OFFSET, &s->preg);

    pci_register_bar(&s->pci, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);
    if (pci_bus_is_express(pci_get_bus(dev))) {
        ret = pcie_endpoint_cap_init(dev, 0x80); //为了尽可能模拟真实硬件，能力列表为0x40-0xFF
        assert(ret > 0);
    } else {
        dev->cap_present &= ~QEMU_PCI_CAP_EXPRESS;
    }

    memory_region_set_log(&s->vram, true, DIRTY_MEMORY_VGA);
}

/*
 * PrismSim close
 *
 * close function for PrismSim device
 */

static void prism_sim_exit(PCIDevice *dev)
{
    PrismSimState *s = PRISM_SIM(dev);

    graphic_console_close(s->con);
}

/*
 * PrismSimClass
 *
 * Class for Prism Simulator PCI Device initialization
 */

static void prism_sim_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PrismSimClass *k = PRISM_SIM_CLASS(klass);

    k->pci.class_id = PCI_CLASS_OTHERS;
    k->pci.vendor_id = 0x1A03; //prism vendor id
    k->pci.device_id = 0x2000; //prism device id
    k->pci.realize = prism_sim_realize;
    k->pci.exit = prism_sim_exit;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

/*
 * endian_get
 *
 * property operate function for get endian framebuffer
 */
static bool prism_get_endian_fb(Object *obj, Error **errp)
{
    PrismSimState *s = PRISM_SIM(obj);
    return s->big_endian_fb;
}


/*
 * endian_set
 *
 * property operate function for set endian framebuffer
 */
static void prism_set_endian_fb(Object *obj, bool val, Error **errp)
{
    PrismSimState *s = PRISM_SIM(obj);
    s->big_endian_fb = val;
}


/*
 * instance_init
 *
 * prismsim device object struct initialize function
 */
static void prism_sim_init(Object *obj){
    PCIDevice *dev = PCI_DEVICE(obj);
    PrismSimState *s = PRISM_SIM(dev);

    s->vgamem = 16 * MiB; //default vram size

    dev->cap_present |= QEMU_PCI_CAP_EXPRESS;

    object_property_add_bool(obj, "prism-sim-endian-framebuffer", //this property can set by compiler option
                             prism_get_endian_fb,
                             prism_set_endian_fb);
}


static const TypeInfo prism_sim_type[] = {
    {
        .name          = TYPE_PRISM_SIM,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(PrismSimState),
        .class_size    = sizeof(PrismSimClass),
        .instance_init = prism_sim_init,
        .class_init    = prism_sim_class_init,
        .interfaces    = (const InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
        },
    }
};

DEFINE_TYPES(prism_sim_type)
