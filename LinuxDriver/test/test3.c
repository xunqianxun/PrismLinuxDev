/*
 * test_modeset.c - Simple DRM Modesetting Test for Prism Driver
 * Compile with: gcc -o test_modeset test_modeset.c -I/usr/include/libdrm -ldrm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* 根据你的日志修改这些 ID，或者让程序自动查找 */
#define TARGET_CONNECTOR_ID 36
#define TARGET_CRTC_ID      34

int main() {
    int fd;
    drmModeRes *res;
    drmModeConnector *conn;
    uint32_t fb_id;
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};
    struct drm_mode_destroy_dumb destroy_req = {0};
    void *map;
    int ret;

    printf("=== Prism DRM Modeset Test ===\n");

    // 1. 打开设备
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("Cannot open /dev/dri/card0");
        return 1;
    }

    // 2. 获取资源 (检查驱动是否正常加载)
    res = drmModeGetResources(fd);
    if (!res) {
        perror("Cannot get resources");
        return 1;
    }
    printf("Resources: %d connectors, %d CRTCs\n", res->count_connectors, res->count_crtcs);

    // 3. 获取 Connector 信息
    conn = drmModeGetConnector(fd, TARGET_CONNECTOR_ID);
    if (!conn) {
        perror("Cannot get connector");
        return 1;
    }

    if (conn->connection != DRM_MODE_CONNECTED) {
        printf("Connector %d is NOT connected!\n", TARGET_CONNECTOR_ID);
        return 1;
    }

    if (conn->count_modes == 0) {
        printf("No valid modes found on connector %d\n", TARGET_CONNECTOR_ID);
        return 1;
    }

    // 使用第一个可用模式
    drmModeModeInfo mode = conn->modes[0];
    printf("Selected Mode: %s (%dx%d)\n", mode.name, mode.hdisplay, mode.vdisplay);

    // 4. 创建 Dumb Buffer (显存分配)
    // 这里的宽度、高度、bpp 必须和硬件要求匹配
    create_req.width = mode.hdisplay;
    create_req.height = mode.vdisplay;
    create_req.bpp = 32; // XRGB8888
    
    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
    if (ret < 0) {
        perror("Create dumb buffer failed");
        return 1;
    }
    printf("Created Dumb Buffer: Handle=%d, Pitch=%d, Size=%llu\n", 
           create_req.handle, create_req.pitch, create_req.size);

    // 5. 添加 Framebuffer 对象
    ret = drmModeAddFB(fd, create_req.width, create_req.height, 24, 32, 
                       create_req.pitch, create_req.handle, &fb_id);
    if (ret) {
        perror("drmModeAddFB failed");
        return 1;
    }
    printf("Created Framebuffer: ID=%d\n", fb_id);

    // 6. 映射显存并写入颜色 (白色)
    // 这一步测试 mmap 接口是否正常
    map_req.handle = create_req.handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
    if (ret) {
        perror("Map dumb failed");
        return 1;
    }

    map = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
    if (map == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // 填充满白色 (0xFFFFFFFF)
    memset(map, 0xFF, create_req.size);
    printf("Buffer filled with white color.\n");

    // 7. 设置 CRTC (最关键的一步: Modeset!)
    // 这会触发驱动中的 atomic_commit 或 set_config
    printf("Setting CRTC %d to Mode %s...\n", TARGET_CRTC_ID, mode.name);
    ret = drmModeSetCrtc(fd, TARGET_CRTC_ID, fb_id, 0, 0, &conn->connector_id, 1, &mode);
    
    if (ret) {
        perror("drmModeSetCrtc FAILED");
        // 如果这里失败，通常是 atomic_check 或 atomic_update 有问题
    } else {
        printf("SUCCESS! Screen should be white now.\n");
        printf("Sleeping for 5 seconds...\n");
        sleep(5);
    }

    // 清理
    munmap(map, create_req.size);
    drmModeRmFB(fd, fb_id);
    destroy_req.handle = create_req.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    close(fd);

    return 0;
}
