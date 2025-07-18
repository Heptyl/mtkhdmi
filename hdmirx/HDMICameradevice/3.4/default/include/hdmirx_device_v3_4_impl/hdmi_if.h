#ifndef LIB_INC_HDMI_IF_H__
#define LIB_INC_HDMI_IF_H__

#define HDMI_MAX_WIDTH 3840
#define HDMI_MAX_HEIGHT 2160

#define FULL_WIDTH 1200
#define FULL_HEIGHT 1920

#define PIP_WIDTH (75 * 16)
#define PIP_HEIGHT (75 * 9)

#define DISPLAY_FPS 60

#define HDMIRX_STATUS_UNLOCKED (0)
#define HDMIRX_STATUS_LOCKED (1)
#define HDMIRX_STATUS_UNDEFINED (-1)

namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace hdmi {


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

enum HdmiRxClrSpc {
    HDMI_RX_CLRSPC_UNKNOWN,
    HDMI_RX_CLRSPC_YC444_601,
    HDMI_RX_CLRSPC_YC422_601,
    HDMI_RX_CLRSPC_YC420_601,

    HDMI_RX_CLRSPC_YC444_709,
    HDMI_RX_CLRSPC_YC422_709,
    HDMI_RX_CLRSPC_YC420_709,

    HDMI_RX_CLRSPC_XVYC444_601,
    HDMI_RX_CLRSPC_XVYC422_601,
    HDMI_RX_CLRSPC_XVYC420_601,

    HDMI_RX_CLRSPC_XVYC444_709,
    HDMI_RX_CLRSPC_XVYC422_709,
    HDMI_RX_CLRSPC_XVYC420_709,

    HDMI_RX_CLRSPC_sYCC444_601,
    HDMI_RX_CLRSPC_sYCC422_601,
    HDMI_RX_CLRSPC_sYCC420_601,

    HDMI_RX_CLRSPC_Adobe_YCC444_601,
    HDMI_RX_CLRSPC_Adobe_YCC422_601,
    HDMI_RX_CLRSPC_Adobe_YCC420_601,

    HDMI_RX_CLRSPC_RGB,
    HDMI_RX_CLRSPC_Adobe_RGB,

    HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous,
    HDMI_RX_CLRSPC_BT_2020_RGB_const_luminous,

    HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous,
    HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous,
    HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous,

    HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous,
    HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous,
    HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous
};

enum HdmiRxRange {
    HDMI_RX_RGB_FULL,
    HDMI_RX_RGB_LIMT,
    HDMI_RX_YCC_FULL,
    HDMI_RX_YCC_LIMT
};


struct HDMIRX_DEV_INFO {
    uint8_t hdmirx5v;
    bool hpd;
    uint32_t power_on;
    uint8_t state;
    uint8_t vid_locked;
    uint8_t aud_locked;
    uint8_t hdcp_version;
    uint8_t hdcp14_decrypted;
    uint8_t hdcp22_decrypted;
};

struct HDMIRX_VID_PARA {
    enum HDMIRX_CS cs;
    enum HdmiRxDP dp;
    enum HdmiRxClrSpc HdmiClrSpc;
    enum HdmiRxRange HdmiRange;
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

struct hdr10InfoPkt {
    uint8_t type;
    bool fgValid;
    uint8_t u1InfoData[32];
};

//#ifdef __cplusplus
//extern "C" {
//#endif


int hdmi_enable(uint64_t en);
int hdmi_get_video_info(struct HDMIRX_VID_PARA *vinfo);
int hdmi_get_device_info(struct HDMIRX_DEV_INFO *dinfo);
uint8_t hdmi_check_cable(void);
uint8_t hdmi_check_video_locked(void);
uint8_t hdmi_check_hdcp_version(void);
int hdmi_get_hdr10_info(struct hdr10InfoPkt *vinfo);
void hdmi_update_lock_status(int lock);
bool hdmi_get_lock_status(void);
struct HDMIRX_VID_PARA* hdmi_query_video_info(void);
struct HDMIRX_DEV_INFO* hdmi_query_device_info(void);
struct hdr10InfoPkt* hdmi_query_hdr_info(void);

//#ifdef __cplusplus
//}
//#endif

}
}
}
}
}

#endif  // LIB_INC_HDMI_IF_H__
