
#include <fcntl.h>
#include <log/log.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hdmi_event.h"
#include "inc/hdmi_if.h"

using ::android::sp;

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

/* copy from kernel, drivers/misc/mediatek/hdmirx/hdmirx.h */

#define HDMI_IOW(num, dtype) _IOW('H', num, dtype)
// #define HDMI_IOR(num, dtype) _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype) _IOWR('H', num, dtype)
// #define HDMI_IO(num) _IO('H', num)

#define MTK_HDMIRX_VID_INFO HDMI_IOWR(1, struct HDMIRX_VID_PARA)
// #define MTK_HDMIRX_AUD_INFO HDMI_IOWR(2, struct HDMIRX_AUD_INFO)
#define MTK_HDMIRX_ENABLE HDMI_IOW(3, unsigned int)
#define MTK_HDMIRX_DEV_INFO HDMI_IOWR(4, struct HDMIRX_DEV_INFO)
#define HDMIRX_DEV_PATH "/dev/hdmirx"

/*****************************************************************************/

/* 1: enable, otherwise: disable */
int hdmi_enable(uint64_t en) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    int fd = open(HDMIRX_DEV_PATH, O_WRONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d", __FUNCTION__, HDMIRX_DEV_PATH, fd);
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_ENABLE, en);
    close(fd);
    return ret;
}

/* could be called only after video locked */
int hdmi_get_video_info(struct HDMIRX_VID_PARA *vinfo) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (vinfo == nullptr) return -EINVAL;

    int fd = open(HDMIRX_DEV_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d", __FUNCTION__, HDMIRX_DEV_PATH, fd);
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_VID_INFO, vinfo);
    close(fd);
    return ret;
}

int hdmi_get_device_info(struct HDMIRX_DEV_INFO *dinfo) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dinfo == nullptr) return -EINVAL;

    int fd = open(HDMIRX_DEV_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d", __FUNCTION__, HDMIRX_DEV_PATH, fd);
        return fd;
    }
    int ret = ioctl(fd, MTK_HDMIRX_DEV_INFO, dinfo);
    close(fd);
    return ret;
}

uint8_t hdmi_check_cable(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d)) return 0;
    return d.hdmirx5v & 0x1;
}

uint8_t hdmi_check_video_locked(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d)) return 0;
    return d.vid_locked;
}

/*****************************************************************************/

static sp<HdmiUEventThread> event_thread;

int hdmi_start_observing(
    struct tv_input_device *dev /* = nullptr */,
    int (*tv_cb)(struct tv_input_device *) /* = nullptr */) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    event_thread = new HdmiUEventThread();
    if (event_thread == nullptr) {
        ALOGE("create uevent thread fail");
        return -EINVAL;
    }
    event_thread->init();
    event_thread->regCallback(dev, tv_cb);
    return event_thread->run("HdmiUEventThread");
}

void hdmi_stop_observing(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (event_thread) {
        event_thread->requestExit();
        // do not wait to avoid hang-up until next event coming
        // event_thread->requestExitAndWait();
        event_thread = nullptr;
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
