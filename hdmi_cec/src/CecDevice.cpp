
#define DEBUG_LOG_TAG "HWCEC"

#include <android-base/logging.h>
#include <errno.h>
#include <linux/cec.h>
#include <linux/ioctl.h>
#include <sys/eventfd.h>
#include <algorithm>
#include <hardware/hdmi_cec.h>

#include "CecDevice.h"
#include "utils/cec_debug.h"

CecDevice::CecDevice(unsigned int portId) {
    mPortId = portId;
    mCecFd = -1;
    mEventThreadExitFd = -1;
    mUeventNode = "change@/devices/virtual/switch/cec_hdmi";
    mUeventCnnProp = "SWITCH_STATE=";
    mConnectStateNode = "/sys/class/switch/cec_hdmi/state";
    mType = CEC_DEVICE_PLAYBACK;
    mIsDpDevice = false;
    mCapsName = "";
    mCapsDriver = "";
    HWCEC_LOGI("CecDevice create \n");
}

CecDevice::~CecDevice() {
    release();
}


int CecDevice::init(const char* path) {

    mCecFd = open(path, O_RDWR);
    if (mCecFd < 0) {
        //LOG(ERROR) << "Failed to open " << path << ", Error = " << strerror(errno);
        return -1;
    }

    mEventThreadExitFd = eventfd(0, EFD_NONBLOCK);
    if (mEventThreadExitFd < 0) {
        //LOG(ERROR) << "Failed to open eventfd, Error = " << strerror(errno);
        release();
        return -1;
    }

    // Ensure the CEC device supports required capabilities
    struct cec_caps caps = {};
    int ret = ioctl(mCecFd, CEC_ADAP_G_CAPS, &caps);
    if (ret) {
        //LOG(ERROR) << "Unable to query cec adapter capabilities, Error = " << strerror(errno);
        release();
        return -1;
    }

    mCapsName = caps.name;
    mCapsDriver = caps.driver;
    HWCEC_LOGI("caps_name %s, caps_driver %s\n", mCapsName.c_str(), mCapsDriver.c_str());

    if (!(caps.capabilities & (CEC_CAP_LOG_ADDRS | CEC_CAP_TRANSMIT | CEC_CAP_PASSTHROUGH))) {
        //LOG(ERROR) << "Wrong cec adapter capabilities " << caps.capabilities;
        release();
        return -1;
    }

    uint32_t mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER_PASSTHRU;
    ret = ioctl(mCecFd, CEC_S_MODE, &mode);
    if (ret) {
        //LOG(ERROR) << "Unable to set initiator mode, Error = " << strerror(errno);
        release();
        return -1;
    }

    return 0;
}

void CecDevice::release() {
    HWCEC_LOGI("CecDevice release \n");
    if (mEventThreadExitFd > 0) {
        uint64_t tmp = 1;
        write(mEventThreadExitFd, &tmp, sizeof(tmp));
        close(mEventThreadExitFd);
        mEventThreadExitFd = -1;
    }

    if (mCecFd > 0) {
        close(mCecFd);
        mCecFd = -1;
    }
}

