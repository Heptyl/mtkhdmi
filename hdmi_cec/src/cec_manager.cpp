#define DEBUG_LOG_TAG "HWCEC"

#include <stdint.h>

#include <hardware/hdmi_cec.h>
#include <cutils/properties.h>

#include "utils/cec_debug.h"
#include "cec_manager.h"


// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(CECManager);

CECManager::CECManager()
    : m_listener(NULL)
{
    HWCEC_LOGD("CECManager");
    m_ucecevent_thread = new UEventCECThread();
    if (m_ucecevent_thread == NULL)
    {
        HWCEC_LOGE("Failed to initialize UEvent thread!!");
        abort();
    }
    HWCEC_LOGD("new UEventCECThread");
    int ret = m_ucecevent_thread->run("HWCECUEventThread");
    HWCEC_LOGD("HWCECUEventThread run: %d", ret);

}

CECManager::~CECManager()
{
    m_listener = NULL;
    if (m_ucecevent_thread != NULL) {
        HWCEC_LOGD("~CECManager requestExit");
        m_ucecevent_thread->requestExit();
        HWCEC_LOGD("~CECManager requestExitAndWait");
        m_ucecevent_thread->requestExitAndWait();
        HWCEC_LOGD("~CECManager clear");
        m_ucecevent_thread.clear();
        m_ucecevent_thread = NULL;
        HWCEC_LOGD("~g_uevent_thread done");
    }

    HWCEC_LOGD("~CECManager done");
}


void CECManager::setListener(const sp<EventListener>& listener)
{
    HWCEC_LOGD(" CECManager setListener");
    m_listener = listener;
}

void CECManager::PlugOut()
{
    if (m_listener != 0)
        m_listener->onPlugOut();
}

void CECManager::PlugIn()
{
    if (m_listener != 0)
        m_listener->onPlugIn();
}

void CECManager::TXStatus()
{
    if (m_listener != 0)
        m_listener->onTXStatus();
}

void CECManager::RXCmd()
{
    if (m_listener != 0)
        m_listener->onRXCmd();
}



