#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "grallocdev.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <utils/Trace.h>
#include <system/graphics.h>
#include <graphics_mtk_defs.h>




#include <android/hardware/graphics/common/1.0/types.h>

namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace gralloc {


static uint32_t AHardwareBuffer_convertFromPixelFormat(unsigned int fmt) {
    switch (fmt) {
    case HAL_PIXEL_FORMAT_YV12:
        return AHARDWAREBUFFER_FORMAT_YV12;
    case HAL_PIXEL_FORMAT_YUYV:
        return AHARDWAREBUFFER_FORMAT_YCbCr_422_I;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        return AHARDWAREBUFFER_FORMAT_YCrCb_420_SP;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case HAL_PIXEL_FORMAT_RGB_888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    case HAL_PIXEL_FORMAT_RGB_565:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case HAL_PIXEL_FORMAT_RGBA_1010102:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    default:
        ALOGW("%s: HAL_PIXEL_FORMAT (0x%x) is not support", __func__, fmt);
        break;
    }
    return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
}

GrallocDevice& GrallocDevice::getInstance()
{
    static GrallocDevice gInstance;
    return gInstance;
}

GrallocDevice::GrallocDevice()
{
}

GrallocDevice::~GrallocDevice()
{
    for (auto& buf : m_buffers)
    {
        buffer_handle_t handle = buf.first;
        AHardwareBuffer* buffer = buf.second;
        ALOGV("%s: rm hnd(%p) hwb(%p)", __func__, handle, buffer);
        AHardwareBuffer_release(buffer);
    }
    m_buffers.clear();
}

status_t GrallocDevice::alloc(AllocParam& param) {
    AHardwareBuffer* buffer = nullptr;
    AHardwareBuffer_Desc desc;

    desc.width = param.width;
    desc.height= param.height;
    desc.layers= 1;
    desc.format= AHardwareBuffer_convertFromPixelFormat(param.format);
    desc.usage = param.usage;
    desc.rfu0 = 0;
    desc.rfu1 = 0;

    status_t error = AHardwareBuffer_allocate(&desc, &buffer);
    buffer_handle_t buffer_hnd = AHardwareBuffer_getNativeHandle(buffer);

    if (buffer && error == NO_ERROR && buffer_hnd != nullptr) {
        AutoMutex l(m_buffers_mutex);
        m_buffers[buffer_hnd] = buffer;
        param.handle = buffer_hnd;
        ALOGV("%s: add hnd(%p) hwb(%p)", __func__, buffer_hnd, buffer);
        return NO_ERROR;
    } else {
        ALOGE("%s: Failed to allocate (%u x %u) format %d error %d", __func__, param.width, param.height, param.format, error);
        return NO_MEMORY;
    }
}

status_t GrallocDevice::free(buffer_handle_t handle)
{
    ATRACE_CALL();
    status_t err = NO_ERROR;
    AutoMutex l(m_buffers_mutex);
    if (m_buffers.find(handle) != m_buffers.end())
    {
        AHardwareBuffer* buffer = nullptr;
        buffer = m_buffers[handle];
        ALOGV("%s: rm hnd(%p) hwb(%p)", __func__, handle, buffer);
        AHardwareBuffer_release(buffer);
        m_buffers.erase(handle);
    }
    else
    {
        ALOGE("Failed to free buffer handle(%p): can't find handle in table",
                handle);
        err = INVALID_OPERATION;
    }

    return err;
}

void GrallocDevice::dump() const
{
    // TODO: dump allocated buffer record
}

}
}
}
}
}
