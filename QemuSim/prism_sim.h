#ifndef PRISM_SIM_H
#define PRISM_SIM_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "hw/pci/pci_device.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "hw/display/bochs-vbe.h"
#include "hw/display/edid.h"
#include "qapi/error.h"


#include "ui/console.h"
#include "ui/qemu-pixman.h"
#include "qom/object.h"


#define TYPE_PRISM_SIM "prism-sim"

#define PCI_PRISM_MMIO_SIZE     0x40

#define PRISM_REGISTER_SIZE   4*16  //16个寄存器，每个寄存器4字节
#define PRISM_SIM_REG_NUMBER 15
#define PRISM_SIM_REG_OFFSET 0x00

#define PRISM_SIM_MODE_REG_FORMATE 0 
#define PRISM_SIM_MODE_REG_BYREPP  1
#define PRISM_SIM_MODE_REG_WIDTH   2
#define PRISM_SIM_MODE_REG_HEIGHT  3
#define PRISM_SIM_MODE_REG_STRIDE  4
#define PRISM_SIM_MODE_REG_OFFSET  5
#define PRISM_SIM_MODE_REG_SIZE    6

struct PrismDisplayMode
 {
    pixman_format_code_t format   ;//格式
    uint32_t             bytepp   ;//每个像素所占的字节数
    uint32_t             width    ;//水平分辨率
    uint32_t             height   ;//垂直分辨率
    uint32_t             stride   ;//步幅，从一行的开头到下一行开头的字节数
    uint64_t             offset   ;//指定了显示缓冲区在显存中的偏移量
    uint64_t             size     ;//显存的大小
 };

typedef struct PrismDisplayMode PrismDisplayMode;

struct PrismSimState {
    PCIDevice pci;
    QemuConsole *con;
    MemoryRegion vram;
    MemoryRegion mmio;
    MemoryRegion preg;

    uint64_t vgamem; //vram size
    uint32_t prism_reg[PRISM_SIM_REG_NUMBER];
    PrismDisplayMode mode;
    bool big_endian_fb;
};

typedef struct PrismSimState PrismSimState;

struct PrismSimClass
{
    PCIDeviceClass pci;
    //暂时这样
};

typedef struct PrismSimClass PrismSimClass;

#endif /* PRISM_SIM_H */