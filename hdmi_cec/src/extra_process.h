#ifndef HWCEC_MSG_EXTRA_HANDLER_H_
#define HWCEC_MSG_EXTRA_HANDLER_H_

#include <utils/threads.h>
#include <utils/Looper.h>
#include "cec_utils.h"

using namespace android;
// ---------------------------------------------------------------------------

enum {
    MSG_HDMI_EVENT = 1,
};

class CecEventHandler : public MessageHandler {

public:
    CecEventHandler();
    virtual ~CecEventHandler();

    virtual void handleMessage(const Message& message);

private:
    Vector<hdmi_event_t> mHdmiEvents;
    android::Mutex mLock;

};

class CecEventCallback : public LooperCallback {
public:
    CecEventCallback();
    virtual ~CecEventCallback();
    void setFd(int fds[]) {
        mReceiveFd = fds[0];
        mSendFd = fds[1];
    }

    virtual int handleEvent(int fd, int events, void* data);
private:
    int handleActiveSource(const hdmi_event_t event);
    int handleStandby(const hdmi_event_t event);
    int handleRoutingChange(const hdmi_event_t event);
    int handleSetStreamPath(const hdmi_event_t event);
    int setNRDPVideoPlatformCaps(bool active);
    int mReceiveFd;
    int mSendFd;
};


class MsgExtraProcessor: public Thread
{
public:
    MsgExtraProcessor();
    virtual ~MsgExtraProcessor();
    void recvCecEvent(const hdmi_event_t event);
    void sendCecEvent(const int opCode);

private:
    virtual bool threadLoop();
    virtual void onFirstRef();

    sp<Looper> mLooper;

    sp<CecEventHandler> mCecEventHandler;

    sp<CecEventCallback> mCecEventCallback;

    bool mInitialized;

    int mReceiveFd;
    int mSendFd;

};


#endif // HWCEC_MSG_EXTRA_HANDLER_H_
