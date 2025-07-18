#define DEBUG_LOG_TAG "HWCEC"

#include <cutils/properties.h>
#include <cstring>

#include "utils/cec_debug.h"

#include "hwcec.h"
#include "cec_event.h"
//#include "cec_manager.h"

#include <pthread.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#include <dirent.h>
#include <unistd.h>

#ifdef MTK_CEC_EXTRA_PROCESS
#include "extra_process.h"
sp<MsgExtraProcessor> mMsgExtraProcessor = NULL;
#endif

using std::stoi;
using std::string;

//#include "linux/hdmitx.h"
// ---------------------------------------------------------------------------

DispatcherThread::DispatcherThread(HWCECMediator* mediator)
    : m_mediator(mediator)
{
    HWCEC_LOGD("DispatcherThread\n");
}

DispatcherThread::~DispatcherThread()
{
    HWCEC_LOGD("~DispatcherThread\n");
}

int DispatcherThread::handlecallback(int cbtype)
{
   int ret = CEC_RESULT_SUCCESS;
   if (m_mediator == NULL){
       HWCEC_LOGE("m_mediator is NULL!!!\n");
       return CEC_RESULT_FAIL;
   }
   if(CEC_PA == cbtype)
   {
       m_mediator->mConnect = HDMI_CONNECTED;
       uint16_t addr = 0xFFFF;
       hdmi_event_t mevent;
       uint16_t pa_temp = m_mediator->mPrePA; //similar to pre Connect
       ret = m_mediator->get_physical_address(&addr);
       //if pa is not same with before, notify to fm to handle.
       if(pa_temp != m_mediator->mPA)
       {
            mevent.type = HDMI_EVENT_HOT_PLUG;
            mevent.dev = &(m_mediator->mDevice->base);
            mevent.hotplug.connected = true;
            mevent.hotplug.port_id = 1;
            if((m_mediator != NULL) && (m_mediator->mCbf != NULL)) {
               HWCEC_LOGD("CEC_PA, callback to controller\n");
               m_mediator->mCbf(&mevent, m_mediator->mcallbacker);

               //if device not standby, wake up tv
               if(m_mediator->mAutoOtp != 0 && m_mediator->mOptionSystemCecControl) {
                   HWCEC_LOGI("CEC_PA, send ImageViewOn to wakeup TV\n");
                   m_mediator->sendImageViewOnLock();
                   m_mediator->sendActiveSourceLock();
               }
            }
       } else if((pa_temp == m_mediator->mPA) && (m_mediator->mPA == 0xFFFF))
       {
       //send image view on to wait TV wakeup
       //work around for sumsung/Toshiba/LG TV
           HWCEC_LOGI("CEC_PA,invalid, send ImageViewOn to wakeup TV\n");
           m_mediator->sendImageViewOnLock();
       }
        m_mediator->mPrePA = m_mediator->mPA;
   }
   else if(CEC_PLUG_OUT == cbtype)
   {
       m_mediator->mConnect = HDMI_NOT_CONNECTED;
       hdmi_event_t mevent;
       mevent.type = HDMI_EVENT_HOT_PLUG;
       mevent.dev = &(m_mediator->mDevice->base);
       mevent.hotplug.connected = false;
       mevent.hotplug.port_id = 1;
       if((m_mediator != NULL) && m_mediator->mCbf) {
           HWCEC_LOGE("CEC_PLUG_OUT, callback to controller\n");
           m_mediator->mCbf(&mevent, m_mediator->mcallbacker);
       }
       //reset mPA to invalid, so that next valid PA will notify to frameworks
       m_mediator->mPA = 0xFFFF;
        m_mediator->mPrePA = 0xFFFF;
   }
   else if(CEC_TX_STATUS == cbtype)
   {
   ret = m_mediator->get_txstatus();
   }
   else if(CEC_RX_CMD == cbtype)
   {
    ret = m_mediator->get_rxcmd();
   }
   else
   {
       HWCEC_LOGE("invalid cbtype=%d\n", cbtype);
       ret = CEC_RESULT_FAIL;
   }
    return ret;
}

pthread_t thread;

void *HWCECMediator::msgEventThread(CecDevice *cecdevice) 
{
    int ret;
    struct pollfd ufds[3] = {
        { cecdevice->mCecFd, POLLIN, 0 },
        { cecdevice->mCecFd, POLLERR, 0 },
        { cecdevice->mEventThreadExitFd, POLLIN, 0 },
    };
    HWCEC_LOGD("%s start!!\n", __func__);

    while (1) {
        ufds[0].revents = 0;
        ufds[1].revents = 0;
        ufds[2].revents = 0;
        HWCEC_LOGD("%s poll \n", __func__);

        ret = poll(ufds, 3, -1);

        if (ret <= 0) {
            HWCEC_LOGD("poll ret = %d\n", ret);
            continue;
        }

        if (ufds[2].revents == POLLIN) {  /* Exit */
            HWCEC_LOGD("%s: msg event thread exit \n", cecdevice->mCapsName.c_str());
            break;
        }

        if (ufds[1].revents == POLLERR) { /* CEC Event */
            //To-Do:
            HWCEC_LOGD("%s: POLLERR\n", cecdevice->mCapsName.c_str());
        }

        HWCEC_LOGD("ufds[0].revents = %x\n", ufds[0].revents);
        if (ufds[0].revents == POLLIN) { /* CEC Driver */
            struct cec_msg msg = { };
            hdmi_event_t event = { };

            ret = ioctl(cecdevice->mCecFd, CEC_RECEIVE, &msg);
            //HWCEC_LOGD("CEC_RECEIVE ret = %d\n", ret);
            if (ret) {
                HWCEC_LOGE("%s: CEC_RECEIVE error (%m)\n", __func__);
                continue;
            }

            if (msg.rx_status != CEC_RX_STATUS_OK) {
                HWCEC_LOGD("%s: rx_status=%d\n", __func__, msg.rx_status);
                continue;
            }

            if (mCbf != NULL) {
                event.type = HDMI_EVENT_CEC_MESSAGE;
                event.dev = &(mDevice->base);
                event.cec.initiator = (cec_logical_address_t)(msg.msg[0] >> 4);
                event.cec.destination = (cec_logical_address_t)(msg.msg[0] & 0xf);
                event.cec.length = msg.len - 1;
                memcpy(event.cec.body, &msg.msg[1], msg.len - 1);
                HWCEC_LOGD("CEC_RECEIVE, init:%d, dest:%d, opcode: 0x%x,size:%d, param[0]=0x%x, param_length:%d\n",
                    event.cec.initiator, event.cec.destination,
                    msg.msg[1],event.cec.length,event.cec.body[1], event.cec.length - 1);

                mCbf(&event, mcallbacker);
#ifdef MTK_CEC_EXTRA_PROCESS
                if(mMsgExtraProcessor != NULL) {
                    mMsgExtraProcessor->recvCecEvent(event);
                }
#endif
            } else {
                HWCEC_LOGE("no event callback for msg\n");
            }
        }
    }

    HWCEC_LOGD("%s exit!", __func__);
    return NULL;
}

// ---------------------------------------------------------------------------
#define HDMI_DEV_NODE "/dev/cec0"

//sp<DispatcherThread> m_dispatcher_thread = NULL;


ANDROID_SINGLETON_STATIC_INSTANCE(HWCECMediator);

HWCECMediator::HWCECMediator()
{
    mPA = 0xFFFF;
    mPrePA = 0xFFFF;
    mLA = CEC_ADDR_UNREGISTERED;
    mCECVersion = property_get_int32("ro.vendor.hdmicec.version", CECVersion);
    mMsgSending = false;
    mMsgResult = HDMI_RESULT_NACK;
    mCurrCode = CEC_MESSAGE_FEATURE_ABORT;
    mCECVendor_id = CECVendor;
    mDevice = NULL;
    mCecServiceControl = false;
    mConnect = HDMI_NOT_CONNECTED;
    mCecEnable = true;
    mportinfo = NULL;
    mOptionSystemCecControl = false;
    mAutoOtp = 0;
    m_cec_fd = -1;
    mCurrentActiveDevice = NULL;

    mCecDeviceType = property_get_int32("ro.hdmi.device_type", 0);

    mHasDpCec = property_get_bool("ro.vendor.hdmicec.dp", false);

    mSupportArc = property_get_bool("ro.vendor.hdmicec.arc", false);

    const char* cecFilename = "/dev/cec";

    for(int i = MIN_DEVICE_ID; i <= MAX_DEVICE_ID; i++) {
        string deviceName = cecFilename + std::to_string(i);;
        if(access(deviceName.c_str(), F_OK) == 0) {
            CecDevice * cecDevice = new CecDevice(i);
            cecDevice->mType = mCecDeviceType;

            if(mCecDeviceType == CEC_DEVICE_TV) {
                cecDevice->mUeventNode = "change@/devices/virtual/hdmirxswitch/hdmi";
                cecDevice->mUeventCnnProp = "SWITCH_STATE=";
                cecDevice->mConnectStateNode = "/sys/class/hdmirxswitch/hdmi/state";;
            }

            int result = cecDevice->init(deviceName.c_str());
            if (result != 0) {
                HWCEC_LOGE("%s initialization failed \n", deviceName.c_str());
                delete cecDevice;
                continue;
            }

            HWCEC_LOGI("leig# add cec device %s, mCecDeviceType %d\n", deviceName.c_str(),mCecDeviceType);
            std::thread msgEventThread(&HWCECMediator::msgEventThread, this, cecDevice);
            mMsgEventThreads.push_back(std::move(msgEventThread));

            mCecDevices.push_back(std::move(cecDevice));
        }
    }
    
    if (mCecDevices.empty()) {
        HWCEC_LOGE("no cec device \n");
        goto fail;
    }
    
    mUeventThread = std::thread (&HWCECMediator::ueventThread, this, nullptr);

    mCurrentActiveDevice = mCecDevices[0];
    m_cec_fd = mCecDevices[0]->mCecFd;


    if(cec_init() != 0){
        goto fail;
    }

    mportinfo = (hdmi_port_info *)malloc(sizeof(hdmi_port_info));
    if(mportinfo != NULL) {
        memset(mportinfo, 0, sizeof(hdmi_port_info));

        mportinfo->type =  mCecDeviceType == CEC_DEVICE_TV ? HDMI_INPUT : HDMI_OUTPUT;
        mportinfo->port_id = 1;
        mportinfo->cec_supported = 1;
        mportinfo->arc_supported = mSupportArc? 1 : 0;
        mportinfo->physical_address = mCecDeviceType == CEC_DEVICE_TV ? CEC_TV_PORT_1_PA : mPA;
    }

    if (getHdmiCecState(mCurrentActiveDevice) > 0 ) {
        mConnect = HDMI_CONNECTED;
    }


#ifdef MTK_CEC_EXTRA_PROCESS
    mMsgExtraProcessor = new MsgExtraProcessor();
    if(mMsgExtraProcessor != NULL) {
        mMsgExtraProcessor->run("MsgExtraProcessor");
    }
#endif
    HWCEC_LOGD("HWCECMediator\n");
    return;
fail:
    hdmicec_close();
    HWCEC_LOGE("HWCECMediator init error\n");
    return;

}

void print_laddrs_info(const struct cec_log_addrs *laddrs) {
    HWCEC_LOGD("--- CEC Log Addrs 信息 ---\n");

    HWCEC_LOGD("  log_addr: ");
    for (int i = 0; i < CEC_MAX_LOG_ADDRS; i++) {
        HWCEC_LOGD("0x%02x ", laddrs->log_addr[i]);
    }
    HWCEC_LOGD("\n");

    HWCEC_LOGD("  log_addr_mask: 0x%04x\n", laddrs->log_addr_mask);
    HWCEC_LOGD("  cec_version: 0x%02x\n", laddrs->cec_version);
    HWCEC_LOGD("  num_log_addrs: %u\n", laddrs->num_log_addrs);
    HWCEC_LOGD("  vendor_id: 0x%08x\n", laddrs->vendor_id);
    HWCEC_LOGD("  flags: 0x%08x\n", laddrs->flags);

    // OSD Name 可能不是以 \0 结尾，所以我们限制打印长度
    HWCEC_LOGD("  osd_name: \"%.*s\"\n", (int)sizeof(laddrs->osd_name), laddrs->osd_name);

    HWCEC_LOGD("  primary_device_type: ");
    for (int i = 0; i < CEC_MAX_LOG_ADDRS; i++) {
        HWCEC_LOGD("0x%02x ", laddrs->primary_device_type[i]);
    }
    HWCEC_LOGD("\n");

    HWCEC_LOGD("  log_addr_type: ");
    for (int i = 0; i < CEC_MAX_LOG_ADDRS; i++) {
        HWCEC_LOGD("0x%02x ", laddrs->log_addr_type[i]);
    }
    HWCEC_LOGD("\n");


}


int HWCECMediator::cec_init(){
    cec_log_addrs laddrs = {};
    cec_caps caps = {};
    uint32_t mode;
    uint16_t pa = CEC_TV_PA;
    int ret;

    // Ensure the CEC device supports required capabilities
    ret = ioctl(m_cec_fd, CEC_ADAP_G_CAPS, &caps);
    HWCEC_LOGD("%s: CEC_ADAP_G_CAPS ret:%d\n", __func__, ret);
    HWCEC_LOGD("caps.name %s, caps.driver %s\n", caps.name, caps.driver);
    if (ret)
        return ret;

    if (!(caps.capabilities & (CEC_CAP_LOG_ADDRS |
                    CEC_CAP_TRANSMIT |
                    CEC_CAP_PASSTHROUGH))) {
        HWCEC_LOGD("%s: wrong cec adapter capabilities %x\n",
                __func__, caps.capabilities);
        return -1;
    }

    // This is an exclusive follower, in addition put the CEC device into passthrough mode
    mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER_PASSTHRU;
    ret = ioctl(m_cec_fd, CEC_S_MODE, &mode);
    HWCEC_LOGD("leig# %s: CEC_S_MODE ret:%d, mode =%d\n", __func__, ret, mode);
    if (ret)
        return ret;

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.vendor.hdmicec.otp.hal", value, "0");
    mAutoOtp = atoi(value);

    if (mCecDeviceType == CEC_DEVICE_TV) {
        ret = ioctl(m_cec_fd, CEC_ADAP_S_PHYS_ADDR, &pa);
        HWCEC_LOGI("leig, set PA address  =0x%4X \n",pa);
        if(ret) {
            HWCEC_LOGI("set PA address failed \n");
        }
        mPA = 0;
    }

    memset(&laddrs, 0, sizeof(laddrs));
    ret = ioctl(m_cec_fd, CEC_ADAP_S_LOG_ADDRS, &laddrs);
    HWCEC_LOGD("%s: CEC_ADAP_S_LOG_ADDRS ret:%d\n", __func__, ret);
    if (ret)
        return ret;
    
    print_laddrs_info(&laddrs);
    HWCEC_LOGD("%s: initialized CEC controller\n", __func__);

    return ret;

}

int HWCECMediator::hdmicec_close(){

    ALOGD("%s\n", __func__);

    return 0;
}


HWCECMediator::~HWCECMediator()
{
    //m_dispatcher_thread = NULL;
    uint64_t tmp = 1;

    if(mportinfo != NULL) {
        free(mportinfo);
        mportinfo = NULL;
    }

    if (mUeventSocket >0) {
        close(mUeventSocket);
        mUeventSocket = -1;
    }

    if (mUeventExitFd > 0) {
        write(mUeventExitFd, &tmp, sizeof(tmp));
        mUeventExitFd = -1;
        mUeventThread.join();

    }

    for (int i = 0; i < mCecDevices.size(); i++) {
        delete mCecDevices[i];
    }

    for (std::thread& eventThread : mMsgEventThreads) {
        if (eventThread.joinable()) {
            eventThread.join();
        }
    }

    mCecDevices.clear();
    mMsgEventThreads.clear();

#ifdef MTK_CEC_EXTRA_PROCESS
    mMsgExtraProcessor = NULL;
#endif
    HWCEC_LOGD("~HWCECMediator\n");
}

void HWCECMediator::setSending(int MsgSending)
{
  mMsgSending = MsgSending;
  //HWCEC_LOGE("setMsgSending , mMsgSending:%d, \n", mMsgSending);
}

bool  HWCECMediator::getSending(int * MsgResult)
{

  *MsgResult = mMsgResult;
  //HWCEC_LOGE("getSending , mMsgSending:%d, mMsgResult:%d, \n",mMsgSending, mMsgResult);
  return mMsgSending;
}

bool  HWCECMediator::isReady()
{
    bool misready = true;
    /*if(mPA == 0xFFFF)
    {
        misready = false;
    }
    else if(mConnect == HDMI_NOT_CONNECTED)
    {
        misready = false;
    }
    else */if(mCecEnable == false)
    {
        misready = false;
    }
  //HWCEC_LOGE("getSending , mMsgSending:%d, mMsgResult:%d, \n",mMsgSending, mMsgResult);
  return misready ;
}

void HWCECMediator::device_open(hwcec_private_device_t* device)
{
    mDevice = device;

    HWCEC_LOGD("device_open\n");
}

void HWCECMediator::device_close(hwcec_private_device_t* device)
{
    HWCEC_LOGD("device_close\n");
}

int HWCECMediator::get_txstatus()
{
    if (m_cec_fd <= 0)  return CEC_RESULT_FAIL ;
    //get AckInfo from drive, then drive will remove this message from queue
    //APK_CEC_ACK_INFO mAckInfo;
    int ret = -1;
    //ret = ioctl(m_cec_fd, MTK_HDMI_GET_CECSTS, &mAckInfo);
    if (ret < 0) {
        HWCEC_LOGE("get_txstatus fail. opcode: 0x%x, errno: %d", mCurrCode, errno);
     }

    //unsigned int curcode = mAckInfo.pv_tag;
    //HWCEC_LOGD("get_txstatus,  cond=%d curcode= 0x%x\n",
    //    mAckInfo.e_ack_cond, curcode);

    //if(curcode == mCurrCode)
    {
       setSending(false);
    //   mMsgResult = mAckInfo.e_ack_cond;
    }
    return CEC_RESULT_SUCCESS;
}

int HWCECMediator::get_rxcmd()
{
    if (m_cec_fd <= 0)  return CEC_RESULT_FAIL ;

    //CEC_FRAME_DESCRIPTION_IO mcecCmd;
    cec_msg msg = {};
	hdmi_event_t event = {};
    int ret = -1;

    //ret = ioctl(m_cec_fd, MTK_HDMI_GET_CECCMD, &mcecCmd);
    ret = ioctl(m_cec_fd, CEC_RECEIVE, &msg);
    if (ret < 0) {
       HWCEC_LOGE("get_rxcmd fail. errno: %d", errno);
    }

    if(isReady() == false) {
        HWCEC_LOGE("CEC not ready, get_rxcmd fail, opcode: 0x%x\n", msg.msg[1]);
        return CEC_RESULT_FAIL;
    }
    //callback to service, java will handle this message.
    if (mCbf){
		event.type = HDMI_EVENT_CEC_MESSAGE;
		event.dev = &(mDevice->base);
		event.cec.initiator = (cec_logical_address_t)(msg.msg[0] >> 4);
		event.cec.destination = (cec_logical_address_t)(msg.msg[0] & 0xf);
		event.cec.length = msg.len;
		memcpy(event.cec.body, &msg.msg[1], msg.len - 1);
	
		mCbf(&event, mcallbacker);

	}else {
        HWCEC_LOGD("mcbf is null, need after do register callback\n");
        return CEC_RESULT_FAIL;
    }
   //print this message out
    HWCEC_LOGD("CEC_RX_CMD, init:%d, dest:%d, opcode: 0x%x,size:%d, param[0]=0x%x, param_length:%d\n",
        event.cec.initiator, event.cec.destination,
        msg.msg[1],event.cec.length,event.cec.body[1], event.cec.length);
    return CEC_RESULT_SUCCESS;
}

int HWCECMediator::add_logical_address(cec_logical_address_t addr)
{
    AutoMutex l(m_lock);
	if (m_cec_fd <= 0)  return CEC_RESULT_FAIL ;
	//CEC_DRV_ADDR_CFG CECAddr;
	cec_log_addrs laddrs;
    unsigned int la_type = CEC_LOG_ADDR_TYPE_UNREGISTERED;
    unsigned int all_dev_types = 0;
    unsigned int prim_type = 0xff;
	int ret = -1;
	//CECAddr.ui1_la_num = 1;
	//CECAddr.ui2_pa = mPA;
	//CECAddr.e_la[0] = addr;
	//HWCEC_LOGD("AddLA,  LA:%d, PA:0x%4X\n",CECAddr.e_la[0],CECAddr.ui2_pa);

    HWCEC_LOGD("AddLA,  addr:%d, PA:0x%4X\n", addr, mPA);

	if (addr >= CEC_ADDR_BROADCAST){
		HWCEC_LOGE("unsupport addr:%d !!!\n", addr);
		return -1;
	}

    ret = ioctl(m_cec_fd, CEC_ADAP_G_LOG_ADDRS, &laddrs);
    if (ret){
        HWCEC_LOGE("get logical address fail: %s\n", strerror(errno));
    }

    HWCEC_LOGD("AddLA,  pre addr:%d, PA:0x%4X\n", laddrs.log_addr[0], mPA);

    if(addr == laddrs.log_addr[0]) {
        HWCEC_LOGD("has same la, not set again:%d, PA:0x%4X\n", laddrs.log_addr[0], mPA);
        return CEC_RESULT_SUCCESS;
    }

    laddrs.cec_version = mCECVersion;
    laddrs.vendor_id = mCECVendor_id;
	
    switch (addr) {
        case CEC_ADDR_TV:
            prim_type = CEC_OP_PRIM_DEVTYPE_TV;
            la_type = CEC_LOG_ADDR_TYPE_TV;
            all_dev_types = CEC_OP_ALL_DEVTYPE_TV;
            break;
        case CEC_ADDR_RECORDER_1:
        case CEC_ADDR_RECORDER_2:
        case CEC_ADDR_RECORDER_3:
            prim_type = CEC_OP_PRIM_DEVTYPE_RECORD;
            la_type = CEC_LOG_ADDR_TYPE_RECORD;
            all_dev_types = CEC_OP_ALL_DEVTYPE_RECORD;
            break;
        case CEC_ADDR_TUNER_1:
        case CEC_ADDR_TUNER_2:
        case CEC_ADDR_TUNER_3:
        case CEC_ADDR_TUNER_4:
            prim_type = CEC_OP_PRIM_DEVTYPE_TUNER;
            la_type = CEC_LOG_ADDR_TYPE_TUNER;
            all_dev_types = CEC_OP_ALL_DEVTYPE_TUNER;
            break;
        case CEC_ADDR_PLAYBACK_1:
        case CEC_ADDR_PLAYBACK_2:
        case CEC_ADDR_PLAYBACK_3:
            prim_type = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
            la_type = CEC_LOG_ADDR_TYPE_PLAYBACK;
            all_dev_types = CEC_OP_ALL_DEVTYPE_PLAYBACK;
            laddrs.flags = CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU;
            break;
        case CEC_ADDR_AUDIO_SYSTEM:
            prim_type = CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM;
            la_type = CEC_LOG_ADDR_TYPE_AUDIOSYSTEM;
            all_dev_types = CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM;
            break;
        case CEC_ADDR_FREE_USE:
            prim_type = CEC_OP_PRIM_DEVTYPE_PROCESSOR;
            la_type = CEC_LOG_ADDR_TYPE_SPECIFIC;
            all_dev_types = CEC_OP_ALL_DEVTYPE_SWITCH;
            break;
        case CEC_ADDR_RESERVED_1:
        case CEC_ADDR_RESERVED_2:
        case CEC_ADDR_UNREGISTERED:
            laddrs.flags = CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK;
            break;
    }

    int retry = 3;
    do {
        laddrs.num_log_addrs = 1;
        laddrs.log_addr[0] = addr;
        laddrs.log_addr_type[0] = la_type;
        laddrs.primary_device_type[0] = prim_type;
        laddrs.all_device_types[0] = all_dev_types;
        laddrs.features[0][0] = 0;
        laddrs.features[0][1] = 0;

        ret = ioctl(m_cec_fd, CEC_ADAP_S_LOG_ADDRS, &laddrs);
        if ((ret == 0) && (laddrs.log_addr[0] == addr)) {
            break;
        }
        HWCEC_LOGE("AddLA fail. addr:%d:%d, ret= %d, errno: %s:%d", addr, laddrs.log_addr[0], ret, strerror(errno), errno);
        usleep(200000);
    } while(retry-- > 0);

    HWCEC_LOGD("AddLA,  LA:%d, PA:0x%4X\n", laddrs.log_addr[0], mPA);
    //after set to drive, save this value for internal use
    mLA = addr;
    return CEC_RESULT_SUCCESS;

}

void HWCECMediator::clear_logical_address()
{
    AutoMutex l(m_lock);
    cec_log_addrs laddrs = {};
    int ret;
    HWCEC_LOGD("Clear LA\n");

    memset(&laddrs, 0, sizeof(laddrs));
    ret = ioctl(m_cec_fd, CEC_ADAP_S_LOG_ADDRS, &laddrs);
    //HWCEC_LOGD("%s: CEC_ADAP_S_LOG_ADDRS ret:%d\n", __func__, ret);
    if (ret)
        HWCEC_LOGD("%s: set la fail ret:%d\n", __func__, ret);

    mLA =  CEC_ADDR_UNREGISTERED;
}

int HWCECMediator::get_physical_address_lock(uint16_t* addr)
{
    AutoMutex l(m_lock);
    if (addr == NULL) {
        return CEC_RESULT_FAIL;
    }
    if (mCecDeviceType == CEC_DEVICE_TV) {
        *addr = CEC_TV_PA;
        return CEC_RESULT_SUCCESS;
    }

    return get_physical_address(addr);
}
int HWCECMediator::get_physical_address(uint16_t* addr)
{
    if (m_cec_fd <= 0)  return CEC_RESULT_FAIL ;
    //reset mPA to invalid value first
    mPA = 0xFFFF;
    //CEC_ADDRESS_IO cecAddr;
    //int ret = -1;
    HWCEC_LOGD("before get_physical_address.addr:0x%4X\n", *addr);
	int ret = ioctl(m_cec_fd, CEC_ADAP_G_PHYS_ADDR, addr);
    //ret = ioctl(m_cec_fd, MTK_HDMI_GET_CECADDR, &cecAddr);
    if (ret < 0) {
        HWCEC_LOGE("GetPA fail. errno: %d", errno);
     }
    if(*addr != 0xFFFF)
    {
        //after get this valid value from drive, save it for internal use
        //HWCEC_LOGE("get_physical_address. valid ui2_pa = 0x%4X\n", cecAddr.ui2_pa);
        mPA = *addr;
        mConnect = HDMI_CONNECTED;
    }
    //*addr = mPA;
    if(mportinfo == NULL) {
        HWCEC_LOGE("mportinfo. is NULL , add: 0x%4X\n", *addr);
    } else {
        mportinfo->physical_address = mCecDeviceType == CEC_DEVICE_TV ? CEC_TV_PORT_1_PA : mPA;
        HWCEC_LOGD("get_physical_address.addr:0x%4X\n", *addr);
    }
    if(mPA == 0xFFFF) {
        return CEC_RESULT_FAIL ;
    } else {
        return CEC_RESULT_SUCCESS;
    }
}

int HWCECMediator::send_message(const cec_message_t* msg)
{
    //add this lock to protect, only one message send in time.
    //wait for callback from drive return to handle next message.
    AutoMutex l(m_lock);
    int ret = -1;
    if (m_cec_fd <= 0)  return HDMI_RESULT_NACK ;
    //CEC_SEND_MSG CECCmd;
    cec_msg cec_msg_info;
    unsigned char mopcode;
	unsigned char operand[14];

    memcpy(&mopcode, msg->body, 1);

    mCurrCode = (unsigned short )mopcode;
	
	memset(&cec_msg_info, 0, sizeof(cec_msg));
	cec_msg_info.msg[0] = (msg->initiator << 4) | msg->destination;

    memcpy(&cec_msg_info.msg[1], msg->body, msg->length);
    cec_msg_info.len = msg->length + 1;

     //length of initiator and destination is 1.  so z_operand_size = mcecCmd.size -1;
     int msg_length = msg->length- 1;
     if(msg_length < 0) {
         msg_length = 0;
     }
     memcpy(operand, (msg->body) + 1, sizeof(operand));
     HWCEC_LOGD("send_message init:%d, dest:%d, opcode: 0x%x, size:%d, body[0]=0x%x\n",
            msg->initiator,msg->destination, mCurrCode, msg_length, operand[0]);
    //ret = ioctl(m_cec_fd, MTK_HDMI_SET_CECCMD, &CECCmd);
	ret = ioctl(m_cec_fd, CEC_TRANSMIT, &cec_msg_info);
    if (ret < 0) {
        HWCEC_LOGE("send_message fail. opcode: 0x%x, errno: %d", mCurrCode, errno);
        return HDMI_RESULT_FAIL;
    }

    //$XBH_PATCH_START
    //$XBH_PATCH_MODIFY
    if (operand[0] == 0xff){
        cec_msg_info.tx_status = 1;
    }
    //$XBH_PATCH_END

    HWCEC_LOGD("send_message end, opcode: 0x%x, cec_msg_info.tx_status:%d\n", mCurrCode, cec_msg_info.tx_status);

    switch (cec_msg_info.tx_status) {
        case CEC_TX_STATUS_OK:
            return HDMI_RESULT_SUCCESS;
        case CEC_TX_STATUS_ARB_LOST:
            return HDMI_RESULT_BUSY;
        case CEC_TX_STATUS_NACK:
            return HDMI_RESULT_NACK;
        default:
            return HDMI_RESULT_FAIL;
    }

}


void HWCECMediator::register_event_callback(event_callback_t callback, void* arg)
{
      AutoMutex l(m_lock);
      mCbf = callback;
      mcallbacker = arg;
      HWCEC_LOGD("register_event_callback, arg=%p\n", arg);
}

void HWCECMediator::get_version(int* version)
{
    AutoMutex l(m_lock);
    *version = mCECVersion;
    HWCEC_LOGD("get_version.version = %d\n", *version);
}

//verdor id need given by OGM
void HWCECMediator::get_vendor_id(uint32_t* vendor_id)
{
    AutoMutex l(m_lock);
   *vendor_id = mCECVendor_id;
   HWCEC_LOGD("get_vendor_id.vendor_id = %d\n", *vendor_id);
}

void HWCECMediator::get_port_info(hdmi_port_info* list[], int* total)
{
    AutoMutex l(m_lock);
    list[0] = mportinfo;
    if(mportinfo != NULL  &&  mPA != 0xFFFF) {
        *total = 1;
    } else {
        *total = 0;
    }
    HWCEC_LOGD("get_port_info, total= %d, mPA=0x%4X\n", *total,  mPA);
}

void HWCECMediator::set_option(int flag, int value)
{
    AutoMutex l(m_lock);

    HWCEC_LOGD("set_option.flag = %d,value=%d, mCecEnable=%d\n", flag, value, mCecEnable);
    //HWCEC_LOGD("set_option do nothing\n");
    if (value == 1 && mCecEnable == true)
    {
        if(mAutoOtp != 0){
            sendImageViewOn();
            sendActiveSource();
        }
    }
    switch (flag) {
        case HDMI_OPTION_SYSTEM_CEC_CONTROL:
            mOptionSystemCecControl = value;
            break;
    }
    return;
}

void HWCECMediator::set_audio_return_channel(int port_id, int flag)
{
    AutoMutex l(m_lock);
    //not support ARC, so do nothing here
    HWCEC_LOGD("set_audio_return_channel ,flag= %d, port_id=%d\n",flag,port_id);
}


int HWCECMediator::is_connected( int port_id)
{
    AutoMutex l(m_lock);
    int mconn = HDMI_NOT_CONNECTED;
    //only port 1 is valid, need give back right connect status.
    //other port is not invalid, always not connected.
    if(port_id == 1)
    {
        mconn =  mConnect;
    }
    HWCEC_LOGD("is_connected, port_id=%d, mconn =%d\n",port_id,mconn);
    return mconn;
}

void HWCECMediator::sendImageViewOnLock()
{
    AutoMutex l(m_lock);
    sendImageViewOn();
}
void HWCECMediator::sendImageViewOn()
{
    //CEC_SEND_MSG CECCmd;
    cec_msg cec_msg_info;
    int ret = -1;
    unsigned char initiator = 4;
    unsigned char destination = 0;
    unsigned char mopcode = 4;

    if(mLA != CEC_ADDR_UNREGISTERED) {
        initiator = mLA;
    }

    memset(&cec_msg_info, 0, sizeof(cec_msg));
    cec_msg_info.msg[0] = (initiator << 4) | destination;
    cec_msg_info.msg[1] = mopcode;
    cec_msg_info.len = 1 + 1;

    //ret = ioctl(m_cec_fd, MTK_HDMI_SET_CECCMD, &CECCmd);
    ret = ioctl(m_cec_fd, CEC_TRANSMIT, &cec_msg_info);
    if (ret < 0) {
        HWCEC_LOGE("sendImageViewOn fail. errno: %d", errno);
    }

    HWCEC_LOGD("sendImageViewOn init:%d, dest:%d, opcode: 0x%x, size:%d, status:%d\n",
        initiator, destination, mopcode, cec_msg_info.len,cec_msg_info.tx_status);

#ifdef MTK_CEC_EXTRA_PROCESS
    if(mMsgExtraProcessor != NULL) {
        mMsgExtraProcessor->sendCecEvent((int)mopcode);
    }
#endif

}

void HWCECMediator::sendActiveSourceLock()
{
    AutoMutex l(m_lock);
    sendActiveSource();
}
void HWCECMediator::sendActiveSource()
{
    cec_msg cec_msg_info;
    int ret = -1;
    unsigned char initiator = 4; //ADDR_PLAYBACK_1
    unsigned char destination = 15; //ADDR_BROADCAST
    unsigned char mopcode = 0x82;

    uint16_t pa = 0xFFFF;
    get_physical_address(&pa);

    if(mLA != CEC_ADDR_UNREGISTERED) {
        initiator = mLA;
    }

    memset(&cec_msg_info, 0, sizeof(cec_msg));
    cec_msg_info.msg[0] = (initiator << 4) | destination;
    cec_msg_info.msg[1] = mopcode;
    cec_msg_info.msg[2] = (pa >> 8) & 0xFF;
    cec_msg_info.msg[3] = pa & 0xFF;

    cec_msg_info.len = 4;

    ret = ioctl(m_cec_fd, CEC_TRANSMIT, &cec_msg_info);
    if (ret < 0) {
        HWCEC_LOGE("sendActiveSource fail. errno: %d", errno);
        return;
    }
    HWCEC_LOGD("sendActiveSource init:%d, dest:%d, opcode: 0x%x, size:%d, status:%d\n",
        initiator, destination, mopcode, cec_msg_info.len, cec_msg_info.tx_status);

}

void HWCECMediator::dumpUevent(const char *buff, int len)
{
    char buffer[CEC_BUFFER_SIZE];
    memset(buffer, '\0', CEC_BUFFER_SIZE);
    memcpy(buffer, buff, len);
    for(int i =0; i < len; i++) {
        if(buffer[i] == '\0') {
            buffer[i] = '\n';
        }
    }
    HWCEC_LOGI("uevent: %s \n", buffer);
}

void HWCECMediator::handlePlugIn(CecDevice *cecdevice)
{
    if(cecdevice->mType == CEC_DEVICE_PLAYBACK) {
        mConnect = HDMI_CONNECTED;
        uint16_t addr = 0xFFFF;
        hdmi_event_t mevent;
        uint16_t pa_temp = mPrePA; //similar to pre Connect
        get_physical_address(&addr);
        //if pa is not same with before, notify to fm to handle.
        if(pa_temp != mPA) {
            mevent.type = HDMI_EVENT_HOT_PLUG;
            mevent.dev = &(mDevice->base);
            mevent.hotplug.connected = true;
            mevent.hotplug.port_id = 1;
            if(mCbf != NULL) {
                HWCEC_LOGD("CEC_PA, callback to controller\n");
                mCbf(&mevent, mcallbacker);

                //if device not standby, wake up tv
                if(mAutoOtp != 0 && mOptionSystemCecControl) {
                    HWCEC_LOGI("CEC_PA, send ImageViewOn to wakeup TV\n");
                    sendImageViewOnLock();
                    sendActiveSourceLock();
                }
            }
        } else if((pa_temp == mPA) && (mPA == 0xFFFF)) {
            //send image view on to wait TV wakeup
            //work around for sumsung/Toshiba/LG TV
            HWCEC_LOGI("CEC_PA,invalid, send ImageViewOn to wakeup TV\n");
            sendImageViewOnLock();
        }
        mPrePA = mPA;
    }

    if(cecdevice->mType == CEC_DEVICE_TV) {
        mConnect = HDMI_CONNECTED;
        hdmi_event_t mevent;
        uint16_t pa = CEC_TV_PA;
        int ret;

        HWCEC_LOGI("set PA address \n");
        ret = ioctl(m_cec_fd, CEC_ADAP_S_PHYS_ADDR, &pa);
        if(ret) {
            HWCEC_LOGI("plug in, set PA address failed \n");
            return;
        }
        mPA = 0;

        mevent.type = HDMI_EVENT_HOT_PLUG;
        mevent.dev = &(mDevice->base);
        mevent.hotplug.connected = true;
        mevent.hotplug.port_id = 1;
        if(mCbf != NULL) {
            HWCEC_LOGD("CEC_PA, callback to controller\n");
            mCbf(&mevent, mcallbacker);
        }
    }

}

void HWCECMediator::handlePlugOut(CecDevice *cecdevice)
{
    mConnect = HDMI_NOT_CONNECTED;
    hdmi_event_t mevent;
    mevent.type = HDMI_EVENT_HOT_PLUG;
    mevent.dev = &(mDevice->base);
    mevent.hotplug.connected = false;
    mevent.hotplug.port_id = 1;
    if(mCbf) {
        HWCEC_LOGE("CEC_PLUG_OUT, callback to controller\n");
        mCbf(&mevent, mcallbacker);
    }
    if(cecdevice->mType == CEC_DEVICE_PLAYBACK) {
        //reset mPA to invalid, so that next valid PA will notify to frameworks
        mPA = 0xFFFF;
        mPrePA = 0xFFFF;
    }

}

void HWCECMediator::handleCecUEvents(const char *buff, int len){
    const char *s = buff;
    int change_cec = 0;
    const char *ueventNode;
    const char *ueventCnnProp;
    CecDevice *ueventdevice;

    for (int i = 0; i < mCecDevices.size(); i++) {
        ueventNode = mCecDevices[i]->mUeventNode.c_str();
        ueventCnnProp = mCecDevices[i]->mUeventCnnProp.c_str();
        change_cec= !strcmp(s, ueventNode);
        if(change_cec != 0) {
            ueventdevice = mCecDevices[i];
            break;
        }
    }

    if (!change_cec)
       return;

    dumpUevent(buff, len);

    int state = 0;
    int state_flag = 0;
    s += strlen(s) + 1;
    //HWCEC_LOGI("handleCECUEvents");

    while (*s)
    {
        if (!strncmp(s, ueventCnnProp, strlen(ueventCnnProp)))
        {
            state = atoi(s + strlen(ueventCnnProp));
            state_flag = 1;
        }

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    if (change_cec && state_flag)
    {
        //HWCEC_LOGI("ucecevents: hdmicec mPreState = %d,  state = %d ", mCecStates, state);
        if ((ueventdevice->mType == CEC_DEVICE_PLAYBACK && state == 0x0)
                || (ueventdevice->mType == CEC_DEVICE_TV && state == 12)) {
            HWCEC_LOGD("ucecevents: hdmicec disconnecting...");
            handlePlugOut(ueventdevice);
        } else  if ((ueventdevice->mType == CEC_DEVICE_PLAYBACK && state == 0x1)
                || (ueventdevice->mType == CEC_DEVICE_TV && state == 11)) {
            // driver HDMIRX_NOTIFY_T
            HWCEC_LOGD("ucecevents: hdmicec connecting...");
            handlePlugIn(ueventdevice);
        } else {
            HWCEC_LOGI("ucecevents: hdmicec  unhandle cmd");
        }
        //cecDevice->mCecStates = state;
    }
}


void *HWCECMediator::ueventThread(void *arg)
{
    int ret = 0;
    char * ucecevent_desc = NULL;
    struct pollfd ueventfds[2];
    struct sockaddr_nl addr_sock;
    int optval = 64 * 1024;
    int yes = 1;

    ucecevent_desc = (char *)malloc(CEC_BUFFER_SIZE);
    if(ucecevent_desc == NULL) {
        HWCEC_LOGE("uevents: threadLoop malloc uevent buffer failed ");
        goto initFail;
    }

    mUeventExitFd = eventfd(0, EFD_NONBLOCK);
    if (mUeventExitFd < 0) {
        HWCEC_LOGE("Unable to create uevent exit fd :%s", strerror(errno));
        goto initFail;
    }

    mUeventSocket = ::socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (mUeventSocket < 0) {
        HWCEC_LOGE("Unable to create uevent socket:%s", strerror(errno));
        goto initFail;
    }

    // When running in a net/user namespace, SO_RCVBUFFORCE will fail because
    // it will check for the CAP_NET_ADMIN capability in the root namespace.
    // Try using SO_RCVBUF if that fails.
    if((::setsockopt(mUeventSocket, SOL_SOCKET, SO_RCVBUFFORCE, &optval, sizeof(optval)) < 0) &&
       (::setsockopt(mUeventSocket, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0))
    {
        HWCEC_LOGE("Unable to set uevent socket SO_RCVBUF/SO_RCVBUFFORCE  option:%s", strerror(errno));
        goto initFail;
    }

    if(::setsockopt(mUeventSocket, SOL_SOCKET, SO_PASSCRED, &yes, sizeof(yes)) <0) {
        HWCEC_LOGE("Unable to set uevent socket SO_REUSEADDR option:%s", strerror(errno));
        goto initFail;
    }

    memset(&addr_sock, 0, sizeof(addr_sock));
    addr_sock.nl_family = AF_NETLINK;
    addr_sock.nl_pid = getpid();
    addr_sock.nl_groups = 0xffffffff;

    HWCEC_LOGD("Start to initialize, nl_pid(%d)", addr_sock.nl_pid);
    if (::bind(mUeventSocket, (struct sockaddr *)&addr_sock, sizeof(addr_sock)) < 0) {
        HWCEC_LOGE("Failed to bind socket:%s(%d)",strerror(errno), errno);
        goto initFail;
    }

    ueventfds[0].fd = mUeventSocket;
    ueventfds[0].events = POLLIN;
    ueventfds[0].revents = 0;

    ueventfds[1].fd = mUeventExitFd;
    ueventfds[1].events = POLLIN;
    ueventfds[1].revents = 0;

    while (1) {
        //HWCEC_LOGD("%s start  uevent \n", __func__);
        ret = poll(ueventfds, 2, -1);
        //HWCEC_LOGD("%s uevent poll ret = %d\n", __func__, ret);

        if (ret <= 0)
            continue;

        memset(ucecevent_desc, '\0' , CEC_BUFFER_SIZE );

        if ((ueventfds[0].revents & POLLIN)) {
            ueventfds[0].revents = 0;
            /* keep last 2 zeroes to ensure double 0 termination */
            int count = recv(mUeventSocket, ucecevent_desc, CEC_BUFFER_SIZE - 2, 0);
            if (count > 0 && ucecevent_desc != NULL)  {
                //HWCEC_LOGD("uevents: threadLoop %s, count:%d...", ucecevent_desc, count);
                //if(DUMP_UEVENT) {
                //dumpUevent(ucecevent_desc, count);
                //}
                handleCecUEvents(ucecevent_desc, count);
            }
        }

        if ((ueventfds[1].revents & POLLIN)) {
            ueventfds[1].revents = 0;
            HWCEC_LOGI("uevent ufds[1].revents = %x\n", ueventfds[1].revents);
            break;
        }

    }

    free(ucecevent_desc);

    HWCEC_LOGI("%s exit! \n", __func__);
    return NULL;

initFail:
    if (mUeventSocket > 0) {
        close(mUeventSocket);
    }
    if (ucecevent_desc != NULL) {
        free(ucecevent_desc);
    }
    if (mUeventExitFd > 0) {
        close(mUeventExitFd);
    }
    HWCEC_LOGI("%s init fail ! \n", __func__);
    return NULL;
}

int HWCECMediator::getHdmiCecState(CecDevice *cecdevice) {
    int state = 0;
    int err = -1;

    if(cecdevice == nullptr) {
        HWCEC_LOGE(" get state, cecdevice is null\n");
        return err;
    }

    if (cecdevice->mType == CEC_DEVICE_PLAYBACK) {
        int len = 0;
        char buf[5] = "0";
        int fd = open(cecdevice->mConnectStateNode.c_str(), O_RDONLY, 0);
        if(fd < 0) {
            HWCEC_LOGE("Failed to open %s:%s(%d)",cecdevice->mConnectStateNode.c_str(), strerror(errno), errno);
            return err;
        }
        len = read(fd, buf, sizeof(buf));
        if (len <= 0) {
            HWCEC_LOGE("read fail %s:%s(%d)",cecdevice->mConnectStateNode.c_str(), strerror(errno), errno);
            close(fd);
            return err;
        }

        state = atoi(buf);
        HWCEC_LOGI(" read state  %s:%d \n", buf, state);
        close(fd);

        if(state == 0xFF) {
            state = 0;
        }
    }

    if (cecdevice->mType == CEC_DEVICE_TV) {
        int fd = open("/dev/hdmirx", O_RDONLY, 0);
        if(fd < 0) {
            HWCEC_LOGE("Failed to open /dev/hdmirx:%s(%d)", strerror(errno), errno);
            return err;
        }
        struct HDMIRX_DEV_INFO dev_info;
        int ret = ioctl(fd, MTK_HDMIRX_DEV_INFO, &dev_info);
        if (ret) {
            HWCEC_LOGE("MTK_HDMIRX_DEV_INFO fail: %s(%d)",strerror(errno), errno);
            close(fd);
            return err;
        }
        HWCEC_LOGI(" read state  %d \n", dev_info.hdmirx5v);

        // hdmirx_drv.c  io_get_dev_info and  status_show
        state = dev_info.hdmirx5v & 0x1;

        close(fd);
    }

    HWCEC_LOGI(" get state  %d \n", state);

    return state;
}
