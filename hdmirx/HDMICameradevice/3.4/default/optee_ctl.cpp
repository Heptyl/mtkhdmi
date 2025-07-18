#define LOG_TAG "HdmirxCamDevSsn@3.4_OPTEE"

#include "optee_if.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>
#include <cutils/log.h>


#define TZCMD_HDCP_HDMI_GEN_KEY         0
#define TZCMD_HDCP_HDMI_QUERY_KEY       30002
#define TZ_TA_HDMIRX_HDCP_UUID      {0xf50ebeef,0x5a8c,0x490a,{0xa2,0x64,0xc1,0xb8,0x6a,0xa2,0x34,0x78}}

static TEEC_Context teec_cxt_hdmi;
static TEEC_Session teec_sess_hdmi;
static bool isConnected = false;

namespace android {
namespace hardware {
namespace camera {
namespace hdmirx {
namespace optee {

static int optee_hdmi_hdcp_close()
{
    if (isConnected) {
        TEEC_CloseSession(&teec_sess_hdmi);
        TEEC_FinalizeContext(&teec_cxt_hdmi);
        isConnected = false;
    }

    return 0;
}

static bool optee_hdmi_hdcp_call(uint32_t cmd, TEEC_Operation* op)
{
    if (!op) {
        ALOGE("%s, op is nullptr", __FUNCTION__);
        return false;
    }

    uint32_t err_origin;
    TEEC_Result res = TEEC_InvokeCommand(&teec_sess_hdmi, cmd, op, &err_origin);
    if (res != TEEC_SUCCESS) {
        ALOGE("%s,TEEC_InvokeCommand(cmd = %d)failed with code: 0x%08x origin "
            "0x%08x", __FUNCTION__, cmd, res, err_origin);
        if (res == TEEC_ERROR_TARGET_DEAD) {
            optee_hdmi_hdcp_close();
        }
        return false;
    }
    return true;
}

static int optee_hdmi_hdcp_connect()
{
    TEEC_Result res;
    TEEC_UUID uuid = TZ_TA_HDMIRX_HDCP_UUID;
    uint32_t err_origin;

    // initialize context
    res = TEEC_InitializeContext(NULL, &teec_cxt_hdmi);
    if (res != TEEC_SUCCESS) {
        ALOGE("%s, TEEC_InitializeContext failed with code 0x%0x", __FUNCTION__, res);
        return (int)res;
    }

    //open session
    res = TEEC_OpenSession(&teec_cxt_hdmi, &teec_sess_hdmi, &uuid, 
            TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        ALOGE("%s, TEEC_OpenSession failed with code 0x%0x origin 0x%0x",
            __FUNCTION__, res, err_origin);
        return (int)res;
    }

    isConnected = true;
    return 0;
}

static int get_hdmi()
{
    TEEC_Operation op;

    if (!isConnected && optee_hdmi_hdcp_connect()) {
        ALOGE("%s, failed to connect hdmi TA.", __FUNCTION__);
        return -1;
    }

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE,
                                    TEEC_NONE,
                                    TEEC_NONE,
                                    TEEC_NONE);

    if (!optee_hdmi_hdcp_call(TZCMD_HDCP_HDMI_GEN_KEY, &op)) {
        ALOGE("%s, gen key failed.", __FUNCTION__);
        return -2;
    }

    return 0;
}

static int query_hdmi_key()
{
    TEEC_Operation op;

    if (!isConnected && optee_hdmi_hdcp_connect()) {
        ALOGE("%s, failed to connect hdmi TA.", __FUNCTION__);
        return -1;
    }

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                                    TEEC_NONE,
                                    TEEC_NONE,
                                    TEEC_NONE);
    op.params[0].value.a = 1;

    if (!optee_hdmi_hdcp_call(TZCMD_HDCP_HDMI_QUERY_KEY, &op)) {
        ALOGE("%s, query hdmi key failed.", __FUNCTION__);
        return -2;
    }

    return 0;
}

int query_hdmi_hdcp_key()
{
    int ret = get_hdmi();
    if (ret != 0) {
        ALOGE("%s, get hdmi failed.", __FUNCTION__);
        goto end;
    }

    ret = query_hdmi_key();
    if (ret != 0) {
        ALOGE("%s, query hdmi key failed.", __FUNCTION__);
        goto end;
    }
end:
    optee_hdmi_hdcp_close();

    return ret;
}


}
}
}
}
}

