#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main(int argc, char **argv)
{
    int fd;
    drmModeRes *resources;
    drmModeConnector *connector = NULL;
    drmModeEncoder *encoder = NULL;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    uint32_t conn_id;

    /* 1. 打开 DRM 设备
     * 对于 QEMU/Bochs，通常是 card0
     */
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("无法打开 /dev/dri/card0");
        return 1;
    }

    /* 2. 获取资源 (Connectors, Encoders, CRTCs) */
    resources = drmModeGetResources(fd);
    if (!resources) {
        perror("无法获取 DRM 资源");
        return 1;
    }

    /* 3. 寻找第一个连接状态为 "CONNECTED" 的连接器 */
    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            break;
        }
        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (!connector) {
        fprintf(stderr, "未找到连接的显示器\n");
        return 1;
    }
    conn_id = connector->connector_id;

    /* 4. 选择分辨率模式
     * 这里简单粗暴地选择该连接器的第一个可用模式
     */
    if (connector->count_modes == 0) {
        fprintf(stderr, "该连接器没有有效的模式\n");
        return 1;
    }
    mode = connector->modes[0];
    printf("选择模式: %s @ %dx%d\n", mode.name, mode.hdisplay, mode.vdisplay);

    /* 5. 寻找对应的 CRTC
     * 流程：Connector -> Encoder -> CRTC
     */
    if (connector->encoder_id) {
        encoder = drmModeGetEncoder(fd, connector->encoder_id);
    } else {
        // 如果当前没有绑定 encoder，找第一个可用的
        encoder = drmModeGetEncoder(fd, connector->encoders[0]);
    }

    if (encoder) {
        crtc_id = encoder->crtc_id;
        // 如果当前 encoder 没有绑定 CRTC，或者我们想简单点，
        // 可以遍历 resources->crtcs 找到一个匹配 encoder->possible_crtcs 的。
        // 但在 bochs/qemu 上，通常第一个 CRTC 就可以用。
        if (!crtc_id) {
             crtc_id = resources->crtcs[0];
        }
        drmModeFreeEncoder(encoder);
    } else {
         // 兜底策略
         crtc_id = resources->crtcs[0];
    }
    
    /* 6. 创建 Dumb Buffer (显存)
     * 这是 bochs/virtio-gpu 等没有硬件加速的驱动的标准做法。
     */
    struct drm_mode_create_dumb create_req = {0};
    create_req.width = mode.hdisplay;
    create_req.height = mode.vdisplay;
    create_req.bpp = 32; // 32位色深 (XRGB8888)
    
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        perror("无法创建 Dumb Buffer");
        return 1;
    }

    /* 7. 创建 Framebuffer 对象 (FB)
     * 将刚才创建的 Dumb Buffer 包装成 DRM 能够理解的 FB 对象
     */
    uint32_t fb_id;
    if (drmModeAddFB(fd, create_req.width, create_req.height, 24, 32, 
                     create_req.pitch, create_req.handle, &fb_id)) {
        perror("无法创建 Framebuffer");
        return 1;
    }

    /* 8. 准备内存映射 (mmap)
     * 为了在 CPU 上画图，我们需要将 Dumb Buffer 映射到用户空间指针。
     */
    struct drm_mode_map_dumb map_req = {0};
    map_req.handle = create_req.handle;
    
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req)) {
        perror("无法准备映射 Dumb Buffer");
        return 1;
    }

    uint32_t *map = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
    if (map == MAP_FAILED) {
        perror("mmap 失败");
        return 1;
    }

    /* 9. 画图！
     * 现在 map 指针指向了屏幕内存。我们可以直接写入颜色。
     * 格式通常是 XRGB8888 (Blue, Green, Red, X)
     */
    printf("开始绘制...\n");
    
    // 全屏填充蓝色
    for (int i = 0; i < (create_req.size / 4); i++) {
        map[i] = 0xFF0000FF; // Blue
    }

    // 画一个红色的矩形在中间
    int start_x = mode.hdisplay / 4;
    int start_y = mode.vdisplay / 4;
    int end_x = start_x * 3;
    int end_y = start_y * 3;
    
    int stride = create_req.pitch / 4; // 以 uint32_t 为单位的步长

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            map[y * stride + x] = 0xFFFF0000; // Red
        }
    }

    /* 10. Modesetting (点亮屏幕)
     * 将 CRTC、Connector 和 FB 绑定，真正将图像推送到屏幕上。
     */
    if (drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &mode)) {
        perror("Modesetting 失败");
        return 1;
    }

    printf("屏幕已点亮！按回车键退出并清理...\n");
    getchar();

    /* 11. 清理工作 */
    munmap(map, create_req.size);
    drmModeRmFB(fd, fb_id);
    
    struct drm_mode_destroy_dumb destroy_req = {0};
    destroy_req.handle = create_req.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(fd);

    return 0;
}