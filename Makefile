MAKEFLAGS += --no-print-directory

# Configuration vatiable 
CORES ?= 4
MEMORY ?= 4G
BOOT ?= 
DEBUG ?=
LOGFILE ?= qemu.log

# Directory variables
ROOT_DIR := $(shell pwd)


# TODO：you need to setting your qemu path
QEMU_DIR := ~/MainDir/Project/qemu
#！！！！！！！！！！！！！！！！！！！！

# TODO：you need to setting your ubuntu image path
ISO_DIR ?= ubuntu-20.04.6-desktop-amd64.iso
#！！！！！！！！！！！！！！！！！！！！

# img and ios path
# this is build by image name same as your iso name
IMG_DIR ?= ubuntu-20.04.6-desktop-amd64.img

# Source use
INSERT_KCONFIG := $(QEMU_DIR)/hw/misc/Kconfig
INSERT_MESON := $(QEMU_DIR)/hw/misc/meson.build
SRC_KCONFIG := Scripts/Kconfig
SRC_MESON := Scripts/meson.build

QEMU_BIN = $(QEMU_DIR)/build/qemu-system-x86_64
#support virtfs you can share the host file system with the vm
# eg:    mkdir -p path/shared
#        sudo mount -t 9p -o trans=virtio,version=9p2000.L host0 LinuxDriver"
QEMU_FLAGS  = -virtfs local,path=LinuxDriver,mount_tag=host0,security_model=passthrough,id=host0
QEMU_FLAGS += -hda $(IMG_DIR) -boot d -enable-kvm -machine q35 
QEMU_FLAGS += -device intel-iommu -smp $(CORES),sockets=1,cores=$(shell echo $$(($(CORES)/2))) -m $(MEMORY)
# you can use: ssh name@localhost -p 10021. to login the vm
QEMU_FLAGS += -net user,hostfwd=tcp::10021-:22
QEMU_FLAGS += -net nic,model=e1000
QEMU_FLAGS += -usbdevice tablet
QEMU_FLAGS += -device prism-sim

#ifeq ($(DEBUG), 1)
#QEMU_FLAGS += -D $(LOGFILE)
#endif 

ifeq ($(BOOT), 1)
QEMU_FLAGS +=  -cdrom $(ISO_DIR)
endif

image:
	@echo "Creating QEMU disk image: $(IMG_DIR)"
	@$(QEMU_DIR)/build/qemu-img create -f qcow2 $(IMG_DIR) 40G

source:
	@echo "Inserting PrismGPU source code into QEMU source files"
	@cp -r QemuSim $(QEMU_DIR)/hw/misc/
	@echo "Inserting $(SRC_KCONFIG) into $(INSERT_KCONFIG) before last line"
	@tmp=$$(mktemp); \
	head -n -1 $(INSERT_KCONFIG) > $$tmp; \
	cat $(SRC_KCONFIG) >> $$tmp; \
	tail -n 1 $(INSERT_KCONFIG) >> $$tmp; \
	mv $$tmp $(INSERT_KCONFIG)
	@echo "Inserting $(SRC_MESON) into $(INSERT_MESON) before last line"
	@cat $(SRC_MESON) >> $(INSERT_MESON)

#start qemu pre work
pre: source image

qemuconfig:
ifeq ($(DEBUG),1)
	@echo "Configuring QEMU with debug options"
	@cd $(QEMU_DIR) && mkdir -p build && cd build && ../configure --enable-kvm \
													 --target-list=x86_64-softmmu \
													 --disable-werror \
													 --enable-pixman \
													 --enable-gtk \
													 --enable-vte \
													 --enable-debug \
													 --disable-strip
else
	@echo "Configuring QEMU"
	@cd $(QEMU_DIR) && mkdir -p build && cd build && ../configure --enable-kvm \
													 --target-list=x86_64-softmmu \
													 --disable-werror \
													 --enable-pixman \
													 --enable-gtk \
													 --enable-vte 
endif

qemubuild:
	@echo "Building QEMU"
	@$(MAKE) -C $(QEMU_DIR)/build -j$(shell nproc)

# qemu compiler and build 
qemu: qemuconfig qemubuild

#running qemu
run:
	@echo "Starting QEMU"
	@$(QEMU_BIN) $(QEMU_FLAGS) -display gtk &


.PHONY: clean
clean:
	@echo "Cleaning all build"
	@rm -rf $(QEMU_DIR)/build 
	@truncate -s 0 $(LOGFILE)
