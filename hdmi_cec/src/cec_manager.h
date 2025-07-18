#ifndef HWCEC_MANAGER_H_
#define HWCEC_MANAGER_H_

#include <utils/Singleton.h>
#include <utils/ThreadDefs.h>

#include "cec_event.h"

using namespace android;

class TestThread : public Thread
{
    public:
    TestThread();
    ~TestThread();

private:
    virtual bool threadLoop();

};


// ---------------------------------------------------------------------------
class CECManager : public Singleton<CECManager>
{
public:
    CECManager();
    ~CECManager();

    struct EventListener : public virtual RefBase
    {
        // onPlugIn() is called to notify a display is plugged
        virtual void onPlugIn() = 0;

        // onPlugOut() is called to notify a display is unplugged
        virtual void onPlugOut() = 0;

      //onTXStatus is called to notify TXStatus update, need to get new status
        virtual void onTXStatus()  = 0;

      //onRXCmd is called to notify RXCmd update, need to get new RX cmd
        virtual void onRXCmd()  = 0;
    };

    // setListener() is used for client to register listener to get event
    void setListener(const sp<EventListener>& listener);

    void PlugOut();

    void PlugIn();

    void TXStatus();

    void RXCmd();

    private:
     // m_listener is used for listen vsync event
    sp<EventListener> m_listener;

    sp<UEventCECThread> m_ucecevent_thread;
};

#endif // HWCEC_MANAGER_H_
