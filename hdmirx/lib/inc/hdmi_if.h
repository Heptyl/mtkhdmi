#ifndef LIB_INC_HDMI_IF_H__
#define LIB_INC_HDMI_IF_H__

#include <hardware/tv_input.h>

#define HDMI_MAX_WIDTH 3840
#define HDMI_MAX_HEIGHT 2160

#define FULL_WIDTH 1200
#define FULL_HEIGHT 1920

#define PIP_SIZE(x) ((x + 1) >> 1)

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

/* copy from kernel, drivers/misc/mediatek/hdmirx/hdmirx.h */

enum HDMIRX_CS {
    HDMI_CS_RGB = 0,
    HDMI_CS_YUV444,
    HDMI_CS_YUV422,
    HDMI_CS_YUV420
};

enum HdmiRxDP {
    HDMIRX_BIT_DEPTH_8_BIT = 0,
    HDMIRX_BIT_DEPTH_10_BIT,
    HDMIRX_BIT_DEPTH_12_BIT,
    HDMIRX_BIT_DEPTH_16_BIT
};

struct HDMIRX_DEV_INFO {
    uint8_t hdmirx5v;
    bool hpd;
    uint32_t power_on;
    uint8_t state;
    uint8_t vid_locked;
    uint8_t aud_locked;
    uint8_t hdcp_version;
};

struct HDMIRX_VID_PARA {
    enum HDMIRX_CS cs;
    enum HdmiRxDP dp;
    uint32_t htotal;
    uint32_t vtotal;
    uint32_t hactive;
    uint32_t vactive;
    uint32_t is_pscan;
    bool hdmi_mode;
    uint32_t frame_rate;
    uint32_t pixclk;
    uint32_t tmdsclk;
    bool is_40x;
    uint32_t id;
};

/*****************************************************************************/

int hdmi_enable(uint64_t en);
int hdmi_get_video_info(struct HDMIRX_VID_PARA *vinfo);
int hdmi_get_device_info(struct HDMIRX_DEV_INFO *dinfo);
uint8_t hdmi_check_cable(void);
uint8_t hdmi_check_video_locked(void);
int hdmi_start_observing(struct tv_input_device *dev = nullptr,
                         int (*tv_cb)(struct tv_input_device *) = nullptr);
void hdmi_stop_observing(void);

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

#endif  // LIB_INC_HDMI_IF_H__
