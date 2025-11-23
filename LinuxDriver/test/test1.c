#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* 辅助函数：将连接状态转换为字符串 */
const char* conn_status_str(drmModeConnection status) {
    switch (status) {
        case DRM_MODE_CONNECTED: return "CONNECTED";
        case DRM_MODE_DISCONNECTED: return "DISCONNECTED";
        case DRM_MODE_UNKNOWNCONNECTION: return "UNKNOWN";
        default: return "INVALID";
    }
}

int main(int argc, char **argv)
{
    const char *dev_name = "/dev/dri/card0"; // 默认设备
    int fd;
    drmVersionPtr ver;
    drmModeResPtr res;

    /* 允许用户通过命令行参数指定设备，例如 ./prism_info /dev/dri/card1 */
    if (argc > 1) {
        dev_name = argv[1];
    }

    printf("Trying to open: %s\n", dev_name);
    fd = open(dev_name, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open %s: %s\n", dev_name, strerror(errno));
        fprintf(stderr, "Hint: Try using 'sudo' or check if the driver is loaded.\n");
        return 1;
    }

    /* 1. 获取并打印驱动版本信息 */
    ver = drmGetVersion(fd);
    if (ver) {
        printf("\n=== Driver Info ===\n");
        printf("Name:    %s\n", ver->name);
        printf("Desc:    %s\n", ver->desc);
        printf("Date:    %s\n", ver->date);
        printf("Version: %d.%d.%d\n", ver->version_major, ver->version_minor, ver->version_patchlevel);
        drmFreeVersion(ver);
    } else {
        fprintf(stderr, "Error: Failed to get driver version\n");
        close(fd);
        return 1;
    }

    /* 2. 获取 DRM 资源 (Connectors, Encoders, CRTCs) */
    res = drmModeGetResources(fd);
    if (!res) {
        fprintf(stderr, "Error: Failed to get DRM resources\n");
        close(fd);
        return 1;
    }

    printf("\n=== Resources ===\n");
    printf("Connectors: %d\n", res->count_connectors);
    printf("Encoders:   %d\n", res->count_encoders);
    printf("CRTCs:      %d\n", res->count_crtcs);
    printf("FBs:        %d\n", res->count_fbs);

    /* 3. 遍历并打印 Connectors */
    printf("\n=== Connectors ===\n");
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        printf("Connector ID: %d\n", conn->connector_id);
        printf("  Status:     %s\n", conn_status_str(conn->connection));
        printf("  Type:       %d\n", conn->connector_type);
        printf("  Encoders:   Current=%d, Possible_Count=%d\n", 
               conn->encoder_id, conn->count_encoders);
        
        /* 打印支持的分辨率模式 */
        if (conn->count_modes > 0) {
            printf("  Modes (%d):\n", conn->count_modes);
            for (int j = 0; j < conn->count_modes; j++) {
                printf("    [%d] %s @ %dHz (%dx%d)\n", 
                       j,
                       conn->modes[j].name, 
                       conn->modes[j].vrefresh,
                       conn->modes[j].hdisplay, 
                       conn->modes[j].vdisplay);
            }
        } else {
            printf("  Modes: None (Did get_modes callback fail?)\n");
        }

        drmModeFreeConnector(conn);
    }

    /* 4. 遍历并打印 Encoders */
    printf("\n=== Encoders ===\n");
    for (int i = 0; i < res->count_encoders; i++) {
        drmModeEncoderPtr enc = drmModeGetEncoder(fd, res->encoders[i]);
        if (!enc) continue;

        printf("Encoder ID: %d\n", enc->encoder_id);
        printf("  Type:           %d\n", enc->encoder_type);
        printf("  Crtc:           %d\n", enc->crtc_id);
        printf("  Possible CRTCs: 0x%x\n", enc->possible_crtcs);
        
        drmModeFreeEncoder(enc);
    }

    /* 5. 遍历并打印 CRTCs */
    printf("\n=== CRTCs ===\n");
    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtcPtr crtc = drmModeGetCrtc(fd, res->crtcs[i]);
        if (!crtc) continue;

        printf("CRTC ID: %d\n", crtc->crtc_id);
        printf("  FB ID:    %d\n", crtc->buffer_id);
        printf("  Position: (%d, %d)\n", crtc->x, crtc->y);
        if (crtc->mode_valid) {
            printf("  Current Mode: %dx%d\n", crtc->mode.hdisplay, crtc->mode.vdisplay);
        } else {
            printf("  Current Mode: Disabled\n");
        }

        drmModeFreeCrtc(crtc);
    }

    drmModeFreeResources(res);
    close(fd);
    return 0;
}