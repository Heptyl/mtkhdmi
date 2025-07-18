#include "MtkHdmiService.h"

#define LOG_TAG "MtkHdmiService"
#define LOG_CTL "debug.MtkHdmiService.enablelog"

#include <utils/String16.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <math.h>
#include "event/hdmi_event.h"
//xbh patch start
#include "board.h"
//xbh patch end

#if defined(MTK_DRM_HDMI_SUPPORT)
#include "hdmitx.h"
#include <android/hardware/boot/1.0/IBootControl.h>
using android::hardware::boot::V1_0::IBootControl;
#else
#include "linux/hdmitx.h"
#endif

#if defined (MTK_DRM_KEY_MNG_SUPPORT)
#include "keyblock.h"
//#include "Keymanage.h"
#include <vendor/mediatek/hardware/keymanage/1.0/IKeymanage.h>
using vendor::mediatek::hardware::keymanage::V1_0::IKeymanage;
#endif

#if defined(MTK_DRM_HDMI_HDCP_SUPPORT) && defined(MTK_OPTEE_SUPPORT)
#include "tee_client_api.h"
//#define TZCMD_HDCP_HDMI_GEN_KEY            0
#define TZCMD_HDCP_HDMI_GEN_1X_KEY            0
#define TZCMD_HDCP_HDMI_GEN_2X_KEY            1
#define TZ_TA_HDMI_HDCP_UUID   {0x99986025,0x2C8D,0x43CA,{0x95,0x98,0xB9,0x0E,0x32,0x4E,0x9D,0x3B}}

static TEEC_Context teec_cxt_hdmi;
static TEEC_Session teec_sess_hdmi;
static bool isConnected = false;
#endif

using vendor::mediatek::hardware::hdmi::V1_0::EDID_t;
using vendor::mediatek::hardware::hdmi::V1_0::Result;
using vendor::mediatek::hardware::hdmi::V1_2::IMtkHdmiCallback;
using vendor::mediatek::hardware::hdmi::V1_3::EDID_More_t;

#define HDMI_ENABLE "persist.vendor.sys.hdmi_hidl.enable"
//#define HDCP_ENABLE "persist.vendor.sys.hdcp.enable"
#define HDMI_VIDEO_AUTO "persist.vendor.sys.hdmi_hidl.auto"
#define HDMI_VIDEO_RESOLUTION "persist.vendor.sys.hdmi_hidl.resolution"
#define HDMI_COLOR_SPACE "persist.vendor.sys.hdmi_hidl.color_space"
#define HDMI_DEEP_COLOR "persist.vendor.sys.hdmi_hidl.deep_color"
#define HDMI_USE_FRAC_MODE "persist.vendor.sys.hdmi_hidl.use_frac"
#define HDMI_ENABLE_HDR "persist.vendor.sys.hdmi_hidl.hdr.enable"
#define HDMI_ENABLE_DV "persist.vendor.sys.hdmi_hidl.dolby.enable"
#define HDMI_DEFAULT_HDR_MODE "persist.vendor.sys.hdmi_hidl.hdr.defmode"

//if this property is 1, we should set hdr on,dv on default;
//if this property is 0, we should set hdr on,dv off default;
#define HDMI_DV_HDR_DRIVER_SUPPORT_PROPERTY "ro.vendor.mtk_dv_hdr_support"

#define EDIDNUM 4
#define AUTO_RESOLUTION 100
#define AUTO_COLORSPACE 100

//add for edp/dp
#define DRM_MODE_CONNECTOR_HDMIA  11
#define DRM_MODE_CONNECTOR_eDP    14
// these two flag will used with resolution value.
// such as , when we set hdmi resolution as RESOLUTION_720X480P_60HZ(2);
// the total resolution set should be
// (((mHdmiCrtcNum << 12) & 0XF000) | RESOLUTION_720X480P_60HZ)
#define DP_RESOLUTION_FLAG       0  //dp/edp means main path
#define HDMI_RESOLUTION_FLAG     1  //hdmi means sub path
// hdmi or dp type refer to /external/libdrm/xf86drmMode.h
#define DRM_MODE_CONNECTOR_DisplayPort  10
#define DRM_MODE_CONNECTOR_HDMIA        11
#define DRM_MODE_CONNECTOR_HDMIB        12
#define DRM_MODE_CONNECTOR_eDP          14

static int mDPInfo[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define DP_RESOLUTION "persist.vendor.sys.hdmi_hidl.dp.res"
#define DP_ENABLE     "persist.vendor.sys.hdmi_hidl.dp.enable"
#define DP_RES_AUTO  "persist.vendor.sys.hdmi_hidl.dp.auto"
#define DP_COLOR_SPACE "persist.vendor.sys.hdmi_hidl.dp.color_space"
#define DP_DEEP_COLOR "persist.vendor.sys.hdmi_hidl.dp.deep_color"
#define DP_ENABLE_HDR "persist.vendor.sys.hdmi_hidl.dp_hdr.enable"
#define DP_ENABLE_DV "persist.vendor.sys.hdmi_hidl.dp_dolby.enable"

//when the app getEdid,we need to known whether HDMI or DP get the EDID.
//so we need to set the prop as 1,after get the edid ,set the prop as 0;
//same when the app set videoresolution.
#define DEAL_HDMI_RESOLUTION_PROP "persist.vendor.sys.hdmi_hidl.hdmi.deal"
#define DEAL_DP_RESOLUTION_PROP "persist.vendor.sys.hdmi_hidl.dp.deal"


/**
 * HDMI resolution definition
 */
#define HDMI_VIDEO_720x480i_60Hz 0
#define HDMI_VIDEO_720x576i_50Hz 1
#define RESOLUTION_720X480P_60HZ 2
#define RESOLUTION_720X576P_50HZ 3
#define RESOLUTION_1280X720P_60HZ 4
#define RESOLUTION_1280X720P_50HZ 5
#define RESOLUTION_1920X1080I_60HZ 6
#define RESOLUTION_1920X1080I_50HZ 7
#define RESOLUTION_1920X1080P_30HZ 8
#define RESOLUTION_1920X1080P_25HZ 9
#define RESOLUTION_1920X1080P_24HZ 10
#define RESOLUTION_1920X1080P_23HZ 11
#define RESOLUTION_1920X1080P_29HZ 12
#define RESOLUTION_1920X1080P_60HZ 13
#define RESOLUTION_1920X1080P_50HZ 14
#define RESOLUTION_1280X720P3D_60HZ 15
#define RESOLUTION_1280X720P3D_50HZ 16
#define RESOLUTION_1920X1080I3D_60HZ 17
#define RESOLUTION_1920X1080I3D_50HZ 18
#define RESOLUTION_1920X1080P3D_24HZ 19
#define RESOLUTION_1920X1080P3D_23HZ 20

#define RESOLUTION_3840X2160P23_976HZ 21
#define RESOLUTION_3840X2160P_24HZ 22
#define RESOLUTION_3840X2160P_25HZ 23
#define RESOLUTION_3840X2160P29_97HZ 24
#define RESOLUTION_3840X2160P_30HZ 25
#define RESOLUTION_4096X2161P_24HZ 26

#define RESOLUTION_3840X2160P_60HZ 27
#define RESOLUTION_3840X2160P_50HZ 28
#define RESOLUTION_4096X2161P_60HZ 29
#define RESOLUTION_4096X2161P_50HZ 30

#define RESOLUTION_1280X720P_59_94HZ 31
#define RESOLUTION_1920X1080P_59_94HZ 32
#define RESOLUTION_3840X2160P_59_94HZ 33
#define RESOLUTION_4096x2160P_59_94HZ 34

#define SINK_NO_DEEP_COLOR     0
#define SINK_DEEP_COLOR_10_BIT 1
#define SINK_DEEP_COLOR_12_BIT 2
#define SINK_DEEP_COLOR_16_BIT 4

#define EDID_SUPPORT_DV_HDR 1 << 1
#define EDID_SUPPORT_DV_HDR_2160P60 1 << 3

#define RES4K60_TMDS_RATE 594
#define RES4K30_TMDS_RATE 297

#define COLOR_SPACE_RGB 0
#define COLOR_SPACE_YCBCR 1

#define HDMI_HDR_MODE             0
#define HDMI_DV_MODE           1
#define DP_RES_AUTO_MODE          2
#define DEAL_HDMI_RESOLUTION_MODE 3
#define DEAL_DP_RESOLUTION_MODE   4
#define DP_COLOR_FORMAT_MODE      5
#define DP_COLOR_DEPTH_MODE       6
#define DP_HDR_MODE               7
#define DP_DV_MODE             8

struct resSinkModeMap {
    int eSinkMode;
    int eResMode;
};

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmi {
namespace V1_4 {
namespace implementation {

sp<HdmiUEventThread> event_thread;

static int hdmi_ioctl(int code, unsigned long value);
int enableHDMI(int enable);
int enableHDMIInit(int enable);
int enableHDCP(int value);
int setHdcpKey(char* key);
bool setDrmKey();
#if defined(MTK_DRM_HDMI_HDCP_SUPPORT) && defined(MTK_OPTEE_SUPPORT)
int optee_hdmi_hdcp_close();
int optee_hdmi_hdcp_connect();
bool optee_hdmi_hdcp_call(uint32_t cmd, TEEC_Operation *op);
int get_hdmi();
#endif
int getCapabilities();
int getDisplayType();
EDID_t getResolutionMask();
int setVideoResolution(int res);


static int mEdid[4];
HDMI_EDID_T mEdidT[2] = {0};
static int DEFAULT_RESOLUTIONS[4] = {RESOLUTION_1920X1080P_30HZ,RESOLUTION_1920X1080P_60HZ,RESOLUTION_1280X720P_60HZ,RESOLUTION_720X480P_60HZ};
static int DEFAULT_ALL_RESOLUTIONS[24] = { RESOLUTION_4096X2161P_24HZ,
                RESOLUTION_3840X2160P_30HZ,RESOLUTION_3840X2160P29_97HZ,
                RESOLUTION_3840X2160P_25HZ,RESOLUTION_3840X2160P_24HZ,
                RESOLUTION_3840X2160P23_976HZ,RESOLUTION_1920X1080P_60HZ,
                RESOLUTION_1920X1080P_50HZ, RESOLUTION_1920X1080P_30HZ,
                RESOLUTION_1920X1080P_25HZ, RESOLUTION_1920X1080P_24HZ,
                RESOLUTION_1920X1080P_23HZ, RESOLUTION_1920X1080I_60HZ,
                RESOLUTION_1920X1080I_50HZ, RESOLUTION_1280X720P_60HZ,
                RESOLUTION_1280X720P_50HZ, RESOLUTION_720X480P_60HZ,
                RESOLUTION_720X576P_50HZ };

static int PREFERED_RESOLUTIONS[22]{
                    RESOLUTION_4096X2161P_50HZ,
                    RESOLUTION_4096X2161P_60HZ,
                    //xbh patch begin
                    //RESOLUTION_3840X2160P_50HZ,
                    RESOLUTION_3840X2160P_60HZ,
                    RESOLUTION_3840X2160P_50HZ,
                    //xbh patch end
                    RESOLUTION_4096X2161P_24HZ,
                    RESOLUTION_3840X2160P_30HZ,
                    RESOLUTION_3840X2160P29_97HZ,
                    RESOLUTION_3840X2160P_25HZ,
                    RESOLUTION_3840X2160P_24HZ,
                    RESOLUTION_3840X2160P23_976HZ,
                    RESOLUTION_1920X1080P_60HZ,
                    RESOLUTION_1920X1080P_50HZ,
                    RESOLUTION_1920X1080P_30HZ,
                    RESOLUTION_1920X1080P_25HZ,
                    RESOLUTION_1920X1080P_24HZ,
                    RESOLUTION_1920X1080P_23HZ,
                    RESOLUTION_1920X1080I_60HZ,
                    RESOLUTION_1920X1080I_50HZ,
                    RESOLUTION_1280X720P_60HZ,
                    RESOLUTION_1280X720P_50HZ,
                    RESOLUTION_720X480P_60HZ,
                    RESOLUTION_720X576P_50HZ };

static int sResolutionMask[15] = {0,0,SINK_480P, SINK_576P,
        SINK_720P60, SINK_720P50, SINK_1080I60, SINK_1080I50, SINK_1080P30,
        SINK_1080P25, SINK_1080P24, SINK_1080P23976, SINK_1080P2997,
        SINK_1080P60, SINK_1080P50 };

#if defined (MTK_INTERNAL_HDMI_SUPPORT)|| defined(MTK_DRM_HDMI_SUPPORT)
static int sResolutionMask_4k2k[10] = { SINK_2160P_23_976HZ, SINK_2160P_24HZ,
    SINK_2160P_25HZ, SINK_2160P_29_97HZ, SINK_2160P_30HZ, SINK_2161P_24HZ,
    SINK_2160P_60HZ, SINK_2160P_50HZ, SINK_2161P_60HZ, SINK_2161P_50HZ};
#endif

#if defined(MTK_DRM_HDMI_SUPPORT)
#define VIDEO_RESOLUTION_MODE 0
#define ENABLE_HDMI_MODE 1
#define ENABLE_HDR_DV_MODE 2
#define SET_COLOR_FORMAT_DEPTH_MODE 3
//#define SET_COLOR_DEPTH_MODE 4

#define HDMI_STATUS_ON -1
#define HDMI_STATUS_OFF -2

sp<IMtkHdmiCallback> mCallback;
#endif

bool mHdmiStateChanged= false;
bool mDpStateChanged= false;
int mHdmiCrtcNum = -1;
int mDpCrtcNum= -1;
int mColorSpaceMode = -1;
int mColorDepthMode = -1;
int mHdrMode        = -1;
int mHdmiEnable = 0;
int mHdmiResolution = 0;
int mHdmiAutoMode = 0;

int getValue(char* key, char* defValue) {
    char buf[PROPERTY_VALUE_MAX];
    property_get(key,buf,defValue);
    ALOGD("getValue: %s, %s" , key, buf);
    return (atoi(buf));
}
int setValue(char* key, int value) {
    char buf[PROPERTY_VALUE_MAX];
    if(sprintf(buf,"%d",value) < 0){
        ALOGD("setValue: sprintf failed");
        return -1;
    }
    int ret = property_set(key,buf);
    ALOGD("setValue: %s, %s" , key, buf);
    return ret;
}

#if defined(MTK_DRM_HDMI_SUPPORT)
bool isMax4k30DoviSink(HDMI_EDID_T edid) {
    bool isMax4k30Dovi = false;
    unsigned int dovi_vsvdb_version = 0xFF;
    unsigned int v1_low_latency_support = 0;
    unsigned int sink_supports_4k60 = 0;
    int maxTMDSRate = 0;

    if (edid.ui1_sink_support_dynamic_hdr  & EDID_SUPPORT_DV_HDR) {
        /* if DV hdr is supported, 4k30 will be supported om 4k sinks.
           ui4_sink_dovi_max_4k30 should be used along with other sink capabilities
           to determine whether sink can support DV only upto 4k@30 */
        isMax4k30Dovi = true;
        dovi_vsvdb_version = edid.ui4_sink_dv_vsvdb_version;
        v1_low_latency_support = edid.ui4_sink_dv_vsvdb_v1_low_latency;
        sink_supports_4k60 = (edid.ui4_sink_hdmi_4k2kvic & SINK_2160P_60HZ) ||
                             (edid.ui4_sink_hdmi_4k2kvic & SINK_2160P_50HZ);

        /* determine maximum bandwidth available */
        if (edid.ui2_sink_max_tmds_character_rate != 0) {
            maxTMDSRate = edid.ui2_sink_max_tmds_character_rate;
            ALOGI("isMax4k30DoviSink: maxTMDSRate1 = %d", maxTMDSRate);
        } else {
            maxTMDSRate = edid.ui1_sink_max_tmds_clock;
            ALOGI("isMax4k30DoviSink: maxTMDSRate2 = %d", maxTMDSRate);
        }
        /* set ui4_sink_dovi_max_4k30 to false only if 4k@60 dovi can be supported */
        if (maxTMDSRate >= RES4K60_TMDS_RATE) {
            if (((dovi_vsvdb_version == 0) || (dovi_vsvdb_version == 1)) &&
                (edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR_2160P60)) {
                isMax4k30Dovi = false;
            } else if ((dovi_vsvdb_version == 1) && (v1_low_latency_support) && (sink_supports_4k60)) {
                isMax4k30Dovi = false;
            } else if ((dovi_vsvdb_version == 2) && (sink_supports_4k60)) {
                isMax4k30Dovi = false;
            }
        }
    }
    ALOGI("isMax4k30DoviSink: isMax4k30Dovi=%d, dovi_vsvdb_version=%u, v1_low_latency_support=%u, sink_supports_4k60=%u",
                    isMax4k30Dovi, dovi_vsvdb_version, v1_low_latency_support, sink_supports_4k60);
    return isMax4k30Dovi;
}
#endif

void MtkHdmiService::refreshEdid() {
    EDID_t s_edid = getResolutionMask();
    //int preEdid = getValue(HDMI_edid,"0")
    ALOGI("refreshEdid s_edid.edid[0] %d" , s_edid.edid[0]);
    ALOGI("refreshEdid s_edid.edid[1] %d" , s_edid.edid[1]);

    //ALOGI("refresh preEdid %d" , preEdid);
    mEdid[0] = s_edid.edid[0];
    mEdid[1] = s_edid.edid[1];
    mEdid[2] = s_edid.edid[2];
    mEdid[3] = s_edid.edid[3];
    mHdmiStateChanged = true;
    setVideoResolution(getValue(HDMI_VIDEO_RESOLUTION,"0xff00"));
}

int getSuitableResolution(int resolution) {
    ALOGI("getSuitableResolution: %d ",resolution);
    int SuitableResolution = resolution;
    if (mEdid[0]!= 0 || mEdid[1]!= 0 || mEdid[2]!= 0 ) {
        int edidTemp = mEdid[0] | mEdid[1];
        int edidTemp_4k2k = mEdid[2];
        ALOGI("getSuitableResolution edidTemp: %d ",edidTemp);
        ALOGI("getSuitableResolution edidTemp_4k2k: %d ",edidTemp_4k2k);
        int* prefered = PREFERED_RESOLUTIONS;
        for (int i = 0; i < 22; i++) {
            int act = *(prefered + i);
            ALOGI("getSuitableResolution act: %d ",act);
            if(act < RESOLUTION_3840X2160P23_976HZ){
                if(act < sizeof(sResolutionMask)/sizeof(int)){
                    if (0 != (edidTemp & sResolutionMask[act])) {
                        SuitableResolution = act;
                        ALOGI("getSuitableResolution resolution: %d ",SuitableResolution);
                        break;
                  }
               }
            }
        #if defined (MTK_INTERNAL_HDMI_SUPPORT)|| defined(MTK_DRM_HDMI_SUPPORT)
            else{
                if (0 != (edidTemp_4k2k & sResolutionMask_4k2k[act - RESOLUTION_3840X2160P23_976HZ])) {
                    ALOGI("getSuitableResolution resolution 4k: %d ",SuitableResolution);
                    SuitableResolution = act;
                    break;
                }
            }
        #endif
        }
    } else {
        SuitableResolution = 2;
        ALOGI("getSuitableResolution edid==null,set solution to 480P60");
    }
    ALOGI("getSuitableResolution resolution final: %d ",SuitableResolution);
    return SuitableResolution;
}

////////////////////////////////////////////
#if defined (MTK_HDMI_SUPPORT)
#if defined (MTK_DRM_KEY_MNG_SUPPORT)
void convertVector2Array_U(std::vector<uint8_t> in, unsigned char *out)
{
    int size = in.size();
    for (int i = 0; i < size; i++) {
        out[i] = in.at(i);
    }
}
#endif
#endif

bool setDrmKey() {
    bool ret = false;
#if defined (MTK_HDMI_SUPPORT)
#if defined (MTK_DRM_KEY_MNG_SUPPORT)
    ALOGI("setDrmKey\n");
    hdmi_hdcp_drmkey hKey;
    int i;
    int ret_temp = 0;
    unsigned char* enckbdrm = NULL;
    unsigned int inlength = 0;

    android::sp<IKeymanage> hdmi_hdcp_client = IKeymanage::getService();

    auto hdmi_hdcp_callback = [&] (const android::hardware::hidl_vec<uint8_t>& oneDrmkeyBlock,
        uint32_t blockLeng, int32_t ret_val)
    {
        ret_temp = ret_val;

        ALOGI("[KM_HIDL] blockLeng = %u\n", blockLeng);
        enckbdrm = (unsigned char *)malloc(blockLeng);
        if (enckbdrm == NULL)
        {
            ALOGI("[KM_HIDL] malloc failed----\n");
        }

        convertVector2Array_U(oneDrmkeyBlock, enckbdrm);
        inlength = blockLeng;
    };

    hdmi_hdcp_client->get_encrypt_drmkey_hidl(HDCP_1X_TX_ID,  hdmi_hdcp_callback);

    if(ret_temp != 0 )
    {
        ALOGI("[KM_HIDL] setHDMIDRMKey get_encrypt_drmkey failed %d", ret_temp);
        if (enckbdrm != NULL)
        {
            free(enckbdrm);
            enckbdrm = NULL;
        }
        return ret;
    }

    memcpy(hKey.u1Hdcpkey, (unsigned char*)enckbdrm, sizeof(hKey.u1Hdcpkey));
    ret = hdmi_ioctl(MTK_HDMI_HDCP_KEY, (long)&hKey);
    ALOGI("setHDMIDRMKey ret = %d\n",ret);

    if (enckbdrm != NULL)
    {
        free(enckbdrm);
        enckbdrm = NULL;
    }

#endif
#elif defined(MTK_DRM_HDMI_HDCP_SUPPORT) && defined(MTK_OPTEE_SUPPORT)
    ALOGI("[MtkHdmiService]setDrmKey\n");
    long hKey = 0.0;
    int key_status = get_hdmi();
    ALOGI("[MtkHdmiService]setDrmKey get_hdmi  : ret = %d\n",key_status);
    if(key_status != 0){
        ALOGI("[MtkHdmiService]setDrmKey get_hdmi failed\n");
    }
    optee_hdmi_hdcp_close();
    //ret = hdmi_ioctl(MTK_HDMI_HDCP_KEY, (long)&hKey);
    //ALOGI("[MtkHdmiService]setHDMIDRMKey hdmi_ioctl : ret = %d\n",ret);
#endif
    return ret;
}

#if defined(MTK_DRM_HDMI_HDCP_SUPPORT) && defined(MTK_OPTEE_SUPPORT)
int optee_hdmi_hdcp_close()
{
    if(isConnected)
    {
        TEEC_CloseSession(&teec_sess_hdmi);
        TEEC_FinalizeContext(&teec_cxt_hdmi);
        isConnected = false;
    }

    return 0;
}

bool optee_hdmi_hdcp_call(uint32_t cmd, TEEC_Operation *op)
{
    if(!op)
        return false;

    uint32_t err_origin;
    TEEC_Result res = TEEC_InvokeCommand(&teec_sess_hdmi, cmd, op, &err_origin);
    if (res != TEEC_SUCCESS) {
        ALOGE("[HDMI_CA:]TEEC_InvokeCommand(cmd=%d) failed with code: 0x%08x origin 0x%08x",
        cmd,res, err_origin);
    if (res == TEEC_ERROR_TARGET_DEAD) {
        optee_hdmi_hdcp_close();
    }
        return false;
    }

    return true;
}

int optee_hdmi_hdcp_connect(){
    TEEC_Result res;
    TEEC_UUID uuid = TZ_TA_HDMI_HDCP_UUID;
    uint32_t err_origin;

    //initialize context
    res = TEEC_InitializeContext(NULL, &teec_cxt_hdmi);
    if (res != TEEC_SUCCESS) {
        ALOGE("[HDMI_CA:]TEEC_InitializeContext failed with code 0x%x", res);
        return (int)res;
    }

    //open session
    ALOGD("[HDMI_CA:]TEEC_Opensession begin");
    res = TEEC_OpenSession(&teec_cxt_hdmi, &teec_sess_hdmi, &uuid, TEEC_LOGIN_PUBLIC,
            NULL, NULL, &err_origin);
	ALOGD("[HDMI_CA:]TEEC_Opensession end");
    if (res != TEEC_SUCCESS) {
        ALOGE("[HDMI_CA:]TEEC_Opensession failed with code 0x%x origin 0x%x",
                res, err_origin);
        return (int)res;
    }

    isConnected = true;
    return 0;
}

int get_hdmi()
{
    TEEC_Operation op;

    if(!isConnected && optee_hdmi_hdcp_connect())
    {
        ALOGE("[HDMI_CA:]get_hdmi:failed to connect hdmi TA.\n");
        return -1;
    }

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE,
                                     TEEC_NONE,
                                     TEEC_NONE,
                                     TEEC_NONE);

    if(!optee_hdmi_hdcp_call(TZCMD_HDCP_HDMI_GEN_1X_KEY,&op))
    {
        ALOGE("[HDMI_CA:]get_hdmi:gen 1x key failed.");
        return -2;
    }

    if(!optee_hdmi_hdcp_call(TZCMD_HDCP_HDMI_GEN_2X_KEY,&op))
    {
        ALOGE("[HDMI_CA:]get_hdmi:gen 2x key failed.");
        return -2;
    }

    return 0;
}
#endif
void startObserving(){
    event_thread = new HdmiUEventThread();
    if (event_thread == NULL)
    {
        ALOGE("Failed to initialize UEvent thread!!");
        abort();
    }
    int ret = event_thread->run("HdmiUEventThread");
    ALOGI("HdmiUEventThread run: %d", ret);
}

void initialize(){
    //hdmi service can not early than hwc 2s
#if !defined (MTK_DRM_HDMI_SUPPORT)
    usleep(2000000);
#endif
    ALOGI("hdmi initialize()");
#if (defined(MTK_HDMI_HDCP_SUPPORT) && defined(MTK_DRM_KEY_MNG_SUPPORT))\
    || (defined(MTK_DRM_HDMI_HDCP_SUPPORT) && defined(MTK_OPTEE_SUPPORT))
    setDrmKey();
#endif
    startObserving();

    enableHDMIInit(getValue(HDMI_ENABLE,"1"));

#if defined(MTK_DRM_HDMI_SUPPORT)
    setValue(DEAL_HDMI_RESOLUTION_PROP, 0);
    setValue(DEAL_DP_RESOLUTION_PROP, 0);
#endif
}

int getCapabilities() {
    int result = 0;
#if defined (MTK_HDMI_SUPPORT)
    //if (hdmi_ioctl(MTK_HDMI_GET_CAPABILITY, (long)&result) == false) {
    //    result = 0;
    //}
#endif
    ALOGI("getCapabilities(%d)\n", result);
    return result;
}

int getDisplayType() {
    int result = 0;
#if defined (MTK_HDMI_SUPPORT)
   /* bool ret = false;
    mtk_dispif_info_t hdmi_info;
    memset((void *)&hdmi_info,0,sizeof(mtk_dispif_info_t));
    ret = hdmi_ioctl(MTK_HDMI_GET_DEV_INFO, (long)&hdmi_info);
    if (ret) {
        if (hdmi_info.displayType == HDMI_SMARTBOOK) {
            result = 1;
        } else if (hdmi_info.displayType == MHL) {
            result = 2;
        } else if (hdmi_info.displayType == SLIMPORT) {
            result = 3;
        }
    }*/
#endif
    ALOGI("getDisplayType(%d)\n", result);
    return result;
}


static int setDeepColor(int color, int deep) {
    int ret = -1;
#if defined (MTK_MT8193_HDMI_SUPPORT)||defined (MTK_INTERNAL_HDMI_SUPPORT)
    hdmi_para_setting h;
      h.u4Data1 = color;
      h.u4Data2 = deep;
      ret = hdmi_ioctl(MTK_HDMI_COLOR_DEEP, (long)&h);
      if (ret >= 0) {
          ALOGI("setDeepColor(%d,%d)\n", color, deep);
      }
#elif defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGD("setDeepColor(%d,%d)", color, deep);
        int color_deep_value = ((0x0F & color) << 4) | (0x0F & deep);
        ALOGD("setDeepColor color_deep_value:%d", color_deep_value);
        color_deep_value = (((mHdmiCrtcNum << 12) & 0XF000) | color_deep_value);
        auto result = mCallback->onHdmiSettingsChange(SET_COLOR_FORMAT_DEPTH_MODE, color_deep_value);
        if (!result.isOk()){
            ALOGD("setDeepColor HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setDeepColor fail!!! mCallback is null!!!");
    }
#endif
    return ret;
}

//---------------------------hdmi implementation start-----------------------

static int hdmi_ioctl(int code, unsigned long value){
    int fd = open("/dev/hdmitx", O_RDONLY, 0);//O_RDONLY;O_RDWR
    int ret = -1;
    if (fd >= 0) {
        ret = ioctl(fd, code, value);
        if (ret < 0) {
            ALOGE("[%s] failed. ioctlCode: %d, errno: %d",
                 __func__, code, errno);
        }
        close(fd);
    } else {
        ALOGE("[%s] open hdmitx failed. errno: %d", __func__, errno);
    }
    ALOGI("[%s] lv ret: %d", __func__,ret);
    return ret;
}

int enableHDMIInit(int value){
    ALOGI("enableHDMIInit = %d", value);
    int ret = -1;
#if defined (MTK_HDMI_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE, value);
    if(ret >= 0 && value == 1)
    {
        setValue(HDMI_ENABLE, value);
    }
#elif defined(MTK_DRM_HDMI_SUPPORT)
    setValue(HDMI_ENABLE, value);
#endif

    return ret;
}

int enableHDMI(int value) {
    ALOGI("enableHDMI = %d", value);
    //bool enable = (value) > 0 ? true : false;
    int ret = -1;
#if defined (MTK_HDMI_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE, value);
    if(ret >= 0)
    {
        setValue(HDMI_ENABLE, value);
    }
#elif defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGD("enableHDMI onHdmiSettingsChange");
        setValue(HDMI_ENABLE, value);
        value = (((mHdmiCrtcNum << 12) & 0XF000) | value);
        auto result = mCallback->onHdmiSettingsChange(ENABLE_HDMI_MODE, value);
        if (!result.isOk()){
            ALOGD("enableHDMI HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("enableHDMI fail!!! mCallback is null!!!");
    }
#endif

    return ret;
}

#if defined(MTK_DRM_HDMI_SUPPORT)
int enableHDMIHdr(int value) {
    ALOGI("[MtkHdmiService] enableHDMIHdr value = %d", value);
    int ret = -1;
#if defined (MTK_HDMI_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_HDR_ENABLE, value);
    if(ret >= 0)
    {
        setValue(HDMI_ENABLE_HDR, value);
    }
#endif
    return ret;
}
#endif

int enableHDCP(int value) {
    ALOGI("enableHDCP = %d", value);
    bool enable = (value) > 0 ? true : false;
    int ret = -1;
#if defined (MTK_MT8193_HDCP_SUPPORT)||defined (MTK_HDMI_HDCP_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_ENABLE_HDCP, (unsigned long)&enable);
#endif
    if(ret >= 0)
    {
        //setValue(HDCP_ENABLE, value);
    }

    return ret;
}

#define HDMI_RES_MASK 0x80
//#define LOGO_DEV_NAME "/dev/block/mmcblk0p9"
#define LOGO_DEV_NAME "/dev/block/by-name/logo"

#if defined(MTK_DRM_HDMI_SUPPORT)

int getHDMIHdrDV(){
    int hdr_value = -1;
    int dv_value = -1;
    //0:HDR OFF + DV OFF  1:HDR ON + DV OFF 2:HDR ON + DV ON
    int hdr_dv_value = -1;
    int hdmi_dv_hdr_driver_support = getValue(HDMI_DV_HDR_DRIVER_SUPPORT_PROPERTY, "-1");
    ALOGD("hdmi_dv_hdr_driver_support:%d", hdmi_dv_hdr_driver_support);
    if (1 == hdmi_dv_hdr_driver_support){
        hdr_value = getValue(HDMI_ENABLE_HDR, "1");
        dv_value = getValue(HDMI_ENABLE_DV, "1");
        if (0 == hdr_value){//if HDR OFF, the user can't set DV, it means DV is off.
            hdr_dv_value = 0;
        } else if ((1 == hdr_value) && (0 == dv_value)){//1:HDR ON + DV OFF
            hdr_dv_value = 1;
        } else if ((1 == hdr_value) && (1 == dv_value)){//2:HDR ON + DV ON
            hdr_dv_value = 2;
        } else {
            hdr_dv_value = 0;
        }
    } else if ((0 == hdmi_dv_hdr_driver_support)|| (-1 == hdmi_dv_hdr_driver_support)){
    //if HDMI_DV_HDR_DRIVER_SUPPORT_PROPERTY is 0, we should set hdr on,DV off default;
        hdr_value = getValue(HDMI_ENABLE_HDR, "1");
        //dv_value = getValue(HDMI_ENABLE_DV, "0");
        if (0 == hdr_value){//if HDR OFF, the user can't set DV, it means DV is off.
            hdr_dv_value = 0;
        } else if ((1 == hdr_value)/* && (0 == dv_value)*/){//1:HDR ON + DV OFF
            hdr_dv_value = 1;
        }/* else if ((1 == hdr_value) && (1 == dv_value)){//2:HDR ON + DV ON
            hdr_dv_value = 2;
        } */else {
            hdr_dv_value = 0;
        }
    }
    ALOGI("[MtkHdmiService] getHDMIHdrDV, %d, %d, %d", hdr_value, dv_value, hdr_dv_value);
    return hdr_dv_value;
}

int setHDMIHdrDobly(int hdr_dv_value) {
    int ret = -1;

    ALOGI("[MtkHdmiService] setHDMIHdrDobly, %d", hdr_dv_value);
#if defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGD("setHDMIHdrDobly onHdmiSettingsChange");
        hdr_dv_value = (((mHdmiCrtcNum << 12) & 0XF000) | hdr_dv_value);
        auto result = mCallback->onHdmiSettingsChange(ENABLE_HDR_DV_MODE, hdr_dv_value);
        if (!result.isOk()){
            ALOGD("setHDMIHdrDobly HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setHDMIHdrDobly fail!!! mCallback is null!!!");
    }
#endif
    return ret;
}


int setVideoConfig(int colorSpace, int colorDepth, int vformat, bool applyConfig) {
    ALOGI("[MtkHdmiService]1 setVideoConfig applyConfig %d colorSpace %d, colorDepth %d, vformat %d", applyConfig, colorSpace, colorDepth, vformat);
    int ret = -1;
    int fd = -1;
    char logo_name[PROPERTY_VALUE_MAX] = {0};

#if defined (MTK_HDMI_SUPPORT)
    if (applyConfig == true)
        ret = hdmi_ioctl(MTK_HDMI_VIDEO_CONFIG, vformat);
#elif defined (MTK_DRM_HDMI_SUPPORT)
    if (applyConfig == true){
        if (mCallback != NULL){
            ALOGD("setVideoConfig onHdmiSettingsChange");
            vformat = (((mHdmiCrtcNum << 12) & 0XF000) | vformat);
            auto result = mCallback->onHdmiSettingsChange(VIDEO_RESOLUTION_MODE, vformat);
            if (!result.isOk()){
                ALOGD("setVideoConfig HdmiCallback onHdmiSettingsChange() failed");
            }
            ret = 0;
        } else {
            ALOGE("setVideoConfig fail!!! mCallback is null!!!");
        }
    }
#endif

#if 1//defined (MTK_ALPS_BOX_SUPPORT)

#if defined(MTK_AB_OTA_UPDATER)
    //const char *ab_suffix = get_suffix();
    sp<IBootControl> module = IBootControl::getService();
    if (module == nullptr) {
        ALOGE("get logo partition (logo_a/logo_b) failed, Error getting bootctrl module. !!!");
        return -1;
    } else {
        uint32_t current_slot = module->getCurrentSlot();
        if (current_slot == 0) {
            //#define LOGO_DEV_NAME "/dev/block/by-name/logo_a"
            ALOGD("setVideoConfig to logo_a");
            strcpy(logo_name, "/dev/block/by-name/logo_a");
            fd = open("/dev/block/by-name/logo_a", O_RDWR);
        } else {
            ALOGD("setVideoConfig to logo_b");
            //#define LOGO_DEV_NAME "/dev/block/by-name/logo_b"
            strcpy(logo_name, "/dev/block/by-name/logo_b");
            fd = open("/dev/block/by-name/logo_b", O_RDWR);
        }
    }
#else
    ALOGD("setVideoConfig to logo");
    strcpy(logo_name, "/dev/block/by-name/logo");
    fd = open("/dev/block/by-name/logo", O_RDWR);
#endif
    if (fd >= 0) {
        char buf[PROPERTY_VALUE_MAX] = {0};
        property_get(HDMI_USE_FRAC_MODE, buf, "");

        //if property is not set to false or is NULL, select a fractional mode
        int selectFracMode = strcmp(buf, "false") ? 1 : 0;

        if (getValue(HDMI_VIDEO_AUTO,"1") == 1){
            vformat = AUTO_RESOLUTION;
        }

        int hdrMode = getValue(HDMI_ENABLE_HDR,"1");
        ALOGI("setVideoConfig hdrMode %d", hdrMode);

        int hdrDefaultMode = HDMI_FORCE_HDR;

        int hdmi_config = (hdrMode << 28) | (hdrDefaultMode << 25) | (selectFracMode << 24) | (colorSpace << 16) | (colorDepth << 8) | vformat | HDMI_RES_MASK;

        int fpos = lseek(fd, -512 ,SEEK_END);
        if (fpos < 0){
            ALOGE("lseek error!!!");
        }

        ret = write(fd, (void*)&hdmi_config, sizeof(hdmi_config));
        ALOGI("[MtkHdmiService]setVideoConfig hdmi_config 0x%x",hdmi_config);
        if (ret < 0) {
            ALOGE("[MtkHdmiService] [%s] failed. ioctlCode: %d, errno: %d",
                __func__, hdmi_config, errno);
        }
        close(fd);
        if (ret > 0){
            hdmi_config = 0;
            fd = open(logo_name, O_RDWR); 
            if (fd >= 0){
                fpos = lseek(fd, -512 ,SEEK_END);
                if (fpos < 0){
                    ALOGE("lseek error read!!!");
                }
                ret = read(fd, (void*)&hdmi_config, sizeof(int));
                ALOGI("[MtkHdmiService]setVideoConfig read result ret = %d",ret);
                if (ret > 0) {
                    ALOGI("[MtkHdmiService]setVideoConfig read write hdmi_config 0x%x",hdmi_config);
                }
                close(fd);
            }
        }
    } else {
        //ALOGE("[MtkHdmiService] [%s] open %s failed. errno:%d %s", __func__, LOGO_DEV_NAME, errno, strerror(errno));
        ALOGE("[MtkHdmiService] [%s] open %s failed. errno:%d %s", __func__, logo_name, errno, strerror(errno));
    }
#endif
    return ret;
}
#else
int setVideoConfig(int vformat) {
    ALOGI("lmf setVideoConfig = %d", vformat);
    int ret = -1;

#if defined (MTK_HDMI_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_VIDEO_CONFIG, vformat);
#elif defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGD("setVideoConfig onHdmiSettingsChange");
        auto result = mCallback->onHdmiSettingsChange(VIDEO_RESOLUTION_MODE, vformat);
        if (!result.isOk()){
            ALOGD("setVideoConfig_old HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setVideoConfig fail!!! mCallback is null!!!");
    }
#endif

#if defined (MTK_ALPS_BOX_SUPPORT)
    int fd = open(LOGO_DEV_NAME, O_RDWR);
    if (fd >= 0) {
        int hdmi_res = vformat|HDMI_RES_MASK;

        lseek(fd, -512 ,SEEK_END);

        ret = write(fd, (void*)&hdmi_res, sizeof(hdmi_res));
        ALOGI("setVideoConfig hdmi_res 0x%x",hdmi_res);
        if (ret < 0) {
            ALOGE("[%s] failed. ioctlCode: %d, errno: %d",
                __func__, vformat, errno);
        }
        close(fd);
    } else {
        ALOGE("[%s] open %s failed. errno:%d %s", __func__, LOGO_DEV_NAME, errno, strerror(errno));
    }
#endif
    return ret;
}
#endif
//-------------------------hdmi implementation end---------------------------------


MtkHdmiService::MtkHdmiService(){
    ALOGD("HIDL MtkHdmiService()");
    initialize();
}

MtkHdmiService::~MtkHdmiService(){
    ALOGD("~HIDL MtkHdmiService()");
    if (event_thread != NULL) {
        ALOGE("~MtkHdmiService requestExit");
        event_thread->requestExit();
        ALOGE("~MtkHdmiService requestExitAndWait");
        event_thread->requestExitAndWait();
        ALOGE("MtkHdmiService clear");
        event_thread = NULL;
        ALOGE("~uevent_thread done");
    }
}

#if defined(MTK_DRM_HDMI_SUPPORT)
void useFracModeIfNeeded(int &suitableResolution)
{
    int inResolution = suitableResolution;
    //choose fractional modes as needed
    char buf[PROPERTY_VALUE_MAX] = {0};
    property_get(HDMI_USE_FRAC_MODE, buf, "false");
    ALOGI("HDMI_USE_FRAC_MODE buf=%s", buf);

    //if property is not set to false or is NULL, select a fractional mode
    int selectFracMode = strcmp(buf, "false") ? 1 : 0;
    ALOGI("HDMI_USE_FRAC_MODE selectFracMode=%d", selectFracMode);
    if (!selectFracMode) {
        ALOGI("useFracModeIfNeeded: selectFracMode=%d, fractional mode not needed", selectFracMode);
        return;
    }

    //choose right fractional mode
    switch(inResolution) {
        case RESOLUTION_3840X2160P_60HZ:
            suitableResolution = RESOLUTION_3840X2160P_59_94HZ;
            ALOGI("useFracModeIfNeeded: 4k@60, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_3840X2160P_30HZ:
            suitableResolution = RESOLUTION_3840X2160P29_97HZ;
            ALOGI("useFracModeIfNeeded: 4k@30, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_3840X2160P_24HZ:
            suitableResolution = RESOLUTION_3840X2160P23_976HZ;
            ALOGI("useFracModeIfNeeded: 4k@24, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_1920X1080P_60HZ:
            suitableResolution = RESOLUTION_1920X1080P_59_94HZ;
            ALOGI("useFracModeIfNeeded: 1080p@60, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_1920X1080P_30HZ:
            suitableResolution = RESOLUTION_1920X1080P_29HZ;
            ALOGI("useFracModeIfNeeded: 1080p@30, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_1920X1080P_24HZ:
            suitableResolution = RESOLUTION_1920X1080P_23HZ;
            ALOGI("useFracModeIfNeeded: 1080p@24, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        case RESOLUTION_1280X720P_60HZ:
            suitableResolution = RESOLUTION_1280X720P_59_94HZ;
            ALOGI("useFracModeIfNeeded: 720p@60, selected frac mode=%d for mode=%d", suitableResolution, inResolution);
            break;
        default:
            ALOGI("useFracModeIfNeeded: default, no change for fractional mode, best mode = %d", inResolution);
            break;
    }
    return;
}

void findBestMode(const int resolution, int *colorSpaceMode, int *colorDepthMode)
{
    int ret = -1;
    HDMI_EDID_T edid;
    int inColorSpaceMode = *colorSpaceMode;
    int inColorDepth = *colorDepthMode;
    int maxTMDSRate = 0;
    bool max4k30DoviSink = false;

    //get EDID capabilities first
#if defined (MTK_HDMI_SUPPORT)
    ret = hdmi_ioctl(MTK_HDMI_GET_EDID, (long)&edid);
    ALOGI("findBestMode: hdmi_ioctl, ret = %d",ret);
    if (ret >= 0) {
        ALOGI("findBestMode: edid.ui4_ntsc_resolution 0x%x", edid.ui4_ntsc_resolution);
        ALOGI("findBestMode: edid.ui4_pal_resolution 0x%x", edid.ui4_pal_resolution);
        ALOGI("findBestMode: edid.ui4_sink_hdmi_4k2kvic 0x%x", edid.ui4_sink_hdmi_4k2kvic);
        ALOGI("findBestMode: edid.ui2_sink_colorimetry 0x%x", edid.ui2_sink_colorimetry);
        ALOGI("findBestMode: edid.ui1_sink_rgb_color_bit 0x%x", edid.ui1_sink_rgb_color_bit);
        ALOGI("findBestMode: edid.ui1_sink_ycbcr_color_bit 0x%x", edid.ui1_sink_ycbcr_color_bit);
        ALOGI("findBestMode: edid.ui1_sink_dc420_color_bit 0x%x", edid.ui1_sink_dc420_color_bit);
        ALOGI("findBestMode: edid.ui1_sink_support_dynamic_hdr 0x%x", edid.ui1_sink_support_dynamic_hdr);
        ALOGI("findBestMode: edid.ui2_sink_max_tmds_character_rate %d", edid.ui2_sink_max_tmds_character_rate);
        ALOGI("findBestMode: edid.ui1_sink_max_tmds_clock %d", edid.ui1_sink_max_tmds_clock);
    } else {
        *colorSpaceMode = HDMI_RGB;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        return;
    }
#else if defined(MTK_DRM_HDMI_SUPPORT)
    memset(&edid, 0, sizeof(HDMI_EDID_T));
    if ((mEdid[0] != 0) || (mEdid[1] != 0) || (mEdid[2] != 0) || (mEdid[3] != 0)){
        edid.ui4_ntsc_resolution = mEdidT[1].ui4_ntsc_resolution;
        ALOGI("findBestMode: edid.ui4_ntsc_resolution 0x%x", edid.ui4_ntsc_resolution);
        edid.ui4_pal_resolution = mEdidT[1].ui4_pal_resolution;
        ALOGI("findBestMode: edid.ui4_pal_resolution 0x%x", edid.ui4_pal_resolution);
        edid.ui4_sink_hdmi_4k2kvic = mEdidT[1].ui4_sink_hdmi_4k2kvic;
        ALOGI("findBestMode: edid.ui4_sink_hdmi_4k2kvic 0x%x", edid.ui4_sink_hdmi_4k2kvic);
        edid.ui2_sink_colorimetry = mEdidT[1].ui2_sink_colorimetry;
        ALOGI("findBestMode: edid.ui2_sink_colorimetry 0x%x", edid.ui2_sink_colorimetry);
        edid.ui1_sink_rgb_color_bit = mEdidT[1].ui1_sink_rgb_color_bit;
        ALOGI("findBestMode: edid.ui1_sink_rgb_color_bit 0x%x", edid.ui1_sink_rgb_color_bit);
        edid.ui1_sink_ycbcr_color_bit = mEdidT[1].ui1_sink_ycbcr_color_bit;
        ALOGI("findBestMode: edid.ui1_sink_ycbcr_color_bit 0x%x", edid.ui1_sink_ycbcr_color_bit);
        edid.ui1_sink_dc420_color_bit = mEdidT[1].ui1_sink_dc420_color_bit;
        ALOGI("findBestMode: edid.ui1_sink_dc420_color_bit 0x%x", edid.ui1_sink_dc420_color_bit);
        edid.ui1_sink_support_dynamic_hdr = mEdidT[1].ui1_sink_support_dynamic_hdr;
        ALOGI("findBestMode: edid.ui1_sink_support_dynamic_hdr 0x%x", edid.ui1_sink_support_dynamic_hdr);
        edid.ui2_sink_max_tmds_character_rate = mEdidT[1].ui2_sink_max_tmds_character_rate;
        ALOGI("findBestMode: edid.ui2_sink_max_tmds_character_rate %d", edid.ui2_sink_max_tmds_character_rate);
        edid.ui1_sink_max_tmds_clock = mEdidT[1].ui1_sink_max_tmds_clock;
        ALOGI("findBestMode: edid.ui1_sink_max_tmds_clock %d", edid.ui1_sink_max_tmds_clock);
    } else {
        *colorSpaceMode = HDMI_RGB;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        return;
	}
#endif

    //determine maximum bandwidth available
    if (edid.ui2_sink_max_tmds_character_rate != 0) {
        maxTMDSRate = edid.ui2_sink_max_tmds_character_rate;
        ALOGI("findBestMode: maxTMDSRate1 = %d", maxTMDSRate);
    } else {
        maxTMDSRate = edid.ui1_sink_max_tmds_clock;
        ALOGI("findBestMode: maxTMDSRate2 = %d", maxTMDSRate);
    }

    //force YUV 422/420 for 4k@50/60 modes.
    if ((resolution == RESOLUTION_3840X2160P_60HZ) || (resolution == RESOLUTION_3840X2160P_50HZ)
        || (resolution == RESOLUTION_3840X2160P_59_94HZ) || (resolution == RESOLUTION_4096X2161P_60HZ)
        || (resolution == RESOLUTION_4096X2161P_50HZ) || (resolution == RESOLUTION_4096x2160P_59_94HZ)) {
        /* next two checks can be combined to single but keeping them separate to be more clear */
        if ((edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR)
            && (edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR_2160P60)
            && (maxTMDSRate >= RES4K60_TMDS_RATE)){
            ALOGI("findBestMode: 4k: Dovi v0/v1 Std. Interface, EDID_SUPPORT_DV_HDR_2160P60,\
                edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
            *colorSpaceMode = HDMI_YCBCR_422;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findBestMode: 4k: EDID_SUPPORT_DV_HDR_2160P60 HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
            return;
        }

        if ((edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR)
            && (maxTMDSRate >= RES4K60_TMDS_RATE)){
            max4k30DoviSink = isMax4k30DoviSink(edid);
            ALOGI("findBestMode: max4k30DoviSink %u", max4k30DoviSink);
            /* if sink is max 4k30 dovi, and user is selecting resolution as 4k60, we do not need
               to use 422 colorspace as VS10 output will be HDR10 which can be safely supported
               in 420 colorspace*/
            if (!max4k30DoviSink) {
                ALOGI("findBestMode: 4k: possibly Dovi LL Interface or v2 std interface, EDID_SUPPORT_DV_HDR,\
                    edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
                *colorSpaceMode = HDMI_YCBCR_422;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findBestMode: 4k: EDID_SUPPORT_DV_HDR HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
                return;
            }
        }

        if ((edid.ui2_sink_colorimetry & SINK_YCBCR_420) || (edid.ui2_sink_colorimetry & SINK_YCBCR_420_CAPABILITY)) {
            *colorSpaceMode = HDMI_YCBCR_420;
            if ((inColorDepth == HDMI_DEEP_COLOR_12_BIT) &&
                (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_12_BIT) &&
                ((maxTMDSRate * 8) >= (RES4K30_TMDS_RATE * 12))) { //rate needed = 594/2 * 12/8 Mcsc
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findBestMode: 4k: HDMI_YCBCR_420 : HDMI_DEEP_COLOR_12_BIT");
                return;
            }
            if (((inColorDepth == HDMI_DEEP_COLOR_12_BIT) || (inColorDepth == HDMI_DEEP_COLOR_10_BIT)) &&
                (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_10_BIT) &&
                ((maxTMDSRate * 8) >= (RES4K30_TMDS_RATE * 10))) { //rate needed = 594/2 * 10/8 Mcsc
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findBestMode: 4k: HDMI_YCBCR_420 : HDMI_DEEP_COLOR_10_BIT");
                return;
            }
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findBestMode: 4k default: HDMI_YCBCR_420:HDMI_NO_DEEP_COLOR");
            return;
        }

        if ((edid.ui2_sink_colorimetry & SINK_YCBCR_422) && (maxTMDSRate >= RES4K60_TMDS_RATE)) {
            *colorSpaceMode = HDMI_YCBCR_422;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findBestMode: 4k: HDMI_YCBCR_422 : HDMI_NO_DEEP_COLOR");
            return;
        }

        //If we are here, sink does not support YCbCr420 sampling otherwise we would have returned
        //above with 420 mode and no deep colors in the worst case. So, we choose deault mode as 422
        //because there are more chances of sink supporting 4k in YCbCr422 and since EDID is reporting
        //sink can support 4k(that is why resolution request is for 4k60/50), it has to be most likely
        //supported in 422 mode despite maybe bandwidth check failure above in 422 case
        *colorSpaceMode = HDMI_YCBCR_422;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        ALOGI("findBestMode: 4k resolution=%d, default colorSpaceMode = HDMI_YCBCR_422, HDMI_NO_DEEP_COLOR", resolution);
        return;
    }

    //non-4k@50/60 DV vision modes
    if (edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR) {
        ALOGI("findBestMode: non-4k: EDID_SUPPORT_DV_HDR,\
                edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
        *colorSpaceMode = HDMI_YCBCR_422;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        ALOGI("findBestMode: non-4k: EDID_SUPPORT_DV_HDR: HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
        goto found;
    }

    switch (inColorDepth){
        case HDMI_DEEP_COLOR_12_BIT:
            if ((edid.ui1_sink_ycbcr_color_bit & SINK_DEEP_COLOR_12_BIT)) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findBestMode: HDMI_YCBCR_444:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            if (edid.ui1_sink_rgb_color_bit & SINK_DEEP_COLOR_12_BIT) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findBestMode: HDMI_RGB:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            if (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_12_BIT) {
                *colorSpaceMode = HDMI_YCBCR_420;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findBestMode: HDMI_YCBCR_420:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            //below we will switch to 10 bits mode if we are not supporting any 12 bits modes
        case HDMI_DEEP_COLOR_10_BIT:
            if (edid.ui1_sink_ycbcr_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findBestMode: HDMI_YCBCR_444:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            if (edid.ui1_sink_rgb_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findBestMode: HDMI_RGB:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            if (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_YCBCR_420;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findBestMode: HDMI_YCBCR_420:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            //below we will switch to 8 bits mode if we are not supporting any 12 or 10 bits modes
        case HDMI_NO_DEEP_COLOR:
            if (edid.ui2_sink_colorimetry & SINK_YCBCR_444) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findBestMode: HDMI_YCBCR_444:HDMI_NO_DEEP_COLOR");
                goto found;
            }
            if (edid.ui2_sink_colorimetry & SINK_RGB) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findBestMode: HDMI_RGB:HDMI_NO_DEEP_COLOR");
                goto found;
            }
            if (edid.ui2_sink_colorimetry & SINK_YCBCR_422) {
                *colorSpaceMode = HDMI_YCBCR_422;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findBestMode: HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
                goto found;
            }
        default:
            *colorSpaceMode = HDMI_RGB;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findBestMode: default: HDMI_RGB:HDMI_NO_DEEP_COLOR");
            break;
    }

found:
    if ((inColorSpaceMode == HDMI_RGB) && (*colorSpaceMode == HDMI_YCBCR_444)) {
        //force RGB if user requested unless 4K as checked below
        ALOGI("findBestMode: force HDMI_RGB");
        *colorSpaceMode = HDMI_RGB;
    }
    //bandwidth check for remaining 4k modes
    if ((maxTMDSRate != 0) && (resolution >= RESOLUTION_3840X2160P23_976HZ) && (resolution <= RESOLUTION_4096X2161P_24HZ)){
        if (*colorSpaceMode != HDMI_YCBCR_420) { //420 needs half the clock and capability checks above should be enough
            if ((*colorDepthMode == HDMI_DEEP_COLOR_12_BIT) && ((maxTMDSRate * 8) < (RES4K30_TMDS_RATE * 12))) {
                ALOGI("findBestMode: bandwidth not enough: %d, changed colorDepthMode to HDMI_DEEP_COLOR_10_BIT", maxTMDSRate);
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
            }
            if ((*colorDepthMode == HDMI_DEEP_COLOR_10_BIT) && ((maxTMDSRate * 8) < (RES4K30_TMDS_RATE * 10))) {
                ALOGI("findBestMode: bandwidth not enough: %d, changed colorDepthMode to HDMI_NO_DEEP_COLOR", maxTMDSRate);
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
            }
        }
    }
    return;
}
#endif


#if defined(MTK_DRM_HDMI_SUPPORT)
int getSuitableDPResolution(int resolution) {
    ALOGI("getSuitableDPResolution: %d ",resolution);
    int SuitableResolution = resolution;
    if (mDPInfo[0]!= 0 || mDPInfo[1]!= 0 || mDPInfo[2]!= 0 ) {
        int edidTemp = mDPInfo[0] | mDPInfo[1];
        int edidTemp_4k2k = mDPInfo[2];
        ALOGI("getSuitableDPResolution edidTemp: %d ",edidTemp);
        ALOGI("getSuitableDPResolution edidTemp_4k2k: %d ",edidTemp_4k2k);
        int* prefered = PREFERED_RESOLUTIONS;
        for (int i = 0; i < 22; i++) {
            int act = *(prefered + i);
            ALOGI("getSuitableDPResolution act: %d ",act);
            if(act < RESOLUTION_3840X2160P23_976HZ){
                if(act < sizeof(sResolutionMask)/sizeof(int)){
                    if (0 != (edidTemp & sResolutionMask[act])) {
                        SuitableResolution = act;
                        ALOGI("getSuitableDPResolution resolution: %d ",SuitableResolution);
                        break;
                  }
               }
            }
        #if defined(MTK_DRM_HDMI_SUPPORT)
            else{
                if (0 != (edidTemp_4k2k & sResolutionMask_4k2k[act - RESOLUTION_3840X2160P23_976HZ])) {
                    ALOGI("getSuitableDPResolution resolution 4k: %d ",SuitableResolution);
                    SuitableResolution = act;
                    break;
                }
            }
        #endif
        }
    } else {
        SuitableResolution = 2;
        ALOGI("getSuitableDPResolution mDPInfo==null,set solution to 480P60");
    }
    ALOGI("getSuitableDPResolution resolution final: %d ",SuitableResolution);
    return SuitableResolution;
}


int setDPResConfig(int vformat) {
    ALOGI("[MtkHdmiService] setDPResConfig vformat %d", vformat);
    int ret = -1;

    if (mCallback != NULL){
        ALOGI("setDPResConfig onHdmiSettingsChange");
        vformat = (((mDpCrtcNum << 12) & 0XF000) | vformat);
        auto result = mCallback->onHdmiSettingsChange(VIDEO_RESOLUTION_MODE, vformat);
        if (!result.isOk()){
            ALOGD("setDPResConfig HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setDPResConfig fail!!! mCallback is null!!!");
    }

    return ret;
}

int getDPHdrDV(){
    int hdr_value = -1;
    int dv_value = -1;
    //0:HDR OFF + DV OFF  1:HDR ON + DV OFF 2:HDR ON + DV ON
    int hdr_dv_value = -1;
    int hdmi_dv_hdr_driver_support = getValue(HDMI_DV_HDR_DRIVER_SUPPORT_PROPERTY, "-1");
    ALOGI("hdmi_dv_hdr_driver_support:%d", hdmi_dv_hdr_driver_support);
    if (1 == hdmi_dv_hdr_driver_support){
        hdr_value = getValue(DP_ENABLE_HDR, "1");
        dv_value = getValue(DP_ENABLE_DV, "1");
        if (0 == hdr_value){//if HDR OFF, the user can't set DV, it means DV is off.
            hdr_dv_value = 0;
        } else if ((1 == hdr_value) && (0 == dv_value)){//1:HDR ON + DV OFF
            hdr_dv_value = 1;
        } else if ((1 == hdr_value) && (1 == dv_value)){//2:HDR ON + DV ON
            hdr_dv_value = 2;
        } else {
            hdr_dv_value = 0;
        }
    } else if ((0 == hdmi_dv_hdr_driver_support)|| (-1 == hdmi_dv_hdr_driver_support)){
    //if HDMI_DV_HDR_DRIVER_SUPPORT_PROPERTY is 0, we should set hdr on,DV off default;
        hdr_value = getValue(DP_ENABLE_HDR, "1");
        //DV_value = getValue(HDMI_ENABLE_DV, "0");
        if (0 == hdr_value){//if HDR OFF, the user can't set DV, it means DV is off.
            hdr_dv_value = 0;
        } else if ((1 == hdr_value)/* && (0 == dv_value)*/){//1:HDR ON + DV OFF
            hdr_dv_value = 1;
        }/* else if ((1 == hdr_value) && (1 == dv_value)){//2:HDR ON + DV ON
            hdr_dv_value = 2;
        } */else {
            hdr_dv_value = 0;
        }
    }
    ALOGI("[MtkHdmiService] getDPHdrDV, %d, %d, %d", hdr_value, dv_value, hdr_dv_value);
    return hdr_dv_value;
}

void findDPBestMode(const int resolution, int *colorSpaceMode, int *colorDepthMode)
{
    int ret = -1;
    HDMI_EDID_T edid;
    int inColorSpaceMode = *colorSpaceMode;
    int inColorDepth = *colorDepthMode;
    int maxTMDSRate = 0;
    bool max4k30DoviSink = false;

    //get EDID capabilities first
#if defined(MTK_DRM_HDMI_SUPPORT)
    memset(&edid, 0, sizeof(HDMI_EDID_T));
    if ((mDPInfo[0] != 0) || (mDPInfo[1] != 0) || (mDPInfo[2] != 0) || (mDPInfo[3] != 0)){
        edid.ui4_ntsc_resolution = mEdidT[0].ui4_ntsc_resolution;
        ALOGI("findDPBestMode: edid.ui4_ntsc_resolution 0x%x", edid.ui4_ntsc_resolution);
        edid.ui4_pal_resolution = mEdidT[0].ui4_pal_resolution;
        ALOGI("findDPBestMode: edid.ui4_pal_resolution 0x%x", edid.ui4_pal_resolution);
        edid.ui4_sink_hdmi_4k2kvic = mEdidT[0].ui4_sink_hdmi_4k2kvic;
        ALOGI("findDPBestMode: edid.ui4_sink_hdmi_4k2kvic 0x%x", edid.ui4_sink_hdmi_4k2kvic);
        edid.ui2_sink_colorimetry = mEdidT[0].ui2_sink_colorimetry;
        ALOGI("findDPBestMode: edid.ui2_sink_colorimetry 0x%x", edid.ui2_sink_colorimetry);
        edid.ui1_sink_rgb_color_bit = mEdidT[0].ui1_sink_rgb_color_bit;
        ALOGI("findDPBestMode: edid.ui1_sink_rgb_color_bit 0x%x", edid.ui1_sink_rgb_color_bit);
        edid.ui1_sink_ycbcr_color_bit = mEdidT[0].ui1_sink_ycbcr_color_bit;
        ALOGI("findDPBestMode: edid.ui1_sink_ycbcr_color_bit 0x%x", edid.ui1_sink_ycbcr_color_bit);
        edid.ui1_sink_dc420_color_bit = mEdidT[0].ui1_sink_dc420_color_bit;
        ALOGI("findDPBestMode: edid.ui1_sink_dc420_color_bit 0x%x", edid.ui1_sink_dc420_color_bit);
        edid.ui1_sink_support_dynamic_hdr = mEdidT[0].ui1_sink_support_dynamic_hdr;
        ALOGI("findDPBestMode: edid.ui1_sink_support_dynamic_hdr 0x%x", edid.ui1_sink_support_dynamic_hdr);
        edid.ui2_sink_max_tmds_character_rate = mEdidT[0].ui2_sink_max_tmds_character_rate;
        ALOGI("findDPBestMode: edid.ui2_sink_max_tmds_character_rate %d", edid.ui2_sink_max_tmds_character_rate);
        edid.ui1_sink_max_tmds_clock = mEdidT[0].ui1_sink_max_tmds_clock;
        ALOGI("findDPBestMode: edid.ui1_sink_max_tmds_clock %d", edid.ui1_sink_max_tmds_clock);
    } else {
        *colorSpaceMode = HDMI_RGB;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        return;
    }
#endif

    //determine maximum bandwidth available
    if (edid.ui2_sink_max_tmds_character_rate != 0) {
        maxTMDSRate = edid.ui2_sink_max_tmds_character_rate;
        ALOGI("findDPBestMode: maxTMDSRate1 = %d", maxTMDSRate);
    } else {
        maxTMDSRate = edid.ui1_sink_max_tmds_clock;
        ALOGI("findDPBestMode: maxTMDSRate2 = %d", maxTMDSRate);
    }

    //force YUV 422/420 for 4k@50/60 modes.
    if ((resolution == RESOLUTION_3840X2160P_60HZ) || (resolution == RESOLUTION_3840X2160P_50HZ)
        || (resolution == RESOLUTION_3840X2160P_59_94HZ) || (resolution == RESOLUTION_4096X2161P_60HZ)
        || (resolution == RESOLUTION_4096X2161P_50HZ) || (resolution == RESOLUTION_4096x2160P_59_94HZ)) {
        /* next two checks can be combined to single but keeping them separate to be more clear */
        if ((edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR)
            && (edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR_2160P60)
            && (maxTMDSRate >= RES4K60_TMDS_RATE)){
            ALOGI("findDPBestMode: 4k: Dovi v0/v1 Std. Interface, EDID_SUPPORT_DV_HDR_2160P60,\
                edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
            *colorSpaceMode = HDMI_YCBCR_422;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findDPBestMode: 4k: EDID_SUPPORT_DV_HDR_2160P60 HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
            return;
        }

        if ((edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR)
            && (maxTMDSRate >= RES4K60_TMDS_RATE)){
            max4k30DoviSink = isMax4k30DoviSink(edid);
            ALOGI("findDPBestMode: max4k30DoviSink %u", max4k30DoviSink);
            /* if sink is max 4k30 dovi, and user is selecting resolution as 4k60, we do not need
               to use 422 colorspace as VS10 output will be HDR10 which can be safely supported
               in 420 colorspace*/
            if (!max4k30DoviSink) {
                ALOGI("findDPBestMode: 4k: possibly Dovi LL Interface or v2 std interface, EDID_SUPPORT_DV_HDR,\
                    edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
                *colorSpaceMode = HDMI_YCBCR_422;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findDPBestMode: 4k: EDID_SUPPORT_DV_HDR HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
                return;
            }
        }

        if ((edid.ui2_sink_colorimetry & SINK_YCBCR_420) || (edid.ui2_sink_colorimetry & SINK_YCBCR_420_CAPABILITY)) {
            *colorSpaceMode = HDMI_YCBCR_420;
            if ((inColorDepth == HDMI_DEEP_COLOR_12_BIT) &&
                (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_12_BIT) &&
                ((maxTMDSRate * 8) >= (RES4K30_TMDS_RATE * 12))) { //rate needed = 594/2 * 12/8 Mcsc
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findDPBestMode: 4k: HDMI_YCBCR_420 : HDMI_DEEP_COLOR_12_BIT");
                return;
            }
            if (((inColorDepth == HDMI_DEEP_COLOR_12_BIT) || (inColorDepth == HDMI_DEEP_COLOR_10_BIT)) &&
                (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_10_BIT) &&
                ((maxTMDSRate * 8) >= (RES4K30_TMDS_RATE * 10))) { //rate needed = 594/2 * 10/8 Mcsc
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findDPBestMode: 4k: HDMI_YCBCR_420 : HDMI_DEEP_COLOR_10_BIT");
                return;
            }
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findDPBestMode: 4k default: HDMI_YCBCR_420:HDMI_NO_DEEP_COLOR");
            return;
        }

        if ((edid.ui2_sink_colorimetry & SINK_YCBCR_422) && (maxTMDSRate >= RES4K60_TMDS_RATE)) {
            *colorSpaceMode = HDMI_YCBCR_422;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findDPBestMode: 4k: HDMI_YCBCR_422 : HDMI_NO_DEEP_COLOR");
            return;
        }

        //If we are here, sink does not support YCbCr420 sampling otherwise we would have returned
        //above with 420 mode and no deep colors in the worst case. So, we choose deault mode as 422
        //because there are more chances of sink supporting 4k in YCbCr422 and since EDID is reporting
        //sink can support 4k(that is why resolution request is for 4k60/50), it has to be most likely
        //supported in 422 mode despite maybe bandwidth check failure above in 422 case
        *colorSpaceMode = HDMI_YCBCR_422;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        ALOGI("findDPBestMode: 4k resolution=%d, default colorSpaceMode = HDMI_YCBCR_422, HDMI_NO_DEEP_COLOR", resolution);
        return;
    }

    //non-4k@50/60 DV vision modes
    if (edid.ui1_sink_support_dynamic_hdr & EDID_SUPPORT_DV_HDR) {
        ALOGI("findDPBestMode: non-4k: EDID_SUPPORT_DV_HDR,\
                edid.ui1_sink_support_dynamic_hdr=0x%x", edid.ui1_sink_support_dynamic_hdr);
        *colorSpaceMode = HDMI_YCBCR_422;
        *colorDepthMode = HDMI_NO_DEEP_COLOR;
        ALOGI("findDPBestMode: non-4k: EDID_SUPPORT_DV_HDR: HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
        goto found;
    }

    switch (inColorDepth){
        case HDMI_DEEP_COLOR_12_BIT:
            if ((edid.ui1_sink_ycbcr_color_bit & SINK_DEEP_COLOR_12_BIT)) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findDPBestMode: HDMI_YCBCR_444:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            if (edid.ui1_sink_rgb_color_bit & SINK_DEEP_COLOR_12_BIT) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findDPBestMode: HDMI_RGB:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            if (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_12_BIT) {
                *colorSpaceMode = HDMI_YCBCR_420;
                *colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
                ALOGI("findDPBestMode: HDMI_YCBCR_420:HDMI_DEEP_COLOR_12_BIT");
                goto found;
            }
            //below we will switch to 10 bits mode if we are not supporting any 12 bits modes
        case HDMI_DEEP_COLOR_10_BIT:
            if (edid.ui1_sink_ycbcr_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findDPBestMode: HDMI_YCBCR_444:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            if (edid.ui1_sink_rgb_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findDPBestMode: HDMI_RGB:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            if (edid.ui1_sink_dc420_color_bit & SINK_DEEP_COLOR_10_BIT) {
                *colorSpaceMode = HDMI_YCBCR_420;
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
                ALOGI("findDPBestMode: HDMI_YCBCR_420:HDMI_DEEP_COLOR_10_BIT");
                goto found;
            }
            //below we will switch to 8 bits mode if we are not supporting any 12 or 10 bits modes
        case HDMI_NO_DEEP_COLOR:
            if (edid.ui2_sink_colorimetry & SINK_YCBCR_444) {
                *colorSpaceMode = HDMI_YCBCR_444;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findDPBestMode: HDMI_YCBCR_444:HDMI_NO_DEEP_COLOR");
                goto found;
            }
            if (edid.ui2_sink_colorimetry & SINK_RGB) {
                *colorSpaceMode = HDMI_RGB;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findDPBestMode: HDMI_RGB:HDMI_NO_DEEP_COLOR");
                goto found;
            }
            if (edid.ui2_sink_colorimetry & SINK_YCBCR_422) {
                *colorSpaceMode = HDMI_YCBCR_422;
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
                ALOGI("findDPBestMode: HDMI_YCBCR_422:HDMI_NO_DEEP_COLOR");
                goto found;
            }
        default:
            *colorSpaceMode = HDMI_RGB;
            *colorDepthMode = HDMI_NO_DEEP_COLOR;
            ALOGI("findDPBestMode: default: HDMI_RGB:HDMI_NO_DEEP_COLOR");
            break;
    }

found:
    if ((inColorSpaceMode == HDMI_RGB) && (*colorSpaceMode == HDMI_YCBCR_444)) {
        //force RGB if user requested unless 4K as checked below
        ALOGI("findDPBestMode: force HDMI_RGB");
        *colorSpaceMode = HDMI_RGB;
    }
    //bandwidth check for remaining 4k modes
    if ((maxTMDSRate != 0) && (resolution >= RESOLUTION_3840X2160P23_976HZ) && (resolution <= RESOLUTION_4096X2161P_24HZ)){
        if (*colorSpaceMode != HDMI_YCBCR_420) { //420 needs half the clock and capability checks above should be enough
            if ((*colorDepthMode == HDMI_DEEP_COLOR_12_BIT) && ((maxTMDSRate * 8) < (RES4K30_TMDS_RATE * 12))) {
                ALOGI("findDPBestMode: bandwidth not enough: %d, changed colorDepthMode to HDMI_DEEP_COLOR_10_BIT", maxTMDSRate);
                *colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
            }
            if ((*colorDepthMode == HDMI_DEEP_COLOR_10_BIT) && ((maxTMDSRate * 8) < (RES4K30_TMDS_RATE * 10))) {
                ALOGI("findDPBestMode: bandwidth not enough: %d, changed colorDepthMode to HDMI_NO_DEEP_COLOR", maxTMDSRate);
                *colorDepthMode = HDMI_NO_DEEP_COLOR;
            }
        }
    }
    return;
}

static int setDPDeepColor(int color, int deep) {
    int ret = -1;
#if defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGI("setDPDeepColor(%d,%d)", color, deep);
        int color_deep_value = ((0x0F & color) << 4) | (0x0F & deep);
        ALOGI("setDPDeepColor color_deep_value:%d", color_deep_value);
        color_deep_value = (((mDpCrtcNum << 12) & 0XF000) | color_deep_value);
        auto result = mCallback->onHdmiSettingsChange(SET_COLOR_FORMAT_DEPTH_MODE, color_deep_value);
        if (!result.isOk()){
            ALOGD("setDPDeepColor HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setDPDeepColor fail!!! mCallback is null!!!");
    }
#endif
    return ret;
}

int setDPHdrDobly(int hdr_dv_value) {
    int ret = -1;

    ALOGI("[MtkHdmiService] setDPHdrDobly, %d", hdr_dv_value);
#if defined (MTK_DRM_HDMI_SUPPORT)
    if (mCallback != NULL){
        ALOGI("setDPHdrDobly onHdmiSettingsChange");
        hdr_dv_value = (((mDpCrtcNum << 12) & 0XF000) | hdr_dv_value);
        auto result = mCallback->onHdmiSettingsChange(ENABLE_HDR_DV_MODE, hdr_dv_value);
        if (!result.isOk()){
            ALOGD("setDPHdrDobly HdmiCallback onHdmiSettingsChange() failed");
        }
        ret = 0;
    } else {
        ALOGE("setDPHdrDobly fail!!! mCallback is null!!!");
    }
#endif
    return ret;
}

int setDPResolution(int resolution){
    char colorSpace[PROPERTY_VALUE_MAX] = {0};
    int colorSpaceMode = -1;
    int colorSpaceModeConfig = -1;
    int colorDepth = 0;
    int colorDepthMode = 0;
    int colorDepthModeConfig = 0;
    char hdrOut[PROPERTY_VALUE_MAX] = {0};
    char hdrDefaultMode[PROPERTY_VALUE_MAX] = {0};
    int hdrMode = 0;
    bool yuv4kSink = false;

    if(getValue(DP_RES_AUTO,"1") == 1)
        setValue(DP_RES_AUTO, 1);

    ALOGI("setDPResolution: resolution = %d" , resolution);
    int suitableResolution = resolution;
    if (resolution >= AUTO_RESOLUTION || getValue(DP_RES_AUTO,"1") == 1) {
        suitableResolution = getSuitableDPResolution(resolution);
        ALOGI("setDPResolution: Auto mode, suitableResolution = %d", suitableResolution);
    } else {
        ALOGI("setDPResolution: Manual mode, resolution= %d, suitableResolution = %d", resolution, suitableResolution);
    }

    //check color format as set by the user. This will return either 'auto', 'rgb' or 'ycbcr'
    property_get(DP_COLOR_SPACE, colorSpace, "auto"); //default is auto

    //determine if 4k sink in which case we should choose YCbCr as color format in auto mode
    if (((mDPInfo[2] & SINK_2160P_60HZ) || (mDPInfo[2] & SINK_2160P_50HZ)) &&
        ((mDPInfo[3] & SINK_YCBCR_420) || (mDPInfo[3] & SINK_YCBCR_420_CAPABILITY) ||
         (mDPInfo[3] & SINK_YCBCR_444))) {
        ALOGI("setDPResolution: mDPInfo[2]=0x%x, mDPInfo[3] = 0x%x, yuv4kSink = true", mDPInfo[2], mDPInfo[3]);
        yuv4kSink = true;
    }
    if (!strcmp(colorSpace, "auto")) {
        colorSpaceMode = yuv4kSink ? HDMI_YCBCR_444: HDMI_RGB;
        colorSpaceModeConfig = AUTO_COLORSPACE;
        ALOGI("setDPResolution: auto mode, yuv4kSink=%d, colorSpaceModeConfig = AUTO_COLORSPACE, colorSpaceMode = %d", yuv4kSink, colorSpaceMode);
    } else if (!strcmp(colorSpace, "rgb")) {
        ALOGI("setDPResolution: rgb mode, initial colorSpaceMode = HDMI_RGB");
        colorSpaceMode = HDMI_RGB;
        colorSpaceModeConfig = HDMI_RGB;
    } else {
        ALOGI("setDPResolution: ycbcr mode, initial colorSpaceMode = HDMI_YCBCR_444");
        colorSpaceMode = HDMI_YCBCR_444; //to start with
        colorSpaceModeConfig = HDMI_YCBCR_444;
    }

    //check color depth as set by the user. This property will return either 8, 10 or 12 as of now
    colorDepth = getValue(DP_DEEP_COLOR,"8"); //default is 8 bits
    if (colorDepth == 12) {
        colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
    } else if (colorDepth == 10) {
        colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
    } else if (colorDepth == 8) {
        colorDepthMode = HDMI_NO_DEEP_COLOR;
    } else {
        colorDepthMode = HDMI_NO_DEEP_COLOR;
    }
    colorDepthModeConfig = colorDepthMode;

    hdrMode = getDPHdrDV();

    int finalResolution = suitableResolution >= AUTO_RESOLUTION ? (suitableResolution - AUTO_RESOLUTION)
                : suitableResolution;

    ALOGI("setDPResolution: User Request: resolution = %d, colorSpace = %s, colorDepth = %d, hdrMode = %d",
                                                                    finalResolution, colorSpace, colorDepth, hdrMode);
    ALOGI("setDPResolution: User Request: resolution = %d, colorSpaceMode = %d, colorDepthMode = %d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);
    findDPBestMode(finalResolution, &colorSpaceMode, &colorDepthMode);
    ALOGI("setDPResolution: Final mode to be set: finalResolution = %d, colorSpaceMode = %d, colorDepth = %d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);

    if ((finalResolution == getValue(DP_RESOLUTION,"0xff00")) &&
        (colorSpaceMode == mColorSpaceMode) &&
        (colorDepthMode == mColorDepthMode) &&
        (hdrMode == mHdrMode)) {
        ALOGI("New dp mode is the same as previous");
        if (!mDpStateChanged) {
            ALOGI("setDPResolution is the same return");
            setDPResConfig(finalResolution);
            return true;
        }
    }

    ALOGI("setDPResolution: Actually setting video resolution = %d, colorSpaceMode=%d, colorDepthMode=%d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);
    setDPDeepColor(colorSpaceMode, colorDepthMode);

    setDPHdrDobly(hdrMode);

    setValue(DP_RESOLUTION, finalResolution);

    //store them for a comparsion next time
    mColorSpaceMode = colorSpaceMode;
    mColorDepthMode = colorDepthMode;
    mHdrMode = hdrMode;
    mDpStateChanged = false;

    return setDPResConfig(finalResolution);
}

int setVideoResolution(int resolution) {
    char colorSpace[PROPERTY_VALUE_MAX] = {0};
    int colorSpaceMode = -1;
    int colorSpaceModeConfig = -1;
    int colorDepth = 0;
    int colorDepthMode = 0;
    int colorDepthModeConfig = 0;
    char hdrOut[PROPERTY_VALUE_MAX] = {0};
    char hdrDefaultMode[PROPERTY_VALUE_MAX] = {0};
    int hdrMode = 0;
    bool yuv4kSink = false;

    if(getValue(HDMI_VIDEO_AUTO,"1") == 1)
        setValue(HDMI_VIDEO_AUTO, 1);

    ALOGI("setVideoResolution: resolution = %d" , resolution);
    int suitableResolution = resolution;
    if (resolution >= AUTO_RESOLUTION || getValue(HDMI_VIDEO_AUTO,"1") == 1) {
        suitableResolution = getSuitableResolution(resolution);
        ALOGI("setVideoResolution: Auto mode, suitableResolution = %d", suitableResolution);
    } else {
        //Remove it.We should set to the resolution which the user selected.
        //suitableResolution = getSuitableResolution(resolution, false);
        ALOGI("setVideoResolution: Manual mode, resolution= %d, suitableResolution = %d", resolution, suitableResolution);
    }

    //fractional modes needed check and update
    //useFracModeIfNeeded(suitableResolution);
    //ALOGI("setVideoResolution: After useFracModeIfNeeded check, suitableResolution = %d", suitableResolution);

    //check color format as set by the user. This will return either 'auto', 'rgb' or 'ycbcr'
    property_get(HDMI_COLOR_SPACE, colorSpace, "auto"); //default is auto

    //determine if 4k sink in which case we should choose YCbCr as color format in auto mode
    if (((mEdid[2] & SINK_2160P_60HZ) || (mEdid[2] & SINK_2160P_50HZ)) &&
        ((mEdid[3] & SINK_YCBCR_420) || (mEdid[3] & SINK_YCBCR_420_CAPABILITY) ||
         (mEdid[3] & SINK_YCBCR_444))) {
        ALOGI("setVideoResolution: mEdid[2]=0x%x, mEdid[3] = 0x%x, yuv4kSink = true", mEdid[2], mEdid[3]);
        yuv4kSink = true;
    }
    if (!strcmp(colorSpace, "auto")) {
        colorSpaceMode = yuv4kSink ? HDMI_YCBCR_444: HDMI_RGB;
        colorSpaceModeConfig = AUTO_COLORSPACE;
        ALOGI("setVideoResolution: auto mode, yuv4kSink=%d, colorSpaceModeConfig = AUTO_COLORSPACE, colorSpaceMode = %d", yuv4kSink, colorSpaceMode);
    } else if (!strcmp(colorSpace, "rgb")) {
        ALOGI("setVideoResolution: rgb mode, initial colorSpaceMode = HDMI_RGB");
        colorSpaceMode = HDMI_RGB;
        colorSpaceModeConfig = HDMI_RGB;
    } else {
        ALOGI("setVideoResolution: ycbcr mode, initial colorSpaceMode = HDMI_YCBCR_444");
        colorSpaceMode = HDMI_YCBCR_444; //to start with
        colorSpaceModeConfig = HDMI_YCBCR_444;
    }

    //check color depth as set by the user. This property will return either 8, 10 or 12 as of now
    colorDepth = getValue(HDMI_DEEP_COLOR,"12"); //default is 12 bits
    if (colorDepth == 12) {
        colorDepthMode = HDMI_DEEP_COLOR_12_BIT;
    } else if (colorDepth == 10) {
        colorDepthMode = HDMI_DEEP_COLOR_10_BIT;
    } else if (colorDepth == 8) {
        colorDepthMode = HDMI_NO_DEEP_COLOR;
    } else {
        colorDepthMode = HDMI_NO_DEEP_COLOR;
    }
    colorDepthModeConfig = colorDepthMode;

    hdrMode = getHDMIHdrDV();
    ALOGI("setVideoResolution: hdrMode = %d", hdrMode);

    int finalResolution = suitableResolution >= AUTO_RESOLUTION ? (suitableResolution - AUTO_RESOLUTION)
                : suitableResolution;

    ALOGI("setVideoResolution: User Request: resolution = %d, colorSpace = %s, colorDepth = %d, hdrMode = %d",
                                                                    finalResolution, colorSpace, colorDepth, hdrMode);
    ALOGI("setVideoResolution: User Request: resolution = %d, colorSpaceMode = %d, colorDepthMode = %d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);
    findBestMode(finalResolution, &colorSpaceMode, &colorDepthMode);
    ALOGI("setVideoResolution: Final mode to be set: finalResolution = %d, colorSpaceMode = %d, colorDepth = %d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);

    if ((finalResolution == getValue(HDMI_VIDEO_RESOLUTION,"0xff00")) &&
        (colorSpaceMode == mColorSpaceMode) &&
        (colorDepthMode == mColorDepthMode) &&
        (hdrMode == mHdrMode)) {
        ALOGI("New HDMI mode is the same as previous");
        if (!mHdmiStateChanged) {
            ALOGI("setVideoResolution is the same return");
            setVideoConfig(colorSpaceModeConfig, colorDepthModeConfig, finalResolution & 0xff, false);
            return true;
        }
    }

    //xbh patch start
    #if defined(XBH_FUNC_LCD_CHIP_CS5803) || defined(XBH_FUNC_LCD_CHIP_EP9129)
    //Force output 4K60Hz RGB 8bit while use HDMI TO VBO chip
    finalResolution = RESOLUTION_3840X2160P_60HZ;
    colorSpaceMode = HDMI_RGB;
    colorDepthMode = HDMI_NO_DEEP_COLOR;
    #endif
    //xbh patch end

    ALOGI("setVideoResolution: Actually setting video resolution = %d, colorSpaceMode=%d, colorDepthMode=%d, hdrMode = %d",
                                                                    finalResolution, colorSpaceMode, colorDepthMode, hdrMode);
    setDeepColor(colorSpaceMode, colorDepthMode);
    //enableHDMIHdr(hdrMode);

    setHDMIHdrDobly(hdrMode);

    setValue(HDMI_VIDEO_RESOLUTION, finalResolution);

    //store them for a comparsion next time
    mColorSpaceMode = colorSpaceMode;
    mColorDepthMode = colorDepthMode;
    mHdrMode = hdrMode;
    mHdmiStateChanged = false;
    int param = (finalResolution & 0xff);
    return setVideoConfig(colorSpaceModeConfig, colorDepthModeConfig, param, true);
}

#else

int setVideoResolution(int resolution) {
    if(getValue(HDMI_VIDEO_AUTO,"1") == 1)
        setValue(HDMI_VIDEO_AUTO, 1);

    ALOGI("setVideoResolution = %d" , resolution);
    int suitableResolution = resolution;
    if (resolution >= AUTO_RESOLUTION || getValue(HDMI_VIDEO_AUTO,"1") == 1) {
        suitableResolution = getSuitableResolution(resolution);
    }
    ALOGI("suitableResolution = %d" , suitableResolution);
    if (suitableResolution == getValue(HDMI_VIDEO_RESOLUTION,"0xff00")) {
        ALOGI("setVideoResolution is the same");
        if (!mHdmiStateChanged) {
            ALOGI("setVideoResolution is the same return");
            return true;
        }
    }
    int finalResolution = suitableResolution >= AUTO_RESOLUTION ? (suitableResolution - AUTO_RESOLUTION)
                : suitableResolution;
    ALOGI("final video resolution = %d ", finalResolution);

    /*if (finalResolution == 27) {
        int* edid_temp = getResolutionMask();
        if (edid_temp[3] & SINK_YCBCR_420) {
            setDeepColor(4, 1);
        } else if (edid_temp[3] & SINK_YCBCR_420_CAPABILITY) {
            setDeepColor(3, 1);
        }
    } else {
        setDeepColor(3, 1);
    }*/
    setValue(HDMI_VIDEO_RESOLUTION, finalResolution);
    mHdmiStateChanged = false;
    int param = (finalResolution & 0xff);
    return setVideoConfig(param);
}
#endif

EDID_t getResolutionMask(){
    ALOGI("getResolutionMask in");
    int ret = -1;
    EDID_t cResult;
    memset(&cResult, 0, sizeof(EDID_t));
#if defined (MTK_HDMI_SUPPORT)
    HDMI_EDID_T edid;
    memset(&edid, 0, sizeof(HDMI_EDID_T));
    ret = hdmi_ioctl(MTK_HDMI_GET_EDID, (long)&edid);
    ALOGI("hdmi_ioctl,ret = %d",ret);
    if (ret >= 0) {
        ALOGI("edid.ui4_ntsc_resolution %4X\n", edid.ui4_ntsc_resolution);
        ALOGI("edid.ui4_pal_resolution %4X\n", edid.ui4_pal_resolution);
        #if defined (MTK_INTERNAL_HDMI_SUPPORT)
        ALOGI("edid.ui4_sink_hdmi_4k2kvic %4X\n", edid.ui4_sink_hdmi_4k2kvic);
        #endif
        ALOGI("edid.ui2_sink_colorimetry %4X\n", edid.ui2_sink_colorimetry);

        cResult.edid[0] = edid.ui4_ntsc_resolution;
        cResult.edid[1] = edid.ui4_pal_resolution;
        #if defined (MTK_INTERNAL_HDMI_SUPPORT)
        cResult.edid[2] = edid.ui4_sink_hdmi_4k2kvic;
        cResult.edid[3] = edid.ui2_sink_colorimetry;
        #else
        cResult.edid[2] = 0;
        cResult.edid[3] = edid.ui2_sink_colorimetry;
        #endif
    }
#elif defined(MTK_DRM_HDMI_SUPPORT)
    int isGetHDMIResolution = getValue(DEAL_HDMI_RESOLUTION_PROP,"0");
    int isGetDPResolution = getValue(DEAL_DP_RESOLUTION_PROP,"0");
    if (1 == isGetDPResolution){
        if ((mDPInfo[0] != 0) || (mDPInfo[1] != 0) || (mDPInfo[2] != 0) || (mDPInfo[3] != 0)){
            cResult.edid[0] = mDPInfo[0];
            cResult.edid[1] = mDPInfo[1];
            cResult.edid[2] = mDPInfo[2];
            cResult.edid[3] = mDPInfo[3];
            ALOGI("dp getEDID, %d, %d, %d, %d\n",cResult.edid[0],cResult.edid[1],cResult.edid[2],cResult.edid[3]);
        }
        setValue(DEAL_DP_RESOLUTION_PROP, 0);
    } else if (1 == isGetHDMIResolution){
        if ((mEdid[0] != 0) || (mEdid[1] != 0) || (mEdid[2] != 0) || (mEdid[3] != 0)){
            cResult.edid[0] = mEdid[0];
            cResult.edid[1] = mEdid[1];
            cResult.edid[2] = mEdid[2];
            cResult.edid[3] = mEdid[3];
            ALOGI("hdmi getEDID, %d, %d, %d, %d\n",cResult.edid[0],cResult.edid[1],cResult.edid[2],cResult.edid[3]);
        }
        setValue(DEAL_HDMI_RESOLUTION_PROP, 0);
    }
#endif


    ALOGI("getEDID\n");
    return cResult;
}


// Methods from ::vendor::mediatek::hardware::hdmi::V1_0::IMtkHdmiService follow.
Return<void> MtkHdmiService::get_resolution_mask(get_resolution_mask_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    ALOGD("get_resolution_mask...");
    EDID_t edid = getResolutionMask();
    _hidl_cb(Result::SUCCEED, edid);
    return Void();
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::enable_hdcp(bool enable) {
    // TODO implement
    ALOGD("enable_hdcp...");
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::enable_hdmi(bool enable) {
    Mutex::Autolock _l(mLock);
    ALOGD("enable_hdmi,enable = %d" , enable);
    int ret = enableHDMI(enable);
    if (ret < 0){
        return Result::FAILED;
    }
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_video_resolution(int32_t resolution) {
    Mutex::Autolock _l(mLock);
    ALOGD("set_video_resolution,resolution = %d" , resolution);
    int ret = -1;
#if defined(MTK_DRM_HDMI_SUPPORT)
    int isGetHDMIResolution = getValue(DEAL_HDMI_RESOLUTION_PROP,"0");
    int isGetDPResolution = getValue(DEAL_DP_RESOLUTION_PROP,"0");
    if (1 == isGetDPResolution){
        ret = setDPResolution(resolution);
        setValue(DEAL_DP_RESOLUTION_PROP, 0);
    } else if (1 == isGetHDMIResolution){
        ret = setVideoResolution(resolution);
        setValue(DEAL_HDMI_RESOLUTION_PROP, 0);
    }
#else
    ret = setVideoResolution(resolution);
#endif
    if (ret < 0){
        return Result::FAILED;
    }
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::enable_hdmi_hdr(bool enable) {
    // TODO implement
    ALOGD("enable_hdmi_hdr,enable = %d" , enable);
#if defined(MTK_DRM_HDMI_SUPPORT)
    int value = 0;
    if (enable){
        value = HDMI_FORCE_HDR;
    } else {
        value = HDMI_FORCE_DEFAULT;
    }
    int ret = enableHDMIHdr(value);
    if (ret < 0){
        return Result::FAILED;
    }
#endif
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_auto_mode(bool enable) {
    Mutex::Autolock _l(mLock);
    ALOGD("set_auto_mode,enable = %d" , enable);
    if (enable){
        setValue(HDMI_VIDEO_AUTO, 1);
    } else {
        setValue(HDMI_VIDEO_AUTO, 0);
    }
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_color_format(int32_t color_format) {
    // TODO implement
    //set color format property here
    ALOGD("set_color_format,color_format = %d" , color_format);
    if (color_format == AUTO_COLORSPACE) {
        property_set(HDMI_COLOR_SPACE,"auto");
    } else if (color_format == COLOR_SPACE_RGB){
        property_set(HDMI_COLOR_SPACE,"rgb");
    } else {
        property_set(HDMI_COLOR_SPACE,"ycbcr");
    }
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_color_depth(int32_t color_depth) {
    // TODO implement
    //set color depth property here
    ALOGD("set_color_depth,color_depth = %d" , color_depth);
    setValue(HDMI_DEEP_COLOR, color_depth);
    return Result::SUCCEED;
}

// Methods from ::vendor::mediatek::hardware::hdmi::V1_1::IMtkHdmiService follow.
Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_mode_type(int32_t is_auto_mode) {
    Mutex::Autolock _l(mLock);
    ALOGD("set_mode_type,is_auto_mode = %d" , is_auto_mode);
    setValue(HDMI_VIDEO_AUTO, is_auto_mode);
    return Result::SUCCEED;
}

Return<void> MtkHdmiService::get_auto_disp_mode(get_auto_disp_mode_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    ALOGD("get_auto_disp_mode...");
    int autoMode = 0;
    _hidl_cb(Result::SUCCEED, autoMode);
    return Void();
}


// Methods from ::vendor::mediatek::hardware::hdmi::V1_2::IMtkHdmiService follow.
Return<void> MtkHdmiService::setHdmiSettingsCallback(const sp<::vendor::mediatek::hardware::hdmi::V1_2::IMtkHdmiCallback>& callback) {
    ALOGD("setHdmiSettingsCallback...");

#if defined(MTK_DRM_HDMI_SUPPORT)
    if (callback != NULL){
        mCallback = callback;
        ALOGI("1 setHdmiSettingsCallback...");
    } else {
        ALOGE("setHdmiSettingsCallback fail!!!");
    }
#endif
    return Void();
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::setEDIDInfo(const ::vendor::mediatek::hardware::hdmi::V1_0::EDID_t& edid) {
    // TODO implement
    ALOGD("setEDIDInfo...");
#if defined(MTK_DRM_HDMI_SUPPORT)
    mEdid[0] = edid.edid[0];
    mEdid[1] = edid.edid[1];
    mEdid[2] = edid.edid[2];
    mEdid[3] = edid.edid[3];
    ALOGD("setEDIDInfo, %d, %d, %d, %d", mEdid[0],mEdid[1],mEdid[2],mEdid[3]);
    if ((mEdid[0]== HDMI_STATUS_ON)|| (mEdid[0]!=0 || mEdid[1] != 0 || mEdid[2]!=0 || mEdid[3] != 0)){
        setValue(HDMI_ENABLE, 1);
        refreshEdid();
    } else if (mEdid[0]== HDMI_STATUS_OFF){
        setValue(HDMI_ENABLE, 0);
    }
#endif
    return Result::SUCCEED;
}


// Methods from ::vendor::mediatek::hardware::hdmi::V1_3::IMtkHdmiService follow.
Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::setHdmiModeValues(int32_t hdmi_mode, int32_t value) {
    ALOGD("setHdmiModeValues,hdmi_mode:%d,value:%d", hdmi_mode, value);
#if defined(MTK_DRM_HDMI_SUPPORT)
    if (HDMI_HDR_MODE == hdmi_mode){
        setValue(HDMI_ENABLE_HDR, value);
    } else if (HDMI_DV_MODE == hdmi_mode){
        setValue(HDMI_ENABLE_DV, value);
    } else if (DP_RES_AUTO_MODE == hdmi_mode){
        setValue(DP_RES_AUTO, value);
    } else if (DEAL_HDMI_RESOLUTION_MODE == hdmi_mode){
        setValue(DEAL_HDMI_RESOLUTION_PROP, value);
    } else if (DEAL_DP_RESOLUTION_MODE == hdmi_mode){
        setValue(DEAL_DP_RESOLUTION_PROP, value);
    } else if (DP_COLOR_FORMAT_MODE == hdmi_mode){
        if (value == AUTO_COLORSPACE) {
            property_set(DP_COLOR_SPACE,"auto");
        } else if (value == COLOR_SPACE_RGB){
            property_set(DP_COLOR_SPACE,"rgb");
        } else {
            property_set(DP_COLOR_SPACE,"ycbcr");
        }
    } else if (DP_COLOR_DEPTH_MODE == hdmi_mode){
        setValue(DP_DEEP_COLOR, value);
    } else if (DP_HDR_MODE == hdmi_mode){
        setValue(DP_ENABLE_HDR, value);
    } else if (DP_DV_MODE == hdmi_mode){
        setValue(DP_ENABLE_DV, value);
    }

#endif
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::setEDIDInfoMore(const ::vendor::mediatek::hardware::hdmi::V1_3::EDID_More_t& edid) {
    ALOGD("setEDIDInfoMore...");
#if defined(MTK_DRM_HDMI_SUPPORT)
    // use edid.edid[11] to get the tx type
    if ((edid.edid[11] == DRM_MODE_CONNECTOR_DisplayPort) || (edid.edid[11] == DRM_MODE_CONNECTOR_eDP)){//edp/dp path/main path
        memset(&mEdidT[0], 0, sizeof(HDMI_EDID_T));
        mEdidT[0].ui4_ntsc_resolution = mDPInfo[0] = edid.edid[0];//1583627;
        mEdidT[0].ui4_pal_resolution = mDPInfo[1] = edid.edid[1];//0;
        mEdidT[0].ui4_sink_hdmi_4k2kvic = mDPInfo[2] = edid.edid[2];//790;
        mEdidT[0].ui2_sink_colorimetry = mDPInfo[3] = edid.edid[3];//5763;

        mEdidT[0].ui4_ntsc_resolution = edid.edid[0];//1583627;
        ALOGI("setEDIDInfoMore dp, ui4_ntsc_resolution:%d", mEdidT[0].ui4_ntsc_resolution);
        mEdidT[0].ui4_pal_resolution = edid.edid[1];//0;
        ALOGI("setEDIDInfoMore dp, ui4_pal_resolution:%d", mEdidT[0].ui4_pal_resolution);
        mEdidT[0].ui4_sink_hdmi_4k2kvic = edid.edid[2];//790;
        ALOGI("setEDIDInfoMore dp, ui4_sink_hdmi_4k2kvic:%d", mEdidT[0].ui4_sink_hdmi_4k2kvic);
        mEdidT[0].ui2_sink_colorimetry = edid.edid[3];//5763;
        ALOGI("setEDIDInfoMore dp, ui2_sink_colorimetry:%d", mEdidT[0].ui2_sink_colorimetry);
        mEdidT[0].ui1_sink_rgb_color_bit = edid.edid[4];//3;
        ALOGI("setEDIDInfoMore dp, ui1_sink_rgb_color_bit:%d", mEdidT[0].ui1_sink_rgb_color_bit);
        mEdidT[0].ui1_sink_ycbcr_color_bit = edid.edid[5];//3;
        ALOGI("setEDIDInfoMore dp, ui1_sink_ycbcr_color_bit:%d", mEdidT[0].ui1_sink_ycbcr_color_bit);
        mEdidT[0].ui1_sink_dc420_color_bit = edid.edid[6];//3;
        ALOGI("setEDIDInfoMore dp, ui1_sink_dc420_color_bit:%d", mEdidT[0].ui1_sink_dc420_color_bit);
        mEdidT[0].ui1_sink_support_dynamic_hdr = edid.edid[7];//22;
        ALOGI("setEDIDInfoMore dp, ui1_sink_support_dynamic_hdr:%d", mEdidT[0].ui1_sink_support_dynamic_hdr);
        mEdidT[0].ui2_sink_max_tmds_character_rate = edid.edid[8];//600;
        ALOGI("setEDIDInfoMore dp, ui2_sink_max_tmds_character_rate:%d", mEdidT[0].ui2_sink_max_tmds_character_rate);
        mEdidT[0].ui1_sink_max_tmds_clock = edid.edid[9];//300;
        ALOGI("setEDIDInfoMore dp, ui1_sink_max_tmds_clock:%d", mEdidT[0].ui1_sink_max_tmds_clock);

        //0 main path or dp/edp path;1 sub path or hdmi path //0;
        ALOGI("setEDIDInfoMore dp, edid.edid[10]:%d", edid.edid[10]);
        mDpCrtcNum = edid.edid[10];
        //type 11 hdmi type; 14 edp type //14;
        ALOGI("setEDIDInfoMore dp, edid.edid[11]:%d", edid.edid[11]);

        ALOGI("setEDIDInfoMore dp, mDPInfo:%d, %d, %d, %d", mDPInfo[0],mDPInfo[1],mDPInfo[2],mDPInfo[3]);
        if (mDPInfo[0]!=0 || mDPInfo[1] != 0 || mDPInfo[2]!=0 || mDPInfo[3] != 0){
            mDpStateChanged = true;
            setDPResolution(getValue(DP_RESOLUTION,"0xff00"));
        }// else if (mEdid[0]== HDMI_STATUS_OFF){
        //	setValue(HDMI_ENABLE, 0);
        //}
    } else {//hdmi path/sub path (edid.edid[11] should be DRM_MODE_CONNECTOR_HDMIA or DRM_MODE_CONNECTOR_HDMIB)
        memset(&mEdidT[1], 0, sizeof(HDMI_EDID_T));
        mEdidT[1].ui4_ntsc_resolution = mEdid[0] = edid.edid[0];//1583627;
        mEdidT[1].ui4_pal_resolution = mEdid[1] = edid.edid[1];//0;
        mEdidT[1].ui4_sink_hdmi_4k2kvic = mEdid[2] = edid.edid[2];//790;
        mEdidT[1].ui2_sink_colorimetry = mEdid[3] = edid.edid[3];//5763;

        mEdidT[1].ui4_ntsc_resolution = edid.edid[0];//1583627;
        ALOGI("setEDIDInfoMore hdmi, ui4_ntsc_resolution:%d", mEdidT[1].ui4_ntsc_resolution);
        mEdidT[1].ui4_pal_resolution = edid.edid[1];//0;
        ALOGI("setEDIDInfoMore hdmi, ui4_pal_resolution:%d", mEdidT[1].ui4_pal_resolution);
        mEdidT[1].ui4_sink_hdmi_4k2kvic = edid.edid[2];//790;
        ALOGI("setEDIDInfoMore hdmi, ui4_sink_hdmi_4k2kvic:%d", mEdidT[1].ui4_sink_hdmi_4k2kvic);
        mEdidT[1].ui2_sink_colorimetry = edid.edid[3];//5763;
        ALOGI("setEDIDInfoMore hdmi, ui2_sink_colorimetry:%d", mEdidT[1].ui2_sink_colorimetry);
        mEdidT[1].ui1_sink_rgb_color_bit = edid.edid[4];//3;
        ALOGI("setEDIDInfoMore hdmi, ui1_sink_rgb_color_bit:%d", mEdidT[1].ui1_sink_rgb_color_bit);
        mEdidT[1].ui1_sink_ycbcr_color_bit = edid.edid[5];//3;
        ALOGI("setEDIDInfoMore hdmi, ui1_sink_ycbcr_color_bit:%d", mEdidT[1].ui1_sink_ycbcr_color_bit);
        mEdidT[1].ui1_sink_dc420_color_bit = edid.edid[6];//3;
        ALOGI("setEDIDInfoMore hdmi, ui1_sink_dc420_color_bit:%d", mEdidT[1].ui1_sink_dc420_color_bit);
        mEdidT[1].ui1_sink_support_dynamic_hdr = edid.edid[7];//22;
        ALOGI("setEDIDInfoMore hdmi, ui1_sink_support_dynamic_hdr:%d", mEdidT[1].ui1_sink_support_dynamic_hdr);
        mEdidT[1].ui2_sink_max_tmds_character_rate = edid.edid[8];//600;
        ALOGI("setEDIDInfoMore hdmi, ui2_sink_max_tmds_character_rate:%d", mEdidT[1].ui2_sink_max_tmds_character_rate);
        mEdidT[1].ui1_sink_max_tmds_clock = edid.edid[9];//300;
        ALOGI("setEDIDInfoMore hdmi, ui1_sink_max_tmds_clock:%d", mEdidT[1].ui1_sink_max_tmds_clock);

        //0 main path or dp/edp path;1 sub path or hdmi path //1;
        ALOGI("setEDIDInfoMore hdmi, edid.edid[10]:%d", edid.edid[10]);
        mHdmiCrtcNum = edid.edid[10];
        //type 11 hdmi type; 14 edp type //11;
        ALOGI("setEDIDInfoMore hdmi, edid.edid[11]:%d", edid.edid[11]);

        ALOGI("setEDIDInfoMore hdmi, %d, %d, %d, %d", mEdid[0],mEdid[1],mEdid[2],mEdid[3]);
        if ((mEdid[0]== HDMI_STATUS_ON)|| (mEdid[0]!=0 || mEdid[1] != 0 || mEdid[2]!=0 || mEdid[3] != 0)){
            setValue(HDMI_ENABLE, 1);
            mHdmiStateChanged = true;
            setVideoResolution(getValue(HDMI_VIDEO_RESOLUTION,"0xff00"));
        } else if (mEdid[0]== HDMI_STATUS_OFF){
            setValue(HDMI_ENABLE, 0);
        }
    }
#endif
    return Result::SUCCEED;
}

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_allm_mode(int32_t mode) {
    ALOGI("[MtkHdmiService] set_allm_mode mode = %d", mode);
    return Result::SUCCEED;
};

Return<::vendor::mediatek::hardware::hdmi::V1_0::Result> MtkHdmiService::set_qms_mode(int32_t mode) {
    ALOGI("[MtkHdmiService] set_qms_mode mode = %d", mode);
    return Result::SUCCEED;
};

// Methods from ::android::hidl::base::V1_0::IBase follow.

IMtkHdmiService* HIDL_FETCH_IMtkHdmiService(const char* /* name */) {
    return new MtkHdmiService();
}

//
}  // namespace implementation
}  // namespace V1_4
}  // namespace hdmi
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
