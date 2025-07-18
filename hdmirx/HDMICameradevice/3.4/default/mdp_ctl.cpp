#define LOG_TAG "HdmirxCamDevSsn@3.4_MDP"

#include <DpAsyncBlitStream.h>
#include <DpIspStream.h>
#include <DpDataType.h>
#include <errno.h>
#include <log/log.h>
#include <pthread.h>
#include <sync/sync.h>
#include <ui/gralloc_extra.h>
#include <unistd.h>
#include <sys/mman.h>
#include <graphics_mtk_defs.h>

#include "mdp_if.h"

#ifdef KERNEL_419
#include "IONDevice.h"
#else
#include "DMABUFDevice.h"
#endif

#include <cutils/properties.h>


namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace mdp {

using ::android::hardware::camera::hdmirx::hdmi::HDMIRX_CS;
#ifdef KERNEL_419
using ::android::hardware::camera::hdmirx::ion::IONDevice;
#else
using ::android::hardware::camera::hdmirx::dma_buf::DMABUFDevice;
#endif
using ::android::hardware::camera::hdmirx::hdmi::HdmiRxClrSpc;
using ::android::hardware::camera::hdmirx::hdmi::HdmiRxRange;

#define MDP_DIRECTION_INPUT 1
#define MDP_DIRECTION_OUTPUT 2

static DpAsyncBlitStream *mdp_stream;
static DpIspStream *mdp_ispStream;
static struct DpPqParam dppqparam;

#define HDMI_RX_BT709               65536 // HAL_DATASPACE_STANDARD_BT709
#define HDMI_RX_BT2020              393216 // HAL_DATASPACE_STANDARD_BT2020
#define HDMI_RX_ST2084              29360128 // HAL_DATASPACE_TRANSFER_ST2084
#define HDMI_RX_HLG                 33554432 // HAL_DATASPACE_TRANSFER_HLG
#define HDMI_RX_RANGE_LIMITED       268435456

#define HDMI_RX_TRANS_FUNC_INDEX    4
#define HDMI_RX_HDR_DATA_INDEX      6
#define HDMI_RX_HDR_NUM_ELEMENTS    12

static int32_t hdr_metadata_key[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static float hdr_meta_data[12] = {};

static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;


uint32_t logLevel = 0;
#define RXLOGV(...) do{ if (logLevel>=4){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGD(...) do{ if (logLevel>=3){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGI(...) do{ if (logLevel>=2){ ALOGD(__VA_ARGS__); }}while(0)
#define RXLOGW(...) do{ if (logLevel>=1){ ALOGW(__VA_ARGS__); }}while(0)
#define RXLOGE(...) do{ ALOGE(__VA_ARGS__); }while(0)

int mdp_parse_hdr_info(struct hdmirx::hdmi::hdr10InfoPkt *hdr10info) {
    if (!hdr10info) return -1;

    pthread_mutex_lock(&gMutex);
    memset(&dppqparam, 0, sizeof(struct DpPqParam));
    dppqparam.enable = dppqparam.enable | PQ_VIDEO_HDR_EN;
    dppqparam.scenario = MEDIA_VIDEO;
    dppqparam.u.video.userScenario = INFO_RX;

    if ((0x07 & (hdr10info->u1InfoData[HDMI_RX_TRANS_FUNC_INDEX])) == 2 )
        dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace = HDMI_RX_ST2084;
    else if ((0x07 & (hdr10info->u1InfoData[HDMI_RX_TRANS_FUNC_INDEX])) == 3 )
        dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace = HDMI_RX_HLG;
    else
    {
        RXLOGD("%s, %d, hdr10info u1InfoData: %u", __FUNCTION__, __LINE__, (0x07 & (hdr10info->u1InfoData[HDMI_RX_TRANS_FUNC_INDEX])));
        dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace = 0;
    }

    dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace |= HDMI_RX_RANGE_LIMITED;

    hdr_meta_data[0] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 0]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 1]) << 8);
    hdr_meta_data[1] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 2]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 3]) << 8);
    hdr_meta_data[2] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 4]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 5]) << 8);
    hdr_meta_data[3] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 6]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 7]) << 8);
    hdr_meta_data[4] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 8]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 9]) << 8);
    hdr_meta_data[5] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 10]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 11]) << 8);
    hdr_meta_data[6] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 12]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 13]) << 8);
    hdr_meta_data[7] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 14]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 15]) << 8);
    hdr_meta_data[8] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 16]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 17]) << 8);
    hdr_meta_data[9] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 18]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 19]) << 8);
    hdr_meta_data[10] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 20]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 21]) << 8);
    hdr_meta_data[11] =
        (0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 22]) |
        ((0xFF & hdr10info->u1InfoData[HDMI_RX_HDR_DATA_INDEX + 23]) << 8);

    for (int i = 0; i < 8; i++)
        hdr_meta_data[i] /= 50000;
    hdr_meta_data[9] /= 10000;

    dppqparam.u.video.HDRMetadata.AOSPHDRStaticMetadata.numElements = HDMI_RX_HDR_NUM_ELEMENTS;
    dppqparam.u.video.HDRMetadata.AOSPHDRStaticMetadata.key = hdr_metadata_key;
    dppqparam.u.video.HDRMetadata.AOSPHDRStaticMetadata.metaData = hdr_meta_data;
    pthread_mutex_unlock(&gMutex);
    return 0;
}

int mdp_open(void) {

    // for debug log
    char hdmirx_log_prop[128];
    property_get("vendor.mtk.hdmirx.log", hdmirx_log_prop, "0");
    logLevel = (uint32_t) atoi(hdmirx_log_prop);
    // for debug log

    if (mdp_stream && mdp_ispStream) {
        return 0;
    }
    if (!mdp_stream) {
        mdp_stream = new DpAsyncBlitStream();
        if (mdp_stream == nullptr) {
            RXLOGE("create MDP stream fail");
            return -EINVAL;
        }

        // notify RX open to mdp
        mdp_stream->setHDMIRXStatus(1);
    }
    if (!mdp_ispStream) {
        mdp_ispStream = new DpIspStream(DpIspStream::ISP_ZSD_STREAM);
        if (mdp_ispStream == nullptr) {
            RXLOGE("create MDP isp stream fail");
            return -EINVAL;
        }
    }
    return 0;
}

void mdp_close(void) {
    if (mdp_stream) {

        // notify RX close to mdp
        mdp_stream->setHDMIRXStatus(0);

        delete mdp_stream;
        mdp_stream = nullptr;
    }
    if (mdp_ispStream) {
        delete mdp_ispStream;
        mdp_ispStream = nullptr;
    }
}

/*
static DpColorFormat hal2dpformat(PixelFormat  format) {

    switch (format) {
    case PixelFormat::RGB_888:
        return DP_COLOR_RGB888;
    case PixelFormat::YV12:
        return DP_COLOR_YV12;
    case PixelFormat::YCBCR_420_888:
        //return DP_COLOR_NV12;
        return DP_COLOR_NV12;
    default:
        return DP_COLOR_UNKNOWN;
    }
}
*/
static DpColorFormat hal2dpformat(int  format) {
    /*
        * YV12, YCRCB_420_SP, I420, YCBCR_420_888
        */
    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:
        return DP_COLOR_YV12;
    case HAL_PIXEL_FORMAT_YCRCB_420_SP:
        return DP_COLOR_NV21;
    case HAL_PIXEL_FORMAT_YCBCR_420_888:
        return DP_COLOR_NV12;
    case HAL_PIXEL_FORMAT_I420:
        return DP_COLOR_I420;
    case HAL_PIXEL_FORMAT_RGB_888:
        return DP_COLOR_RGB888;
    case HAL_PIXEL_FORMAT_RGBA_1010102:
        return DP_COLOR_RGBA1010102;
    case HAL_PIXEL_FORMAT_YCBCR_422_I:
        return DP_COLOR_YUY2;
    default:
        RXLOGE("%s, format = %d", __FUNCTION__, format);
        return DP_COLOR_UNKNOWN;
    }
}

static DP_PROFILE_ENUM hal2dpprofile(HdmiRxClrSpc  profile, HdmiRxRange range, int32_t mdp_profile, int32_t direction) {

    switch (profile) {
        case hdmi::HDMI_RX_CLRSPC_YC444_601:
        case hdmi::HDMI_RX_CLRSPC_YC422_601:
        case hdmi::HDMI_RX_CLRSPC_YC420_601:
        case hdmi::HDMI_RX_CLRSPC_XVYC444_601:
        case hdmi::HDMI_RX_CLRSPC_XVYC422_601:
        case hdmi::HDMI_RX_CLRSPC_XVYC420_601:
        case hdmi::HDMI_RX_CLRSPC_sYCC444_601:
        case hdmi::HDMI_RX_CLRSPC_sYCC422_601:
        case hdmi::HDMI_RX_CLRSPC_sYCC420_601:
        case hdmi::HDMI_RX_CLRSPC_Adobe_YCC444_601:
        case hdmi::HDMI_RX_CLRSPC_Adobe_YCC422_601:
        case hdmi::HDMI_RX_CLRSPC_Adobe_YCC420_601:
            if (MDP_DIRECTION_INPUT == direction) {
                if (MDP_PROFILE_AUTO == mdp_profile) {
                    if (range == hdmi::HDMI_RX_RGB_FULL || range == hdmi::HDMI_RX_YCC_FULL)
                        return DP_PROFILE_FULL_BT601;
                    else
                        return DP_PROFILE_BT601;
                } else if (MDP_PROFILE_FULL == mdp_profile) {
                    return DP_PROFILE_FULL_BT601;
                } else if (MDP_PROFILE_LIMIT == mdp_profile) {
                    return DP_PROFILE_BT601;
                } else {
                    return DP_PROFILE_BT601;
                }
            } else if (MDP_DIRECTION_OUTPUT == direction){
                return DP_PROFILE_FULL_BT601;
            } else {
                return DP_PROFILE_BT601;
            }
        case hdmi::HDMI_RX_CLRSPC_RGB:
        case hdmi::HDMI_RX_CLRSPC_Adobe_RGB:
        case hdmi::HDMI_RX_CLRSPC_YC444_709:
        case hdmi::HDMI_RX_CLRSPC_YC422_709:
        case hdmi::HDMI_RX_CLRSPC_YC420_709:
        case hdmi::HDMI_RX_CLRSPC_XVYC444_709:
        case hdmi::HDMI_RX_CLRSPC_XVYC422_709:
        case hdmi::HDMI_RX_CLRSPC_XVYC420_709:
            if (MDP_DIRECTION_INPUT == direction) {
                if (MDP_PROFILE_AUTO == mdp_profile) {
                    if (range == hdmi::HDMI_RX_RGB_FULL || range == hdmi::HDMI_RX_YCC_FULL)
                        return DP_PROFILE_FULL_BT709;
                    else
                        return DP_PROFILE_BT709;
                } else if (MDP_PROFILE_FULL == mdp_profile) {
                    return DP_PROFILE_FULL_BT709;
                } else if (MDP_PROFILE_LIMIT == mdp_profile) {
                    return DP_PROFILE_BT709;
                } else {
                    return DP_PROFILE_BT709;
                }
            } else if (MDP_DIRECTION_OUTPUT == direction) {
                return DP_PROFILE_FULL_BT709;
            } else {
                return DP_PROFILE_BT709;
            }
        case hdmi::HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_RGB_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous:
        case hdmi::HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous:
           if (MDP_DIRECTION_INPUT == direction) {
                if (MDP_PROFILE_AUTO == mdp_profile) {
                    if (range == hdmi::HDMI_RX_RGB_FULL || range == hdmi::HDMI_RX_YCC_FULL)
                        return DP_PROFILE_FULL_BT2020;
                    else
                        return DP_PROFILE_BT2020;
                } else if (MDP_PROFILE_FULL == mdp_profile) {
                    return DP_PROFILE_FULL_BT2020;
                } else if (MDP_PROFILE_LIMIT == mdp_profile) {
                    return DP_PROFILE_BT2020;
                } else {
                    return DP_PROFILE_BT2020;
                }
            } else if (MDP_DIRECTION_OUTPUT == direction) {
                return DP_PROFILE_FULL_BT2020;
            } else {
                return DP_PROFILE_BT2020;
            }
        default:
            RXLOGE("%s, profile = %d, range = %d", __FUNCTION__, profile, range);
            if (MDP_DIRECTION_OUTPUT == direction)
                return DP_PROFILE_FULL_BT709;
            else if (MDP_DIRECTION_INPUT == direction) {
                if (mdp_profile == MDP_PROFILE_FULL) {
                    return DP_PROFILE_FULL_BT709;
                } else if (mdp_profile == MDP_PROFILE_LIMIT) {
                    return DP_PROFILE_BT709;
                } else {
                    return DP_PROFILE_BT709;
                }
            } else {
                return DP_PROFILE_BT709;
            }
    }
}

int mdp_trigger(buffer_handle_t buffer, int32_t *fence, int32_t mdp_profile, HDMIRX_VID_PARA *info) {
    uint32_t job;
    int32_t ion_fd;
    int out_w, out_h, in_w, in_h, output_format;
    DpColorFormat in_fmt;
    DpRect src_rect;
    uint32_t size[3] = {0};
    uint32_t in_size[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    int err = 0;
    DP_PROFILE_ENUM in_profile = DP_PROFILE_FULL_BT709;
    DP_PROFILE_ENUM out_profile = DP_PROFILE_FULL_BT709;
    uint8_t in_Bpp;

    if (info)
    {
        in_profile = hal2dpprofile(info->HdmiClrSpc, info->HdmiRange, mdp_profile, MDP_DIRECTION_INPUT);
        out_profile = hal2dpprofile(info->HdmiClrSpc, info->HdmiRange, mdp_profile, MDP_DIRECTION_OUTPUT);
    }

    // output buffer info
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ION_FD, &ion_fd);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_WIDTH, &out_w);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_HEIGHT, &out_h);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size[0]);
    err |= gralloc_extra_query(buffer, GRALLOC_EXTRA_GET_FORMAT, &output_format);
    if (err != GRALLOC_EXTRA_OK) {
        RXLOGE("gralloc_extra_query fail");
        return -EINVAL;
    }

    // Job + Fence
    err |= mdp_stream->createJob(job, *fence);
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_stream->setConfigBegin(job);
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }


#ifdef MTK_HDMI_RXVC_HDR_SUPPORT
    pthread_mutex_lock(&gMutex);
    if (info)
    {
        if (info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_YC444_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_YC422_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_YC420_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_XVYC444_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_XVYC422_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_XVYC420_709 ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_RGB ||
            info->HdmiClrSpc == hdmirx::hdmi::HDMI_RX_CLRSPC_Adobe_RGB) {
            dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace |= HDMI_RX_BT709;

        }
        if (info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous ||
            info->HdmiClrSpc ==
                hdmirx::hdmi::HDMI_RX_CLRSPC_BT_2020_RGB_const_luminous) {
            dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace |= HDMI_RX_BT2020;
        }
    }
    RXLOGD("dataspace: %d",
        dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace);
#ifdef MTK_HDMI_RXVC_DS_SUPPORT
    dppqparam.enable = dppqparam.enable | PQ_SHP_EN;
    dppqparam.scenario = MEDIA_VIDEO;
    dppqparam.u.video.id = 1;
    dppqparam.u.video.userScenario = INFO_RX;
    dppqparam.u.video.grallocExtraHandle = buffer;
#endif
    if (0 != mdp_stream->setPQParameter(0, dppqparam))
        RXLOGE("%s, %d setPQParameter failed.", __FUNCTION__, __LINE__);
    pthread_mutex_unlock(&gMutex);
#else
#ifdef MTK_HDMI_RXVC_DS_SUPPORT
    memset(&dppqparam, 0, sizeof(DpPqParam));
    dppqparam.enable = dppqparam.enable | PQ_SHP_EN;
    dppqparam.scenario = MEDIA_VIDEO;
    dppqparam.u.video.id = 1;
    dppqparam.u.video.userScenario = INFO_RX;
    dppqparam.u.video.grallocExtraHandle = buffer;

    if (0 != mdp_stream->setPQParameter(0, dppqparam))
        RXLOGE("%s, %d setPQParameter failed.", __FUNCTION__, __LINE__);
#endif
#endif

    // input format : rx data is always plane 1
    if (info) {
        in_w = info->hactive;
        in_h = info->vactive;
        if (info->cs == HDMIRX_CS::HDMI_CS_YUV420)
            in_w *= 2;

        if (info->cs == HDMIRX_CS::HDMI_CS_RGB) {
            if (info->dp == hdmirx::hdmi::HDMIRX_BIT_DEPTH_8_BIT) {
                // check if HDR enable
                if (dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace & HDMI_RX_ST2084||
                    dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace & HDMI_RX_HLG)
                    in_fmt = DP_COLOR_YUV444_FROMRGB;
                else
                    in_fmt = DP_COLOR_RGB888;
                in_Bpp = 3;
            }
            else {
                // check if HDR enable
                if (dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace & HDMI_RX_ST2084 ||
                    dppqparam.u.video.HDRMetadata.AOSPHDRDataSpace.dataSpace & HDMI_RX_HLG)
                    in_fmt = DP_COLOR_YUV444_10L_FROMRGB;
                else
                    in_fmt = DP_COLOR_RGBA1010102;
                in_Bpp = 4;
            }
        }
        //else if (info->cs == HDMIRX_CS::HDMI_CS_YUV422)
            //if (info->dp == hdmirx::hdmi::HDMIRX_BIT_DEPTH_8_BIT)
                //in_fmt = eYUY2;
            //else
                //in_fmt = eRGBA1010102;
        else {
            if (info->dp == hdmirx::hdmi::HDMIRX_BIT_DEPTH_8_BIT) {
                in_fmt = DP_COLOR_YUV444_FROMRGB;
                in_Bpp = 3;
            }
            else {
                in_fmt = DP_COLOR_YUV444_10L_FROMRGB;
                in_Bpp = 4;
            }
        }
        RXLOGD("1213in_w:%d, in_h:%d, in_fmt:%d, in_profile=%d", in_w, in_h, in_fmt, in_profile);
    } else {
        in_w = out_w;
        in_h = out_h;
        in_fmt = eYUYV;
        RXLOGD("1214in_w:%d, in_h:%d", in_w, in_h);
    }
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = in_w;
    src_rect.h = in_h;

    err |= mdp_stream->setSrcBuffer(ion_fd, in_size, DP_COLOR_GET_PLANE_COUNT(in_fmt));
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }

    err |= mdp_stream->setSrcConfig(in_w, in_h, in_w*in_Bpp, in_w, in_fmt, in_profile, eInterlace_None, DP_SECURE_NONE, 0, 0,
        // PORT_HDMI_RX);
        6);

    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_stream->setSrcCrop(0, src_rect);
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }
    if (out_h > out_w) mdp_stream->setRotate(0, 270);
    //size[0] = out_h * out_w;
    //size[1] = (out_h * out_w) >> 2;
    //size[2] = (out_h * out_w) >> 2;
    int output_plane_num = 0;
    int32_t output_YPitch = 0;
    int32_t output_UVPitch = 0;

    // for output
    if (output_format == HAL_PIXEL_FORMAT_YCBCR_420_888 ||
        output_format == HAL_PIXEL_FORMAT_YCRCB_420_SP) { //NV12, NV21
        size[0] = out_h * out_w;
        size[1] = (out_h * out_w) >> 1;
        size[2] = 0;
        output_plane_num = 2;
        output_YPitch = out_w;
        output_UVPitch = out_w;
    } else if (output_format == HAL_PIXEL_FORMAT_YV12 ||
        output_format == HAL_PIXEL_FORMAT_I420) {
        size[0] = out_h * out_w;
        size[1] = (out_h * out_w) >> 2;
        size[2] = (out_h * out_w) >> 2;
        output_plane_num = 3;
        output_YPitch = out_w;
        output_UVPitch = out_w / 2;
    } else if (output_format == HAL_PIXEL_FORMAT_RGB_888) {
        size[0] = out_h * out_w * 3;
        size[1] = 0;
        size[2] = 0;
        output_plane_num = 1;
        output_YPitch = out_w*3;
        output_UVPitch = 0;
    } else if (output_format == HAL_PIXEL_FORMAT_RGBA_1010102) {
        size[0] = out_h * out_w * 4;
        size[1] = 0;
        size[2] = 0;
        output_plane_num = 1;
        output_YPitch = out_w*4;
        output_UVPitch = 0;
    } else if (output_format == HAL_PIXEL_FORMAT_YCBCR_422_I) {
        size[0] = out_h * out_w *2 ;
        size[1] = 0;
        size[2] = 0;
        output_plane_num = 1;
        output_YPitch = out_w*2;
        output_UVPitch = 0;
    } else {
        RXLOGE("%s, %d, output format(%d) is not support.", __FUNCTION__, __LINE__, output_format);
        return -1;
    }
    err |= mdp_stream->setDstBuffer(0, ion_fd, size, output_plane_num);
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }

    err |= mdp_stream->setDstConfig(0, out_w, out_h, output_YPitch, output_UVPitch, hal2dpformat(output_format), out_profile);
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }

    RXLOGD("1215in_w:%d, in_h:%d, in_fmt:%d, in_profile:%d, out_w:%d, out_h:%d, out_fmt:%d out_profile:%d ",
        in_w, in_h, in_fmt, in_profile, out_w, out_h, hal2dpformat(output_format), out_profile);


    err |= mdp_stream->setConfigEnd();
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_stream->invalidate();
    if (0 != err) {
        RXLOGE("%s, %d", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

static int get_va_mmap(int32_t ion_fd, uint32_t img_size, int* share_fd, void**va)
{
    int cur_share_fd = -1;
    int res = 0;
    void* ptr = nullptr;

#ifdef KERNEL_419
    res = IONDevice::getInstance().ionImport(ion_fd, &cur_share_fd);
    if (res != 0 || cur_share_fd < 0) {
        RXLOGE("ionImport is failed: %s(%d), ion_fd(%d)", strerror(errno), res,
        ion_fd);
        return -1;
    }

    ptr = IONDevice::getInstance().ionMMap(ion_fd, img_size, PROT_READ,
        MAP_SHARED, cur_share_fd);
    if (ptr == nullptr) {
        RXLOGE("ion mmap fail.");
        return -1;
    }
#else
    res = DMABUFDevice::getInstance().dmabufAlloc(ion_fd, img_size, &cur_share_fd);
    if (res != 0 || cur_share_fd < 0) {
        RXLOGE("dmabufAlloc is failed: %s(%d), ion_fd(%d)", strerror(errno), res,
        ion_fd);
        return -1;
    }

    ptr = DMABUFDevice::getInstance().dmabufMMap(ion_fd, img_size, PROT_READ,
        MAP_SHARED, cur_share_fd);
    if (ptr == nullptr) {
        RXLOGE("ion mmap fail.");
        return -1;
    }
#endif



    *share_fd = cur_share_fd;
    *va = ptr;
    ptr = nullptr;
    return 0;
}

static int release_va_unmmap(int32_t ion_fd, uint32_t img_size, int* share_fd, void** va)
{
#ifdef KERNEL_419
    if (IONDevice::getInstance().ionMUnmap(ion_fd, *va, img_size) != 0) {
        RXLOGE("ion unmmap failed.");
        return -1;
    }

    *va = nullptr;

    if (*share_fd != -1 && IONDevice::getInstance().ionClose(*share_fd)) {
        RXLOGE("ion close is failed: %s, share_fd(%d)", strerror(errno), *share_fd);
        return -1;
    }
#else
    if (DMABUFDevice::getInstance().dmabufMUnmap(ion_fd, *va, img_size) != 0) {
        RXLOGE("dma_buf unmmap failed.");
        return -1;
    }

    *va = nullptr;

    if (*share_fd != -1 && DMABUFDevice::getInstance().dmabufClose(*share_fd)) {
        RXLOGE("dma_buf close is failed: %s, share_fd(%d)", strerror(errno), *share_fd);
        return -1;
    }
#endif
    return 0;
}

int mdp_1in1out(buffer_handle_t input_handle, int input_width, int input_height,
                buffer_handle_t output_handle, int output_width, int output_height)
{
    int32_t input_ion_fd, output_ion_fd;
    int err = 0;
    uint32_t output_size_list[3] = {0};
    uint32_t input_size_list[3] = {0};
    int32_t output_plane_num = 0;
    int32_t input_plane_num = 0;
    void* output_va = nullptr;
    int output_share_fd = -1;
    uint32_t output_mmap_size = 0;
    int32_t input_YPitch = 0;
    int32_t input_UVPitch = 0;
    int32_t output_YPitch = 0;
    int32_t output_UVPitch = 0;
    bool convert_flag = true;
    uint8_t *outputva[3] = {nullptr};
    int input_format, output_format;

    err |= gralloc_extra_query(input_handle, GRALLOC_EXTRA_GET_ION_FD, &input_ion_fd);
    err |= gralloc_extra_query(input_handle, GRALLOC_EXTRA_GET_FORMAT, &input_format);
    err |= gralloc_extra_query(output_handle, GRALLOC_EXTRA_GET_ION_FD, &output_ion_fd);
    err |= gralloc_extra_query(output_handle, GRALLOC_EXTRA_GET_FORMAT, &output_format);
    if (err != GRALLOC_EXTRA_OK) {
        RXLOGE("%s, %d, gralloc_extra_query fail", __FUNCTION__, __LINE__);
        return -1;
    }

#if 0
    // for input YUYV
    input_size_list[0] = input_width * input_height * 2;
    input_size_list[1] = 0;
    input_size_list[2] = 0;
    input_plane_num = 1;
#endif
    if (input_format != HAL_PIXEL_FORMAT_YV12) {
        RXLOGE("input format is not YV12");
        return -1;
    }
    // for input YV12
    input_size_list[0] = input_width * input_height;
    input_size_list[1] = (input_width * input_height) >> 2;
    input_size_list[2] = (input_width * input_height) >> 2;
    input_plane_num = 3;
    input_YPitch = input_width;
    input_UVPitch = input_width / 2;

    err |= mdp_ispStream->queueSrcBuffer(input_ion_fd, input_size_list, input_plane_num);
    if (err != 0) {
        RXLOGE("%s, %d, queue source buffer failed.", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_ispStream->setSrcConfig(input_width, input_height, input_YPitch, input_UVPitch,
                                        DP_COLOR_YV12, DP_PROFILE_BT601, eInterlace_None, 0,
                                        false, DP_SECURE_NONE);
    if (err != 0) {
        RXLOGE("%s, %d, set source config failed.", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_ispStream->setSrcCrop(0, 0, 0, 0, 0, input_width, input_height);
    if (err != 0) {
        RXLOGE("%s, %d, set source crop failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    // for output
    if (output_format == HAL_PIXEL_FORMAT_YCBCR_420_888 ||
        output_format == HAL_PIXEL_FORMAT_YCRCB_420_SP) { //NV12 * NV21
        output_size_list[0] = output_height * output_width;
        output_size_list[1] = (output_height * output_width) >> 1;
        output_size_list[2] = 0;
        output_plane_num = 2;
        output_YPitch = output_width;
        output_UVPitch = output_width;
        output_mmap_size = output_height * output_width * 3 / 2;
    } else if (output_format == HAL_PIXEL_FORMAT_YV12 ||
               output_format == HAL_PIXEL_FORMAT_I420 ) {
        output_size_list[0] = output_height * output_width;
        output_size_list[1] = (output_height * output_width) >> 2;
        output_size_list[2] = (output_height * output_width) >> 2;
        output_plane_num = 3;
        output_YPitch = output_width;
        output_UVPitch = output_width / 2;
        output_mmap_size = output_height * output_width * 3 / 2;
    } else {
        RXLOGE("%s, %d, output format(%d) is not included{NV12, YV12}.", __FUNCTION__, __LINE__, output_format);
        return -1;
    }

    if (0 != get_va_mmap(output_ion_fd, output_mmap_size, &output_share_fd, (void **)&output_va))
    {
        RXLOGE("%s, %d, get output va failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    outputva[0] = (uint8_t*)output_va;
    outputva[1] = outputva[0] + output_size_list[0];
    outputva[2] = outputva[1] + output_size_list[1];

    err |= mdp_ispStream->queueDstBuffer(0, output_ion_fd,
                                        output_size_list, output_plane_num);
    if (err != 0) {
        RXLOGE("%s, %d, output queue dst buffer failed.", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_ispStream->setDstConfig(0, output_width, output_height,
                                        output_YPitch, output_UVPitch,
                                        hal2dpformat(output_format));
    if (err != 0) {
        RXLOGE("%s, %d, output set dst config failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    err |= mdp_ispStream->startStream();
    if (err != 0) {
        RXLOGE("%s, %d, start stream failed.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueSrcBuffer();
    if (err != 0) {
        RXLOGE("%s, %d, dequeue src buffer.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueDstBuffer(0, (void **)&outputva, true);
    if (err != 0) {
        RXLOGE("%s, %d, dequeue dst buffer.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->stopStream();
    if (err != 0) {
        RXLOGE("%s, %d, stop stream.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueFrameEnd();
    if (err != 0) {
        RXLOGE("%s, %d, dequeue frame end.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }

Err:
    if (0 != release_va_unmmap(output_ion_fd, output_mmap_size, &output_share_fd, (void **)&output_va))
    {
        RXLOGE("%s, %d, unmmap output0 va failed.", __FUNCTION__, __LINE__);
        convert_flag = false;;
    }

    if (!convert_flag)
        return -1;

    return 0;
}

int mdp_1in2out(buffer_handle_t input_handle, int input_width, int input_height,
                buffer_handle_t output0_handle, int output0_width, int output0_height,
                buffer_handle_t output1_handle, int output1_width, int output1_height)
{
    int32_t input_ion_fd, output0_ion_fd, output1_ion_fd;
    int err = 0;
    uint32_t output0_size_list[3] = {0};
    uint32_t output1_size_list[3] = {0};
    uint32_t input_size_list[3] = {0};
    int32_t output0_plane_num = 0;
    int32_t output1_plane_num = 0;
    int32_t input_plane_num = 0;
    int32_t output0_YPitch = 0;
    int32_t output0_UVPitch = 0;
    int32_t output1_YPitch = 0;
    int32_t output1_UVPitch = 0;
    int32_t input_YPitch = 0;
    int32_t input_UVPitch = 0;
    void* output0_va = nullptr;
    void* output1_va = nullptr;
    int output0_share_fd = -1;
    int output1_share_fd = -1;
    uint32_t output0_mmap_size = 0;
    uint32_t output1_mmap_size = 0;
    bool convert_flag = true;
    uint8_t *output0va[3] = {nullptr};
    uint8_t *output1va[3] = {nullptr};
    int input_format, output0_format, output1_format;

    err |= gralloc_extra_query(input_handle, GRALLOC_EXTRA_GET_ION_FD, &input_ion_fd);
    err |= gralloc_extra_query(input_handle, GRALLOC_EXTRA_GET_FORMAT, &input_format);
    err |= gralloc_extra_query(output0_handle, GRALLOC_EXTRA_GET_ION_FD, &output0_ion_fd);
    err |= gralloc_extra_query(output0_handle, GRALLOC_EXTRA_GET_FORMAT, &output0_format);
    err |= gralloc_extra_query(output1_handle, GRALLOC_EXTRA_GET_ION_FD, &output1_ion_fd);
    err |= gralloc_extra_query(output1_handle, GRALLOC_EXTRA_GET_FORMAT, &output1_format);
    if (err != GRALLOC_EXTRA_OK) {
        RXLOGE("%s, %d, gralloc_extra_query fail", __FUNCTION__, __LINE__);
        return -1;
    }

    if (input_format == HAL_PIXEL_FORMAT_YV12)
    {
        // for input YV12
        input_size_list[0] = input_width * input_height;
        input_size_list[1] = (input_width * input_height) >> 2;
        input_size_list[2] = (input_width * input_height) >> 2;
        input_plane_num = 3;
        input_YPitch = input_width;
        input_UVPitch = input_width / 2;
    }
    else
    {
        // for input RGB888
        input_size_list[0] = input_width * input_height * 3;
        input_size_list[1] = 0;
        input_size_list[2] = 0;
        input_plane_num = 1;
        input_YPitch = input_width*3;
        input_UVPitch = 0;
    }

    err |= mdp_ispStream->queueSrcBuffer(input_ion_fd, input_size_list, input_plane_num);
    if (err != 0) {
        RXLOGE("%s, %d, queue source buffer failed.", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_ispStream->setSrcConfig(input_width, input_height, input_YPitch, input_UVPitch,
                                        hal2dpformat(input_format), DP_PROFILE_BT601, eInterlace_None, 0,
                                        false, DP_SECURE_NONE);
    if (err != 0) {
        RXLOGE("%s, %d, set source config failed.", __FUNCTION__, __LINE__);
        return -1;
    }
    err |= mdp_ispStream->setSrcCrop(0, 0, 0, 0, 0, input_width, input_height);
    if (err != 0) {
        RXLOGE("%s, %d, set source crop failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    // for output 0
    if (output0_format == HAL_PIXEL_FORMAT_YCBCR_420_888 ||
        output0_format == HAL_PIXEL_FORMAT_YCRCB_420_SP) { //NV12
        output0_size_list[0] = output0_height * output0_width;
        output0_size_list[1] = (output0_height * output0_width) >> 1;
        output0_size_list[2] = 0;
        output0_plane_num = 2;
        output0_YPitch = output0_width;
        output0_UVPitch = output0_width;
        output0_mmap_size = output0_height * output0_width * 3 / 2;
    } else if (output0_format == HAL_PIXEL_FORMAT_YV12 ||
                output0_format == HAL_PIXEL_FORMAT_I420) {
        output0_size_list[0] = output0_height * output0_width;
        output0_size_list[1] = (output0_height * output0_width) >> 2;
        output0_size_list[2] = (output0_height * output0_width) >> 2;
        output0_plane_num = 3;
        output0_YPitch = output0_width;
        output0_UVPitch = output0_width / 2;
        output0_mmap_size = output0_height * output0_width * 3 / 2;
    } else if (output0_format == HAL_PIXEL_FORMAT_RGB_888) {
        output0_size_list[0] = output0_height * output0_width * 3;
        output0_size_list[1] = 0;
        output0_size_list[2] = 0;
        output0_plane_num = 1;
        output0_YPitch = output0_width * 3;
        output0_UVPitch = 0;
        output0_mmap_size = output0_height * output0_width * 3;
    } else if (output0_format == HAL_PIXEL_FORMAT_RGBA_1010102) {
        output0_size_list[0] = output0_height * output0_width * 4;
        output0_size_list[1] = 0;
        output0_size_list[2] = 0;
        output0_plane_num = 1;
        output0_YPitch = output0_width * 4;
        output0_UVPitch = 0;
        output0_mmap_size = output0_height * output0_width * 4;
    } else if (output0_format == HAL_PIXEL_FORMAT_YCBCR_422_I) {
        output0_size_list[0] = output0_height * output0_width * 2;
        output0_size_list[1] = 0;
        output0_size_list[2] = 0;
        output0_plane_num = 1;
        output0_YPitch = output0_width * 2;
        output0_UVPitch = 0;
        output0_mmap_size = output0_height * output0_width * 2;
    } else {
        RXLOGE("%s, %d, output0 format(%d) is not support.", __FUNCTION__, __LINE__, output0_format);
        goto Err;
    }

    if (0 != get_va_mmap(output0_ion_fd, output0_mmap_size,&output0_share_fd, (void **)&output0_va))
    {
        RXLOGE("%s, %d, get output0 va failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    output0va[0] = (uint8_t*)output0_va;
    output0va[1] = output0va[0] + output0_size_list[0];
    output0va[2] = output0va[1] + output0_size_list[1];

    err |= mdp_ispStream->queueDstBuffer(0, output0_ion_fd,
                                        output0_size_list, output0_plane_num);
    if (err != 0) {
        RXLOGE("%s, %d, output0 queue dst buffer failed.", __FUNCTION__, __LINE__);
        goto Err;
    }
    err |= mdp_ispStream->setDstConfig(0, output0_width, output0_height,
                                        output0_YPitch, output0_UVPitch,
                                      hal2dpformat(output0_format));
    if (err != 0) {
        RXLOGE("%s, %d, output0 set dst config failed.", __FUNCTION__, __LINE__);
        goto Err;
    }

     // for output 1
    if (output1_format == HAL_PIXEL_FORMAT_YCBCR_420_888 ||
        output1_format == HAL_PIXEL_FORMAT_YCRCB_420_SP) { //NV12
        output1_size_list[0] = output1_height * output1_width;
        output1_size_list[1] = (output1_height * output1_width) >> 1;
        output1_size_list[2] = 0;
        output1_plane_num = 2;
        output1_YPitch = output1_width;
        output1_UVPitch = output1_width;
        output1_mmap_size = output1_height * output1_width * 3 / 2;
    } else if (output1_format == HAL_PIXEL_FORMAT_YV12 ||
                output1_format == HAL_PIXEL_FORMAT_I420) {
        output1_size_list[0] = output1_height * output1_width;
        output1_size_list[1] = (output1_height * output1_width) >> 2;
        output1_size_list[2] = (output1_height * output1_width) >> 2;
        output1_plane_num = 3;
        output1_YPitch = output1_width;
        output1_UVPitch = output1_width / 2;
        output1_mmap_size = output1_height * output1_width * 3 / 2;
    } else if (output1_format == HAL_PIXEL_FORMAT_RGB_888) {
        output1_size_list[0] = output1_height * output1_width * 3;
        output1_size_list[1] = 0;
        output1_size_list[2] = 0;
        output1_plane_num = 1;
        output1_YPitch = output1_width * 3;
        output1_UVPitch = 0;
        output1_mmap_size = output1_height * output1_width * 3;
    } else if (output1_format == HAL_PIXEL_FORMAT_RGBA_1010102) {
        output1_size_list[0] = output1_height * output1_width * 4;
        output1_size_list[1] = 0;
        output1_size_list[2] = 0;
        output1_plane_num = 1;
        output1_YPitch = output1_width * 4;
        output1_UVPitch = 0;
        output1_mmap_size = output1_height * output1_width * 4;
    } else if (output1_format == HAL_PIXEL_FORMAT_YCBCR_422_I) {
        output1_size_list[0] = output1_height * output1_width * 2;
        output1_size_list[1] = 0;
        output1_size_list[2] = 0;
        output1_plane_num = 1;
        output1_YPitch = output1_width * 2;
        output1_UVPitch = 0;
        output1_mmap_size = output1_height * output1_width * 2;
    } else {
        RXLOGE("%s, %d, output1 format(%d) is not s not support.", __FUNCTION__, __LINE__, output1_format);
        goto Err;
    }

    if (0 != get_va_mmap(output1_ion_fd, output1_mmap_size, &output1_share_fd, (void **)&output1_va))
    {
        RXLOGE("%s, %d, get output1 va failed.", __FUNCTION__, __LINE__);
        goto Err0;
    }

    output1va[0] = (uint8_t*)output1_va;
    output1va[1] = output1va[0] + output1_size_list[0];
    output1va[2] = output1va[1] + output1_size_list[1];

    err |= mdp_ispStream->queueDstBuffer(1, output1_ion_fd,
                                        output1_size_list, output1_plane_num);
    if (err != 0) {
        RXLOGE("%s, %d, output1 queue dst buffer failed.", __FUNCTION__, __LINE__);
        goto Err;
    }
    err |= mdp_ispStream->setDstConfig(1, output1_width, output1_height,
                                        output1_YPitch, output1_UVPitch,
                                        hal2dpformat(output1_format));
    if (err != 0) {
        RXLOGE("%s, %d, output1 set dst config failed.", __FUNCTION__, __LINE__);
        goto Err;
    }

    RXLOGD("mdp_1in2out 1215in_w:%d, in_h:%d, in_fmt:%d, out_w:%d, out_h:%d, out_fmt:%d, out1_w:%d, out1_h:%d, out1_fmt:%d",
        input_width, input_height, hal2dpformat(input_format), output0_width, output0_height, hal2dpformat(output0_format),
        output1_width, output1_height, hal2dpformat(output1_format));

    err |= mdp_ispStream->startStream();
    if (err != 0) {
        RXLOGE("%s, %d, start stream failed.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueSrcBuffer();
    if (err != 0) {
        RXLOGE("%s, %d, dequeue src buffer fail.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueDstBuffer(0, (void**)output0va, true);
    if (err != 0) {
        RXLOGE("%s, %d, dequeue port 0 dst buffer.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueDstBuffer(1, (void**)output1va, true);
    if (err != 0) {
        RXLOGE("%s, %d, dequeue port 1 dst buffer.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->stopStream();
    if (err != 0) {
        RXLOGE("%s, %d, stop stream.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
    err |= mdp_ispStream->dequeueFrameEnd();
    if (err != 0) {
        RXLOGE("%s, %d, dequeue frame end.", __FUNCTION__, __LINE__);
        convert_flag = false;
        goto Err;
    }
Err:
    if (0 != release_va_unmmap(output1_ion_fd, output1_mmap_size, &output1_share_fd, (void **)&output1_va))
    {
        RXLOGE("%s, %d, unmmap output1 va failed.", __FUNCTION__, __LINE__);
        convert_flag = false;
    }
Err0:
    if (0 != release_va_unmmap(output0_ion_fd, output0_mmap_size, &output0_share_fd, (void **)&output0_va))
    {
        RXLOGE("%s, %d, unmmap output0 va failed.", __FUNCTION__, __LINE__);
        convert_flag = false;
    }
    if (!convert_flag)
        return -1;

    return 0;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
