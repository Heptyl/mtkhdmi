
#include <AVSyncService.h>
#include <binder/IServiceManager.h>
#include <graphics_mtk_defs.h>
#include <mtk_sideband_handle.h>
#include <mtkbufferqueue/SurfaceExt.h>
#include <pthread.h>
#include <ui/gralloc_extra.h>

#include "inc/mdp_if.h"

using namespace AVSYNC_SERVICE;

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

#define PIXEL_FORMAT HAL_PIXEL_FORMAT_YUYV

static const size_t buf_count = 3;  // number of buffer
static uint32_t vdec_id;
static sp<IAVSyncService> service;
static sp<SurfaceExt> surface;
static ANativeWindow *mNativeWindow;
static pthread_mutex_t m_lock;

static int get_avsync(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (service == nullptr) {
        String16 serviceName(AVSyncService::getServiceName());
        service = interface_cast<IAVSyncService>(
            defaultServiceManager()->getService(serviceName));
        if (service == nullptr) {
            ALOGE("get avsync serivce fail");
            return -EINVAL;
        }
    }
    return 0;
}

static int create_buffer_queue(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (get_avsync()) return -EINVAL;
    sp<IGraphicBufferProducerExt> producer;
    service->createBufferQueue(vdec_id, &producer);
    if (producer == nullptr) {
        ALOGE("createBufferQueue fail");
        return -EINVAL;
    }
    surface = new SurfaceExt(producer, true);
    mNativeWindow = surface.get();
    return 0;
}

int fbm_get_display(uint32_t *w, uint32_t *h) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (get_avsync()) return -EINVAL;
    service->getDisplaySize(w, h);
    if (*h > *w) *h = (*w * 9) >> 4; /* 16:9 */
    return 0;
}

static int get_display_size(int pip, int *w, int *h) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    uint32_t width = FULL_WIDTH, height = FULL_HEIGHT;
    fbm_get_display(&width, &height);
    if (pip) {
        *w = PIP_SIZE(width);
        *h = PIP_SIZE(height);
    } else {
        *w = width;
        *h = height;
    }
    return 0;
}

static int config_output_buffer(int w, int h, uint64_t usage, int format) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    int err;

    err = native_window_api_connect(mNativeWindow, NATIVE_WINDOW_API_MEDIA);
    if (err < 0) {
        ALOGE("native_window_api_connect fail");
        return err;
    }
    err = native_window_set_buffers_dimensions(mNativeWindow, w, h);
    if (err < 0) {
        ALOGE("native_window_set_buffers_dimensions fail");
        return err;
    }
    err = native_window_set_buffers_format(mNativeWindow, format);
    if (err < 0) {
        ALOGE("native_window_set_buffers_format fail");
        return err;
    }
    err = native_window_set_usage(mNativeWindow, usage);
    if (err < 0) {
        ALOGE("native_window_set_usage fail");
        return err;
    }
    err = native_window_set_buffer_count(mNativeWindow, buf_count);
    if (err < 0) {
        ALOGE("native_window_set_buffer_count fail");
        return err;
    }
    return 0;
}

static int process_mdp(struct HDMIRX_VID_PARA *info, uint32_t frame_no) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    int err = 0;
    int fence = -1;
    ANativeWindowBuffer *buf;
    sp<GraphicBuffer> graphicBuffer;
    buffer_handle_t buffer;

    // timestamp
    gralloc_extra_ion_sf_info_t ext_info = {};
    ext_info.timestamp = 1000 * frame_no / info->frame_rate;
    ext_info.status = 1;
    ext_info.videobuffer_status = 0x80000000;

    pthread_mutex_lock(&m_lock);
    // check end?
    if (surface == nullptr) {
        err = -EINVAL;
        goto err;
    }

    // dequeue buffer
    err = native_window_dequeue_buffer_and_wait(mNativeWindow, &buf);
    if (err < 0) {
        ALOGE("native_window_dequeue_buffer_and_wait fail");
        goto err;
    }
    graphicBuffer = GraphicBuffer::from(buf);
    buffer = graphicBuffer->handle;

    // mdp process
    err = mdp_trigger(buffer, &fence, info);
    if (err < 0) {
        goto err;
    }

    // timestamp
    gralloc_extra_perform(buffer, GRALLOC_EXTRA_SET_SF_INFO, &ext_info);

    // queue buffer
    err = mNativeWindow->queueBuffer(mNativeWindow,
                                     graphicBuffer->getNativeBuffer(), fence);
    if (err < 0) {
        ALOGE("queueBuffer fail");
        goto err;
    }

err:
    pthread_mutex_unlock(&m_lock);
    return err;
}

static void *mdpThread(void *para) {
    uint32_t frame_no = 0;
    while (1) {
        int err = process_mdp((struct HDMIRX_VID_PARA *)para, ++frame_no);
        if (err < 0) break;
        usleep(1000);
    }
    return nullptr;
}

/*****************************************************************************/

void fbm_set_display(int fps, int w, int p, int h) { /* for debug purpose */
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    hwcLayerInfo layerInfo;
    layerInfo.bufInfo.layer_enable = true;
    layerInfo.bufInfo.release_fence_fd = -1;
    layerInfo.bufInfo.ion_fd = -1;
    layerInfo.bufInfo.pts = 0;
    layerInfo.bufInfo.fps = fps * 1000; /* source */
    layerInfo.bufInfo.src.x = 0;
    layerInfo.bufInfo.src.y = 0;
    layerInfo.bufInfo.src.width = w;
    layerInfo.bufInfo.src.height = h;
    layerInfo.bufInfo.src.pitch = p;
    layerInfo.bufInfo.crop.x = 0;
    layerInfo.bufInfo.crop.y = 0;
    layerInfo.bufInfo.crop.width = w;
    layerInfo.bufInfo.crop.height = h;
    layerInfo.bufInfo.crop.pitch = p;
    layerInfo.bufInfo.tgt.x = 0;
    layerInfo.bufInfo.tgt.y = h * 2 / 3;
    layerInfo.bufInfo.tgt.width = w;
    layerInfo.bufInfo.tgt.height = h;
    layerInfo.bufInfo.tgt.pitch = p;
    layerInfo.bufInfo.src_fmt = PIXEL_FORMAT;
    service->setSideBandInfo(1, vdec_id, true);
    service->setLayerInfo(vdec_id, layerInfo);
}

int fbm_open(int pip, struct HDMIRX_VID_PARA *info) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    pthread_t tid;
    int err;
    int w, h;

    vdec_id = getpid();
    err = create_buffer_queue();
    if (err < 0) return err;
    err = get_display_size(pip, &w, &h);
    if (err < 0) return err;
    err = config_output_buffer(
        w, h, GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP,
        PIXEL_FORMAT);
    if (err < 0) return err;

    pthread_mutex_init(&m_lock, NULL);
    // create thread
    pthread_create(&tid, NULL, mdpThread, info);
    return 0;
}

void fbm_close(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    pthread_mutex_lock(&m_lock);
    if (surface) {
        native_window_api_disconnect(mNativeWindow, NATIVE_WINDOW_API_MEDIA);
        surface = nullptr;
    }
    pthread_mutex_unlock(&m_lock);
    if (service) {
        service->destroyBufferQueue(vdec_id);
        service = nullptr;
    }
}

native_handle_t *fbm_create_sideband_handle(void) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    mtk_sideband_native_handle_t *handle =
        (mtk_sideband_native_handle_t *)native_handle_create(
            MTK_SIDEBAND_HANDLE_FD_NUM, MTK_SIDEBAND_HANDLE_INT_NUM);
    if (handle == nullptr) {
        ALOGE("native_handle_create fail");
    } else {
        handle->vdec_id = vdec_id;
        handle->vdp_id = 0;
        handle->video_only = 1;
        handle->audioHwSync = 0;
    }
    return (native_handle_t *)handle;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
