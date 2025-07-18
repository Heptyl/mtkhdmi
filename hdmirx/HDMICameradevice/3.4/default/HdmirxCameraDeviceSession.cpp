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
#define LOG_TAG "HdmirxCamDevSsn@3.4"
//#define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>

#include <inttypes.h>
#include "HdmirxCameraDeviceSession.h"
#include "mdp_if.h"
#include "mtk_metadata_tag.h"

#include "android-base/macros.h"
#include <utils/Timers.h>
#include <utils/Trace.h>
#include <linux/videodev2.h>
#include <sync/sync.h>
#include <cutils/properties.h>


#define HAVE_JPEG // required for libyuv.h to export MJPEG decode APIs
#include <libyuv.h>

#include <jpeglib.h>

#include <ui/gralloc_extra.h>


namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_4 {
namespace implementation {

using ::android::hardware::camera::hdmirx::mdp::mdp_open;
using ::android::hardware::camera::hdmirx::mdp::mdp_close;
using ::android::hardware::camera::hdmirx::mdp::mdp_trigger;
using ::android::hardware::camera::hdmirx::mdp::mdp_1in1out;
using ::android::hardware::camera::hdmirx::mdp::mdp_1in2out;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_check_video_locked;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_get_video_info;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_get_device_info;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_check_hdcp_version;
using ::android::hardware::camera::hdmirx::gralloc::GrallocDevice;
using ::android::hardware::camera::hdmirx::hdmi::HDMIRX_CS;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_get_lock_status;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_query_video_info;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_query_device_info;
using ::android::hardware::camera::hdmirx::hdmi::hdmi_query_hdr_info;


namespace {
// Size of request/result metadata fast message queue. Change to 0 to always use hwbinder buffer.
static constexpr size_t kMetadataMsgQueueSize = 1 << 18 /* 256kB */;

const int kBadFramesAfterStreamOn = 1; // drop x frames after streamOn to get rid of some initial
                                       // bad frames. TODO: develop a better bad frame detection
                                       // method
constexpr int MAX_RETRY = 15; // Allow retry some ioctl failures a few times to account for some
                             // webcam showing temporarily ioctl failures.
constexpr int IOCTL_RETRY_SLEEP_US = 33000; // 33ms * MAX_RETRY = 0.5 seconds

// Constants for tryLock during dumpstate
static constexpr int kDumpLockRetries = 50;
static constexpr int kDumpLockSleep = 60000;
const int kSyncWaitTimeoutMs = 5000;

enum {
    BUFFER_STATUS_FREE = 0,
    BUFFER_STATUS_DEQUEUED,
    BUFFER_STATUS_ENQUEUED
};


uint32_t logLevel = 0;
#define RXLOGV(...) do{ if (logLevel>=4){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGD(...) do{ if (logLevel>=3){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGI(...) do{ if (logLevel>=2){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGW(...) do{ if (logLevel>=1){ ALOGW(__VA_ARGS__); }}while(0)
#define RXLOGE(...) do{ ALOGE(__VA_ARGS__); }while(0)

#define IOCTL_LOCK_RETRY_SLEEP_US (100000) // 100ms
#define IOCTL_LOCK_RETRY_COUNT (5)

bool tryLock(Mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.tryLock() == NO_ERROR) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

bool tryLock(std::mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.try_lock()) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

} // Anonymous namespace

// Static instances
const int HdmirxCameraDeviceSession::kMaxProcessedStream;
const int HdmirxCameraDeviceSession::kMaxStallStream;
HandleImporter HdmirxCameraDeviceSession::sHandleImporter;

HdmirxCameraDeviceSession::HdmirxCameraDeviceSession(
        const sp<ICameraDeviceCallback>& callback,
        const HdmirxCameraConfig& cfg,
        const std::vector<SupportedV4L2Format>& sortedFormats,
        const CroppingType& croppingType,
        const common::V1_0::helper::CameraMetadata& chars,
        const std::string& cameraId,
        unique_fd v4l2Fd) :
        mCallback(callback),
        mCfg(cfg),
        mCameraCharacteristics(chars),
        mSupportedFormats(sortedFormats),
        mCroppingType(croppingType),
        mCameraId(cameraId),
        mV4l2Fd(std::move(v4l2Fd)),
        mMaxThumbResolution(getMaxThumbResolution()),
        mMaxJpegResolution(getMaxJpegResolution()) {
        }

bool HdmirxCameraDeviceSession::initialize() {
    if (mV4l2Fd.get() < 0) {
        RXLOGE("%s: invalid v4l2 device fd %d!", __FUNCTION__, mV4l2Fd.get());
        return true;
    }
    {
        std::lock_guard<std::mutex> lk(mGrallocBufferLock);
        for (int i = 0; i < GRALLOC_BUFFER_MAX_NUM; i++) {
            interBuffers[i].status = BUFFER_STATUS_FREE;
        }
    }

    if (mdp_open() != 0) {
        RXLOGE("%s: mdp open failed!", __FUNCTION__);
        return true;
    }

    mExifMake = "Generic UVC webcam";
    mExifModel = "Generic UVC webcam";

    initOutputThread();
    if (mOutputThread == nullptr) {
        RXLOGE("%s: init OutputThread failed!", __FUNCTION__);
        return true;
    }
    mOutputThread->setExifMakeModel(mExifMake, mExifModel);

    status_t status = initDefaultRequests();
    if (status != OK) {
        RXLOGE("%s: init default requests failed!", __FUNCTION__);
        return true;
    }

    mRequestMetadataQueue = std::make_unique<RequestMetadataQueue>(
            kMetadataMsgQueueSize, false /* non blocking */);
    if (!mRequestMetadataQueue->isValid()) {
        RXLOGE("%s: invalid request fmq", __FUNCTION__);
        return true;
    }
    mResultMetadataQueue = std::make_shared<ResultMetadataQueue>(
            kMetadataMsgQueueSize, false /* non blocking */);
    if (!mResultMetadataQueue->isValid()) {
        RXLOGE("%s: invalid result fmq", __FUNCTION__);
        return true;
    }

    // TODO: check is PRIORITY_DISPLAY enough?
    mOutputThread->run("ExtCamOut", PRIORITY_URGENT_DISPLAY);
    return false;
}

int HdmirxCameraDeviceSession::getGrallocBufferIndex() {
    std::lock_guard<std::mutex> lk(mGrallocBufferLock);
    for (int i = 0; i < GRALLOC_BUFFER_MAX_NUM; i++) {
        if (interBuffers[i].status == BUFFER_STATUS_ENQUEUED)
            return i;
    }
    for (int i = 0; i < GRALLOC_BUFFER_MAX_NUM; i++) {
        if (interBuffers[i].status == BUFFER_STATUS_FREE)
            return i;
    }
    return -1;
}

bool HdmirxCameraDeviceSession::isInitFailed() {
    Mutex::Autolock _l(mLock);
    if (!mInitialized) {
        mInitFail = initialize();
        mInitialized = true;
    }
    return mInitFail;
}

void HdmirxCameraDeviceSession::initOutputThread() {
    mOutputThread = new OutputThread(this, mCroppingType, mCameraCharacteristics);
}

void HdmirxCameraDeviceSession::closeOutputThread() {
    closeOutputThreadImpl();
}

void HdmirxCameraDeviceSession::closeOutputThreadImpl() {
    if (mOutputThread) {
        mOutputThread->flush();
        mOutputThread->requestExit();
        mOutputThread->join();
        mOutputThread.clear();
    }
}

Status HdmirxCameraDeviceSession::initStatus() const {
    Mutex::Autolock _l(mLock);
    Status status = Status::OK;
    if (mInitFail || mClosed) {
        ALOGI("%s: sesssion initFailed %d closed %d", __FUNCTION__, mInitFail, mClosed);
        status = Status::INTERNAL_ERROR;
    }
    return status;
}

HdmirxCameraDeviceSession::~HdmirxCameraDeviceSession() {
    if (!isClosed()) {
        RXLOGE("HdmirxCameraDeviceSession deleted before close!");
        close(/*callerIsDtor*/true);
    }
    std::lock_guard<std::mutex> lk(mGrallocBufferLock);
    for (int i = 0; i < GRALLOC_BUFFER_MAX_NUM; i++) {
        if (interBuffers[i].status == BUFFER_STATUS_ENQUEUED) {
            if (NO_ERROR != GrallocDevice::getInstance().free(interBuffers[i].param.handle)) {
                    RXLOGE("%s, the %d gralloc buffer Failed to free memory size(w=%d,h=%d,fmt=%d,usage=%" PRIx64 ")"
                        , __FUNCTION__, i, interBuffers[i].param.width, interBuffers[i].param.height,
                        interBuffers[i].param.format, interBuffers[i].param.usage);
            }
        } else if (interBuffers[i].status == BUFFER_STATUS_DEQUEUED) {
            RXLOGW("%s, the %d gralloc buffer is in dequeued(w=%d,h=%d,fmt=%d,usage=%" PRIx64 ")"
                        , __FUNCTION__, i, interBuffers[i].param.width, interBuffers[i].param.height,
                        interBuffers[i].param.format, interBuffers[i].param.usage);
        } else {
            RXLOGD("%s, the %d gralloc buffer is in free status", __FUNCTION__, i);
        }
    }
}


void HdmirxCameraDeviceSession::dumpState(const native_handle_t* handle) {
    if (handle->numFds != 1 || handle->numInts != 0) {
        RXLOGE("%s: handle must contain 1 FD and 0 integers! Got %d FDs and %d ints",
                __FUNCTION__, handle->numFds, handle->numInts);
        return;
    }
    int fd = handle->data[0];

    bool intfLocked = tryLock(mInterfaceLock);
    if (!intfLocked) {
        dprintf(fd, "!! HdmirxCameraDeviceSession interface may be deadlocked !!\n");
    }

    if (isClosed()) {
        dprintf(fd, "Hdmirx camera %s is closed\n", mCameraId.c_str());
        return;
    }

    bool streaming = false;
    size_t v4L2BufferCount = 0;
    SupportedV4L2Format streamingFmt;
    {
        bool sessionLocked = tryLock(mLock);
        if (!sessionLocked) {
            dprintf(fd, "!! HdmirxCameraDeviceSession mLock may be deadlocked !!\n");
        }
        streaming = mV4l2Streaming;
        streamingFmt = mV4l2StreamingFmt;
        v4L2BufferCount = mV4L2BufferCount;

        if (sessionLocked) {
            mLock.unlock();
        }
    }

    std::unordered_set<uint32_t>  inflightFrames;
    {
        bool iffLocked = tryLock(mInflightFramesLock);
        if (!iffLocked) {
            dprintf(fd,
                    "!! HdmirxCameraDeviceSession mInflightFramesLock may be deadlocked !!\n");
        }
        inflightFrames = mInflightFrames;
        if (iffLocked) {
            mInflightFramesLock.unlock();
        }
    }

    dprintf(fd, "Hdmirx camera %s V4L2 FD %d, cropping type %s, %s\n",
            mCameraId.c_str(), mV4l2Fd.get(),
            (mCroppingType == VERTICAL) ? "vertical" : "horizontal",
            streaming ? "streaming" : "not streaming");
    if (streaming) {
        // TODO: dump fps later
        dprintf(fd, "Current V4L2 format %c%c%c%c %dx%d @ %ffps\n",
                streamingFmt.fourcc & 0xFF,
                (streamingFmt.fourcc >> 8) & 0xFF,
                (streamingFmt.fourcc >> 16) & 0xFF,
                (streamingFmt.fourcc >> 24) & 0xFF,
                streamingFmt.width, streamingFmt.height,
                mV4l2StreamingFps);

        size_t numDequeuedV4l2Buffers = 0;
        {
            std::lock_guard<std::mutex> lk(mV4l2BufferLock);
            numDequeuedV4l2Buffers = mNumDequeuedV4l2Buffers;
        }
        dprintf(fd, "V4L2 buffer queue size %zu, dequeued %zu\n",
                v4L2BufferCount, numDequeuedV4l2Buffers);
    }

    dprintf(fd, "In-flight frames (not sorted):");
    for (const auto& frameNumber : inflightFrames) {
        dprintf(fd, "%d, ", frameNumber);
    }
    dprintf(fd, "\n");
    mOutputThread->dump(fd);
    dprintf(fd, "\n");

    if (intfLocked) {
        mInterfaceLock.unlock();
    }

    return;
}

Return<void> HdmirxCameraDeviceSession::constructDefaultRequestSettings(
        V3_2::RequestTemplate type,
        V3_2::ICameraDeviceSession::constructDefaultRequestSettings_cb _hidl_cb) {
    V3_2::CameraMetadata outMetadata;
    Status status = constructDefaultRequestSettingsRaw(
            static_cast<RequestTemplate>(type), &outMetadata);
    _hidl_cb(status, outMetadata);
    return Void();
}

Status HdmirxCameraDeviceSession::constructDefaultRequestSettingsRaw(RequestTemplate type,
        V3_2::CameraMetadata *outMetadata) {
    CameraMetadata emptyMd;
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }

    switch (type) {
        case RequestTemplate::PREVIEW:
        case RequestTemplate::STILL_CAPTURE:
        case RequestTemplate::VIDEO_RECORD:
        case RequestTemplate::VIDEO_SNAPSHOT: {
            *outMetadata = mDefaultRequests[type];
            break;
        }
        case RequestTemplate::MANUAL:
        case RequestTemplate::ZERO_SHUTTER_LAG:
            // Don't support MANUAL, ZSL templates
            status = Status::ILLEGAL_ARGUMENT;
            break;
        default:
            RXLOGE("%s: unknown request template type %d", __FUNCTION__, static_cast<int>(type));
            status = Status::ILLEGAL_ARGUMENT;
            break;
    }
    return status;
}

Return<void> HdmirxCameraDeviceSession::configureStreams(
        const V3_2::StreamConfiguration& streams,
        ICameraDeviceSession::configureStreams_cb _hidl_cb) {
    V3_2::HalStreamConfiguration outStreams;
    V3_3::HalStreamConfiguration outStreams_v33;
    Mutex::Autolock _il(mInterfaceLock);

    Status status = configureStreams(streams, &outStreams_v33);
    size_t size = outStreams_v33.streams.size();
    outStreams.streams.resize(size);
    for (size_t i = 0; i < size; i++) {
        outStreams.streams[i] = outStreams_v33.streams[i].v3_2;
    }
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> HdmirxCameraDeviceSession::configureStreams_3_3(
        const V3_2::StreamConfiguration& streams,
        ICameraDeviceSession::configureStreams_3_3_cb _hidl_cb) {
    V3_3::HalStreamConfiguration outStreams;
    Mutex::Autolock _il(mInterfaceLock);


    Status status = configureStreams(streams, &outStreams);
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> HdmirxCameraDeviceSession::configureStreams_3_4(
        const V3_4::StreamConfiguration& requestedConfiguration,
        ICameraDeviceSession::configureStreams_3_4_cb _hidl_cb)  {
    V3_2::StreamConfiguration config_v32;
    V3_3::HalStreamConfiguration outStreams_v33;
    V3_4::HalStreamConfiguration outStreams;
    Mutex::Autolock _il(mInterfaceLock);

    config_v32.operationMode = requestedConfiguration.operationMode;
    config_v32.streams.resize(requestedConfiguration.streams.size());
    uint32_t blobBufferSize = 0;
    int numStallStream = 0;
    for (size_t i = 0; i < config_v32.streams.size(); i++) {
        config_v32.streams[i] = requestedConfiguration.streams[i].v3_2;
        if (config_v32.streams[i].format == PixelFormat::BLOB) {
            blobBufferSize = requestedConfiguration.streams[i].bufferSize;
            numStallStream++;
        }
    }

    // Fail early if there are multiple BLOB streams
    if (numStallStream > kMaxStallStream) {
        RXLOGE("%s: too many stall streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxStallStream, numStallStream);
        _hidl_cb(Status::ILLEGAL_ARGUMENT, outStreams);
        return Void();
    }

    Status status = configureStreams(config_v32, &outStreams_v33, blobBufferSize);

    outStreams.streams.resize(outStreams_v33.streams.size());
    for (size_t i = 0; i < outStreams.streams.size(); i++) {
        outStreams.streams[i].v3_3 = outStreams_v33.streams[i];
    }
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> HdmirxCameraDeviceSession::getCaptureRequestMetadataQueue(
    ICameraDeviceSession::getCaptureRequestMetadataQueue_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    _hidl_cb(*mRequestMetadataQueue->getDesc());
    return Void();
}

Return<void> HdmirxCameraDeviceSession::getCaptureResultMetadataQueue(
    ICameraDeviceSession::getCaptureResultMetadataQueue_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    _hidl_cb(*mResultMetadataQueue->getDesc());
    return Void();
}

Return<void> HdmirxCameraDeviceSession::processCaptureRequest(
        const hidl_vec<CaptureRequest>& requests,
        const hidl_vec<BufferCache>& cachesToRemove,
        ICameraDeviceSession::processCaptureRequest_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    updateBufferCaches(cachesToRemove);

    uint32_t numRequestProcessed = 0;
    Status s = Status::OK;
    for (size_t i = 0; i < requests.size(); i++, numRequestProcessed++) {
        s = processOneCaptureRequest(requests[i]);
        if (s != Status::OK) {
            break;
        }
    }

    _hidl_cb(s, numRequestProcessed);
    return Void();
}

Return<void> HdmirxCameraDeviceSession::processCaptureRequest_3_4(
        const hidl_vec<V3_4::CaptureRequest>& requests,
        const hidl_vec<V3_2::BufferCache>& cachesToRemove,
        ICameraDeviceSession::processCaptureRequest_3_4_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    updateBufferCaches(cachesToRemove);

    uint32_t numRequestProcessed = 0;
    Status s = Status::OK;
    for (size_t i = 0; i < requests.size(); i++, numRequestProcessed++) {
        s = processOneCaptureRequest(requests[i].v3_2);
        if (s != Status::OK) {
            break;
        }
    }

    _hidl_cb(s, numRequestProcessed);
    return Void();
}

Return<Status> HdmirxCameraDeviceSession::flush() {
    ATRACE_CALL();
    Mutex::Autolock _il(mInterfaceLock);
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }
    mOutputThread->flush();
    return Status::OK;
}

Return<void> HdmirxCameraDeviceSession::close(bool callerIsDtor) {
    Mutex::Autolock _il(mInterfaceLock);
    bool closed = isClosed();
    if (!closed) {
        if (callerIsDtor) {
            closeOutputThreadImpl();
        } else {
            closeOutputThread();
        }

        Mutex::Autolock _l(mLock);
        // free all buffers
        {
            Mutex::Autolock _l(mCbsLock);
            for(auto pair : mStreamMap) {
                cleanupBuffersLocked(/*Stream ID*/pair.first);
            }
        }
        //v4l2StreamOffLocked();
        // for hdmi camera
        mdp_close();
        RXLOGV("%s: closing V4L2 camera FD %d", __FUNCTION__, mV4l2Fd.get());
        mV4l2Fd.reset();
        mClosed = true;
    }

    return Void();
}

Status HdmirxCameraDeviceSession::importRequestLocked(
    const CaptureRequest& request,
    hidl_vec<buffer_handle_t*>& allBufPtrs,
    hidl_vec<int>& allFences) {
    return importRequestLockedImpl(request, allBufPtrs, allFences);
}

Status HdmirxCameraDeviceSession::importBuffer(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf) {
    Mutex::Autolock _l(mCbsLock);
    return importBufferLocked(streamId, bufId, buf, outBufPtr, allowEmptyBuf);
}

Status HdmirxCameraDeviceSession::importBufferLocked(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf) {
    return importBufferImpl(
            mCirculatingBuffers, sHandleImporter, streamId,
            bufId, buf, outBufPtr, allowEmptyBuf);
}

void HdmirxCameraDeviceSession::setGrallocBufferStatus(int index) {
    std::lock_guard<std::mutex> lm(mGrallocBufferLock);
    interBuffers[index].status = BUFFER_STATUS_ENQUEUED;
}


Status HdmirxCameraDeviceSession::importRequestLockedImpl(
        const CaptureRequest& request,
        hidl_vec<buffer_handle_t*>& allBufPtrs,
        hidl_vec<int>& allFences,
        bool allowEmptyBuf) {
    size_t numOutputBufs = request.outputBuffers.size();
    size_t numBufs = numOutputBufs;
    // Validate all I/O buffers
    hidl_vec<buffer_handle_t> allBufs;
    hidl_vec<uint64_t> allBufIds;
    allBufs.resize(numBufs);
    allBufIds.resize(numBufs);
    allBufPtrs.resize(numBufs);
    allFences.resize(numBufs);
    std::vector<int32_t> streamIds(numBufs);

    for (size_t i = 0; i < numOutputBufs; i++) {
        allBufs[i] = request.outputBuffers[i].buffer.getNativeHandle();
        allBufIds[i] = request.outputBuffers[i].bufferId;
        allBufPtrs[i] = &allBufs[i];
        streamIds[i] = request.outputBuffers[i].streamId;
    }

    {
        Mutex::Autolock _l(mCbsLock);
        for (size_t i = 0; i < numBufs; i++) {
            Status st = importBufferLocked(
                    streamIds[i], allBufIds[i], allBufs[i], &allBufPtrs[i],
                    allowEmptyBuf);
            if (st != Status::OK) {
                // Detailed error logs printed in importBuffer
                return st;
            }
        }
    }

    // All buffers are imported. Now validate output buffer acquire fences
    for (size_t i = 0; i < numOutputBufs; i++) {
        if (!sHandleImporter.importFence(
                request.outputBuffers[i].acquireFence, allFences[i])) {
            RXLOGE("%s: output buffer %zu acquire fence is invalid", __FUNCTION__, i);
            cleanupInflightFences(allFences, i);
            return Status::INTERNAL_ERROR;
        }
    }

    return Status::OK;
}

void HdmirxCameraDeviceSession::cleanupInflightFences(
        hidl_vec<int>& allFences, size_t numFences) {
    for (size_t j = 0; j < numFences; j++) {
        sHandleImporter.closeFence(allFences[j]);
    }
}

int HdmirxCameraDeviceSession::waitForV4L2BufferReturnLocked(std::unique_lock<std::mutex>& lk) {
    ATRACE_CALL();
    std::chrono::seconds timeout = std::chrono::seconds(kBufferWaitTimeoutSec);
    mLock.unlock();
    auto st = mV4L2BufferReturned.wait_for(lk, timeout);
    // Here we introduce a order where mV4l2BufferLock is acquired before mLock, while
    // the normal lock acquisition order is reversed. This is fine because in most of
    // cases we are protected by mInterfaceLock. The only thread that can cause deadlock
    // is the OutputThread, where we do need to make sure we don't acquire mLock then
    // mV4l2BufferLock
    mLock.lock();
    if (st == std::cv_status::timeout) {
        RXLOGE("%s: wait for V4L2 buffer return timeout!", __FUNCTION__);
        return -1;
    }
    return 0;
}

bool HdmirxCameraDeviceSession::checkHdcpVersionProperty(void) {
    char dump_prop_str[PROPERTY_VALUE_MAX] = {0};
    if (property_get("vendor.hdmirx.hdcp.version", dump_prop_str, NULL) > 0) {
        int enable = atoi(dump_prop_str);
        RXLOGD("%s, %d, enable: %d.", __FUNCTION__, __LINE__, enable);
        if (enable == 1)
            return true;
    }
    return false;
}

Status HdmirxCameraDeviceSession::processOneCaptureRequest(const CaptureRequest & request)
{
    ATRACE_CALL();
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }

    if (request.inputBuffer.streamId != -1) {
        RXLOGE("%s: Hdmirx camera does not support reprocessing!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    Mutex::Autolock _l(mLock);

    const camera_metadata_t *rawSettings = nullptr;
    bool converted = true;

    CameraMetadata settingsFmq;  // settings from FMQ
    if (request.fmqSettingsSize > 0) {
        // non-blocking read; client must write metadata before calling
        // processOneCaptureRequest
        settingsFmq.resize(request.fmqSettingsSize);
        bool read = mRequestMetadataQueue->read(settingsFmq.data(), request.fmqSettingsSize);
        if (read) {
            converted = V3_2::implementation::convertFromHidl(settingsFmq, &rawSettings);
        } else {
            RXLOGE("%s: capture request settings metadata couldn't be read from fmq!", __FUNCTION__);
            converted = false;
        }
    } else {
        converted = V3_2::implementation::convertFromHidl(request.settings, &rawSettings);
    }
    if (converted && rawSettings != nullptr) {
        mLatestReqSetting = rawSettings;
    }
    if (!converted) {
        RXLOGE("%s: capture request settings metadata is corrupt!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }
    if (mFirstRequest && rawSettings == nullptr) {
        RXLOGE("%s: capture request settings must not be null for first request!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    hidl_vec<buffer_handle_t*> allBufPtrs;
    hidl_vec<int> allFences;
    size_t numOutputBufs = request.outputBuffers.size();

    if (numOutputBufs == 0 || numOutputBufs > 2) {
        RXLOGE("%s: capture request must have one or two output buffer!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    status = importRequestLocked(request, allBufPtrs, allFences);
    if (status != Status::OK) {
        RXLOGE("%s, %d, import request locked failed.", __FUNCTION__, __LINE__);
        return status;
    }

    int32_t mdp_profile = MDP_PROFILE_AUTO;
    camera_metadata_entry mdp_profile_meta =
        mLatestReqSetting.find(MTK_CAMERA_MDP_PROFILE);
    if (0 == mdp_profile_meta.count) {
        RXLOGE("%s: mdp profile meta count is 0", __FUNCTION__);
        mdp_profile = MDP_PROFILE_AUTO;
    } else {
        mdp_profile = mdp_profile_meta.data.i32[0];
        RXLOGD("%s: mdp profile: %d", __FUNCTION__, mdp_profile);
    }

    std::shared_ptr<HalRequest> halReq = std::make_shared<HalRequest>();
    halReq->frameNumber = request.frameNumber;
    halReq->setting = mLatestReqSetting;
    halReq->buffers.resize(numOutputBufs);
    for (size_t i = 0; i < numOutputBufs; i++) {
        HalStreamBuffer& halBuf = halReq->buffers[i];
        int streamId = halBuf.streamId = request.outputBuffers[i].streamId;
        halBuf.bufferId = request.outputBuffers[i].bufferId;
        const Stream& stream = mStreamMap[streamId];
        halBuf.width = stream.width;
        halBuf.height = stream.height;
        halBuf.format = stream.format;
        halBuf.usage = stream.usage;
        halBuf.bufPtr = allBufPtrs[i];
        halBuf.acquireFence = allFences[i];
        halBuf.fenceTimeout = false;

        RXLOGD("%s, %d, w: %d, h: %d, format: 0x%x, the %zu buffer fence: %d usage:0x%x",
            __FUNCTION__, __LINE__,halBuf.width, halBuf.height, halBuf.format,
            i, halBuf.acquireFence, (uint32_t)halBuf.usage);


        halReq->width = stream.width > halReq->width? stream.width : halReq->width;
        halReq->height = stream.height > halReq->height? stream.height : halReq->height;
        halReq->handle = *(halBuf.bufPtr);
        halReq->usage = halBuf.usage;
    }

    // hal temp buff format
    halReq->format = HAL_PIXEL_FORMAT_RGB_888; // encoder didn't support RGBA1010102
    halReq->buf_num = numOutputBufs;
    halReq->status = true;

    if (halReq->buf_num == 2) {
        int index = getGrallocBufferIndex();
        if (index == -1) {
            RXLOGE("Failed to get gralloc buffer index.");
            return Status::INTERNAL_ERROR;
        }
        RXLOGD("%s, %d, get gralloc buffer index: %d.", __FUNCTION__, __LINE__, index);
        halReq->buf_index = index;
        std::lock_guard<std::mutex> lm(mGrallocBufferLock);
        if (interBuffers[index].status == BUFFER_STATUS_FREE) {
            interBuffers[index].param.width = halReq->width;
            interBuffers[index].param.height = halReq->height;
            interBuffers[index].param.format = halReq->format;
            interBuffers[index].param.usage = halReq->usage;
            if (NO_ERROR != GrallocDevice::getInstance().alloc(interBuffers[index].param)) {
                RXLOGE("Failed to allocate memory size(w=%d,h=%d,fmt=0x%x,usage=%" PRIx64 ")"
                        , interBuffers[index].param.width, interBuffers[index].param.height,
                        interBuffers[index].param.format, interBuffers[index].param.usage);
                interBuffers[index].status = BUFFER_STATUS_FREE;
                return Status::INTERNAL_ERROR;
            }
        } else {
            if (interBuffers[index].param.width != halReq->width ||
                interBuffers[index].param.height != halReq->height) {
                if (NO_ERROR != GrallocDevice::getInstance().free(interBuffers[index].param.handle)) {
                    RXLOGE("Failed to free memory size(w=%d,h=%d,fmt=%d,usage=%" PRIx64 ")"
                        , interBuffers[index].param.width, interBuffers[index].param.height,
                        interBuffers[index].param.format, interBuffers[index].param.usage);
                    interBuffers[index].status = BUFFER_STATUS_ENQUEUED;
                    return Status::INTERNAL_ERROR;
                }
                interBuffers[index].param.width = halReq->width;
                interBuffers[index].param.height = halReq->height;
                interBuffers[index].param.format = halReq->format;
                interBuffers[index].param.usage = halReq->usage;
                if (NO_ERROR != GrallocDevice::getInstance().alloc(interBuffers[index].param)) {
                    RXLOGE("Failed to allocate memory size(w=%d,h=%d,fmt=0x%x,usage=%" PRIx64 ")"
                            , interBuffers[index].param.width, interBuffers[index].param.height,
                            interBuffers[index].param.format, interBuffers[index].param.usage);
                    interBuffers[index].status = BUFFER_STATUS_FREE;
                    return Status::INTERNAL_ERROR;
                }
            }
        }
        interBuffers[index].status = BUFFER_STATUS_DEQUEUED;
        halReq->handle = interBuffers[index].param.handle;
    }

    if (halReq->buf_num == 1) {
        if (*(halReq->buffers[0].bufPtr) == nullptr) {
            halReq->buffers[0].fenceTimeout = true;
            halReq->status = false;
        } else if (halReq->buffers[0].acquireFence >= 0) {
            int ret = sync_wait(halReq->buffers[0].acquireFence, kSyncWaitTimeoutMs);
            if (ret) {
               halReq->buffers[0].fenceTimeout = true;
               RXLOGW("%s, %d. sync wait timeout.", __FUNCTION__, __LINE__);
               halReq->status = false;
            } else {
                ::close(halReq->buffers[0].acquireFence);
                halReq->buffers[0].acquireFence = -1;
            }
        }
    }
#if 0
    if (halReq->status && (checkHdcpVersionProperty())&& (hdmi_check_hdcp_version() == 1)) {
        RXLOGE("%s, %d, hdcp version is in pretected.", __FUNCTION__, __LINE__);
        halReq->status = false;
    }
#endif
    int32_t fence = -1;

    struct HDMIRX_VID_PARA *pVideo_info = NULL;
    struct HDMIRX_DEV_INFO *pDevice_info = NULL;

    pVideo_info = hdmi_query_video_info();
    pDevice_info = hdmi_query_device_info();

    if (pVideo_info == NULL || pDevice_info == NULL)
        halReq->status = false;


    if (halReq->status) {
        if (pVideo_info)
        {
            if (pVideo_info->hactive <= 0 || pVideo_info->vactive <= 0) {
                RXLOGE("%s: video info invalid, width: %d, height: %d", __FUNCTION__,
                pVideo_info->hactive, pVideo_info->vactive);
                halReq->status = false;
            }
        }
    }


#ifdef MTK_HDMI_RXVC_HDR_SUPPORT
    if (halReq->status) {
        struct hdmirx::hdmi::hdr10InfoPkt *phdr10info = hdmi_query_hdr_info();
        int ret = 0;

        if (phdr10info == NULL) {
            RXLOGE("%s, %d, hdmi rx get hdr info failed.", __FUNCTION__, __LINE__);
            halReq->status = false;
        }
        else
        {
            ret = hdmirx::mdp::mdp_parse_hdr_info(phdr10info);
            if (ret < 0 || !halReq->status) {
                RXLOGE("%s, %d, mdp parse hdr info failed.", __FUNCTION__, __LINE__);
                halReq->status = false;
            }
        }
    }
#endif

    if (halReq->status) {
        if (pVideo_info)
        {
            if (pVideo_info->cs == HDMIRX_CS::HDMI_CS_YUV420)
                halReq->hdmirx_width = pVideo_info->hactive * 2;
            else
                halReq->hdmirx_width = pVideo_info->hactive;
            halReq->hdmirx_height = pVideo_info->vactive;
            halReq->hdmirx_dvi = pVideo_info->hdmi_mode? 1 : 0;
            halReq->hdmirx_framerate = pVideo_info->frame_rate;
            halReq->hdmirx_interlace = pVideo_info->is_pscan;
        }

        if (pDevice_info)
        {
            if (pDevice_info->hdcp_version == 14)
                halReq->hdmirx_hdcp = pDevice_info->hdcp14_decrypted;
            else if (pDevice_info->hdcp_version == 22)
                halReq->hdmirx_hdcp = pDevice_info->hdcp22_decrypted;
            else
                halReq->hdmirx_hdcp = 0;
        }
    }

    if (halReq->status) {
        if (0 > mdp_trigger(halReq->handle, &fence, mdp_profile, pVideo_info)) {
            RXLOGE("Failed to trigger mdp");
            halReq->status = false;
        }
    }


    if (numOutputBufs == 2 && !halReq->status) {
        setGrallocBufferStatus(halReq->buf_index);
    }

    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.insert(halReq->frameNumber);
    }

    halReq->fence = fence;

    RXLOGD("%s, %d, frame id: %d", __FUNCTION__, __LINE__, request.frameNumber);

    // Send request to OutputThread for the rest of processing
    mOutputThread->submitRequest(halReq);
    mFirstRequest = false;
    return Status::OK;
}

void HdmirxCameraDeviceSession::notifyShutter(uint32_t frameNumber, nsecs_t shutterTs) {
    NotifyMsg msg;
    msg.type = MsgType::SHUTTER;
    msg.msg.shutter.frameNumber = frameNumber;
    msg.msg.shutter.timestamp = shutterTs;
    mCallback->notify({msg});
}

void HdmirxCameraDeviceSession::notifyError(
        uint32_t frameNumber, int32_t streamId, ErrorCode ec) {
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.erase(frameNumber);
    }// add my lmx
    NotifyMsg msg;
    msg.type = MsgType::ERROR;
    msg.msg.error.frameNumber = frameNumber;
    msg.msg.error.errorStreamId = streamId;
    msg.msg.error.errorCode = ec;
    mCallback->notify({msg});
}

//TODO: refactor with processCaptureResult
Status HdmirxCameraDeviceSession::processCaptureRequestError(
        const std::shared_ptr<HalRequest>& req,
        /*out*/std::vector<NotifyMsg>* outMsgs,
        /*out*/std::vector<CaptureResult>* outResults) {
    ATRACE_CALL();

    if (outMsgs == nullptr) {
        notifyShutter(req->frameNumber, req->shutterTs);
        notifyError(/*frameNum*/req->frameNumber, /*stream*/-1, ErrorCode::ERROR_REQUEST);
    } else {
        NotifyMsg shutter;
        shutter.type = MsgType::SHUTTER;
        shutter.msg.shutter.frameNumber = req->frameNumber;
        shutter.msg.shutter.timestamp = req->shutterTs;

        NotifyMsg error;
        error.type = MsgType::ERROR;
        error.msg.error.frameNumber = req->frameNumber;
        error.msg.error.errorStreamId = -1;
        error.msg.error.errorCode = ErrorCode::ERROR_REQUEST;
        outMsgs->push_back(shutter);
        outMsgs->push_back(error);
    }

    // Fill output buffers
    hidl_vec<CaptureResult> results;
    results.resize(1);
    CaptureResult& result = results[0];
    result.frameNumber = req->frameNumber;
    result.partialResult = 1;
    result.inputBuffer.streamId = -1;
    result.outputBuffers.resize(req->buffers.size());
    for (size_t i = 0; i < req->buffers.size(); i++) {
        result.outputBuffers[i].streamId = req->buffers[i].streamId;
        result.outputBuffers[i].bufferId = req->buffers[i].bufferId;
        result.outputBuffers[i].status = BufferStatus::ERROR;
        if (req->buffers[i].acquireFence >= 0) {
            native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
            if (!handle) {
                RXLOGE("%d, native_handle_create return nullptr!", __LINE__);
                return Status::INTERNAL_ERROR;
            }
            handle->data[0] = req->buffers[i].acquireFence;
            result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
        }
    }

    // update inflight records
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.erase(req->frameNumber);
    }

    if (outResults == nullptr) {
        // Callback into framework
        invokeProcessCaptureResultCallback(results, /* tryWriteFmq */true);
        freeReleaseFences(results);
    } else {
        outResults->push_back(result);
    }
    return Status::OK;
}

Status HdmirxCameraDeviceSession::processCaptureResult(std::shared_ptr<HalRequest>& req) {
    ATRACE_CALL();

    // NotifyShutter
    notifyShutter(req->frameNumber, req->shutterTs);

    // Fill output buffers
    hidl_vec<CaptureResult> results;
    results.resize(1);
    CaptureResult& result = results[0];
    result.frameNumber = req->frameNumber;
    result.partialResult = 1;
    result.inputBuffer.streamId = -1;
    result.outputBuffers.resize(req->buffers.size());
    for (size_t i = 0; i < req->buffers.size(); i++) {
        result.outputBuffers[i].streamId = req->buffers[i].streamId;
        result.outputBuffers[i].bufferId = req->buffers[i].bufferId;
        if (req->buffers[i].fenceTimeout) {
            result.outputBuffers[i].status = BufferStatus::ERROR;
            //if (req->buffers[i].acquireFence >= 0) { req->fence
            if (req->fence >= 0) {
                native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
                if (!handle) {
                    RXLOGE("%d, native_handle_create return nullptr!", __LINE__);
                    return Status::INTERNAL_ERROR;
                }
                //handle->data[0] = req->buffers[i].acquireFence;
                handle->data[0] = ::dup(req->fence);
                result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
            }
            notifyError(req->frameNumber, req->buffers[i].streamId, ErrorCode::ERROR_BUFFER);
        } else {
            result.outputBuffers[i].status = BufferStatus::OK;
            // TODO: refactor
            //if (req->buffers[i].acquireFence >= 0) {
            if (req->fence >= 0) {
                native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
                if (!handle) {
                    RXLOGE("%d, native_handle_create return nullptr!", __LINE__);
                    return Status::INTERNAL_ERROR;
                }
                //handle->data[0] = req->buffers[i].acquireFence;
                handle->data[0] = ::dup(req->fence);
                result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
            }
        }
    }

    // Fill capture result metadata
    fillCaptureResult(req->setting, req->shutterTs);
    fillHdmiRxInfoResult(req->setting, req->hdmirx_width, req->hdmirx_height,
		req->hdmirx_dvi, req->hdmirx_framerate, req->hdmirx_hdcp, req->hdmirx_interlace);
    const camera_metadata_t *rawResult = req->setting.getAndLock();
    V3_2::implementation::convertToHidl(rawResult, &result.result);
    req->setting.unlock(rawResult);

    // update inflight records
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.erase(req->frameNumber);
    }

    // Callback into framework
    invokeProcessCaptureResultCallback(results, /* tryWriteFmq */true);

    if (req->status && req->fence >= 0) {
        ATRACE_NAME("wait_fence");
        int ret = sync_wait(req->fence, kSyncWaitTimeoutMs);
        if (ret) {
            RXLOGE("%s: sync wait timeout", __FUNCTION__);
        }

        ::close(req->fence);
        req->fence = -1;
    }

    freeReleaseFences(results);
    return Status::OK;
}

void HdmirxCameraDeviceSession::invokeProcessCaptureResultCallback(
        hidl_vec<CaptureResult> &results, bool tryWriteFmq) {
    if (mProcessCaptureResultLock.tryLock() != OK) {
        const nsecs_t NS_TO_SECOND = 1000000000;
        RXLOGV("%s: previous call is not finished! waiting 1s...", __FUNCTION__);
        if (mProcessCaptureResultLock.timedLock(/* 1s */NS_TO_SECOND) != OK) {
            RXLOGE("%s: cannot acquire lock in 1s, cannot proceed",
                    __FUNCTION__);
            return ;
        }
    }

    if (tryWriteFmq && mResultMetadataQueue->availableToWrite() > 0) {
        for (CaptureResult &result : results) {
            if (result.result.size() > 0) {
                if (mResultMetadataQueue->write(result.result.data(), result.result.size())) {
                    result.fmqResultSize = result.result.size();
                    result.result.resize(0);
                } else {
                    RXLOGW("%s: couldn't utilize fmq, fall back to hwbinder", __FUNCTION__);
                    result.fmqResultSize = 0;
                }
            } else {
                result.fmqResultSize = 0;
            }
        }
    }

    auto status = mCallback->processCaptureResult(results);
    if (!status.isOk()) {
        RXLOGE("%s: processCaptureResult ERROR : %s", __FUNCTION__,
              status.description().c_str());
    }
    mProcessCaptureResultLock.unlock();
}

HdmirxCameraDeviceSession::OutputThread::OutputThread(
        wp<OutputThreadInterface> parent, CroppingType ct,
        const common::V1_0::helper::CameraMetadata& chars) :
        mParent(parent), mCroppingType(ct), mCameraCharacteristics(chars) {}

HdmirxCameraDeviceSession::OutputThread::~OutputThread() {}

void HdmirxCameraDeviceSession::OutputThread::setExifMakeModel(
        const std::string& make, const std::string& model) {
    mExifMake = make;
    mExifModel = model;
}

int HdmirxCameraDeviceSession::OutputThread::cropAndScaleLocked(
        sp<AllocatedFrame>& in, const Size& outSz, YCbCrLayout* out) {
    Size inSz = {in->mWidth, in->mHeight};

    int ret;
    if (inSz == outSz) {
        ret = in->getLayout(out);
        if (ret != 0) {
            RXLOGE("%s: failed to get input image layout", __FUNCTION__);
            return ret;
        }
        return ret;
    }

    // Cropping to output aspect ratio
    IMapper::Rect inputCrop;
    ret = getCropRect(mCroppingType, inSz, outSz, &inputCrop);
    if (ret != 0) {
        RXLOGE("%s: failed to compute crop rect for output size %dx%d",
                __FUNCTION__, outSz.width, outSz.height);
        return ret;
    }

    YCbCrLayout croppedLayout;
    ret = in->getCroppedLayout(inputCrop, &croppedLayout);
    if (ret != 0) {
        RXLOGE("%s: failed to crop input image %dx%d to output size %dx%d",
                __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        return ret;
    }

    if ((mCroppingType == VERTICAL && inSz.width == outSz.width) ||
            (mCroppingType == HORIZONTAL && inSz.height == outSz.height)) {
        // No scale is needed
        *out = croppedLayout;
        return 0;
    }

    auto it = mScaledYu12Frames.find(outSz);
    sp<AllocatedFrame> scaledYu12Buf;
    if (it != mScaledYu12Frames.end()) {
        scaledYu12Buf = it->second;
    } else {
        it = mIntermediateBuffers.find(outSz);
        if (it == mIntermediateBuffers.end()) {
            RXLOGE("%s: failed to find intermediate buffer size %dx%d",
                    __FUNCTION__, outSz.width, outSz.height);
            return -1;
        }
        scaledYu12Buf = it->second;
    }
    // Scale
    YCbCrLayout outLayout;
    ret = scaledYu12Buf->getLayout(&outLayout);
    if (ret != 0) {
        RXLOGE("%s: failed to get output buffer layout", __FUNCTION__);
        return ret;
    }

    ret = libyuv::I420Scale(
            static_cast<uint8_t*>(croppedLayout.y),
            croppedLayout.yStride,
            static_cast<uint8_t*>(croppedLayout.cb),
            croppedLayout.cStride,
            static_cast<uint8_t*>(croppedLayout.cr),
            croppedLayout.cStride,
            inputCrop.width,
            inputCrop.height,
            static_cast<uint8_t*>(outLayout.y),
            outLayout.yStride,
            static_cast<uint8_t*>(outLayout.cb),
            outLayout.cStride,
            static_cast<uint8_t*>(outLayout.cr),
            outLayout.cStride,
            outSz.width,
            outSz.height,
            // TODO: b/72261744 see if we can use better filter without losing too much perf
            libyuv::FilterMode::kFilterNone);

    if (ret != 0) {
        RXLOGE("%s: failed to scale buffer from %dx%d to %dx%d. Ret %d",
                __FUNCTION__, inputCrop.width, inputCrop.height,
                outSz.width, outSz.height, ret);
        return ret;
    }

    *out = outLayout;
    mScaledYu12Frames.insert({outSz, scaledYu12Buf});
    return 0;
}


int HdmirxCameraDeviceSession::OutputThread::cropAndScaleThumbLocked(
        sp<AllocatedFrame>& in, const Size &outSz, YCbCrLayout* out) {
    Size inSz  {in->mWidth, in->mHeight};

    if ((outSz.width * outSz.height) >
        (mYu12ThumbFrame->mWidth * mYu12ThumbFrame->mHeight)) {
        RXLOGE("%s: Requested thumbnail size too big (%d,%d) > (%d,%d)",
              __FUNCTION__, outSz.width, outSz.height,
              mYu12ThumbFrame->mWidth, mYu12ThumbFrame->mHeight);
        return -1;
    }

    int ret;

    /* This will crop-and-zoom the input YUV frame to the thumbnail size
     * Based on the following logic:
     *  1) Square pixels come in, square pixels come out, therefore single
     *  scale factor is computed to either make input bigger or smaller
     *  depending on if we are upscaling or downscaling
     *  2) That single scale factor would either make height too tall or width
     *  too wide so we need to crop the input either horizontally or vertically
     *  but not both
     */

    /* Convert the input and output dimensions into floats for ease of math */
    float fWin = static_cast<float>(inSz.width);
    float fHin = static_cast<float>(inSz.height);
    float fWout = static_cast<float>(outSz.width);
    float fHout = static_cast<float>(outSz.height);

    /* Compute the one scale factor from (1) above, it will be the smaller of
     * the two possibilities. */
    float scaleFactor = std::min( fHin / fHout, fWin / fWout );

    /* Since we are crop-and-zooming (as opposed to letter/pillar boxing) we can
     * simply multiply the output by our scaleFactor to get the cropped input
     * size. Note that at least one of {fWcrop, fHcrop} is going to wind up
     * being {fWin, fHin} respectively because fHout or fWout cancels out the
     * scaleFactor calculation above.
     *
     * Specifically:
     *  if ( fHin / fHout ) < ( fWin / fWout ) we crop the sides off
     * input, in which case
     *    scaleFactor = fHin / fHout
     *    fWcrop = fHin / fHout * fWout
     *    fHcrop = fHin
     *
     * Note that fWcrop <= fWin ( because ( fHin / fHout ) * fWout < fWin, which
     * is just the inequality above with both sides multiplied by fWout
     *
     * on the other hand if ( fWin / fWout ) < ( fHin / fHout) we crop the top
     * and the bottom off of input, and
     *    scaleFactor = fWin / fWout
     *    fWcrop = fWin
     *    fHCrop = fWin / fWout * fHout
     */
    float fWcrop = scaleFactor * fWout;
    float fHcrop = scaleFactor * fHout;

    /* Convert to integer and truncate to an even number */
    Size cropSz = { 2*static_cast<uint32_t>(fWcrop/2.0f),
                    2*static_cast<uint32_t>(fHcrop/2.0f) };

    /* Convert to a centered rectange with even top/left */
    IMapper::Rect inputCrop {
        2*static_cast<int32_t>((inSz.width - cropSz.width)/4),
        2*static_cast<int32_t>((inSz.height - cropSz.height)/4),
        static_cast<int32_t>(cropSz.width),
        static_cast<int32_t>(cropSz.height) };

    if ((inputCrop.top < 0) ||
        (inputCrop.top >= static_cast<int32_t>(inSz.height)) ||
        (inputCrop.left < 0) ||
        (inputCrop.left >= static_cast<int32_t>(inSz.width)) ||
        (inputCrop.width <= 0) ||
        (inputCrop.width + inputCrop.left > static_cast<int32_t>(inSz.width)) ||
        (inputCrop.height <= 0) ||
        (inputCrop.height + inputCrop.top > static_cast<int32_t>(inSz.height)))
    {
        RXLOGE("%s: came up with really wrong crop rectangle",__FUNCTION__);
        RXLOGE("%s: input layout %dx%d to for output size %dx%d",
             __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        RXLOGE("%s: computed input crop +%d,+%d %dx%d",
             __FUNCTION__, inputCrop.left, inputCrop.top,
             inputCrop.width, inputCrop.height);
        return -1;
    }

    YCbCrLayout inputLayout;
    ret = in->getCroppedLayout(inputCrop, &inputLayout);
    if (ret != 0) {
        RXLOGE("%s: failed to crop input layout %dx%d to for output size %dx%d",
             __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        RXLOGE("%s: computed input crop +%d,+%d %dx%d",
             __FUNCTION__, inputCrop.left, inputCrop.top,
             inputCrop.width, inputCrop.height);
        return ret;
    }
    RXLOGV("%s: crop input layout %dx%d to for output size %dx%d",
          __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
    RXLOGV("%s: computed input crop +%d,+%d %dx%d",
          __FUNCTION__, inputCrop.left, inputCrop.top,
          inputCrop.width, inputCrop.height);


    // Scale
    YCbCrLayout outFullLayout;

    ret = mYu12ThumbFrame->getLayout(&outFullLayout);
    if (ret != 0) {
        RXLOGE("%s: failed to get output buffer layout", __FUNCTION__);
        return ret;
    }


    ret = libyuv::I420Scale(
            static_cast<uint8_t*>(inputLayout.y),
            inputLayout.yStride,
            static_cast<uint8_t*>(inputLayout.cb),
            inputLayout.cStride,
            static_cast<uint8_t*>(inputLayout.cr),
            inputLayout.cStride,
            inputCrop.width,
            inputCrop.height,
            static_cast<uint8_t*>(outFullLayout.y),
            outFullLayout.yStride,
            static_cast<uint8_t*>(outFullLayout.cb),
            outFullLayout.cStride,
            static_cast<uint8_t*>(outFullLayout.cr),
            outFullLayout.cStride,
            outSz.width,
            outSz.height,
            libyuv::FilterMode::kFilterNone);

    if (ret != 0) {
        RXLOGE("%s: failed to scale buffer from %dx%d to %dx%d. Ret %d",
                __FUNCTION__, inputCrop.width, inputCrop.height,
                outSz.width, outSz.height, ret);
        return ret;
    }

    *out = outFullLayout;
    return 0;
}

/*
 * TODO: There needs to be a mechanism to discover allocated buffer size
 * in the HAL.
 *
 * This is very fragile because it is duplicated computation from:
 * frameworks/av/services/camera/libcameraservice/device3/Camera3Device.cpp
 *
 */

/* This assumes mSupportedFormats have all been declared as supporting
 * HAL_PIXEL_FORMAT_BLOB to the framework */
Size HdmirxCameraDeviceSession::getMaxJpegResolution() const {
    Size ret { 0, 0 };
    for(auto & fmt : mSupportedFormats) {
        if(fmt.width * fmt.height > ret.width * ret.height) {
            ret = Size { fmt.width, fmt.height };
        }
    }
    return ret;
}

Size HdmirxCameraDeviceSession::getMaxThumbResolution() const {
    return getMaxThumbnailResolution(mCameraCharacteristics);
}

ssize_t HdmirxCameraDeviceSession::getJpegBufferSize(
        uint32_t width, uint32_t height) const {
    // Constant from camera3.h
    const ssize_t kMinJpegBufferSize = 256 * 1024 + sizeof(CameraBlob);
    // Get max jpeg size (area-wise).
    if (mMaxJpegResolution.width == 0) {
        RXLOGE("%s: Do not have a single supported JPEG stream",
                __FUNCTION__);
        return BAD_VALUE;
    }

    // Get max jpeg buffer size
    ssize_t maxJpegBufferSize = 0;
    camera_metadata_ro_entry jpegBufMaxSize =
            mCameraCharacteristics.find(ANDROID_JPEG_MAX_SIZE);
    if (jpegBufMaxSize.count == 0) {
        RXLOGE("%s: Can't find maximum JPEG size in static metadata!",
              __FUNCTION__);
        return BAD_VALUE;
    }
    maxJpegBufferSize = jpegBufMaxSize.data.i32[0];

    if (maxJpegBufferSize <= kMinJpegBufferSize) {
        RXLOGE("%s: ANDROID_JPEG_MAX_SIZE (%zd) <= kMinJpegBufferSize (%zd)",
              __FUNCTION__, maxJpegBufferSize, kMinJpegBufferSize);
        return BAD_VALUE;
    }

    // Calculate final jpeg buffer size for the given resolution.
    float scaleFactor = ((float) (width * height)) /
            (mMaxJpegResolution.width * mMaxJpegResolution.height);
    ssize_t jpegBufferSize = scaleFactor * (maxJpegBufferSize - kMinJpegBufferSize) +
            kMinJpegBufferSize;
    if (jpegBufferSize > maxJpegBufferSize) {
        jpegBufferSize = maxJpegBufferSize;
    }

    return jpegBufferSize;
}

int HdmirxCameraDeviceSession::OutputThread::createJpegLocked(
        HalStreamBuffer &halBuf,
        const common::V1_0::helper::CameraMetadata& setting)
{
    ATRACE_CALL();
    int ret;
    auto lfail = [&](auto... args) {
        RXLOGE(args...);

        return 1;
    };
    auto parent = mParent.promote();
    if (parent == nullptr) {
       RXLOGE("%s: session has been disconnected!", __FUNCTION__);
       return 1;
    }

    RXLOGV("%s: HAL buffer sid: %d bid: %" PRIu64 " w: %u h: %u",
          __FUNCTION__, halBuf.streamId, static_cast<uint64_t>(halBuf.bufferId),
          halBuf.width, halBuf.height);
    RXLOGV("%s: HAL buffer fmt: %x usage: %" PRIx64 " ptr: %p",
          __FUNCTION__, halBuf.format, static_cast<uint64_t>(halBuf.usage),
          halBuf.bufPtr);
    RXLOGV("%s: YV12 buffer %d x %d",
          __FUNCTION__,
          mYu12Frame->mWidth, mYu12Frame->mHeight);

    int jpegQuality, thumbQuality;
    Size thumbSize;
    bool outputThumbnail = true;

    if (setting.exists(ANDROID_JPEG_QUALITY)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_QUALITY);
        jpegQuality = entry.data.u8[0];
    } else {
        return lfail("%s: ANDROID_JPEG_QUALITY not set",__FUNCTION__);
    }

    if (setting.exists(ANDROID_JPEG_THUMBNAIL_QUALITY)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
        thumbQuality = entry.data.u8[0];
    } else {
        return lfail(
            "%s: ANDROID_JPEG_THUMBNAIL_QUALITY not set",
            __FUNCTION__);
    }

    if (setting.exists(ANDROID_JPEG_THUMBNAIL_SIZE)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_THUMBNAIL_SIZE);
        thumbSize = Size { static_cast<uint32_t>(entry.data.i32[0]),
                           static_cast<uint32_t>(entry.data.i32[1])
        };
        if (thumbSize.width == 0 && thumbSize.height == 0) {
            outputThumbnail = false;
        }
    } else {
        return lfail(
            "%s: ANDROID_JPEG_THUMBNAIL_SIZE not set", __FUNCTION__);
    }

    /* Cropped and scaled YU12 buffer for main and thumbnail */
    YCbCrLayout yu12Main;
    Size jpegSize { halBuf.width, halBuf.height };

    /* Compute temporary buffer sizes accounting for the following:
     * thumbnail can't exceed APP1 size of 64K
     * main image needs to hold APP1, headers, and at most a poorly
     * compressed image */
    const ssize_t maxThumbCodeSize = 64 * 1024;
    const ssize_t maxJpegCodeSize = mBlobBufferSize == 0 ?
            parent->getJpegBufferSize(jpegSize.width, jpegSize.height) :
            mBlobBufferSize;

    /* Check that getJpegBufferSize did not return an error */
    if (maxJpegCodeSize < 0) {
        return lfail(
            "%s: getJpegBufferSize returned %zd",__FUNCTION__,maxJpegCodeSize);
    }


    /* Hold actual thumbnail and main image code sizes */
    size_t thumbCodeSize = 0, jpegCodeSize = 0;
    /* Temporary thumbnail code buffer */
    std::vector<uint8_t> thumbCode(outputThumbnail ? maxThumbCodeSize : 0);

    YCbCrLayout yu12Thumb;
    if (outputThumbnail) {
        ret = cropAndScaleThumbLocked(mYu12Frame, thumbSize, &yu12Thumb);

        if (ret != 0) {
            return lfail(
                "%s: crop and scale thumbnail failed!", __FUNCTION__);
        }
    }

    /* Scale and crop main jpeg */
    ret = cropAndScaleLocked(mYu12Frame, jpegSize, &yu12Main);

    if (ret != 0) {
        return lfail("%s: crop and scale main failed!", __FUNCTION__);
    }

    /* Encode the thumbnail image */
    if (outputThumbnail) {
        ret = encodeJpegYU12(thumbSize, yu12Thumb,
                thumbQuality, 0, 0,
                &thumbCode[0], maxThumbCodeSize, thumbCodeSize);

        if (ret != 0) {
            return lfail("%s: thumbnail encodeJpegYU12 failed with %d",__FUNCTION__, ret);
        }
    }

    /* Combine camera characteristics with request settings to form EXIF
     * metadata */
    common::V1_0::helper::CameraMetadata meta(mCameraCharacteristics);
    meta.append(setting);

    /* Generate EXIF object */
    std::unique_ptr<ExifUtils> utils(ExifUtils::create());
    /* Make sure it's initialized */
    utils->initialize();

    utils->setFromMetadata(meta, jpegSize.width, jpegSize.height);
    utils->setMake(mExifMake);
    utils->setModel(mExifModel);

    ret = utils->generateApp1(outputThumbnail ? &thumbCode[0] : 0, thumbCodeSize);

    if (!ret) {
        return lfail("%s: generating APP1 failed", __FUNCTION__);
    }

    /* Get internal buffer */
    size_t exifDataSize = utils->getApp1Length();
    const uint8_t* exifData = utils->getApp1Buffer();

    /* Lock the HAL jpeg code buffer */
    void *bufPtr = sHandleImporter.lock(
            *(halBuf.bufPtr), halBuf.usage, maxJpegCodeSize);

    if (!bufPtr) {
        return lfail("%s: could not lock %zu bytes", __FUNCTION__, maxJpegCodeSize);
    }

    /* Encode the main jpeg image */
    ret = encodeJpegYU12(jpegSize, yu12Main,
            jpegQuality, exifData, exifDataSize,
            bufPtr, maxJpegCodeSize, jpegCodeSize);

    /* TODO: Not sure this belongs here, maybe better to pass jpegCodeSize out
     * and do this when returning buffer to parent */
    CameraBlob blob { CameraBlobId::JPEG, static_cast<uint32_t>(jpegCodeSize) };
    void *blobDst =
        reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bufPtr) +
                           maxJpegCodeSize -
                           sizeof(CameraBlob));
    memcpy(blobDst, &blob, sizeof(CameraBlob));

    /* Unlock the HAL jpeg code buffer */
    int relFence = sHandleImporter.unlock(*(halBuf.bufPtr));
    if (relFence >= 0) {
        halBuf.acquireFence = relFence;
    }

    /* Check if our JPEG actually succeeded */
    if (ret != 0) {
        return lfail(
            "%s: encodeJpegYU12 failed with %d",__FUNCTION__, ret);
    }

    RXLOGV("%s: encoded JPEG (ret:%d) with Q:%d max size: %zu",
          __FUNCTION__, ret, jpegQuality, maxJpegCodeSize);

    return 0;
}

bool HdmirxCameraDeviceSession::OutputThread::threadLoop() {
    std::shared_ptr<HalRequest> req;

    int req_buffer_num;
    buffer_handle_t input_handle, output0_handle, output1_handle;
    int input_height, output0_height, output1_height;
    int input_width, output0_width, output1_width;

    auto parent = mParent.promote();
    if (parent == nullptr) {
       RXLOGE("%s: session has been disconnected!", __FUNCTION__);
       return false;
    }

    // TODO: maybe we need to setup a sensor thread to dq/enq v4l frames
    //       regularly to prevent v4l buffer queue filled with stale buffers
    //       when app doesn't program a preveiw request
    waitForNextRequest(&req);
    if (req == nullptr) {
        // No new request, wait again
        return true;
    }

    // set magic id for rx hal buffer
    // add gralloc_extra_perform
    int err = 0;
    gralloc_extra_ion_sf_info_t sf_info;
    err = gralloc_extra_query(req->handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);
    sf_info.magic = 0xF0F0;
    uint64_t pts = req->shutterTs & 0xFFFFFFFF0000ULL;
    sf_info.sequence = req->frameNumber;
    sf_info.timestamp = (uint32_t)(pts >> 16);
    err = gralloc_extra_perform(req->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &sf_info);


    auto onDeviceError = [&](auto... args) {
        RXLOGE(args...);
        parent->notifyError(
                req->frameNumber, /*stream*/-1, ErrorCode::ERROR_DEVICE);
        signalRequestDone();
        return false;
    };

    if (!req->status) {
        Status st = parent->processCaptureRequestError(req);
        if (st != Status::OK)
            return onDeviceError("%s: mdp trigger fail or video unlock.", __FUNCTION__);
        signalRequestDone();
        return true;
    }

    std::unique_lock<std::mutex> lk(mBufferLock);
    if (req->buf_num == 2) {
        input_handle = req->handle;
        input_width = req->width;
        input_height = req->height;

        req_buffer_num = 0;
        for (auto& halBuf : req->buffers) {
            if (*(halBuf.bufPtr) == nullptr) {
                RXLOGW("%s: buffer for stream %d missing", __FUNCTION__, halBuf.streamId);
                halBuf.fenceTimeout = true;
            } else if (halBuf.acquireFence >= 0) {
                int ret = sync_wait(halBuf.acquireFence, kSyncWaitTimeoutMs);
                if (ret) {
                    halBuf.fenceTimeout = true;
                } else {
                    ::close(halBuf.acquireFence);
                    halBuf.acquireFence = -1;
                }
            }

            if (halBuf.fenceTimeout) {
                continue;
            }

            req_buffer_num++;

            if (req_buffer_num == 1) {
                output0_handle = *(halBuf.bufPtr);
                output0_height = halBuf.height;
                output0_width = halBuf.width;
            } else if (req_buffer_num == 2) {
                output1_handle = *(halBuf.bufPtr);
                output1_height = halBuf.height;
                output1_width = halBuf.width;
            }
        }

        int ret = 0;
        std::string err_message = "err: ";

        // wait fence before copy rx video
        if (req->status && req->fence >= 0) {
            int ret = sync_wait(req->fence, kSyncWaitTimeoutMs);
            if (ret) {
                RXLOGE("%s: sync wait timeout", __FUNCTION__);
                req = nullptr;
                return onDeviceError("%s: %d, sync wait timeout.",
                __FUNCTION__, __LINE__);;
            }
            RXLOGD("%s, %d, frame id = %d, fence = %d, sync wait ok.",
                __FUNCTION__, __LINE__, req->frameNumber, req->fence);

            ::close(req->fence);
            req->fence = -1;
        }
        // wait fence before copy rx video

        if (req_buffer_num == 0) {
            RXLOGW("%s: %d, request buffer num is zero.", __FUNCTION__, __LINE__);
            lk.unlock();
            Status st = parent->processCaptureRequestError(req);
            if (st != Status::OK)
                return onDeviceError("%s: %d, request buffer num is zero.",
                __FUNCTION__, __LINE__);
            signalRequestDone();
            return true;
        } else if (req_buffer_num == 1) {
            if (0 != mdp_1in1out(input_handle, input_width, input_height,
                                  output0_handle, output0_width,
                                  output0_height)) {
                ret = -1;
                err_message = err_message +
                    std::string("mdp 1 in 1 out failed!");
            }
        } else if (req_buffer_num == 2) {
            if (0 != mdp_1in2out(input_handle, input_width, input_height,
                                  output0_handle, output0_width,
                                  output0_height, output1_handle,
                                  output1_width, output1_height)) {
                ret = -1;
                err_message = err_message +
                    std::string("mdp 1 in 2 out failed!");
            }
        }

        parent->setGrallocBufferStatus(req->buf_index);

        if (ret == -1) {
            lk.unlock();
            return onDeviceError("%s, %d: %s", __FUNCTION__, __LINE__, err_message.c_str());
        }
    }
    // Don't hold the lock while calling back to parent
    lk.unlock();

    Status st = parent->processCaptureResult(req);
    if (st != Status::OK) {
        return onDeviceError("%s: failed to process capture result!", __FUNCTION__);
    }
    signalRequestDone();
    return true;
}

Status HdmirxCameraDeviceSession::OutputThread::allocateIntermediateBuffers(
        const Size& v4lSize, const Size& thumbSize,
        const hidl_vec<Stream>& streams,
        uint32_t blobBufferSize) {
    std::lock_guard<std::mutex> lk(mBufferLock);
    if (mScaledYu12Frames.size() != 0) {
        RXLOGE("%s: intermediate buffer pool has %zu inflight buffers! (expect 0)",
                __FUNCTION__, mScaledYu12Frames.size());
        return Status::INTERNAL_ERROR;
    }

    // Allocating intermediate YU12 frame
    if (mYu12Frame == nullptr || mYu12Frame->mWidth != v4lSize.width ||
            mYu12Frame->mHeight != v4lSize.height) {
        mYu12Frame.clear();
        mYu12Frame = new AllocatedFrame(v4lSize.width, v4lSize.height);
        int ret = mYu12Frame->allocate(&mYu12FrameLayout);
        if (ret != 0) {
            RXLOGE("%s: allocating YU12 frame failed!", __FUNCTION__);
            return Status::INTERNAL_ERROR;
        }
    }

    // Allocating intermediate YU12 thumbnail frame
    if (mYu12ThumbFrame == nullptr ||
        mYu12ThumbFrame->mWidth != thumbSize.width ||
        mYu12ThumbFrame->mHeight != thumbSize.height) {
        mYu12ThumbFrame.clear();
        mYu12ThumbFrame = new AllocatedFrame(thumbSize.width, thumbSize.height);
        int ret = mYu12ThumbFrame->allocate(&mYu12ThumbFrameLayout);
        if (ret != 0) {
            RXLOGE("%s: allocating YU12 thumb frame failed!", __FUNCTION__);
            return Status::INTERNAL_ERROR;
        }
    }

    // Allocating scaled buffers
    for (const auto& stream : streams) {
        Size sz = {stream.width, stream.height};
        if (sz == v4lSize) {
            continue; // Don't need an intermediate buffer same size as v4lBuffer
        }
        if (mIntermediateBuffers.count(sz) == 0) {
            // Create new intermediate buffer
            sp<AllocatedFrame> buf = new AllocatedFrame(stream.width, stream.height);
            int ret = buf->allocate();
            if (ret != 0) {
                RXLOGE("%s: allocating intermediate YU12 frame %dx%d failed!",
                            __FUNCTION__, stream.width, stream.height);
                return Status::INTERNAL_ERROR;
            }
            mIntermediateBuffers[sz] = buf;
        }
    }

    // Remove unconfigured buffers
    auto it = mIntermediateBuffers.begin();
    while (it != mIntermediateBuffers.end()) {
        bool configured = false;
        auto sz = it->first;
        for (const auto& stream : streams) {
            if (stream.width == sz.width && stream.height == sz.height) {
                configured = true;
                break;
            }
        }
        if (configured) {
            it++;
        } else {
            it = mIntermediateBuffers.erase(it);
        }
    }

    mBlobBufferSize = blobBufferSize;
    return Status::OK;
}

void HdmirxCameraDeviceSession::OutputThread::clearIntermediateBuffers() {
    std::lock_guard<std::mutex> lk(mBufferLock);
    mYu12Frame.clear();
    mYu12ThumbFrame.clear();
    mIntermediateBuffers.clear();
    mBlobBufferSize = 0;
}

Status HdmirxCameraDeviceSession::OutputThread::submitRequest(
        const std::shared_ptr<HalRequest>& req) {
    std::unique_lock<std::mutex> lk(mRequestListLock);
    mRequestList.push_back(req);
    lk.unlock();
    mRequestCond.notify_one();
    return Status::OK;
}

void HdmirxCameraDeviceSession::OutputThread::flush() {
    ATRACE_CALL();
    auto parent = mParent.promote();
    if (parent == nullptr) {
       RXLOGE("%s: session has been disconnected!", __FUNCTION__);
       return;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    std::list<std::shared_ptr<HalRequest>> reqs = std::move(mRequestList);
    mRequestList.clear();
    if (mProcessingRequest) {
        std::chrono::seconds timeout = std::chrono::seconds(kFlushWaitTimeoutSec);
        auto st = mRequestDoneCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            RXLOGE("%s: wait for inflight request finish timeout!", __FUNCTION__);
        }
    }

    RXLOGV("%s: flusing inflight requests", __FUNCTION__);
    lk.unlock();
    for (const auto& req : reqs) {
        parent->processCaptureRequestError(req);
    }
}

std::list<std::shared_ptr<HalRequest>>
HdmirxCameraDeviceSession::OutputThread::switchToOffline() {
    ATRACE_CALL();
    std::list<std::shared_ptr<HalRequest>> emptyList;
    auto parent = mParent.promote();
    if (parent == nullptr) {
       RXLOGE("%s: session has been disconnected!", __FUNCTION__);
       return emptyList;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    std::list<std::shared_ptr<HalRequest>> reqs = std::move(mRequestList);
    mRequestList.clear();
    if (mProcessingRequest) {
        std::chrono::seconds timeout = std::chrono::seconds(kFlushWaitTimeoutSec);
        auto st = mRequestDoneCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            RXLOGE("%s: wait for inflight request finish timeout!", __FUNCTION__);
        }
    }
    lk.unlock();
    clearIntermediateBuffers();
    RXLOGV("%s: returning %zu request for offline processing", __FUNCTION__, reqs.size());
    return reqs;
}

void HdmirxCameraDeviceSession::OutputThread::waitForNextRequest(
        std::shared_ptr<HalRequest>* out) {
    ATRACE_CALL();
    if (out == nullptr) {
        RXLOGE("%s: out is null", __FUNCTION__);
        return;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    int waitTimes = 0;
    while (mRequestList.empty()) {
        if (exitPending()) {
            return;
        }
        std::chrono::milliseconds timeout = std::chrono::milliseconds(kReqWaitTimeoutMs);
        auto st = mRequestCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            waitTimes++;
            if (waitTimes == kReqWaitTimesMax) {
                // no new request, return
                return;
            }
        }
    }


    if (!mRequestList.empty()) {
        *out = mRequestList.front();

        mRequestList.pop_front();

        nsecs_t shutterTs = systemTime(SYSTEM_TIME_MONOTONIC);
        (*out)->shutterTs = shutterTs;

        mProcessingRequest = true;
        mProcessingFrameNumer = (*out)->frameNumber;
    }

    lk.unlock();
}

void HdmirxCameraDeviceSession::OutputThread::signalRequestDone() {
    std::unique_lock<std::mutex> lk(mRequestListLock);
    mProcessingRequest = false;
    mProcessingFrameNumer = 0;
    lk.unlock();
    mRequestDoneCond.notify_one();
}

void HdmirxCameraDeviceSession::OutputThread::dump(int fd) {
    std::lock_guard<std::mutex> lk(mRequestListLock);
    if (mProcessingRequest) {
        dprintf(fd, "OutputThread processing frame %d\n", mProcessingFrameNumer);
    } else {
        dprintf(fd, "OutputThread not processing any frames\n");
    }
    dprintf(fd, "OutputThread request list contains frame: ");
    for (const auto& req : mRequestList) {
        dprintf(fd, "%d, ", req->frameNumber);
    }
    dprintf(fd, "\n");
}

void HdmirxCameraDeviceSession::cleanupBuffersLocked(int id) {
    for (auto& pair : mCirculatingBuffers.at(id)) {
        sHandleImporter.freeBuffer(pair.second);
    }
    mCirculatingBuffers[id].clear();
    mCirculatingBuffers.erase(id);
}

void HdmirxCameraDeviceSession::updateBufferCaches(const hidl_vec<BufferCache>& cachesToRemove) {
    Mutex::Autolock _l(mCbsLock);
    for (auto& cache : cachesToRemove) {
        auto cbsIt = mCirculatingBuffers.find(cache.streamId);
        if (cbsIt == mCirculatingBuffers.end()) {
            // The stream could have been removed
            continue;
        }
        CirculatingBuffers& cbs = cbsIt->second;
        auto it = cbs.find(cache.bufferId);
        if (it != cbs.end()) {
            sHandleImporter.freeBuffer(it->second);
            cbs.erase(it);
        } else {
            RXLOGE("%s: stream %d buffer %" PRIu64 " is not cached",
                    __FUNCTION__, cache.streamId, cache.bufferId);
        }
    }
}

bool HdmirxCameraDeviceSession::isSupported(const Stream& stream,
        const std::vector<SupportedV4L2Format>& supportedFormats,
        const HdmirxCameraConfig& devCfg) {
    int32_t ds = static_cast<int32_t>(stream.dataSpace);
    PixelFormat fmt = stream.format;
    uint32_t width = stream.width;
    uint32_t height = stream.height;
    // TODO: check usage flags

    if (stream.streamType != StreamType::OUTPUT) {
        RXLOGE("%s: does not support non-output stream type", __FUNCTION__);
        return false;
    }

    if (stream.rotation != StreamRotation::ROTATION_0) {
        RXLOGE("%s: does not support stream rotation", __FUNCTION__);
        return false;
    }

    switch (fmt) {
        case PixelFormat::BLOB:
            if (ds != static_cast<int32_t>(Dataspace::V0_JFIF)) {
                ALOGI("%s: BLOB format does not support dataSpace %x", __FUNCTION__, ds);
                return false;
            }
            break;
        case PixelFormat::IMPLEMENTATION_DEFINED:
        case PixelFormat::YCBCR_420_888:
        case PixelFormat::YV12:
        case PixelFormat::RGB_888:
        case PixelFormat::RGBA_1010102:
            // TODO: check what dataspace we can support here.
            // intentional no-ops.
            break;
        case PixelFormat::Y16:
            if (!devCfg.depthEnabled) {
                ALOGI("%s: Depth is not Enabled", __FUNCTION__);
                return false;
            }
            if (!(ds & Dataspace::DEPTH)) {
                ALOGI("%s: Y16 supports only dataSpace DEPTH", __FUNCTION__);
                return false;
            }
            break;
        default:
            ALOGI("%s: does not support format %x", __FUNCTION__, fmt);
            return false;
    }

    // Assume we can convert any V4L2 format to any of supported output format for now, i.e,
    // ignoring v4l2Fmt.fourcc for now. Might need more subtle check if we support more v4l format
    // in the futrue.
    for (const auto& v4l2Fmt : supportedFormats) {
        RXLOGD("w: %d, h: %d", v4l2Fmt.width, v4l2Fmt.height);
        if (width == v4l2Fmt.width && height == v4l2Fmt.height) {
            return true;
        }
    }
    ALOGI("%s: resolution %dx%d is not supported", __FUNCTION__, width, height);
    return false;
}

int HdmirxCameraDeviceSession::v4l2StreamOffLocked() {
    if (!mV4l2Streaming) {
        return OK;
    }

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        if (mNumDequeuedV4l2Buffers != 0)  {
            RXLOGE("%s: there are %zu inflight V4L buffers",
                __FUNCTION__, mNumDequeuedV4l2Buffers);
            return -1;
        }
    }
    mV4L2BufferCount = 0;

    // VIDIOC_STREAMOFF
    v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_STREAMOFF, &capture_type)) < 0) {
        RXLOGE("%s: STREAMOFF failed: %s", __FUNCTION__, strerror(errno));
        return -errno;
    }

    // VIDIOC_REQBUFS: clear buffers
    v4l2_requestbuffers req_buffers{};
    req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buffers.memory = V4L2_MEMORY_MMAP;
    req_buffers.count = 0;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_REQBUFS, &req_buffers)) < 0) {
        RXLOGE("%s: REQBUFS failed: %s", __FUNCTION__, strerror(errno));
        return -errno;
    }

    mV4l2Streaming = false;
    return OK;
}

int HdmirxCameraDeviceSession::setV4l2FpsLocked(double fps) {
    // VIDIOC_G_PARM/VIDIOC_S_PARM: set fps
    v4l2_streamparm streamparm = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    // The following line checks that the driver knows about framerate get/set.
    int ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_G_PARM, &streamparm));
    if (ret != 0) {
        if (errno == -EINVAL) {
            RXLOGW("%s: device does not support VIDIOC_G_PARM", __FUNCTION__);
        }
        return -errno;
    }
    // Now check if the device is able to accept a capture framerate set.
    if (!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        RXLOGW("%s: device does not support V4L2_CAP_TIMEPERFRAME", __FUNCTION__);
        return -EINVAL;
    }

    // fps is float, approximate by a fraction.
    const int kFrameRatePrecision = 10000;
    streamparm.parm.capture.timeperframe.numerator = kFrameRatePrecision;
    streamparm.parm.capture.timeperframe.denominator =
        (fps * kFrameRatePrecision);

    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_S_PARM, &streamparm)) < 0) {
        RXLOGE("%s: failed to set framerate to %f: %s", __FUNCTION__, fps, strerror(errno));
        return -1;
    }

    double retFps = streamparm.parm.capture.timeperframe.denominator /
            static_cast<double>(streamparm.parm.capture.timeperframe.numerator);
    if (std::fabs(fps - retFps) > 1.0) {
        RXLOGE("%s: expect fps %f, got %f instead", __FUNCTION__, fps, retFps);
        return -1;
    }
    mV4l2StreamingFps = fps;
    return 0;
}

int HdmirxCameraDeviceSession::configureV4l2StreamLocked(
        const SupportedV4L2Format& v4l2Fmt, double requestFps) {
    ATRACE_CALL();
    int ret = v4l2StreamOffLocked();
    if (ret != OK) {
        RXLOGE("%s: stop v4l2 streaming failed: ret %d", __FUNCTION__, ret);
        return ret;
    }

    // VIDIOC_S_FMT w/h/fmt
    v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = v4l2Fmt.width;
    fmt.fmt.pix.height = v4l2Fmt.height;
    fmt.fmt.pix.pixelformat = v4l2Fmt.fourcc;
    ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_S_FMT, &fmt));
    if (ret < 0) {
        int numAttempt = 0;
        while (ret < 0) {
            RXLOGW("%s: VIDIOC_S_FMT failed, wait 33ms and try again", __FUNCTION__);
            usleep(IOCTL_RETRY_SLEEP_US); // sleep and try again
            ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_S_FMT, &fmt));
            if (numAttempt == MAX_RETRY) {
                break;
            }
            numAttempt++;
        }
        if (ret < 0) {
            RXLOGE("%s: S_FMT ioctl failed: %s", __FUNCTION__, strerror(errno));
            return -errno;
        }
    }

    if (v4l2Fmt.width != fmt.fmt.pix.width || v4l2Fmt.height != fmt.fmt.pix.height ||
            v4l2Fmt.fourcc != fmt.fmt.pix.pixelformat) {
        RXLOGE("%s: S_FMT expect %c%c%c%c %dx%d, got %c%c%c%c %dx%d instead!", __FUNCTION__,
                v4l2Fmt.fourcc & 0xFF,
                (v4l2Fmt.fourcc >> 8) & 0xFF,
                (v4l2Fmt.fourcc >> 16) & 0xFF,
                (v4l2Fmt.fourcc >> 24) & 0xFF,
                v4l2Fmt.width, v4l2Fmt.height,
                fmt.fmt.pix.pixelformat & 0xFF,
                (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
                (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
                (fmt.fmt.pix.pixelformat >> 24) & 0xFF,
                fmt.fmt.pix.width, fmt.fmt.pix.height);
        return -EINVAL;
    }
    uint32_t bufferSize = fmt.fmt.pix.sizeimage;
    ALOGI("%s: V4L2 buffer size is %d", __FUNCTION__, bufferSize);
    uint32_t expectedMaxBufferSize = kMaxBytesPerPixel * fmt.fmt.pix.width * fmt.fmt.pix.height;
    if ((bufferSize == 0) || (bufferSize > expectedMaxBufferSize)) {
        RXLOGE("%s: V4L2 buffer size: %u looks invalid. Expected maximum size: %u", __FUNCTION__,
                bufferSize, expectedMaxBufferSize);
        return -EINVAL;
    }
    mMaxV4L2BufferSize = bufferSize;

    const double kDefaultFps = 30.0;
    double fps = 1000.0;
    if (requestFps != 0.0) {
        fps = requestFps;
    } else {
        double maxFps = -1.0;
        // Try to pick the slowest fps that is at least 30
        for (const auto& fr : v4l2Fmt.frameRates) {
            double f = fr.getDouble();
            if (maxFps < f) {
                maxFps = f;
            }
            if (f >= kDefaultFps && f < fps) {
                fps = f;
            }
        }
        if (fps == 1000.0) {
            fps = maxFps;
        }
    }

    int fpsRet = setV4l2FpsLocked(fps);
    if (fpsRet != 0 && fpsRet != -EINVAL) {
        RXLOGE("%s: set fps failed: %s", __FUNCTION__, strerror(fpsRet));
        return fpsRet;
    }

    uint32_t v4lBufferCount = (fps >= kDefaultFps) ?
            mCfg.numVideoBuffers : mCfg.numStillBuffers;
    // VIDIOC_REQBUFS: create buffers
    v4l2_requestbuffers req_buffers{};
    req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buffers.memory = V4L2_MEMORY_MMAP;
    req_buffers.count = v4lBufferCount;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_REQBUFS, &req_buffers)) < 0) {
        RXLOGE("%s: VIDIOC_REQBUFS failed: %s", __FUNCTION__, strerror(errno));
        return -errno;
    }

    // Driver can indeed return more buffer if it needs more to operate
    if (req_buffers.count < v4lBufferCount) {
        RXLOGE("%s: VIDIOC_REQBUFS expected %d buffers, got %d instead",
                __FUNCTION__, v4lBufferCount, req_buffers.count);
        return NO_MEMORY;
    }

    // VIDIOC_QUERYBUF:  get buffer offset in the V4L2 fd
    // VIDIOC_QBUF: send buffer to driver
    mV4L2BufferCount = req_buffers.count;
    for (uint32_t i = 0; i < req_buffers.count; i++) {
        v4l2_buffer buffer = {
                .index = i, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};

        if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_QUERYBUF, &buffer)) < 0) {
            RXLOGE("%s: QUERYBUF %d failed: %s", __FUNCTION__, i,  strerror(errno));
            return -errno;
        }

        if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_QBUF, &buffer)) < 0) {
            RXLOGE("%s: QBUF %d failed: %s", __FUNCTION__, i,  strerror(errno));
            return -errno;
        }
    }

    // VIDIOC_STREAMON: start streaming
    v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_STREAMON, &capture_type));
    if (ret < 0) {
        int numAttempt = 0;
        while (ret < 0) {
            RXLOGW("%s: VIDIOC_STREAMON failed, wait 33ms and try again", __FUNCTION__);
            usleep(IOCTL_RETRY_SLEEP_US); // sleep 100 ms and try again
            ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_STREAMON, &capture_type));
            if (numAttempt == MAX_RETRY) {
                break;
            }
            numAttempt++;
        }
        if (ret < 0) {
            RXLOGE("%s: VIDIOC_STREAMON ioctl failed: %s", __FUNCTION__, strerror(errno));
            return -errno;
        }
    }

    // Swallow first few frames after streamOn to account for bad frames from some devices
    for (int i = 0; i < kBadFramesAfterStreamOn; i++) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_DQBUF, &buffer)) < 0) {
            RXLOGE("%s: DQBUF fails: %s", __FUNCTION__, strerror(errno));
            return -errno;
        }

        if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_QBUF, &buffer)) < 0) {
            RXLOGE("%s: QBUF index %d fails: %s", __FUNCTION__, buffer.index, strerror(errno));
            return -errno;
        }
    }

    ALOGI("%s: start V4L2 streaming %dx%d@%ffps",
                __FUNCTION__, v4l2Fmt.width, v4l2Fmt.height, fps);
    mV4l2StreamingFmt = v4l2Fmt;
    mV4l2Streaming = true;
    return OK;
}

sp<V4L2Frame> HdmirxCameraDeviceSession::dequeueV4l2FrameLocked(/*out*/nsecs_t* shutterTs) {
    ATRACE_CALL();
    sp<V4L2Frame> ret = nullptr;

    if (shutterTs == nullptr) {
        RXLOGE("%s: shutterTs must not be null!", __FUNCTION__);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lk(mV4l2BufferLock);
        if (mNumDequeuedV4l2Buffers == mV4L2BufferCount) {
            int waitRet = waitForV4L2BufferReturnLocked(lk);
            if (waitRet != 0) {
                return ret;
            }
        }
    }

    ATRACE_BEGIN("VIDIOC_DQBUF");
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_DQBUF, &buffer)) < 0) {
        RXLOGE("%s: DQBUF fails: %s", __FUNCTION__, strerror(errno));
        return ret;
    }
    ATRACE_END();

    if (buffer.index >= mV4L2BufferCount) {
        RXLOGE("%s: Invalid buffer id: %d", __FUNCTION__, buffer.index);
        return ret;
    }

    if (buffer.flags & V4L2_BUF_FLAG_ERROR) {
        RXLOGE("%s: v4l2 buf error! buf flag 0x%x", __FUNCTION__, buffer.flags);
        // TODO: try to dequeue again
    }

    if (buffer.bytesused > mMaxV4L2BufferSize) {
        RXLOGE("%s: v4l2 buffer bytes used: %u maximum %u", __FUNCTION__, buffer.bytesused,
                mMaxV4L2BufferSize);
        return ret;
    }

    if (buffer.flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC) {
        // Ideally we should also check for V4L2_BUF_FLAG_TSTAMP_SRC_SOE, but
        // even V4L2_BUF_FLAG_TSTAMP_SRC_EOF is better than capture a timestamp now
        *shutterTs = static_cast<nsecs_t>(buffer.timestamp.tv_sec)*1000000000LL +
                buffer.timestamp.tv_usec * 1000LL;
    } else {
        *shutterTs = systemTime(SYSTEM_TIME_MONOTONIC);
    }

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        mNumDequeuedV4l2Buffers++;
    }
    return new V4L2Frame(
            mV4l2StreamingFmt.width, mV4l2StreamingFmt.height, mV4l2StreamingFmt.fourcc,
            buffer.index, mV4l2Fd.get(), buffer.bytesused, buffer.m.offset);
}

void HdmirxCameraDeviceSession::enqueueV4l2Frame(const sp<V4L2Frame>& frame) {
    ATRACE_CALL();
    frame->unmap();
    ATRACE_BEGIN("VIDIOC_QBUF");
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = frame->mBufferIndex;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_QBUF, &buffer)) < 0) {
        RXLOGE("%s: QBUF index %d fails: %s", __FUNCTION__,
                frame->mBufferIndex, strerror(errno));
        return;
    }
    ATRACE_END();

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        mNumDequeuedV4l2Buffers--;
    }
    mV4L2BufferReturned.notify_one();
}

Status HdmirxCameraDeviceSession::isStreamCombinationSupported(
        const V3_2::StreamConfiguration& config,
        const std::vector<SupportedV4L2Format>& supportedFormats,
        const HdmirxCameraConfig& devCfg) {
    if (config.operationMode != StreamConfigurationMode::NORMAL_MODE) {
        RXLOGE("%s: unsupported operation mode: %d", __FUNCTION__, config.operationMode);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (config.streams.size() == 0) {
        RXLOGE("%s: cannot configure zero stream", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    int numProcessedStream = 0;
    int numStallStream = 0;
    for (const auto& stream : config.streams) {
        // Check if the format/width/height combo is supported
        if (!isSupported(stream, supportedFormats, devCfg)) {
            return Status::ILLEGAL_ARGUMENT;
        }
        if (stream.format == PixelFormat::BLOB) {
            numStallStream++;
        } else {
            numProcessedStream++;
        }
    }

    if (numProcessedStream > kMaxProcessedStream) {
        RXLOGE("%s: too many processed streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxProcessedStream, numProcessedStream);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (numStallStream > kMaxStallStream) {
        RXLOGE("%s: too many stall streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxStallStream, numStallStream);
        return Status::ILLEGAL_ARGUMENT;
    }

    return Status::OK;
}

Status HdmirxCameraDeviceSession::configureStreams(
        const V3_2::StreamConfiguration& config,
        V3_3::HalStreamConfiguration* out,
        uint32_t blobBufferSize) {
    ATRACE_CALL();

    Status status = isStreamCombinationSupported(config, mSupportedFormats, mCfg);
    if (status != Status::OK) {
        return status;
    }

    status = initStatus();
    if (status != Status::OK) {
        return status;
    }


    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        if (!mInflightFrames.empty()) {
            RXLOGE("%s: trying to configureStreams while there are still %zu inflight frames!",
                    __FUNCTION__, mInflightFrames.size());
            return Status::INTERNAL_ERROR;
        }
    }

    // for debug log
    char hdmirx_log_prop[128];
    property_get("vendor.mtk.hdmirx.log", hdmirx_log_prop, "0");
    logLevel = (uint32_t) atoi(hdmirx_log_prop);
    // for debug log

    Mutex::Autolock _l(mLock);
    {
        Mutex::Autolock _l(mCbsLock);
        // Add new streams
        for (const auto& stream : config.streams) {
            if (mStreamMap.count(stream.id) == 0) {
                mStreamMap[stream.id] = stream;
                mCirculatingBuffers.emplace(stream.id, CirculatingBuffers{});
            }
        }

        // Cleanup removed streams
        for(auto it = mStreamMap.begin(); it != mStreamMap.end();) {
            int id = it->first;
            bool found = false;
            for (const auto& stream : config.streams) {
                if (id == stream.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Unmap all buffers of deleted stream
                cleanupBuffersLocked(id);
                it = mStreamMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    mV4L2BufferCount = mCfg.numVideoBuffers;
    out->streams.resize(config.streams.size());
    for (size_t i = 0; i < config.streams.size(); i++) {
        out->streams[i].overrideDataSpace = config.streams[i].dataSpace;
        out->streams[i].v3_2.id = config.streams[i].id;
        // TODO: double check should we add those CAMERA flags
        mStreamMap[config.streams[i].id].usage =
                out->streams[i].v3_2.producerUsage = config.streams[i].usage |
                BufferUsage::CPU_WRITE_OFTEN |
                BufferUsage::CAMERA_OUTPUT;
        out->streams[i].v3_2.consumerUsage = 0;
        out->streams[i].v3_2.maxBuffers  = mV4L2BufferCount;


        switch (config.streams[i].format) {
            case PixelFormat::BLOB:
                RXLOGV("%s: PixelFormat::BLOB format 0x%x", __FUNCTION__, PixelFormat::BLOB);
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::YCBCR_420_888:
                RXLOGV("%s: PixelFormat::YCBCR_420_888 format 0x%x", __FUNCTION__, PixelFormat::YCBCR_420_888);
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::YV12: // Used by SurfaceTexture
                RXLOGV("%s: PixelFormat::YV12 format 0x%x", __FUNCTION__, PixelFormat::YV12);
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::Y16:
                RXLOGV("%s: PixelFormat::Y16 format 0x%x", __FUNCTION__, PixelFormat::Y16);
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::RGB_888:
                RXLOGV("%s: PixelFormat::RGB_888 format 0x%x", __FUNCTION__, PixelFormat::RGB_888);
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::RGBA_1010102:
                RXLOGV("%s: PixelFormat::RGBA_1010102 format 0x%x", __FUNCTION__, PixelFormat::RGBA_1010102);
                // No override
                out->streams[i].v3_2.overrideFormat = (config.streams[i].usage & BufferUsage::VIDEO_ENCODER) ?
                        PixelFormat::RGB_888 : config.streams[i].format;
                break;
            case PixelFormat::IMPLEMENTATION_DEFINED:
            {
                RXLOGV("%s: PixelFormat::IMPLEMENTATION_DEFINED format 0x%x", __FUNCTION__, PixelFormat::IMPLEMENTATION_DEFINED);
                // Override based on RX source depth

                // wait for signal lock first
                int Lock_retry_count = 0;
                while((hdmi_check_video_locked() != 1))
                {
                    RXLOGE("%s, %d, check hdmirx lock failed.", __FUNCTION__, __LINE__);
                    usleep(IOCTL_LOCK_RETRY_SLEEP_US);
                    Lock_retry_count ++;

                    if (Lock_retry_count > IOCTL_LOCK_RETRY_COUNT)
                        break;
                }

                struct HDMIRX_VID_PARA* pVideo_info = hdmi_query_video_info();
                if (pVideo_info == NULL)
                {
                    RXLOGE("%s, %d, check video info failed.", __FUNCTION__, __LINE__);
                    out->streams[i].v3_2.overrideFormat = PixelFormat::RGB_888;
                }
                else
                {
                    RXLOGD("%s, %d, check video info dp=%d cs=%d frame_rate=%d\n", __FUNCTION__, __LINE__, pVideo_info->dp, pVideo_info->cs, pVideo_info->frame_rate);
                    // convert all color fmt to RGB
                    if (pVideo_info->dp == hdmirx::hdmi::HDMIRX_BIT_DEPTH_8_BIT) {
                        out->streams[i].v3_2.overrideFormat = PixelFormat::RGB_888;
                    }
                    else {
                        // video encoder is not support 10bit yet
                        out->streams[i].v3_2.overrideFormat = (config.streams[i].usage & BufferUsage::VIDEO_ENCODER) ?
                        PixelFormat::RGB_888 : PixelFormat::RGBA_1010102;
                    }
                }

                // Save overridden formt in mStreamMap
                mStreamMap[config.streams[i].id].format = out->streams[i].v3_2.overrideFormat;
                RXLOGV("%s: defeined format 0x%x", __FUNCTION__, out->streams[i].v3_2.overrideFormat);
            }
                break;
            default:
                RXLOGE("%s: unsupported format 0x%x", __FUNCTION__, config.streams[i].format);
                return Status::ILLEGAL_ARGUMENT;
        }

    }

    mFirstRequest = true;
    return Status::OK;
}

bool HdmirxCameraDeviceSession::isClosed() {
    Mutex::Autolock _l(mLock);
    return mClosed;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define UPDATE(md, tag, data, size)               \
do {                                              \
    if ((md).update((tag), (data), (size))) {     \
        RXLOGE("Update " #tag " failed!");         \
        return BAD_VALUE;                         \
    }                                             \
} while (0)

status_t HdmirxCameraDeviceSession::initDefaultRequests() {
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata md;

    const uint8_t aberrationMode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF;
    UPDATE(md, ANDROID_COLOR_CORRECTION_ABERRATION_MODE, &aberrationMode, 1);

    const int32_t exposureCompensation = 0;
    UPDATE(md, ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &exposureCompensation, 1);

    const uint8_t videoStabilizationMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    UPDATE(md, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &videoStabilizationMode, 1);

    const uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    const uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    UPDATE(md, ANDROID_CONTROL_AE_MODE, &aeMode, 1);

    const uint8_t aePrecaptureTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    UPDATE(md, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &aePrecaptureTrigger, 1);

    const uint8_t afMode = ANDROID_CONTROL_AF_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AF_MODE, &afMode, 1);

    const uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    UPDATE(md, ANDROID_CONTROL_AF_TRIGGER, &afTrigger, 1);

    const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    UPDATE(md, ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    const uint8_t effectMode = ANDROID_CONTROL_EFFECT_MODE_OFF;
    UPDATE(md, ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    const uint8_t flashMode = ANDROID_FLASH_MODE_OFF;
    UPDATE(md, ANDROID_FLASH_MODE, &flashMode, 1);

    const int32_t thumbnailSize[] = {240, 180};
    UPDATE(md, ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);

    const uint8_t jpegQuality = 90;
    UPDATE(md, ANDROID_JPEG_QUALITY, &jpegQuality, 1);
    UPDATE(md, ANDROID_JPEG_THUMBNAIL_QUALITY, &jpegQuality, 1);

    const int32_t jpegOrientation = 0;
    UPDATE(md, ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    const uint8_t oisMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    UPDATE(md, ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &oisMode, 1);

    const uint8_t nrMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
    UPDATE(md, ANDROID_NOISE_REDUCTION_MODE, &nrMode, 1);

    const int32_t testPatternModes = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    UPDATE(md, ANDROID_SENSOR_TEST_PATTERN_MODE, &testPatternModes, 1);

    const uint8_t fdMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    UPDATE(md, ANDROID_STATISTICS_FACE_DETECT_MODE, &fdMode, 1);

    const uint8_t hotpixelMode = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
    UPDATE(md, ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, &hotpixelMode, 1);

    bool support30Fps = false;
    int32_t maxFps = std::numeric_limits<int32_t>::min();
    for (const auto& supportedFormat : mSupportedFormats) {
        for (const auto& fr : supportedFormat.frameRates) {
            int32_t framerateInt = static_cast<int32_t>(fr.getDouble());
            if (maxFps < framerateInt) {
                maxFps = framerateInt;
            }
            if (framerateInt == 30) {
                support30Fps = true;
                break;
            }
        }
        if (support30Fps) {
            break;
        }
    }
    int32_t defaultFramerate = support30Fps ? 30 : maxFps;
    int32_t defaultFpsRange[] = {defaultFramerate / 2, defaultFramerate};
    UPDATE(md, ANDROID_CONTROL_AE_TARGET_FPS_RANGE, defaultFpsRange, ARRAY_SIZE(defaultFpsRange));

    uint8_t antibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antibandingMode, 1);

    const uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_MODE, &controlMode, 1);

    // mdp profile
    const int32_t mdp_profile = MDP_PROFILE_AUTO;
    UPDATE(md, MTK_CAMERA_MDP_PROFILE, &mdp_profile, 1);

    auto requestTemplates = hidl_enum_range<RequestTemplate>();
    for (RequestTemplate type : requestTemplates) {
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata mdCopy = md;
        uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        switch (type) {
            case RequestTemplate::PREVIEW:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
                break;
            case RequestTemplate::STILL_CAPTURE:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
                break;
            case RequestTemplate::VIDEO_RECORD:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
                break;
            case RequestTemplate::VIDEO_SNAPSHOT:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
                break;
            default:
                RXLOGV("%s: unsupported RequestTemplate type %d", __FUNCTION__, type);
                continue;
        }
        UPDATE(mdCopy, ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);

        camera_metadata_t* rawMd = mdCopy.release();
        CameraMetadata hidlMd;
        hidlMd.setToExternal(
                (uint8_t*) rawMd, get_camera_metadata_size(rawMd));
        mDefaultRequests[type] = hidlMd;
        free_camera_metadata(rawMd);
    }

    return OK;
}

status_t HdmirxCameraDeviceSession::fillHdmiRxInfoResult(common::V1_0::helper::CameraMetadata &md,
	int32_t width, int32_t height, int32_t dvi, int32_t framerate, int32_t hdcp, int32_t interlace) {
	return fillHdmiRxInfo(md, width, height, dvi, framerate, hdcp, interlace);
}

status_t HdmirxCameraDeviceSession::fillCaptureResult(
        common::V1_0::helper::CameraMetadata &md, nsecs_t timestamp) {
    bool afTrigger = false;
    {
        std::lock_guard<std::mutex> lk(mAfTriggerLock);
        afTrigger = mAfTrigger;
        if (md.exists(ANDROID_CONTROL_AF_TRIGGER)) {
            camera_metadata_entry entry = md.find(ANDROID_CONTROL_AF_TRIGGER);
            if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_START) {
                mAfTrigger = afTrigger = true;
            } else if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_CANCEL) {
                mAfTrigger = afTrigger = false;
            }
        }
    }

    // For USB camera, the USB camera handles everything and we don't have control
    // over AF. We only simply fake the AF metadata based on the request
    // received here.
    uint8_t afState;
    if (afTrigger) {
        afState = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
    } else {
        afState = ANDROID_CONTROL_AF_STATE_INACTIVE;
    }
    UPDATE(md, ANDROID_CONTROL_AF_STATE, &afState, 1);

    camera_metadata_ro_entry activeArraySize =
            mCameraCharacteristics.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);

    return fillCaptureResultCommon(md, timestamp, activeArraySize);
}

#undef ARRAY_SIZE
#undef UPDATE

}  // namespace implementation
}  // namespace V3_4
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
