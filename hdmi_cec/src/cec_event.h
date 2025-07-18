#ifndef HWCEC_EVENT_H_
#define HWCEC_EVENT_H_

#include <utils/threads.h>

using namespace android;
// ---------------------------------------------------------------------------

class UEventCECThread : public Thread
{
public:
    UEventCECThread();
    virtual ~UEventCECThread();

    void CECThreadinitialize();

protected:
    mutable Mutex meventlock;

private:
    virtual bool threadLoop();

    void handleCECUEvents(const char *buff, int len);

    int m_socket;

    int mPreState = -1;

};


#endif // HWCEC_EVENT_H_
