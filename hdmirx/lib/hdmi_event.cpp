
#include "hdmi_event.h"

#include <cutils/properties.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/Log.h>
#include <utils/threads.h>

#define UEVENT_BUF_SIZE 2048

using android::AutoMutex;

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

/* copy from kernel, drivers/misc/mediatek/hdmirx/hdmirx.h */
enum HDMIRX_NOTIFY_T {
    HDMI_RX_PWR_5V_CHANGE = 0,
    HDMI_RX_TIMING_LOCK,
    HDMI_RX_TIMING_UNLOCK,
    HDMI_RX_AUD_LOCK,
    HDMI_RX_AUD_UNLOCK,
    HDMI_RX_ACP_PKT_CHANGE,
    HDMI_RX_AVI_INFO_CHANGE,
    HDMI_RX_AUD_INFO_CHANGE,
    HDMI_RX_HDR_INFO_CHANGE,
    HDMI_RX_EDID_CHANGE,
    HDMI_RX_HDCP_VERSION,
    HDMI_RX_PLUG_IN,   /// 11
    HDMI_RX_PLUG_OUT,  /// 12
};

/*****************************************************************************/

HdmiUEventThread::HdmiUEventThread(void)
    : m_socket(-1), _dev(nullptr), _tv_cb(nullptr) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
}

HdmiUEventThread::~HdmiUEventThread() {
    if (m_socket > 0) {
        close(m_socket);
    }
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
}

void HdmiUEventThread::init(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    struct sockaddr_nl addr_sock;
    int optval = 64 * 1024;

    m_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (m_socket < 0) {
        ALOGE("Unable to create uevent socket:%s", strerror(errno));
        return;
    }
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVBUFFORCE, &optval,
                   sizeof(optval)) < 0) {
        ALOGE("Unable to set uevent socket SO_RCVBUF option:%s(%d)",
              strerror(errno), errno);
    }

    memset(&addr_sock, 0, sizeof(addr_sock));
    addr_sock.nl_family = AF_NETLINK;
    // addr_sock.nl_pad = 0xcec;
    addr_sock.nl_pid = getpid() << 3;
    addr_sock.nl_groups = 0xffffffff;

    ALOGI("Start to initialize, nl_pid(%d)", addr_sock.nl_pid);
    if (bind(m_socket, (struct sockaddr *)&addr_sock, sizeof(addr_sock)) < 0) {
        ALOGE("Failed to bind socket:%s(%d)", strerror(errno), errno);
        close(m_socket);
        return;
    }
}

void HdmiUEventThread::regCallback(struct tv_input_device *dev,
                                   int (*tv_cb)(struct tv_input_device *)) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    _dev = dev;
    _tv_cb = tv_cb;
}

void HdmiUEventThread::handleHdmiUEvents(const char *buff, int len) {
    // ALOGV("%s@%d", __FUNCTION__, __LINE__);
    const char *s = buff;
    if (strcmp(s, "change@/devices/virtual/hdmirxswitch/hdmi")) return;
    int state = 0;
    s += strnlen(s, UEVENT_BUF_SIZE - 1) + 1;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE="))) {
            state = atoi(s + strlen("SWITCH_STATE="));
            ALOGD("uevents: SWITCH_STATE=%d", state);
        }
        ALOGV("uevents: s=%p, %s", s, s);
        s += strnlen(s, UEVENT_BUF_SIZE - 1) + 1;
        if (s - buff >= len) break;
    }

    switch (state) {
    case HDMI_RX_PWR_5V_CHANGE:
    case HDMI_RX_AVI_INFO_CHANGE:
        if (_tv_cb) (*_tv_cb)(_dev);
        break;
    default:
        break;
    }
}

bool HdmiUEventThread::threadLoop() {
    // ALOGV("%s@%d", __FUNCTION__, __LINE__);
    AutoMutex l(m_lock);
    struct pollfd fds;
    static char uevent_desc[UEVENT_BUF_SIZE * 2];

    fds.fd = m_socket;
    fds.events = POLLIN;
    fds.revents = 0;
    int ret = poll(&fds, 1, -1);

    if (ret > 0 && (fds.revents & POLLIN)) {
        /* keep last 2 zeroes to ensure double 0 termination */
        int count = recv(m_socket, uevent_desc, sizeof(uevent_desc) - 2, 0);
        // ALOGD("[MtkHdmiService]count = %d ", count);
        if (count > 0) handleHdmiUEvents(uevent_desc, count);
    }
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
