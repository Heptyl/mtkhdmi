// FIXME: your file license if you have one

#pragma once

#include <vendor/mediatek/hardware/hdmi/1.2/IMtkHdmiCallback.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace vendor::mediatek::hardware::hdmi::implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct MtkHdmiCallback : public V1_2::IMtkHdmiCallback {
    // Methods from ::vendor::mediatek::hardware::hdmi::V1_2::IMtkHdmiCallback follow.
    Return<void> onHdmiSettingsChange(int32_t mode, int32_t value) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.

};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" IMtkHdmiCallback* HIDL_FETCH_IMtkHdmiCallback(const char* name);

}  // namespace vendor::mediatek::hardware::hdmi::implementation
