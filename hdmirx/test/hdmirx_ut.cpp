#define DEBUG_LOG_TAG "HDMI_RX_UT"

#include <grallocdev.h>
#include <graphics_mtk_defs.h>
#include <hardware/gralloc.h>
#include <inttypes.h>
#include <malloc.h>
#include <priv.h>

#include "fbm_if.h"
#include "hdmi_if.h"
#include "mdp_if.h"
#include "stc_if.h"

using namespace vendor::mediatek::hardware::hdmirx::V1_0::implementation;

#define DUMP(a)                  \
    do {                         \
        printf(#a " = %d\n", a); \
    } while (0)

static void print_cmd(void) {
    printf("0) get stc\n");
    printf("1) enable hdmi      2) disable hdmi\n");
    printf("3) get video info   4) get device info\n");
    printf("5) check cable      6) check video locked\n");
    printf("7) mdp init         8) mdp dump\n");
    printf("a) start observing  b) stop observing\n");
    printf("c) fbm init (full)  d) fbm init (pip)     e) fbm close\n");
    printf("f) fbm output size\n");
    printf("h) help             q) quit\n");
}

/*****************************************************************************/

static void get_video_info(void) {
    struct HDMIRX_VID_PARA v;
    if (hdmi_get_video_info(&v) < 0) return;

    DUMP(v.cs);
    DUMP(v.dp);
    DUMP(v.htotal);
    DUMP(v.vtotal);
    DUMP(v.hactive);
    DUMP(v.vactive);
    DUMP(v.is_pscan);
    DUMP(v.hdmi_mode);
    DUMP(v.frame_rate);
    DUMP(v.pixclk);
    DUMP(v.tmdsclk);
    DUMP(v.is_40x);
    DUMP(v.id);
}

static void get_device_info(void) {
    struct HDMIRX_DEV_INFO d;
    if (hdmi_get_device_info(&d) < 0) return;

    DUMP(d.hdmirx5v);
    DUMP(d.hpd);
    DUMP(d.power_on);
    DUMP(d.state);
    DUMP(d.vid_locked);
    DUMP(d.aud_locked);
    DUMP(d.hdcp_version);
}

static void check_cable(void) {
    if (hdmi_check_cable())
        printf("hdmi connected\n");
    else
        printf("hdmi disconnected\n");
}

static void check_video_locked(void) {
    if (hdmi_check_video_locked())
        printf("video locked\n");
    else
        printf("video unlocked\n");
}

static void get_stc(void) {
    int64_t stc = 0;
    if (stc_get(&stc) < 0) return;

    printf("stc = %" PRId64 "\n", stc);
}

/*****************************************************************************/
static void dump_buffer(buffer_handle_t buffer) {
    int err = 0;
    uint32_t format;
    int32_t ion_fd, stride, w, h, size;

    // buffer info
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_FORMAT, &format);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ION_FD, &ion_fd);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_STRIDE, &stride);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_WIDTH, &w);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_HEIGHT, &h);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size);
    if (err != GRALLOC_EXTRA_OK) {
        printf("Failed to get buffer info\n");
        return;
    }
    AVSYNC_SERVICE::dump_buf(HAL_PIXEL_FORMAT_YUYV, ion_fd, stride, w, h, size);
}

static int request_capture_done(struct tv_input_device *,
                                tv_input_event_t *event, int status) {
    if (status) {
        printf("capture fail\n");
    } else {
        dump_buffer(event->capture_result.buffer);
        printf("capture success\n");
    }
    GrallocDevice::getInstance().free(event->capture_result.buffer);
    free(event);
    return 0;
}

static void mdp_init(void) {
    mdp_open();
    mdp_reg_done_callback(nullptr, request_capture_done);
}

static void mdp_dump(void) {
    struct HDMIRX_VID_PARA v;
    GrallocDevice::AllocParam param;

    if (hdmi_check_cable() && hdmi_check_video_locked() &&
        hdmi_get_video_info(&v) == 0) {
        param.width = v.hactive;
        param.height = v.vactive;
    } else {
        param.width = HDMI_MAX_WIDTH;
        param.height = HDMI_MAX_HEIGHT;
    }
    param.format = HAL_PIXEL_FORMAT_YUYV;
    param.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_SW_READ_OFTEN;

    status_t err = GrallocDevice::getInstance().alloc(param);
    if (NO_ERROR != err) {
        printf("Failed to allocate memory\n");
        return;
    }

    tv_input_event_t *event = (tv_input_event_t *)malloc(sizeof(*event));
    memset(event, 0, sizeof(*event));
    event->capture_result.buffer = param.handle;
    mdp_capture(event);
}

/*****************************************************************************/

struct HDMIRX_VID_PARA v;

static void fbm_init(int pip) {
    uint32_t w = FULL_WIDTH, h = FULL_HEIGHT;
    mdp_open();
    if (hdmi_check_cable() && hdmi_check_video_locked() &&
        hdmi_get_video_info(&v) == 0) {
        fbm_open(pip, &v);
    } else {
        fbm_open(pip);
    }
    fbm_get_display(&w, &h);
    if (pip) {
        w = PIP_SIZE(w);
        h = PIP_SIZE(h);
    }
    fbm_set_display(v.frame_rate, w, w, h);
}

static void fbm_output(void) {
    uint32_t w = 0, h = 0;
    fbm_get_display(&w, &h);
    printf("w=%d, h=%d\n", w, h);
}

/*****************************************************************************/

int main(void) {
    char ch;
    do {
        ch = getchar();
        switch (ch) {
        case '0': get_stc();              break;
        case '1': hdmi_enable(1);         break;
        case '2': hdmi_enable(0);         break;
        case '3': get_video_info();       break;
        case '4': get_device_info();      break;
        case '5': check_cable();          break;
        case '6': check_video_locked();   break;
        case '7': mdp_init();             break;
        case '8': mdp_dump();             break;
        case 'a': hdmi_start_observing(); break;
        case 'q': fbm_close(); mdp_close(); [[fallthrough]];
        case 'b': hdmi_stop_observing();  break;
        case 'h': print_cmd();            break;
        case 'c': fbm_init(0);            break;
        case 'd': fbm_init(1);            break;
        case 'e': fbm_close();            break;
        case 'f': fbm_output();           break;
        default:                          break;
        }
    } while (ch != 'q');
    return 0;
}
