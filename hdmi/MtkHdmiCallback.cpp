// FIXME: your file license if you have one

#include "MtkHdmiCallback.h"

namespace vendor::mediatek::hardware::hdmi::implementation {

// Methods from ::vendor::mediatek::hardware::hdmi::V1_2::IMtkHdmiCallback follow.
Return<void> MtkHdmiCallback::onHdmiSettingsChange(int32_t mode, int32_t value) {
    // TODO implement
    return Void();
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

//IMtkHdmiCallback* HIDL_FETCH_IMtkHdmiCallback(const char* /* name */) {
    //return new MtkHdmiCallback();
//}
//
}  // namespace vendor::mediatek::hardware::hdmi::implementation
