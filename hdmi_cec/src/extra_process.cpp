#define DEBUG_LOG_TAG "HWCEC_MEP"


#include <hardware/hdmi_cec.h>

#include <cutils/properties.h>
#include <utils/threads.h>

#include "utils/cec_debug.h"
#include "extra_process.h"
#include "cec_utils.h"
#include "hwcec.h"

MsgExtraProcessor::MsgExtraProcessor()
{
    mInitialized = false;
    mReceiveFd = 0;
    mSendFd = 0;
    HWCEC_LOGI("MsgExtraProcessor\n");
}

MsgExtraProcessor::~MsgExtraProcessor()
{
    HWCEC_LOGI("~MsgExtraProcessor");
}


void MsgExtraProcessor::recvCecEvent(const hdmi_event_t event)
{
    //HWCEC_LOGI("recvCecEvent");
    //mLooper->sendMessage(mCecEventHandler, Message(MSG_HDMI_EVENT));
    if(mInitialized) {
        ssize_t nWritten = ::write(mSendFd, &event, sizeof(hdmi_event_t));
    }
}

void MsgExtraProcessor::sendCecEvent(const int opCode)
{
    //HWCEC_LOGI("sendCecEvent");
    if(mInitialized) {
        mLooper->sendMessage(mCecEventHandler, Message(opCode));
    }
    //ssize_t nWritten = ::write(mSendFd, &event, sizeof(hdmi_event_t));
}

void MsgExtraProcessor::onFirstRef()
{

    HWCEC_LOGI("MsgExtraProcessor,onFirstRef\n");

    int fds[2];
    int ret = ::pipe(fds);
    if (ret < 0) {
        HWCEC_LOGE("Failed to create pipe\n");
        return;
    }

    mReceiveFd = fds[0];
    mSendFd = fds[1];

    mCecEventCallback = new CecEventCallback();
    mCecEventCallback->setFd(fds);

    mLooper = Looper::prepare(false /*allowNonCallbacks*/);

    ret = mLooper->addFd(mReceiveFd,
                            Looper::POLL_CALLBACK,
                            Looper::EVENT_INPUT,
                            mCecEventCallback,
                            nullptr /*data*/);

    if(ret != 1) {
        HWCEC_LOGE("Failed to add binder FD to Looper");
        return;
    }

    mCecEventHandler = new CecEventHandler();
    mInitialized = true;
}


bool MsgExtraProcessor::threadLoop()
{
    bool pollError = false;
    HWCEC_LOGI("threadLoop");
    do {
        //printf("about to poll...\n");
        int32_t ret = mLooper->pollOnce(-1);
        switch (ret) {
            case Looper::POLL_WAKE:
                //("ALOOPER_POLL_WAKE\n");
                break;
            case Looper::POLL_CALLBACK:
                //("ALOOPER_POLL_CALLBACK\n");
                break;
            case Looper::POLL_TIMEOUT:
                // timeout (should not happen)
                HWCEC_LOGE("Looper::POLL_TIMEOUT");
                break;
            case Looper::POLL_ERROR:
                HWCEC_LOGE("ALOOPER_POLL_TIMEOUT\n");
                pollError = true;
                break;
            default:
                // should not happen
                HWCEC_LOGE("Looper::pollOnce() returned unknown status %d", ret);
                break;
        }
    } while (!pollError);

      return true;
}

CecEventHandler::CecEventHandler()
{
    HWCEC_LOGI("CecEventHandler\n");
}

CecEventHandler::~CecEventHandler()
{
    HWCEC_LOGI("~CecEventHandler");
}

void CecEventHandler::handleMessage(const Message& message)
{
    switch (message.what) {
    case CEC_OP_OTP_IMAGE_VIEW_ON:
      property_set("vendor.mtk.nfxprop.activeCecState", "active");
      HWCEC_LOGI("handleMessage CEC_OP_ACTIVE_SOURCE ");
      break;
    default:
      break;
    }
}

CecEventCallback::CecEventCallback()
{
    HWCEC_LOGI("CecEventCallback\n");
    mReceiveFd = 0;
    mSendFd = 0;
}

CecEventCallback::~CecEventCallback()
{
    HWCEC_LOGI("~CecEventCallback");
}

int CecEventCallback::handleEvent(int /* fd */, int /* events */, void* /* data */)
{
    //HWCEC_LOGI("handleEvent");

    //char *buf = (char *)malloc(sizeof(hdmi_event_t));
    hdmi_event_t event = { };

    ssize_t nRead = ::read(mReceiveFd, &event, sizeof(hdmi_event_t));

    if(nRead < sizeof(hdmi_event_t)) {
        HWCEC_LOGE("event read error. \n");
        //return 1 to continue receiving callback
        return 1;
    }

    //memcpy(&event, buf, sizeof(hdmi_event_t));

    //HWCEC_LOGD("CEC_RECEIVE, init:%d, dest:%d, opcode: 0x%x,size:%d, param[0]=0x%x, param_length:%d\n",
    //            event.cec.initiator,
    //            event.cec.destination,
    //            event.cec.body[0],
    //            event.cec.length,
    //            event.cec.body[1],
    //            event.cec.length - 1);

    int opCode = 0;
    if(event.cec.length > 0) {
        opCode= event.cec.body[0];

        switch(opCode) {
        case CEC_OP_ACTIVE_SOURCE:
            handleActiveSource(event);
            break;
        case CEC_OP_STANDBY:
            handleStandby(event);
            break;
        case CEC_OP_RC_ROUTING_CHANGE:
            handleRoutingChange(event);
            break;
        case CEC_OP_RC_SET_STREAM_PATH:
            handleSetStreamPath(event);
            break;
        default:
            //HWCEC_LOGI("no action");
            break;
        }
    }else {
        HWCEC_LOGE("event cec length <= 0 \n");
    }

    //return 1 to continue receiving callback
    return 1;
}

int CecEventCallback::handleActiveSource(const hdmi_event_t event)
{
    uint16_t addr_local = 0xFFFF, addr_new = 0xFFFF;

    HWCECMediator::getInstance().get_physical_address(&addr_local);

    if (event.cec.length > 2) {
        addr_new = (event.cec.body[1] &0xFF) << 8 | (event.cec.body[2] & 0xFF);
        //HWCEC_LOGD("handleActiveSource, param[0]=0x%x, param[1]=0x%x \n", event.cec.body[1], event.cec.body[2]);
    }

    HWCEC_LOGI("handleActiveSource, addr:0x%x, ePa:0x%x \n", addr_local, addr_new);

    if(addr_local == 0xFFFF || addr_new == 0xFFFF) {
        HWCEC_LOGE("handleActiveSource addr error \n");
        return -1;
    }

    if(addr_local != addr_new) {
        setNRDPVideoPlatformCaps(false);
    } else {
        setNRDPVideoPlatformCaps(true);
    }

    return 0;
}
int CecEventCallback::handleStandby(const hdmi_event_t event)
{
    HWCEC_LOGI("handleStandby \n");
    setNRDPVideoPlatformCaps(false);
    return 0;
}

int CecEventCallback::handleRoutingChange(const hdmi_event_t event)
{
    uint16_t addr_local = 0xFFFF, addr_orig = 0xFFFF, addr_new = 0xFFFF;

    HWCECMediator::getInstance().get_physical_address(&addr_local);

    if (event.cec.length > 4) {
        addr_orig = (event.cec.body[1] &0xFF) << 8 | (event.cec.body[2] & 0xFF);
        addr_new = (event.cec.body[3] &0xFF) << 8 | (event.cec.body[4] & 0xFF);
        //HWCEC_LOGD("handleActiveSource, param[0]=0x%x, param[1]=0x%x, param[2]=0x%x, param[3]=0x%x \n",
        //           event.cec.body[1], event.cec.body[2], event.cec.body[3], event.cec.body[4]);
    }

    HWCEC_LOGI("handleRoutingChange, addr_local=0x%x, addr_orig=0x%x, addr_new=0x%x \n", addr_local, addr_orig, addr_new);

    if(addr_local == 0xFFFF || addr_new == 0xFFFF) {
        HWCEC_LOGE("handleRoutingChange addr error \n");
        return -1;
    }

    if(addr_new != addr_local) {
        setNRDPVideoPlatformCaps(false);
    } else {
        setNRDPVideoPlatformCaps(true);
    }

    return 0;
}

int CecEventCallback::handleSetStreamPath(const hdmi_event_t event)
{
    uint16_t addr_local = 0xFFFF,addr_new = 0xFFFF;

    HWCECMediator::getInstance().get_physical_address(&addr_local);

    if (event.cec.length > 2) {
        addr_new = (event.cec.body[1] &0xFF) << 8 | (event.cec.body[2] & 0xFF);
        //HWCEC_LOGD("handleActiveSource, param[0]=0x%x, param[1]=0x%x\n",
        //           event.cec.body[1], event.cec.body[2]);
    }

    HWCEC_LOGI("handleSetStreamPath, addr_local=0x%x, addr_new=0x%x \n", addr_local, addr_new);

    if(addr_local == 0xFFFF || addr_new == 0xFFFF) {
        HWCEC_LOGE("handleSetStreamPath addr error \n");
        return -1;
    }

    if(addr_new != addr_local) {
        setNRDPVideoPlatformCaps(false);
    } else {
        setNRDPVideoPlatformCaps(true);
    }

    return 0;
}

int CecEventCallback::setNRDPVideoPlatformCaps(bool active)
{
    // NFXService transform it
    //settings get global nrdp_video_platform_capabilities {"activeCecState" : "active"}

    if(active) {
        property_set("vendor.mtk.nfxprop.activeCecState", "active");
    } else {
        property_set("vendor.mtk.nfxprop.activeCecState", "inactive");
    }
    return 0;
}




