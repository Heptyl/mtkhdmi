/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef HDMITX_H
#define HDMITX_H
#define HDMI_DEV_DRV "/dev/hdmitx"
typedef enum {
    HDMI_DEEP_COLOR_AUTO = 0,
    HDMI_NO_DEEP_COLOR,
    HDMI_DEEP_COLOR_10_BIT,
    HDMI_DEEP_COLOR_12_BIT,
    HDMI_DEEP_COLOR_16_BIT
} HDMI_DEEP_COLOR_T;

typedef enum {
    HDMI_RGB = 0,
    HDMI_RGB_FULL,
    HDMI_YCBCR_444,
    HDMI_YCBCR_422,
    HDMI_YCBCR_420,
    HDMI_XV_YCC,
    HDMI_YCBCR_444_FULL,
    HDMI_YCBCR_422_FULL,
    HDMI_YCBCR_420_FULL
} HDMI_OUT_COLOR_SPACE_T;

#define SINK_480P (1 << 0)
#define SINK_720P60 (1 << 1)
#define SINK_1080I60 (1 << 2)
#define SINK_1080P60 (1 << 3)
#define SINK_480P_1440 (1 << 4)
#define SINK_480P_2880 (1 << 5)
#define SINK_480I (1 << 6)
#define SINK_480I_1440 (1 << 7)
#define SINK_480I_2880 (1 << 8)
#define SINK_1080P30 (1 << 9)
#define SINK_576P (1 << 10)
#define SINK_720P50 (1 << 11)
#define SINK_1080I50 (1 << 12)
#define SINK_1080P50 (1 << 13)
#define SINK_576P_1440 (1 << 14)
#define SINK_576P_2880 (1 << 15)
#define SINK_576I (1 << 16)
#define SINK_576I_1440 (1 << 17)
#define SINK_576I_2880 (1 << 18)
#define SINK_1080P25 (1 << 19)
#define SINK_1080P24 (1 << 20)
#define SINK_1080P23976 (1 << 21)
#define SINK_1080P2997 (1 << 22)

/*the 2160 mean 3840x2160 */
#define SINK_2160P_23_976HZ (1 << 0)
#define SINK_2160P_24HZ (1 << 1)
#define SINK_2160P_25HZ (1 << 2)
#define SINK_2160P_29_97HZ (1 << 3)
#define SINK_2160P_30HZ (1 << 4)
/*the 2161 mean 4096x2160 */
#define SINK_2161P_24HZ (1 << 5)
#define SINK_2161P_25HZ (1 << 6)
#define SINK_2161P_30HZ (1 << 7)
#define SINK_2160P_50HZ (1 << 8)
#define SINK_2160P_60HZ (1 << 9)
#define SINK_2161P_50HZ (1 << 10)
#define SINK_2161P_60HZ (1 << 11)

typedef enum {
  HDMI_STATUS_OK = 0,
  HDMI_STATUS_NOT_IMPLEMENTED,
  HDMI_STATUS_ALREADY_SET,
  HDMI_STATUS_ERROR,
} HDMI_STATUS;
typedef enum {
  SMART_BOOK_DISCONNECTED = 0,
  SMART_BOOK_CONNECTED,
} SMART_BOOK_STATE;
#define SINK_YCBCR_444 (1<<0)
#define SINK_YCBCR_422 (1<<1)
#define SINK_XV_YCC709 (1<<2)
#define SINK_XV_YCC601 (1<<3)
#define SINK_METADATA0 (1<<4)
#define SINK_METADATA1 (1<<5)
#define SINK_METADATA2 (1<<6)
#define SINK_RGB       (1<<7)
#define SINK_COLOR_SPACE_BT2020_CYCC  (1<<8)
#define SINK_COLOR_SPACE_BT2020_YCC  (1<<9)
#define SINK_COLOR_SPACE_BT2020_RGB  (1<<10)
#define SINK_YCBCR_420 (1<<11)
#define SINK_YCBCR_420_CAPABILITY (1<<12)

typedef enum {
    HDMI_FORCE_DEFAULT,
    HDMI_FORCE_SDR,
    HDMI_FORCE_HDR,
    HDMI_FORCE_HDR10,
    HDMI_FORCE_DV,
    HDMI_FORCE_HLG,
    HDMI_FORCE_HDR10_PLUS,
} HDMI_FORCE_HDR_ENABLE;

typedef enum {
  HDMI_POWER_STATE_OFF = 0,
  HDMI_POWER_STATE_ON,
  HDMI_POWER_STATE_STANDBY,
} HDMI_POWER_STATE;
typedef enum {
  HDMI_MAX_CHANNEL_2 = 0x2,
  HDMI_MAX_CHANNEL_3 = 0x3,
  HDMI_MAX_CHANNEL_4 = 0x4,
  HDMI_MAX_CHANNEL_5 = 0x5,
  HDMI_MAX_CHANNEL_6 = 0x6,
  HDMI_MAX_CHANNEL_7 = 0x7,
  HDMI_MAX_CHANNEL_8 = 0x8,
} AUDIO_MAX_CHANNEL;
typedef enum {
  HDMI_MAX_SAMPLERATE_32 = 0x1,
  HDMI_MAX_SAMPLERATE_44 = 0x2,
  HDMI_MAX_SAMPLERATE_48 = 0x3,
  HDMI_MAX_SAMPLERATE_96 = 0x4,
  HDMI_MAX_SAMPLERATE_192 = 0x5,
} AUDIO_MAX_SAMPLERATE;
typedef enum {
  HDMI_MAX_BITWIDTH_16 = 0x1,
  HDMI_MAX_BITWIDTH_24 = 0x2,
} AUDIO_MAX_BITWIDTH;
typedef enum {
  HDMI_SCALE_ADJUSTMENT_SUPPORT = 0x01,
  HDMI_ONE_RDMA_LIMITATION = 0x02,
  HDMI_PHONE_GPIO_REUSAGE = 0x04,
  HDMI_FACTORY_MODE_NEW = 0x1000,
} HDMI_CAPABILITY;
typedef enum {
  HDMI_TO_TV = 0x0,
  HDMI_TO_SMB,
} hdmi_device_type;
typedef enum {
  HDMI_IS_DISCONNECTED = 0,
  HDMI_IS_CONNECTED = 1,
  HDMI_IS_RES_CHG = 0x11,
} hdmi_connect_status;
#define MAKE_MTK_HDMI_FORMAT_ID(id,bpp) (((id) << 8) | (bpp))
typedef enum {
  MTK_HDMI_FORMAT_UNKNOWN = 0,
  MTK_HDMI_FORMAT_RGB565 = MAKE_MTK_HDMI_FORMAT_ID(1, 2),
  MTK_HDMI_FORMAT_RGB888 = MAKE_MTK_HDMI_FORMAT_ID(2, 3),
  MTK_HDMI_FORMAT_BGR888 = MAKE_MTK_HDMI_FORMAT_ID(3, 3),
  MTK_HDMI_FORMAT_ARGB8888 = MAKE_MTK_HDMI_FORMAT_ID(4, 4),
  MTK_HDMI_FORMAT_ABGR8888 = MAKE_MTK_HDMI_FORMAT_ID(5, 4),
  MTK_HDMI_FORMAT_YUV422 = MAKE_MTK_HDMI_FORMAT_ID(6, 2),
  MTK_HDMI_FORMAT_XRGB8888 = MAKE_MTK_HDMI_FORMAT_ID(7, 4),
  MTK_HDMI_FORMAT_XBGR8888 = MAKE_MTK_HDMI_FORMAT_ID(8, 4),
  MTK_HDMI_FORMAT_BPP_MASK = 0xFF,
} MTK_HDMI_FORMAT;
typedef struct {
  bool is_audio_enabled;
  bool is_video_enabled;
} hdmi_device_status;
typedef struct {
  void * src_base_addr;
  void * src_phy_addr;
  int src_fmt;
  unsigned int src_pitch;
  unsigned int src_offset_x, src_offset_y;
  unsigned int src_width, src_height;
  int next_buff_idx;
  int identity;
  int connected_type;
  unsigned int security;
} hdmi_video_buffer_info;
typedef struct {
  int ion_fd;
  unsigned int index;
  int fence_fd;
} hdmi_buffer_info;
#define MTK_HDMI_NO_FENCE_FD ((int) (- 1))
#define MTK_HDMI_NO_ION_FD ((int) (- 1))
typedef struct {
  unsigned int u4Addr;
  unsigned int u4Data;
} hdmi_device_write;
typedef struct {
  unsigned int u4Data1;
  unsigned int u4Data2;
} hdmi_para_setting;
typedef struct {
  unsigned char u1Hdcpkey[287];
} hdmi_hdcp_key;
typedef struct {
  unsigned char u1Hdcpkey[384];
} hdmi_hdcp_drmkey;
typedef struct {
  unsigned char u1sendsltdata[15];
} send_slt_data;
#define EDID_LENGTH 256
typedef struct _HDMI_EDID_T {
    unsigned int ui4_ntsc_resolution;	/* use EDID_VIDEO_RES_T, there are many resolution */
    unsigned int ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_native_ntsc_resolution;	/* use EDID_VIDEO_RES_T, only one NTSC resolution, Zero means none native NTSC resolution is avaiable */
    unsigned int ui4_sink_native_pal_resolution;	/* use EDID_VIDEO_RES_T, only one resolution, Zero means none native PAL resolution is avaiable */
    unsigned int ui4_sink_cea_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_cea_pal_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_1st_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned int ui4_sink_1st_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
    unsigned short ui2_sink_colorimetry;	/* use EDID_VIDEO_COLORIMETRY_T */
    unsigned char ui1_sink_rgb_color_bit;	/* color bit for RGB */
    unsigned char ui1_sink_ycbcr_color_bit;	/* color bit for YCbCr */
    unsigned char ui1_sink_dc420_color_bit;
    unsigned short ui2_sink_aud_dec;	/* use EDID_AUDIO_DECODER_T */
    unsigned char ui1_sink_is_plug_in;	/* 1: Plug in 0:Plug Out */
    unsigned int ui4_hdmi_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
    unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_type;	/* use EDID_A_FMT_CH_TYPE1 */
    unsigned int ui4_hdmi_ac3_ch_type;	/* use AVD_AC3_CH_TYPE */
    unsigned int ui4_hdmi_ac3_ch3ch4ch5ch7_type;	/* use AVD_AC3_CH_TYPE1 */
    unsigned int ui4_hdmi_ec3_ch_type;	/* AVD_DOLBY_PLUS_CH_TYPE */
    unsigned int ui4_hdmi_ec3_ch3ch4ch5ch7_type;	/* AVD_DOLBY_PLUS_CH_TYPE1 */
    unsigned int ui4_hdmi_dts_ch_type;
    unsigned int ui4_hdmi_dts_ch3ch4ch5ch7_type;
    unsigned int ui4_hdmi_dts_hd_ch_type;
    unsigned int ui4_hdmi_dts_hd_ch3ch4ch5ch7_type;
    unsigned int ui4_hdmi_pcm_bit_size;
    unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_bit_size;
    unsigned int ui4_dac_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
    unsigned char ui1_sink_support_dolby_atoms;
    unsigned char ui1_sink_i_latency_present;
    unsigned int ui1_sink_p_audio_latency;
    unsigned int ui1_sink_p_video_latency;
    unsigned int ui1_sink_i_audio_latency;
    unsigned int ui1_sink_i_video_latency;
    unsigned char ui1ExtEdid_Revision;
    unsigned char ui1Edid_Version;
    unsigned char ui1Edid_Revision;
    unsigned char ui1_Display_Horizontal_Size;
    unsigned char ui1_Display_Vertical_Size;
    unsigned int ui4_ID_Serial_Number;
    unsigned int ui4_sink_cea_3D_resolution;
    unsigned char ui1_sink_support_ai;	/* 0: not support AI, 1:support AI */
    unsigned short ui2_sink_cec_address;
    unsigned short ui1_sink_max_tmds_clock;
    unsigned short ui2_sink_3D_structure;
    unsigned int ui4_sink_cea_FP_SUP_3D_resolution;
    unsigned int ui4_sink_cea_TOB_SUP_3D_resolution;
    unsigned int ui4_sink_cea_SBS_SUP_3D_resolution;
    unsigned short ui2_sink_ID_manufacturer_name;	/* (08H~09H) */
    unsigned short ui2_sink_ID_product_code;	/* (0aH~0bH) */
    unsigned int ui4_sink_ID_serial_number;	/* (0cH~0fH) */
    unsigned char ui1_sink_week_of_manufacture;	/* (10H) */
    unsigned char ui1_sink_year_of_manufacture;	/* (11H)  base on year 1990 */
    unsigned char b_sink_SCDC_present;
    unsigned char b_sink_LTE_340M_sramble;
    unsigned int ui4_sink_hdmi_4k2kvic;
    unsigned short ui2_sink_max_tmds_character_rate;
    unsigned char ui1_sink_support_static_hdr;
    unsigned char ui1_sink_support_dynamic_hdr;
    unsigned char ui1_sink_hdr10plus_app_version;
    unsigned char ui1_sink_hdr_content_max_luminance;
    unsigned char ui1_sink_hdr_content_max_frame_average_luminance;
    unsigned char ui1_sink_hdr_content_min_luminance;
    unsigned int ui4_sink_dv_vsvdb_length;
    unsigned int ui4_sink_dv_vsvdb_version;
    unsigned int ui4_sink_dv_vsvdb_v1_low_latency;
    unsigned int ui4_sink_dv_vsvdb_v2_interface;
    unsigned int ui4_sink_dv_vsvdb_low_latency_support;
    unsigned int ui4_sink_dv_vsvdb_v2_supports_10b_12b_444;
    unsigned int ui4_sink_dv_vsvdb_support_backlight_control;
    unsigned int ui4_sink_dv_vsvdb_backlt_min_lumal;
    unsigned int ui4_sink_dv_vsvdb_tmin;
    unsigned int ui4_sink_dv_vsvdb_tmax;
    unsigned int ui4_sink_dv_vsvdb_tminPQ;
    unsigned int ui4_sink_dv_vsvdb_tmaxPQ;
    unsigned int ui4_sink_dv_vsvdb_Rx;
    unsigned int ui4_sink_dv_vsvdb_Ry;
    unsigned int ui4_sink_dv_vsvdb_Gx;
    unsigned int ui4_sink_dv_vsvdb_Gy;
    unsigned int ui4_sink_dv_vsvdb_Bx;
    unsigned int ui4_sink_dv_vsvdb_By;
    unsigned int ui4_sink_dv_vsvdb_Wx;
    unsigned int ui4_sink_dv_vsvdb_Wy;
    unsigned char ui1_sink_dv_block[32];
    unsigned char ui1rawdata_edid[EDID_LENGTH];
} HDMI_EDID_T;
typedef struct {
  unsigned int ui4_sink_FP_SUP_3D_resolution;
  unsigned int ui4_sink_TOB_SUP_3D_resolution;
  unsigned int ui4_sink_SBS_SUP_3D_resolution;
} MHL_3D_SUPP_T;
typedef struct {
  unsigned char ui1_la_num;
  unsigned char e_la[3];
  unsigned short ui2_pa;
  unsigned short h_cecm_svc;
} CEC_DRV_ADDR_CFG;
typedef struct {
  unsigned char destination : 4;
  unsigned char initiator : 4;
} CEC_HEADER_BLOCK_IO;
typedef struct {
  CEC_HEADER_BLOCK_IO header;
  unsigned char opcode;
  unsigned char operand[15];
} CEC_FRAME_BLOCK_IO;
typedef struct {
  unsigned char size;
  unsigned char sendidx;
  unsigned char reTXcnt;
  void * txtag;
  CEC_FRAME_BLOCK_IO blocks;
} CEC_FRAME_DESCRIPTION_IO;
typedef struct _CEC_FRAME_INFO {
  unsigned char ui1_init_addr;
  unsigned char ui1_dest_addr;
  unsigned short ui2_opcode;
  unsigned char aui1_operand[14];
  unsigned int z_operand_size;
} CEC_FRAME_INFO;
typedef struct _CEC_SEND_MSG {
  CEC_FRAME_INFO t_frame_info;
  unsigned char b_enqueue_ok;
} CEC_SEND_MSG;
typedef struct {
  unsigned char ui1_la;
  unsigned short ui2_pa;
} CEC_ADDRESS_IO;
typedef struct {
  unsigned char u1Size;
  unsigned char au1Data[14];
} CEC_GETSLT_DATA;
typedef struct {
  unsigned int u1adress;
  unsigned int pu1Data;
} READ_REG_VALUE;
typedef struct {
  unsigned char e_hdmi_aud_in;
  unsigned char e_iec_frame;
  unsigned char e_hdmi_fs;
  unsigned char e_aud_code;
  unsigned char u1Aud_Input_Chan_Cnt;
  unsigned char e_I2sFmt;
  unsigned char u1HdmiI2sMclk;
  unsigned char bhdmi_LCh_status[5];
  unsigned char bhdmi_RCh_status[5];
} HDMITX_AUDIO_PARA;
#define HDMI_IOW(num,dtype) _IOW('H', num, dtype)
#define HDMI_IOR(num,dtype) _IOR('H', num, dtype)
#define HDMI_IOWR(num,dtype) _IOWR('H', num, dtype)
#define HDMI_IO(num) _IO('H', num)
#define MTK_HDMI_AUDIO_VIDEO_ENABLE HDMI_IO(1)
#define MTK_HDMI_AUDIO_ENABLE HDMI_IO(2)
#define MTK_HDMI_VIDEO_ENABLE HDMI_IO(3)
#define MTK_HDMI_GET_CAPABILITY HDMI_IOWR(4, HDMI_CAPABILITY)
#define MTK_HDMI_GET_DEVICE_STATUS HDMI_IOWR(5, hdmi_device_status)
#define MTK_HDMI_VIDEO_CONFIG HDMI_IOWR(6, int)
#define MTK_HDMI_AUDIO_CONFIG HDMI_IOWR(7, int)
#define MTK_HDMI_FORCE_FULLSCREEN_ON HDMI_IOWR(8, int)
#define MTK_HDMI_FORCE_FULLSCREEN_OFF HDMI_IOWR(9, int)
#define MTK_HDMI_IPO_POWEROFF HDMI_IOWR(10, int)
#define MTK_HDMI_IPO_POWERON HDMI_IOWR(11, int)
#define MTK_HDMI_POWER_ENABLE HDMI_IOW(12, int)
#define MTK_HDMI_PORTRAIT_ENABLE HDMI_IOW(13, int)
#define MTK_HDMI_FORCE_OPEN HDMI_IOWR(14, int)
#define MTK_HDMI_FORCE_CLOSE HDMI_IOWR(15, int)
#define MTK_HDMI_IS_FORCE_AWAKE HDMI_IOWR(16, int)
#define MTK_HDMI_POST_VIDEO_BUFFER HDMI_IOW(20, hdmi_video_buffer_info)
#define MTK_HDMI_AUDIO_SETTING HDMI_IOWR(21, HDMITX_AUDIO_PARA)
#define MTK_HDMI_FACTORY_MODE_ENABLE HDMI_IOW(30, int)
#define MTK_HDMI_FACTORY_GET_STATUS HDMI_IOWR(31, int)
#define MTK_HDMI_FACTORY_DPI_TEST HDMI_IOWR(32, int)
#define MTK_HDMI_USBOTG_STATUS HDMI_IOWR(33, int)
#define MTK_HDMI_GET_DRM_ENABLE HDMI_IOWR(34, int)
#define MTK_HDMI_GET_DEV_INFO HDMI_IOWR(35, mtk_dispif_info_t)
#define MTK_HDMI_PREPARE_BUFFER HDMI_IOW(36, hdmi_buffer_info)
#define MTK_HDMI_SCREEN_CAPTURE HDMI_IOW(37, unsigned long)
#define MTK_HDMI_WRITE_DEV HDMI_IOWR(52, hdmi_device_write)
#define MTK_HDMI_READ_DEV HDMI_IOWR(53, unsigned int)
#define MTK_HDMI_ENABLE_LOG HDMI_IOWR(54, unsigned int)
#define MTK_HDMI_CHECK_EDID HDMI_IOWR(55, unsigned int)
#define MTK_HDMI_INFOFRAME_SETTING HDMI_IOWR(56, hdmi_para_setting)
#define MTK_HDMI_COLOR_DEEP HDMI_IOWR(57, hdmi_para_setting)
#define MTK_HDMI_ENABLE_HDCP HDMI_IOWR(58, unsigned int)
#define MTK_HDMI_STATUS HDMI_IOWR(59, unsigned int)
#define MTK_HDMI_HDCP_KEY HDMI_IOWR(60, hdmi_hdcp_key)
#define MTK_HDMI_GET_EDID HDMI_IOWR(61, HDMI_EDID_T)
#define MTK_HDMI_SETLA HDMI_IOWR(62, CEC_DRV_ADDR_CFG)
#define MTK_HDMI_GET_CECCMD HDMI_IOWR(63, CEC_FRAME_DESCRIPTION_IO)
#define MTK_HDMI_SET_CECCMD HDMI_IOWR(64, CEC_SEND_MSG)
#define MTK_HDMI_CEC_ENABLE HDMI_IOWR(65, unsigned int)
#define MTK_HDMI_GET_CECADDR HDMI_IOWR(66, CEC_ADDRESS_IO)
#define MTK_HDMI_CECRX_MODE HDMI_IOWR(67, unsigned int)
#define MTK_HDMI_SENDSLTDATA HDMI_IOWR(68, send_slt_data)
#define MTK_HDMI_GET_SLTDATA HDMI_IOWR(69, CEC_GETSLT_DATA)
#define MTK_HDMI_VIDEO_MUTE HDMI_IOWR(70, int)
#define MTK_HDMI_READ HDMI_IOWR(81, unsigned int)
#define MTK_HDMI_WRITE HDMI_IOWR(82, unsigned int)
#define MTK_HDMI_CMD HDMI_IOWR(83, unsigned int)
#define MTK_HDMI_DUMP HDMI_IOWR(84, unsigned int)
#define MTK_HDMI_DUMP6397 HDMI_IOWR(85, unsigned int)
#define MTK_HDMI_DUMP6397_W HDMI_IOWR(86, unsigned int)
#define MTK_HDMI_CBUS_STATUS HDMI_IOWR(87, unsigned int)
#define MTK_HDMI_CONNECT_STATUS HDMI_IOWR(88, unsigned int)
#define MTK_HDMI_DUMP6397_R HDMI_IOWR(89, unsigned int)
#define MTK_MHL_GET_DCAP HDMI_IOWR(90, unsigned int)
#define MTK_MHL_GET_3DINFO HDMI_IOWR(91, unsigned int)
#define MTK_HDMI_HDCP HDMI_IOWR(92, unsigned int)
#define MTK_HDMI_FACTORY_CHIP_INIT HDMI_IOWR(94, int)
#define MTK_HDMI_FACTORY_JUDGE_CALLBACK HDMI_IOWR(95, int)
#define MTK_HDMI_FACTORY_START_DPI_AND_CONFIG HDMI_IOWR(96, int)
#define MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF HDMI_IOWR(97, int)
#endif
