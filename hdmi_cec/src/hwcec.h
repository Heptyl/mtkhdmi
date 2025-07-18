#ifndef _HWHDMICEC_H_
#define _HWHDMICEC_H_

#include <hardware/hardware.h>
#include <hardware/hdmi_cec.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>

#include <errno.h>
#include <thread>
#include <vector>

#include "CecDevice.h"

using namespace android;
using std::shared_ptr;
using std::thread;
using std::vector;

#define CEC_BUFFER_SIZE 2048 *2

// ---------------------------------------------------------------------------
#define CEC_MAX_MSG_SIZE	16
#define CEC_MAX_LOG_ADDRS 4
#define MIN_DEVICE_ID 0
#define MAX_DEVICE_ID 1
#define CEC_TV_PA               ((uint16_t)0x0000)
#define CEC_TV_PORT_1_PA        ((uint16_t)0x1000)


typedef struct hwcec_private_device_t
{
    hdmi_cec_device base;

    /* our private state goes below here */
    uint32_t tag;
    //hwc_procs_t* procs;
} hwcec_private_device_t;

typedef struct cec_msg {
	unsigned long long tx_ts;
	unsigned long long rx_ts;
	unsigned int len;
	unsigned int timeout;
	unsigned int sequence;
	unsigned int flags;
	unsigned char msg[CEC_MAX_MSG_SIZE];
	unsigned char reply;
	unsigned char rx_status;
	unsigned char tx_status;
	unsigned char tx_arb_lost_cnt;
	unsigned char tx_nack_cnt;
	unsigned char tx_low_drive_cnt;
	unsigned char tx_error_cnt;
} cec_msg;

/**
 * struct cec_event_state_change - used when the CEC adapter changes state.
 * @phys_addr: the current physical address
 * @log_addr_mask: the current logical address mask
 */
struct cec_event_state_change {
	unsigned short phys_addr;
	unsigned short log_addr_mask;
};
/**
 * struct cec_event_lost_msgs - tells you how many messages were lost.
 * @lost_msgs: how many messages were lost.
 */
struct cec_event_lost_msgs {
	unsigned int lost_msgs;
};
/**
 * struct cec_event - CEC event structure
 * @ts: the timestamp of when the event was sent.
 * @event: the event.
 * array.
 * @state_change: the event payload for CEC_EVENT_STATE_CHANGE.
 * @lost_msgs: the event payload for CEC_EVENT_LOST_MSGS.
 * @raw: array to pad the union.
 */
struct cec_event {
	unsigned long long ts;
	unsigned int event;
	unsigned int flags;
	union {
		struct cec_event_state_change state_change;
		struct cec_event_lost_msgs lost_msgs;
		unsigned int raw[16];
	};
};
/**
 * struct cec_log_addrs - CEC logical addresses structure.
 * @log_addr: the claimed logical addresses. Set by the driver.
 * @log_addr_mask: current logical address mask. Set by the driver.
 * @cec_version: the CEC version that the adapter should implement. Set by the
 *	caller.
 * @num_log_addrs: how many logical addresses should be claimed. Set by the
 *	caller.
 * @vendor_id: the vendor ID of the device. Set by the caller.
 * @flags: flags.
 * @osd_name: the OSD name of the device. Set by the caller.
 * @primary_device_type: the primary device type for each logical address.
 *	Set by the caller.
 * @log_addr_type: the logical address types. Set by the caller.
 * @all_device_types: CEC 2.0: all device types represented by the logical
 *	address. Set by the caller.
 * @features:	CEC 2.0: The logical address features. Set by the caller.
 */
struct cec_log_addrs {
	unsigned char log_addr[CEC_MAX_LOG_ADDRS];
	unsigned short log_addr_mask;
	unsigned char cec_version;
	unsigned char num_log_addrs;
	unsigned int vendor_id;
	unsigned int flags;
	unsigned char osd_name[15];
	unsigned char primary_device_type[CEC_MAX_LOG_ADDRS];
	unsigned char log_addr_type[CEC_MAX_LOG_ADDRS];

	/* CEC 2.0 */
	unsigned char all_device_types[CEC_MAX_LOG_ADDRS];
	unsigned char features[CEC_MAX_LOG_ADDRS][12];
};
/**
 * struct cec_caps - CEC capabilities structure.
 * @driver: name of the CEC device driver.
 * @name: name of the CEC device. @driver + @name must be unique.
 * @available_log_addrs: number of available logical addresses.
 * @capabilities: capabilities of the CEC adapter.
 * @version: version of the CEC adapter framework.
 */
struct cec_caps {
	unsigned char driver[32];
	unsigned char name[32];
	unsigned int available_log_addrs;
	unsigned int capabilities;
	unsigned int version;
};

/* Adapter capabilities */
#define CEC_ADAP_G_CAPS		_IOWR('a',  0, struct cec_caps)
#define CEC_ADAP_G_PHYS_ADDR	_IOR('a',  1, __u16)
#define CEC_ADAP_S_PHYS_ADDR	_IOW('a',  2, __u16)
#define CEC_ADAP_G_LOG_ADDRS	_IOR('a',  3, struct cec_log_addrs)
#define CEC_ADAP_S_LOG_ADDRS	_IOWR('a',  4, struct cec_log_addrs)
/* Transmit/receive a CEC command */
#define CEC_TRANSMIT		_IOWR('a',  5, struct cec_msg)
#define CEC_RECEIVE		_IOWR('a',  6, struct cec_msg)
/* Dequeue CEC events */
#define CEC_DQEVENT		_IOWR('a',  7, struct cec_event)
/*
 * Get and set the message handling mode for this filehandle.
 */
#define CEC_G_MODE		_IOR('a',  8, __u32)
#define CEC_S_MODE		_IOW('a',  9, __u32)

/* Primary Device Type Operand (prim_devtype) */
#define CEC_OP_PRIM_DEVTYPE_TV				0
#define CEC_OP_PRIM_DEVTYPE_RECORD			1
#define CEC_OP_PRIM_DEVTYPE_TUNER			3
#define CEC_OP_PRIM_DEVTYPE_PLAYBACK			4
#define CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM			5
#define CEC_OP_PRIM_DEVTYPE_SWITCH			6
#define CEC_OP_PRIM_DEVTYPE_PROCESSOR			7

#define CEC_MSG_SET_MENU_LANGUAGE			0x32
#define CEC_MSG_REPORT_FEATURES				0xa6	/* HDMI 2.0 */

/* The logical address types that the CEC device wants to claim */
#define CEC_LOG_ADDR_TYPE_TV		0
#define CEC_LOG_ADDR_TYPE_RECORD	1
#define CEC_LOG_ADDR_TYPE_TUNER		2
#define CEC_LOG_ADDR_TYPE_PLAYBACK	3
#define CEC_LOG_ADDR_TYPE_AUDIOSYSTEM	4
#define CEC_LOG_ADDR_TYPE_SPECIFIC	5
#define CEC_LOG_ADDR_TYPE_UNREGISTERED	6
/* All Device Types Operand (all_device_types) */
#define CEC_OP_ALL_DEVTYPE_TV				0x80
#define CEC_OP_ALL_DEVTYPE_RECORD			0x40
#define CEC_OP_ALL_DEVTYPE_TUNER			0x20
#define CEC_OP_ALL_DEVTYPE_PLAYBACK			0x10
#define CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM			0x08
#define CEC_OP_ALL_DEVTYPE_SWITCH			0x04
/* Allow a fallback to unregistered */
#define CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK	(1 << 0)
/* Passthrough RC messages to the input subsystem */
#define CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU	(1 << 1)
/* CDC-Only device: supports only CDC messages */
#define CEC_LOG_ADDRS_FL_CDC_ONLY		(1 << 2)

/* cec_msg tx/rx_status field */
#define CEC_TX_STATUS_OK		(1 << 0)
#define CEC_TX_STATUS_ARB_LOST		(1 << 1)
#define CEC_TX_STATUS_NACK		(1 << 2)
#define CEC_TX_STATUS_LOW_DRIVE		(1 << 3)
#define CEC_TX_STATUS_ERROR		(1 << 4)
#define CEC_TX_STATUS_MAX_RETRIES	(1 << 5)
#define CEC_TX_STATUS_ABORTED		(1 << 6)
#define CEC_TX_STATUS_TIMEOUT		(1 << 7)

#define CEC_RX_STATUS_OK		(1 << 0)
#define CEC_RX_STATUS_TIMEOUT		(1 << 1)
#define CEC_RX_STATUS_FEATURE_ABORT	(1 << 2)
#define CEC_RX_STATUS_ABORTED		(1 << 3)

/* Event that occurs when the adapter state changes */
#define CEC_EVENT_STATE_CHANGE		1

#define CEC_PHYS_ADDR_INVALID		0xffff
/* Userspace has to configure the physical address */
#define CEC_CAP_PHYS_ADDR	(1 << 0)
/* Userspace has to configure the logical addresses */
#define CEC_CAP_LOG_ADDRS	(1 << 1)
/* Userspace can transmit messages (and thus become follower as well) */
#define CEC_CAP_TRANSMIT	(1 << 2)
/*
 * Passthrough all messages instead of processing them.
 */
#define CEC_CAP_PASSTHROUGH	(1 << 3)
/* Supports remote control */
#define CEC_CAP_RC		(1 << 4)
/* Hardware can monitor all messages, not just directed and broadcast. */
#define CEC_CAP_MONITOR_ALL	(1 << 5)
/* Hardware can use CEC only if the HDMI HPD pin is high. */
#define CEC_CAP_NEEDS_HPD	(1 << 6)
/* Hardware can monitor CEC pin transitions */
#define CEC_CAP_MONITOR_PIN	(1 << 7)

/* The message handling modes */
/* Modes for initiator */
#define CEC_MODE_NO_INITIATOR		(0x0 << 0)
#define CEC_MODE_INITIATOR		(0x1 << 0)
#define CEC_MODE_EXCL_INITIATOR		(0x2 << 0)
#define CEC_MODE_INITIATOR_MSK		0x0f

/* Modes for follower */
#define CEC_MODE_NO_FOLLOWER		(0x0 << 4)
#define CEC_MODE_FOLLOWER		(0x1 << 4)
#define CEC_MODE_EXCL_FOLLOWER		(0x2 << 4)
#define CEC_MODE_EXCL_FOLLOWER_PASSTHRU	(0x3 << 4)
#define CEC_MODE_MONITOR_PIN		(0xd << 4)
#define CEC_MODE_MONITOR		(0xe << 4)
#define CEC_MODE_MONITOR_ALL		(0xf << 4)
#define CEC_MODE_FOLLOWER_MSK		0xf0

/*
 * error code used for native handle.
 */
enum {
    CEC_RESULT_SUCCESS = 0,
    CEC_RESULT_FAIL = -1,
};

/*
 * callback event code used for native handle.
 */
enum {
    CEC_PLUG_OUT = 0,
    CEC_PA = 1,
    CEC_TX_STATUS = 2,
    CEC_RX_CMD = 3,
};

struct HDMIRX_DEV_INFO {
	unsigned char hdmirx5v;
	bool hpd;
	uint32_t power_on;
	unsigned char state;
	unsigned char vid_locked;
	unsigned char aud_locked;
	unsigned char hdcp_version;
};

#define HDMI_IOW(num, dtype)  _IOW('H', num, dtype)
#define HDMI_IOR(num, dtype) _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype) _IOWR('H', num, dtype)
#define HDMI_IO(num) _IO('H', num)
#define MTK_HDMIRX_DEV_INFO HDMI_IOWR(4, struct HDMIRX_DEV_INFO)

#define CECVendor 0x000CE7
#define CECVersion 0x5

class HWCECMediator;

class DispatcherThread  : public RefBase
{
public:
    DispatcherThread(HWCECMediator* mediator);
    ~DispatcherThread();

    int handlecallback(int cbtype);


private:

    HWCECMediator* m_mediator;
};

// ---------------------------------------------------------------------------

class HWCECMediator : public Singleton<HWCECMediator>
{
public:
    HWCECMediator();

    ~HWCECMediator();

    void device_open(hwcec_private_device_t* device);

    void device_close(hwcec_private_device_t* device);

    int add_logical_address(cec_logical_address_t addr);

    void clear_logical_address();

    int get_physical_address_lock(uint16_t* addr);

    int get_physical_address(uint16_t* addr);

    int send_message(const cec_message_t* msg);

    void register_event_callback(event_callback_t callback, void* arg);

    void get_version(int* version);

    void get_vendor_id(uint32_t* vendor_id);

    void get_port_info(hdmi_port_info* list[], int* total);

    void set_option(int flag, int value);

    void set_audio_return_channel(int port_id, int flag);

    int is_connected(int port_id);

    int get_txstatus();

    int get_rxcmd();

    void * ueventThread(void *arg);
    void handleCecUEvents(const char *buff, int len);
    void dumpUevent(const char *buff, int len);
    void handlePlugOut(CecDevice *cecdevice);
    void handlePlugIn(CecDevice *cecdevice);
    void * msgEventThread(CecDevice *cecdevice);
    int getHdmiCecState(CecDevice *cecdevice);

    //send image view on
    void sendImageViewOn();
    void sendActiveSource();
    void sendImageViewOnLock();
    void sendActiveSourceLock();
    void setSending(int MsgSending);

    bool  getSending(int * MsgResult);
    //get cec is ready now. if PA not valid , isReady=false;
    //if disable or hdmi not connect , isReady=false
    bool  isReady();
    //This is save current physical address
    uint16_t  mPA;
    //This is save previous physical address
    uint16_t  mPrePA;
    //This is save current logical address
    cec_logical_address_t  mLA;
    //This is save current CEC Version
    uint32_t mCECVersion;
    //This is save current CEC Vendor ID
    uint32_t mCECVendor_id;
    //This is indicate whether the message is sending, need wait for drive tx status callback
    bool mMsgSending;
    //This is save current cec message send result callback from drive
    int mMsgResult;
    //This is save current cec message opcode
    uint16_t  mCurrCode;
    //This is save current callback func, when received callback from drive, need callback to framework
    event_callback_t mCbf = NULL;
    //This is save current the cec hw device point
    hwcec_private_device_t* mDevice;
    //This is indicate whether system control now
    bool mCecServiceControl;
    //This is indicate whether the hdmi is connect
    int mConnect;
    //This is indicate whether system control now
    bool mCecEnable;
    //This is save port information
    hdmi_port_info *mportinfo;
    //HDMI_OPTION_SYSTEM_CEC_CONTROL,
    bool mOptionSystemCecControl;
    //
    void * mcallbacker = NULL;
    // Auto One Touch Play
    int   mAutoOtp;

    //friend void *msgEventThread(void *arg);
    int hdmicec_close();
    int cec_init();

protected:
    mutable Mutex m_lock;

private:
    int m_cec_fd;
    //int exit_fd = -1;

    // Uevent params
    thread mUeventThread;
    int mUeventExitFd = -1;
    int mUeventSocket = -1;
    //device node for CEC uevent
    std::string mUeventNode;
    // connect property for CEC uevent.
    std::string mUeventCnnProp;

    vector<thread> mMsgEventThreads;
    vector<CecDevice *> mCecDevices;
    CecDevice* mCurrentActiveDevice;

    unsigned int mCecDeviceType;
    bool mHasDpCec;
    bool mSupportArc;

};

#endif // _HWHDMICEC_H_
