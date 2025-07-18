
#include <fcntl.h>
#include <log/log.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>

#include "inc/stc_if.h"

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

/* copy from kernel, drivers/misc/mediatek/stc/stc.h */

struct mtk_stc_info {
    int stc_id;
    int64_t stc_value;
};

// #define MTK_STC_IOW(num, dtype) _IOW('B', num, dtype)
#define MTK_STC_IOR(num, dtype) _IOR('B', num, dtype)
// #define MTK_STC_IOWR(num, dtype) _IOWR('B', num, dtype)
// #define MTK_STC_IO(num) _IO('B', num)

// #define MTK_STC_IOCTL_ALLOC MTK_STC_IO(0x0)
// #define MTK_STC_IOCTL_FREE MTK_STC_IO(0x1)
// #define MTK_STC_IOCTL_START MTK_STC_IOW(0x2, int)
// #define MTK_STC_IOCTL_STOP MTK_STC_IOW(0x3, int)
// #define MTK_STC_IOCTL_SET MTK_STC_IOW(0x4, struct mtk_stc_info)
#define MTK_STC_IOCTL_GET MTK_STC_IOR(0x5, struct mtk_stc_info)
// #define MTK_STC_IOCTL_ADJUST MTK_STC_IOW(0x6, struct mtk_stc_info)
#define STC_DEV_PATH "/dev/mtk_stc"

/*****************************************************************************/

/* stc was supposed to start already */
int stc_get(int64_t *stc) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (stc == nullptr) return -EINVAL;

    struct mtk_stc_info info = {};
    int fd = open(STC_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s, open %s fail %d", __FUNCTION__, STC_DEV_PATH, fd);
        return fd;
    }
    int ret = ioctl(fd, MTK_STC_IOCTL_GET, &info);
    close(fd);
    if (ret >= 0) *stc = info.stc_value;
    return ret;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
