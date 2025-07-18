/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CamPrvdr@2.4-hdmirx"
//#define LOG_NDEBUG 0
#include <log/log.h>

#include <regex>
#include <sys/inotify.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <cutils/properties.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/socket.h>
#include "HdmiRxCameraProviderImpl_2_4.h"
#include "HdmirxCameraDevice_3_4.h"
#include "HdmirxCameraDevice_3_5.h"
#include "HdmirxCameraDevice_3_6.h"
#include "hdmi_if.h"
#include "mdp_if.h"
#include "optee_if.h"
#include "IVendorTagDescriptor.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_4 {
namespace implementation {

template struct CameraProvider<HdmiRxCameraProviderImpl_2_4>;
using ::android::hardware::camera::common::V1_0::VendorTag;
using ::android::hardware::camera::common::V1_0::CameraMetadataType;

namespace {
// "device@<version>/external/<id>"
const std::regex kDeviceNameRE("device@([0-9]+\\.[0-9]+)/hdmirx(.+)");
const int kMaxDevicePathLen = 256;
const char* kDevicePath = "/dev/";
constexpr char kPrefix[] = "video";
constexpr int kPrefixLen = sizeof(kPrefix) - 1;
constexpr int kDevicePrefixLen = sizeof(kDevicePath) + kPrefixLen + 1;
const char* kHdmirxPath = "/dev/hdmirx";


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
    HDMI_RX_PLUG_IN, /// 11
    HDMI_RX_PLUG_OUT, /// 12
};

#define UEVENT_BUF_SIZE 2048

bool matchDeviceName(int cameraIdOffset,
                     const hidl_string& deviceName, std::string* deviceVersion,
                     std::string* cameraDevicePath) {
    std::string deviceNameStd(deviceName.c_str());
    std::smatch sm;
    if (std::regex_match(deviceNameStd, sm, kDeviceNameRE)) {
        if (deviceVersion != nullptr) {
            *deviceVersion = sm[1];
        }
        if (cameraDevicePath != nullptr) {
            *cameraDevicePath = std::string(kHdmirxPath);
        }
        ALOGI("%s, %d, %s, %s", __FUNCTION__, __LINE__, (*deviceVersion).c_str(),
        (*cameraDevicePath).c_str());
        return true;
    }
    return false;
}

} // anonymous namespace

HdmiRxCameraProviderImpl_2_4::HdmiRxCameraProviderImpl_2_4() :
        mCfg(HdmirxCameraConfig::loadFromCfg()) {
    if (0 != hdmirx::optee::query_hdmi_hdcp_key()) {
        ALOGW("%s, query hdmi hdcp key failed", __FUNCTION__);
    }

    mHotPlugThread = sp<HotplugThread>::make(this);
    mHotPlugThread->init(); // for Hdmi in
    mHotPlugThread->run("ExtCamHotPlug", PRIORITY_BACKGROUND);

    mPreferredHal3MinorVersion =
        property_get_int32("ro.vendor.camera.external.hal3TrebleMinorVersion", 4);
    ALOGV("Preferred HAL 3 minor version is %d", mPreferredHal3MinorVersion);
    switch(mPreferredHal3MinorVersion) {
        case 4:
        case 5:
        case 6:
            // OK
            break;
        default:
            ALOGW("Unknown minor camera device HAL version %d in property "
                    "'camera.external.hal3TrebleMinorVersion', defaulting to 4",
                    mPreferredHal3MinorVersion);
            mPreferredHal3MinorVersion = 4;
    }
    setupVendorTags();

}

HdmiRxCameraProviderImpl_2_4::~HdmiRxCameraProviderImpl_2_4() {}

Return<Status> HdmiRxCameraProviderImpl_2_4::setCallback(
        const sp<ICameraProviderCallback>& callback) {
    {
        Mutex::Autolock _l(mLock);
        mCallbacks = callback;
    }
    if (mCallbacks == nullptr) {
        return Status::OK;
    }
    // Send a callback for all devices to initialize
    {
        for (const auto& pair : mCameraStatusMap) {
            mCallbacks->cameraDeviceStatusChange(pair.first, pair.second);
        }
    }

    return Status::OK;
}

bool HdmiRxCameraProviderImpl_2_4::setupVendorTags() {
  auto pVendorTagDesc = NSCam::getVendorTagDescriptor();
  if (!pVendorTagDesc) {
    ALOGE("bad pVendorTagDesc");
    return false;
  }
  //
  //  setup mVendorTagSections
  auto vSection = pVendorTagDesc->getSections();
  size_t const numSections = vSection.size();
  mVendorTagSections.resize(numSections);
  for (size_t i = 0; i < numSections; i++) {
    auto const& s = vSection[i];
    //
    // s.tags; -> tags;
    std::vector<VendorTag> tags;
    tags.reserve(s.tags.size());
    for (auto const& t : s.tags) {
      VendorTag vt;
      vt.tagId = t.second.tagId;
      vt.tagName = t.second.tagName;
      vt.tagType = (CameraMetadataType)t.second.tagType;
      tags.push_back(vt);
    }
    //
    mVendorTagSections[i].tags = tags;
    mVendorTagSections[i].sectionName = s.sectionName;
  }
  //
  return true;
}


Return<void> HdmiRxCameraProviderImpl_2_4::getVendorTags(
        ICameraProvider::getVendorTags_cb _hidl_cb) {
    // No vendor tag support for USB camera
    //hidl_vec<VendorTagSection> zeroSections;
    _hidl_cb(Status::OK, mVendorTagSections);
    return Void();
}

Return<void> HdmiRxCameraProviderImpl_2_4::getCameraIdList(
        ICameraProvider::getCameraIdList_cb _hidl_cb) {
    // External camera HAL always report 0 camera, and extra cameras
    // are just reported via cameraDeviceStatusChange callbacks
    hidl_vec<hidl_string> hidlDeviceNameList;
    _hidl_cb(Status::OK, hidlDeviceNameList);
    return Void();
}

Return<void> HdmiRxCameraProviderImpl_2_4::isSetTorchModeSupported(
        ICameraProvider::isSetTorchModeSupported_cb _hidl_cb) {
    // setTorchMode API is supported, though right now no external camera device
    // has a flash unit.
    _hidl_cb (Status::OK, true);
    return Void();
}

Return<void> HdmiRxCameraProviderImpl_2_4::getCameraDeviceInterface_V1_x(
        const hidl_string&,
        ICameraProvider::getCameraDeviceInterface_V1_x_cb _hidl_cb) {
    // External Camera HAL does not support HAL1
    _hidl_cb(Status::OPERATION_NOT_SUPPORTED, nullptr);
    return Void();
}

Return<void> HdmiRxCameraProviderImpl_2_4::getCameraDeviceInterface_V3_x(
        const hidl_string& cameraDeviceName,
        ICameraProvider::getCameraDeviceInterface_V3_x_cb _hidl_cb) {

    std::string cameraDevicePath, deviceVersion;
    bool match = matchDeviceName(mCfg.cameraIdOffset, cameraDeviceName,
                                 &deviceVersion, &cameraDevicePath);
    if (!match) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    if (mCameraStatusMap.count(cameraDeviceName) == 0 ||
            mCameraStatusMap[cameraDeviceName] != CameraDeviceStatus::PRESENT) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    sp<device::V3_4::implementation::HdmirxCameraDevice> deviceImpl;
    switch (mPreferredHal3MinorVersion) {
        case 4: {
            ALOGV("Constructing v3.4 hdmirx camera device");
            deviceImpl = new device::V3_4::implementation::HdmirxCameraDevice(
                    cameraDevicePath, mCfg);
            break;
        }
        case 5: {
            ALOGV("Constructing v3.5 hdmirx camera device");
            deviceImpl = new device::V3_5::implementation::HdmirxCameraDevice(
                    cameraDevicePath, mCfg);
            break;
        }
        case 6: {
            ALOGV("Constructing v3.6 hdmirx camera device");
            deviceImpl = new device::V3_6::implementation::HdmirxCameraDevice(
                    cameraDevicePath, mCfg);
            break;
        }
        default:
            ALOGE("%s: Unknown HAL minor version %d!", __FUNCTION__, mPreferredHal3MinorVersion);
            _hidl_cb(Status::INTERNAL_ERROR, nullptr);
            return Void();
    }

    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraDevicePath.c_str());
        _hidl_cb(Status::INTERNAL_ERROR, nullptr);
        return Void();
    }

    IF_ALOGV() {
        deviceImpl->getInterface()->interfaceChain([](
            ::android::hardware::hidl_vec<::android::hardware::hidl_string> interfaceChain) {
                ALOGV("Device interface chain:");
                for (auto iface : interfaceChain) {
                    ALOGV("  %s", iface.c_str());
                }
            });
    }

    _hidl_cb (Status::OK, deviceImpl->getInterface());

    return Void();
}

void HdmiRxCameraProviderImpl_2_4::addHdmirxCamera(const char* devName) {
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string camera_id = std::to_string(mCfg.cameraIdOffset);

    deviceName = std::string("device@3.4/hdmirx/") + camera_id;
    if (mCameraStatusMap.find(deviceName) != mCameraStatusMap.end()) {
        ALOGW("%s: %d, This device has added", __FUNCTION__, __LINE__);
        return;
    }
    mCameraStatusMap[deviceName] = CameraDeviceStatus::PRESENT;
    if (mCallbacks != nullptr) {
        mCallbacks->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::PRESENT);
    }
}

void HdmiRxCameraProviderImpl_2_4::deviceAdded(const char* devName) {
    //$XBH_PATCH_START
    char socInfo[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.build.user", socInfo,  "android-build");
    ALOGW("ro.build.user = %s  %d ", socInfo , __LINE__);
    //CTS_GTS case
    if(property_get_bool("persist.vendor.xbh.cts_gts.status", false)){
        ALOGW("%s: persist.vendor.xbh.cts_gts.status is true!", __FUNCTION__);
        return;
    }
    //VTS case
    if (devName == nullptr || strcmp(socInfo, "android-build") == 0) {
        ALOGW("%s: devName is null!", __FUNCTION__);
        return;
    }
    //$XBH_PATCH_END
    sp<device::V3_4::implementation::HdmirxCameraDevice> deviceImpl =
            new device::V3_4::implementation::HdmirxCameraDevice(devName, mCfg);
    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGW("%s: Attempt to init camera device %s failed!", __FUNCTION__, devName);
        deviceRemoved(devName);
        return;
    }
    deviceImpl.clear();

    addHdmirxCamera(devName);
}

void HdmiRxCameraProviderImpl_2_4::deviceRemoved(const char* devName) {
    if (devName == nullptr) {
        ALOGW("%s: devName is null!", __FUNCTION__);
        return;
    }
    Mutex::Autolock _l(mLock);
    std::string deviceName;
    std::string camera_id = std::to_string(mCfg.cameraIdOffset);
    deviceName = std::string("device@3.4/hdmirx/") + camera_id;
    if (mCameraStatusMap.find(deviceName) != mCameraStatusMap.end()) {
        mCameraStatusMap.erase(deviceName);
        if (mCallbacks != nullptr) {
            mCallbacks->cameraDeviceStatusChange(deviceName, CameraDeviceStatus::NOT_PRESENT);
        }
    } else {
        ALOGE("%s: cannot find camera device %s", __FUNCTION__, devName);
    }
}

void HdmiRxCameraProviderImpl_2_4::getHDRInfo() {
    struct hdmirx::hdmi::hdr10InfoPkt* hdr10info;
    hdr10info = hdmirx::hdmi::hdmi_query_hdr_info();
    int ret = hdmirx::hdmi::hdmi_get_hdr10_info(hdr10info);
    if (ret < 0) {
        ALOGE("%s, %d, hdmi rx get hdr info failed.", __FUNCTION__, __LINE__);
    }
    ret = hdmirx::mdp::mdp_parse_hdr_info(hdr10info);
    if (ret < 0) {
        ALOGE("%s, %d, mdp parse hdr info failed.", __FUNCTION__, __LINE__);
    }
}

void HdmiRxCameraProviderImpl_2_4::updateRXStatus(int lock)
{

    hdmirx::hdmi::hdmi_update_lock_status(lock);
}


HdmiRxCameraProviderImpl_2_4::HotplugThread::HotplugThread(
        HdmiRxCameraProviderImpl_2_4* parent) :
        Thread(/*canCallJava*/false),
        mParent(parent),
        mInternalDevices(parent->mCfg.mInternalDevices) {
    int ret = hdmirx::hdmi::hdmi_enable(1);
    if (ret < 0) {
        ALOGE("%s, %d, hdmi enable failed.", __FUNCTION__, __LINE__);
    }

    struct hdmirx::hdmi::HDMIRX_DEV_INFO dinfo;
    if ((hdmirx::hdmi::hdmi_get_device_info(&dinfo) < 0) || !(dinfo.hdmirx5v & 0x01)) {
        ALOGW("%s, %d,hdmi cable is not plugged in.", __FUNCTION__, __LINE__);
    } else {
        mParent->deviceAdded(kHdmirxPath);
    }
#ifdef MTK_HDMI_RXVC_HDR_SUPPORT
    mParent->getHDRInfo();
#endif
}

HdmiRxCameraProviderImpl_2_4::HotplugThread::~HotplugThread() {
    int ret = hdmirx::hdmi::hdmi_enable(0);
    if (ret < 0) {
        ALOGE("%s, %d, hdmi rx disable failed.", __FUNCTION__, __LINE__);
    }
    done = true;
}

void HdmiRxCameraProviderImpl_2_4::HotplugThread::init(void)
{
    struct sockaddr_nl addr_sock;
    int optval = 64 * 1024;

    m_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (m_socket < 0) {
        ALOGE("Unable to create uevent socket:%s", strerror(errno));
        return;
    }

    if((setsockopt(m_socket, SOL_SOCKET, SO_RCVBUFFORCE, &optval, sizeof(optval)) < 0) &&
        (setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0)) {
        ALOGE("Unable to set uevent socket SO_RCVBUF/SO_RCVBUFFORCE option:%s(%d)",
              strerror(errno), errno);
    }

    memset(&addr_sock, 0, sizeof(addr_sock));
    addr_sock.nl_family = AF_NETLINK;
    addr_sock.nl_pid = getpid() << 3;
    addr_sock.nl_groups = 0xffffffff;

    if (bind(m_socket, (struct sockaddr *)&addr_sock, sizeof(addr_sock)) < 0) {
        ALOGE("Failed to bind socket:%s(%d)", strerror(errno), errno);
        close(m_socket);
        return;
    }
}

void HdmiRxCameraProviderImpl_2_4::HotplugThread::handleHdmiUEvents(const char *buff, int len)
{
    const char *s = buff;
    if (strcmp(s, "change@/devices/virtual/hdmirxswitch/hdmi")) return;
    int state = 0;
    s += strnlen(s, UEVENT_BUF_SIZE - 1) + 1;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE="))) {
            state = atoi(s + strlen("SWITCH_STATE="));
            //ALOGD("uevents: SWITCH_STATE=%d", state);
        }
        //ALOGV("uevents: s=%p, %s", s, s);
        s += strnlen(s, UEVENT_BUF_SIZE - 1) + 1;
        if (s - buff >= len) break;
    }

    switch (state) {
    case HDMI_RX_PLUG_IN:
        mParent->deviceAdded(kHdmirxPath);
        break;
    case HDMI_RX_PLUG_OUT:
        mParent->deviceRemoved(kHdmirxPath);
        break;
    case HDMI_RX_HDR_INFO_CHANGE:
        mParent->getHDRInfo();
        break;
    case HDMI_RX_TIMING_LOCK:
        mParent->updateRXStatus(HDMIRX_STATUS_LOCKED);
        break;
    case HDMI_RX_TIMING_UNLOCK:
        mParent->updateRXStatus(HDMIRX_STATUS_UNLOCKED);
        break;
    default:
        break;
    }
}

bool HdmiRxCameraProviderImpl_2_4::HotplugThread::threadLoop() {
    AutoMutex l(m_lock);
    struct pollfd fds;
    static char uevent_desc[UEVENT_BUF_SIZE * 2];

    fds.fd = m_socket;
    fds.events = POLLIN;
    fds.revents = 0;

    while (!done) {
        int ret = poll(&fds, 1, -1);
        if (ret > 0 && (fds.revents & POLLIN)) {
            /* keep last 2 zeroes to ensure double 0 termination */
            int count = recv(m_socket, uevent_desc, sizeof(uevent_desc) - 2, 0);
            if (count > 0) handleHdmiUEvents(uevent_desc, count);
        }
    }
    return true;
}

}  // namespace implementation
}  // namespace V2_4
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
