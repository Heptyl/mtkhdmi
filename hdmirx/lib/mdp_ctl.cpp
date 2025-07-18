
#include <DpAsyncBlitStream.h>
#include <errno.h>
#include <log/log.h>
#include <pthread.h>
#include <sync/sync.h>
#include <ui/gralloc_extra.h>
#include <unistd.h>

#include "inc/mdp_if.h"

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

static DpAsyncBlitStream *mdp_stream;
static tv_input_device *tv_dev;
static int (*capture_done_cb)(struct tv_input_device *dev,
                              tv_input_event_t *event, int status);

struct capture_request {
    int32_t fence;
    tv_input_event_t *event;
};

int mdp_open(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (mdp_stream) {
        return 0;
    }
    mdp_stream = new DpAsyncBlitStream();
    if (mdp_stream == nullptr) {
        ALOGE("create MDP stream fail");
        return -EINVAL;
    }
    return 0;
}

void mdp_close(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (mdp_stream) {
        delete mdp_stream;
        mdp_stream = nullptr;
    }
}

void mdp_reg_done_callback(struct tv_input_device *dev,
                           int (*cb)(struct tv_input_device *dev,
                                     tv_input_event_t *event, int status)) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    tv_dev = dev;
    capture_done_cb = cb;
}

static void *captureThread(void *para) {
    struct capture_request *req = (struct capture_request *)para;
    const int timeout = -1;
    int status;

    status = sync_wait(req->fence, timeout);
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    close(req->fence);
    if (capture_done_cb) capture_done_cb(tv_dev, req->event, status);
    free(req);
    return nullptr;
}

int mdp_trigger(buffer_handle_t buffer, int32_t *fence,
                struct HDMIRX_VID_PARA *info) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    uint32_t job;
    int32_t ion_fd;
    int out_w, out_h, in_w, in_h;
    DpColorFormat in_fmt;
    DpRect src_rect;
    uint32_t size[3] = {0};
    int err = 0;

    // output buffer info
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ION_FD, &ion_fd);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_WIDTH, &out_w);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_HEIGHT, &out_h);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size[0]);
    if (err != GRALLOC_EXTRA_OK) {
        ALOGE("gralloc_extra_query fail");
        return -EINVAL;
    }

    // input format
    if (info) {
        in_w = info->hactive;
        in_h = info->vactive;
        if (in_h >= HDMI_MAX_HEIGHT && info->cs == HDMI_CS_YUV420) in_w *= 2;
        in_fmt = (info->cs == HDMI_CS_RGB) ? eRGB888 : eI444;
    } else {  // splitter pattern gen.
        in_w = out_w;
        in_h = out_h;
        in_fmt = eYUYV;
    }
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = in_w;
    src_rect.h = in_h;

    // Job + Fence
    mdp_stream->createJob(job, *fence);
    mdp_stream->setConfigBegin(job);
    mdp_stream->setSrcConfig(in_w, in_h, in_fmt, eInterlace_None, 0,
                             PORT_HDMI_RX);
    mdp_stream->setSrcCrop(0, src_rect);
    mdp_stream->setDstBuffer(0, ion_fd, size, 1);
    mdp_stream->setDstConfig(0, out_w, out_h, out_w * 2, 0, eYUYV);
    if (in_w >= HDMI_MAX_WIDTH) mdp_stream->setUser(DP_BLIT_HWC_120FPS);
    mdp_stream->setConfigEnd();
    mdp_stream->invalidate();
    return 0;
}

int mdp_capture(tv_input_event_t *event) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    pthread_t tid;
    int32_t fence;
    int err;
    err = mdp_trigger(event->capture_result.buffer, &fence);
    if (err < 0) {
        return err;
    }

    // create thread
    struct capture_request *req =
        (struct capture_request *)malloc(sizeof(*req));
    req->fence = fence;
    req->event = event;
    pthread_create(&tid, NULL, captureThread, req);
    return 0;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
