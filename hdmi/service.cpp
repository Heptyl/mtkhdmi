#define LOG_TAG "vendor.mediatek.hardware.hdmi@1.4-service"

#include <vendor/mediatek/hardware/hdmi/1.4/IMtkHdmiService.h>

#include <hidl/HidlTransportSupport.h>
#include <hidl/LegacySupport.h>

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::defaultPassthroughServiceImplementation;

using vendor::mediatek::hardware::hdmi::V1_4::IMtkHdmiService;

int main() {
    ALOGD("HIDL MtkHdmiService() main");
    return defaultPassthroughServiceImplementation<IMtkHdmiService>();
}
