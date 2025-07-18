/*
 * Copyright 2014 The Android Open Source Project
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

#include <cutils/native_handle.h>
#include <errno.h>
#include <fcntl.h>
#include <graphics_mtk_defs.h>
#include <hardware/tv_input.h>
#include <log/log.h>
#include <malloc.h>

#include "fbm_if.h"
#include "hdmi_if.h"
#include "mdp_if.h"

using namespace vendor::mediatek::hardware::hdmirx::V1_0::implementation;

/*****************************************************************************/

typedef struct tv_input_private {
    tv_input_device_t device;

    // Callback related data
    const tv_input_callback_ops_t *callback;
    void *callback_data;
} tv_input_private_t;

static int tv_input_device_open(const struct hw_module_t *module,
                                const char *name, struct hw_device_t **device);

static struct hw_module_methods_t tv_input_module_methods = {
    .open = tv_input_device_open};

tv_input_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 0,
        .version_minor = 1,
        .id = TV_INPUT_HARDWARE_MODULE_ID,
        .name = "Sample TV input module",
        .author = "The Android Open Source Project",
        .methods = &tv_input_module_methods,
    }};

/*****************************************************************************/

#define HDMI_DEV_ID 1
#define HDMI_PORT_ID 1
/*
 * HDMI_STREAM_NUM
 * 1: only tunnel mode (full screen)
 * 2: add tunnel mode (picture-in-picture)
 * 3: add normal mode (unsupported after treble)
 */
#define HDMI_STREAM_NUM 2

static int opened_stream_id;
static struct HDMIRX_VID_PARA video_info;

static int stream_change(struct tv_input_device *dev) {
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv == nullptr || priv->callback == nullptr) return -EINVAL;

    ALOGI("stream changed");
    tv_input_event_t event;
    event.type = TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED;
    event.device_info.device_id = HDMI_DEV_ID;
    priv->callback->notify(dev, &event, priv->callback_data);
    return 0;
}

static int request_capture_done(struct tv_input_device *dev,
                                tv_input_event_t *event, int status) {
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv == nullptr || priv->callback == nullptr || event == nullptr)
        return -EINVAL;

    if (status) {
        event->type = TV_INPUT_EVENT_CAPTURE_FAILED;
        event->capture_result.error_code = -ETIME;
    }
    priv->callback->notify(dev, event, priv->callback_data);
    free(event);
    return 0;
}

/*****************************************************************************/

static void tv_input_available(struct tv_input_device *dev) {
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    tv_input_event_t event;
    event.type = TV_INPUT_EVENT_DEVICE_AVAILABLE;
    event.device_info.device_id = HDMI_DEV_ID;
    event.device_info.type = TV_INPUT_TYPE_HDMI;
    event.device_info.hdmi.port_id = HDMI_PORT_ID;
    // event.device_info.audio_type = AUDIO_DEVICE_NONE;
    event.device_info.audio_type = AUDIO_DEVICE_IN_HDMI;
    event.device_info.audio_address = "";
    priv->callback->notify(dev, &event, priv->callback_data);
}

static int tv_input_initialize(struct tv_input_device *dev,
                               const tv_input_callback_ops_t *callback,
                               void *data) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || callback == nullptr) return -EINVAL;
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv->callback != nullptr) return -EEXIST;

    ALOGI("initialize");
    priv->callback = callback;
    priv->callback_data = data;
    /* HDMI-RX is built-in and always available */
    tv_input_available(dev);
    return 0;
}

static tv_stream_config_t *tv_input_stream_config(void) {
    tv_stream_config_t *config =
        (tv_stream_config_t *)malloc(sizeof(config) * HDMI_STREAM_NUM);
    uint32_t w = FULL_WIDTH, h = FULL_HEIGHT;
    fbm_get_display(&w, &h);
    {
        config[0].stream_id = 0;
        config[0].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        config[0].max_video_width = w;
        config[0].max_video_height = h;
    }
    if (HDMI_STREAM_NUM > 1) {
        config[1].stream_id = 1;
        config[1].type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        config[1].max_video_width = PIP_SIZE(w);
        config[1].max_video_height = PIP_SIZE(h);
    }
    if (HDMI_STREAM_NUM > 2) {
        config[2].stream_id = 2;
        config[2].type = TV_STREAM_TYPE_BUFFER_PRODUCER;
        config[2].max_video_width = HDMI_MAX_WIDTH;
        config[2].max_video_height = HDMI_MAX_HEIGHT;
    }
    return config;
}

static int tv_input_get_stream_configurations(
    const struct tv_input_device *dev, int device_id, int *num_configurations,
    const tv_stream_config_t **configs) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || device_id != HDMI_DEV_ID ||
        num_configurations == nullptr || configs == nullptr)
        return -EINVAL;

    if (hdmi_check_cable()) {
        ALOGI("HDMI connected");
        *num_configurations = HDMI_STREAM_NUM;
        *configs = tv_input_stream_config();
    } else {
        ALOGI("HDMI disconnected");
        *num_configurations = 0;
    }
    return 0;
}

static int tv_input_open_stream(struct tv_input_device *dev, int device_id,
                                tv_stream_t *stream) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || device_id != HDMI_DEV_ID || stream == nullptr ||
        stream->stream_id < 0 || stream->stream_id >= HDMI_STREAM_NUM)
        return -EINVAL;
    if (opened_stream_id)
        return (opened_stream_id == stream->stream_id) ? -EEXIST : -EBUSY;
    if (!hdmi_check_video_locked() || hdmi_get_video_info(&video_info) < 0)
        return -EBUSY;

    ALOGI("open stream: %d", stream->stream_id);
    mdp_open();
    if (HDMI_STREAM_NUM > 2 && stream->stream_id == 2) {
        mdp_reg_done_callback(dev, request_capture_done);
        stream->type = TV_STREAM_TYPE_BUFFER_PRODUCER;
        stream->buffer_producer.width = video_info.hactive;
        stream->buffer_producer.height = video_info.vactive;
        stream->buffer_producer.usage = 0;
        stream->buffer_producer.format = HAL_PIXEL_FORMAT_YUYV;
    } else {
        stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        if (fbm_open(stream->stream_id, &video_info) < 0) return -EINVAL;
        stream->sideband_stream_source_handle = fbm_create_sideband_handle();
    }
    opened_stream_id = stream->stream_id;
    return 0;
}

static int tv_input_close_stream(struct tv_input_device *dev, int device_id,
                                 int stream_id) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || device_id != HDMI_DEV_ID || stream_id < 0 ||
        stream_id >= HDMI_STREAM_NUM)
        return -EINVAL;
    if (stream_id != opened_stream_id) return -ENOENT;

    ALOGI("close stream: %d", stream_id);
    if (!(HDMI_STREAM_NUM > 2 && stream_id == 2)) fbm_close();
    mdp_close();
    opened_stream_id = 0;
    return 0;
}

/* capture is only for normal mode */
static int tv_input_request_capture(struct tv_input_device *dev, int device_id,
                                    int stream_id, buffer_handle_t buffer,
                                    uint32_t seq) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || device_id != HDMI_DEV_ID || stream_id != 1)
        return -EINVAL;
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv == nullptr || priv->callback == nullptr) return -EINVAL;

    tv_input_event_t *event = (tv_input_event_t *)malloc(sizeof(*event));
    event->type = TV_INPUT_EVENT_CAPTURE_SUCCEEDED;
    event->capture_result.buffer = buffer;
    event->capture_result.device_id = device_id;
    event->capture_result.stream_id = stream_id;
    event->capture_result.seq = seq;
    event->capture_result.error_code = 0;
    return mdp_capture(event);
}

static int tv_input_cancel_capture(struct tv_input_device *dev, int device_id,
                                   int stream_id, uint32_t seq) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (dev == nullptr || device_id != HDMI_DEV_ID || stream_id != 1)
        return -EINVAL;
    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv == nullptr || priv->callback == nullptr) return -EINVAL;

    tv_input_event_t event;
    event.type = TV_INPUT_EVENT_CAPTURE_FAILED;
    event.capture_result.device_id = device_id;
    event.capture_result.stream_id = stream_id;
    event.capture_result.seq = seq;
    event.capture_result.error_code = -ECANCELED;
    priv->callback->notify(dev, &event, priv->callback_data);
    return 0;
}

/*****************************************************************************/

static int tv_input_device_close(struct hw_device_t *dev) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);

    hdmi_stop_observing();
    hdmi_enable(0);

    tv_input_private_t *priv = reinterpret_cast<tv_input_private_t *>(dev);
    if (priv) free(priv);
    return 0;
}

/*****************************************************************************/

static int tv_input_device_open(const struct hw_module_t *module,
                                const char *name, struct hw_device_t **device) {
    ALOGV("%s@%d", __FUNCTION__, __LINE__);
    if (strcmp(name, TV_INPUT_DEFAULT_DEVICE)) return -EINVAL;

    tv_input_private_t *dev = (tv_input_private_t *)malloc(sizeof(*dev));
    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version = TV_INPUT_DEVICE_API_VERSION_0_1;
    dev->device.common.module = const_cast<hw_module_t *>(module);
    dev->device.common.close = tv_input_device_close;

    dev->device.initialize = tv_input_initialize;
    dev->device.get_stream_configurations = tv_input_get_stream_configurations;
    dev->device.open_stream = tv_input_open_stream;
    dev->device.close_stream = tv_input_close_stream;
    dev->device.request_capture = tv_input_request_capture;
    dev->device.cancel_capture = tv_input_cancel_capture;
    *device = &dev->device.common;

    /* initialize hdmi */
    hdmi_enable(1);
    hdmi_start_observing(reinterpret_cast<tv_input_device *>(dev),
                         stream_change);
    return 0;
}
