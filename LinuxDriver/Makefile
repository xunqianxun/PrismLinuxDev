MAKEFLAGS += --no-print-directory

MODULE_NAME := prism-drv

$(MODULE_NAME)-objs := prism.o

obj-m += $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

EXTRA_CFLAGS += -Wall -Wextra -DDEBUG

EXTRA_CFLAGS += -I$(KDIR)/include/drm


all:
	@echo "Building kernel module in $(PWD)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "Cleaning up build files..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean


INSTALL_PATH ?= /lib/modules/$(shell uname -r)/extra

install: all
	@echo "Installing module to $(INSTALL_PATH)..."
	@mkdir -p $(INSTALL_PATH)
	@install -m 644 $(MODULE_NAME).ko $(INSTALL_PATH)/
	@echo "Running depmod..."
	@/sbin/depmod -a
	@echo "Install complete. You can now use 'modprobe $(MODULE_NAME)'"

uninstall:
	@echo "Removing module from $(INSTALL_PATH)..."
	@rm -f $(INSTALL_PATH)/$(MODULE_NAME).ko
	@echo "Running depmod..."
	@/sbin/depmod -a
	@echo "Uninstall complete."


load: all
	@echo "Loading module $(MODULE_NAME).ko..."
	@sudo insmod ./$(MODULE_NAME).ko

unload:
	@echo "Unloading module $(MODULE_NAME)..."
	@sudo rmmod $(MODULE_NAME) || true