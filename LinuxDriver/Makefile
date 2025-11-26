# 模块最终生成的名称 (prism-drv.ko)
MODULE_NAME := prism-drv

# 模块由哪些对象文件组成
# 这里将 prism.c 编译为 prism.o，最终链接成 prism-drv.ko
$(MODULE_NAME)-y := prism_drv.o prism_plane.o
obj-m += $(MODULE_NAME).o

# 内核构建目录
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 编译器标志 (使用 ccflags-y 替代旧的 EXTRA_CFLAGS)
ccflags-y := -Wall -Wextra -DDEBUG
# 如果需要特定的 DRM 头文件路径
ccflags-y += -I$(KDIR)/include/drm

# 防止 modpost 警告
KBUILD_MODPOST_WARN=1

all:
	@echo "Building kernel module $(MODULE_NAME) in $(PWD)..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@echo "Cleaning up build files..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean

INSTALL_PATH ?= /lib/modules/$(shell uname -r)/extra

install: all
	@echo "Installing module to $(INSTALL_PATH)..."
	@sudo mkdir -p $(INSTALL_PATH)
	@sudo install -m 644 $(MODULE_NAME).ko $(INSTALL_PATH)/
	@echo "Running depmod to update dependencies..."
	@sudo depmod -a
	@echo "Install complete. You can now use 'sudo modprobe $(MODULE_NAME)'"

uninstall:
	@echo "Removing module from $(INSTALL_PATH)..."
	@sudo rm -f $(INSTALL_PATH)/$(MODULE_NAME).ko
	@echo "Running depmod..."
	@sudo depmod -a
	@echo "Uninstall complete."

# --- 增强的加载命令 ---
load: all
	@echo "Attempting to load dependencies..."
	@-sudo modprobe drm
	@-sudo modprobe drm_kms_helper
	@-sudo modprobe drm_vram_helper
	@-sudo modprobe drm_ttm_helper
	@echo "Loading module $(MODULE_NAME).ko..."
	@# 如果 insmod 失败，打印 dmesg 的最后几行帮助调试
	@if ! sudo insmod ./$(MODULE_NAME).ko; then \
		echo ""; \
		echo "Error loading module. Last 10 lines of dmesg:"; \
		echo "---------------------------------------------"; \
		dmesg | tail -n 10; \
		echo "---------------------------------------------"; \
		exit 1; \
	fi
	@echo "Module loaded successfully!"

unload:
	@echo "Unloading module $(MODULE_NAME)..."
	@if lsmod | grep -q "^$(MODULE_NAME)"; then \
		sudo rmmod $(MODULE_NAME); \
		echo "Unloaded."; \
	else \
		echo "Module not loaded."; \
	fi