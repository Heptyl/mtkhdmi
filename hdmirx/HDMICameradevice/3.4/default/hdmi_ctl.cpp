#define LOG_TAG "HdmirxCamDevSsn@3.4_HDMI"

#include <fcntl.h>
#include <log/log.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#include "hdmi_if.h"
#include "utils/Mutex.h"


namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace hdmi {


/* copy from kernel, drivers/misc/mediatek/hdmirx/hdmirx.h */

#define HDMI_IOW(num, dtype) _IOW('H', num, dtype)
// #define HDMI_IOR(num, dtype) _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype) _IOWR('H', num, dtype)
// #define HDMI_IO(num) _IO('H', num)

#define MTK_HDMIRX_VID_INFO HDMI_IOWR(1, struct HDMIRX_VID_PARA)
// #define MTK_HDMIRX_AUD_INFO HDMI_IOWR(2, struct HDMIRX_AUD_INFO)
#define MTK_HDMIRX_ENABLE HDMI_IOW(3, unsigned int)
#define MTK_HDMIRX_DEV_INFO HDMI_IOWR(4, struct HDMIRX_DEV_INFO)
#define MTK_HDMIRX_PKT HDMI_IOWR(6, struct hdr10InfoPkt)
#define MTK_HDMIRX_AVI HDMI_IOWR(8, struct hdr10InfoPkt)
#define HDMIRX_DEV_PATH "/dev/hdmirx"

#define IOCTL_LOCK_RETRY_SLEEP_US (100000) // 100ms
#define IOCTL_LOCK_RETRY_COUNT (5)

static int g_Hdmirx_lock_status = -1;
static Mutex m_hdmiLock;
static struct HDMIRX_VID_PARA g_vinfo;
static struct HDMIRX_DEV_INFO g_devinfo;
static struct hdr10InfoPkt g_hdrinfo;

/*****************************************************************************/

/* 1: enable, otherwise: disable */
int hdmi_enable(uint64_t en) {
    int fd = open(HDMIRX_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d, %s", __FUNCTION__, HDMIRX_DEV_PATH, fd, strerror(errno));
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_ENABLE, en);
    close(fd);
    return ret;
}

int hdmi_get_hdr10_info(struct hdr10InfoPkt *vinfo) {
    if (vinfo == nullptr) return -EINVAL;
    int fd = open(HDMIRX_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d, %s", __FUNCTION__, HDMIRX_DEV_PATH, fd, strerror(errno));
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_PKT, vinfo);
    close(fd);
    return ret;
}

/* could be called only after video locked */
int hdmi_get_video_info(struct HDMIRX_VID_PARA *vinfo) {
    if (vinfo == nullptr) return -EINVAL;
    int fd = open(HDMIRX_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d, %s", __FUNCTION__, HDMIRX_DEV_PATH, fd, strerror(errno));
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_VID_INFO, vinfo);
    close(fd);
    return ret;
}

int hdmi_get_device_info(struct HDMIRX_DEV_INFO *dinfo) {
    if (dinfo == nullptr) return -EINVAL;
    int fd = open(HDMIRX_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d, %s", __FUNCTION__, HDMIRX_DEV_PATH, fd, strerror(errno));
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_DEV_INFO, dinfo);
    close(fd);
    return ret;
}

uint8_t hdmi_check_cable(void) {
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d)) return 0;
    return d.hdmirx5v & 0x1;
}

uint8_t hdmi_check_video_locked(void) {
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d)) return 0;
    return d.vid_locked;
}

uint8_t hdmi_check_hdcp_version(void) {
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d)) return 0;
    ALOGD("%s, %d, hdcp version: %d", __FUNCTION__, __LINE__, d.hdcp_version);
    if (d.hdcp_version == 14 || d.hdcp_version == 22)
        return 1;
    else
        return 0;
}

void hdmi_update_lock_status(int lock)
{
    uint8_t wait_lock_count = 0;

    Mutex::Autolock _l(m_hdmiLock);

    if (g_Hdmirx_lock_status == HDMIRX_STATUS_UNDEFINED) // first query video before rx timing lock, for boot up with rx cable case
    {
        while(hdmi_check_video_locked() != 1)
        {
            ALOGI("hdmi_update_lock_status wait rx lock");
            usleep(IOCTL_LOCK_RETRY_SLEEP_US);
            wait_lock_count ++;

            if (wait_lock_count > IOCTL_LOCK_RETRY_COUNT)
                break;
        }

        if (wait_lock_count > IOCTL_LOCK_RETRY_COUNT)
            g_Hdmirx_lock_status = HDMIRX_STATUS_UNLOCKED;
        else
            g_Hdmirx_lock_status = HDMIRX_STATUS_LOCKED;
    }
    else
        g_Hdmirx_lock_status = lock;


    if (g_Hdmirx_lock_status == HDMIRX_STATUS_LOCKED)
    {
        if (hdmi_get_video_info(&g_vinfo) < 0)
        {
            ALOGE("%s, %d, check video info failed.", __FUNCTION__, __LINE__);
        }
        else
        {
            ALOGI("%s, %d, check video info id=%u, w=%u, h=%u.", __FUNCTION__, __LINE__,
                g_vinfo.id, g_vinfo.hactive, g_vinfo.vactive);
        }

        if (hdmi_get_device_info(&g_devinfo) < 0)
        {
            ALOGE("%s, %d, check device info failed.", __FUNCTION__, __LINE__);
        }
        else
        {
            ALOGI("%s, %d, check device info hdcp_ver=%u, hdcp14_decrypted=%u, hdcp22_decrypted=%u.", __FUNCTION__, __LINE__,
                g_devinfo.hdcp_version, g_devinfo.hdcp14_decrypted, g_devinfo.hdcp22_decrypted);

        }

        if (hdmi_get_hdr10_info(&g_hdrinfo) < 0)
        {
            ALOGE("%s, %d, check hdr10 info failed.", __FUNCTION__, __LINE__);
        }
        else
        {
            ALOGI("%s, %d, check device info hdr type=%u.", __FUNCTION__, __LINE__, g_hdrinfo.type);

        }

    }
    else
    {
        memset((void*)&g_vinfo, 0, sizeof(HDMIRX_DEV_INFO));
        memset((void*)&g_devinfo, 0, sizeof(HDMIRX_DEV_INFO));
        memset((void*)&g_hdrinfo, 0, sizeof(hdr10InfoPkt));

        ALOGI("%s, %d, hdmirx unlock.", __FUNCTION__, __LINE__);
    }

}

bool hdmi_get_lock_status(void)
{
    Mutex::Autolock _l(m_hdmiLock);

    return g_Hdmirx_lock_status;
}


int hdmi_update_video_info(void)
{
    int fd = open(HDMIRX_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d, %s", __FUNCTION__, HDMIRX_DEV_PATH, fd, strerror(errno));
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_VID_INFO, &g_vinfo);
    close(fd);
    return ret;
}

struct HDMIRX_VID_PARA* hdmi_query_video_info(void)
{
    if (g_Hdmirx_lock_status == HDMIRX_STATUS_UNDEFINED) // first query without update lock status ever
        hdmi_update_lock_status(HDMIRX_STATUS_UNDEFINED);

    if (g_Hdmirx_lock_status == HDMIRX_STATUS_LOCKED)
        return &g_vinfo;
    else
        return NULL;
}

struct HDMIRX_DEV_INFO* hdmi_query_device_info(void)
{
    if (g_Hdmirx_lock_status == HDMIRX_STATUS_LOCKED)
        return &g_devinfo;
    else
        return NULL;
}

struct hdr10InfoPkt* hdmi_query_hdr_info(void)
{

    if (g_Hdmirx_lock_status == HDMIRX_STATUS_LOCKED)
        return &g_hdrinfo;
    else
        return NULL;
}



}
}
}
}
}
