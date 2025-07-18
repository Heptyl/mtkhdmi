#define DEBUG_LOG_TAG "HWCEC"

#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <hardware/hdmi_cec.h>

#include <cutils/properties.h>
#include <utils/threads.h>

#include "utils/cec_debug.h"

#include "cec_event.h"
#include "cec_manager.h"

// ---------------------------------------------------------------------------
#define CEC_BUFFER_SIZE 2048 * 2

UEventCECThread::UEventCECThread()
{
    HWCEC_LOGD("UEventTestThread\n");
    m_socket = -1;
    CECThreadinitialize();
}

UEventCECThread::~UEventCECThread()
{
    if (m_socket > 0)  {
        close(m_socket);
    }
    HWCEC_LOGD("~UEventCECThread");
}

void UEventCECThread::CECThreadinitialize()
{
    struct sockaddr_nl addr_sock;
    int optval = 64 * 1024;

    int yes = 1;

    m_socket = ::socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (m_socket < 0)
    {
        HWCEC_LOGE("Unable to create uevent socket:%s", strerror(errno));
        return;
    }

    // When running in a net/user namespace, SO_RCVBUFFORCE will fail because
    // it will check for the CAP_NET_ADMIN capability in the root namespace.
    // Try using SO_RCVBUF if that fails.
    if((::setsockopt(m_socket, SOL_SOCKET, SO_RCVBUFFORCE, &optval, sizeof(optval)) < 0) &&
       (::setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0))
    {
        HWCEC_LOGE("Unable to set uevent socket SO_RCVBUF/SO_RCVBUFFORCE  option:%s", strerror(errno));
        close(m_socket);
        return;
    }

    if (setsockopt(m_socket, SOL_SOCKET, SO_PASSCRED, &yes, sizeof(yes)) < 0) {
        HWCEC_LOGE("Unable to set uevent socket SO_PASSCRED option:%s", strerror(errno));
        close(m_socket);
        return;
    }

    memset(&addr_sock, 0, sizeof(addr_sock));
    addr_sock.nl_family = AF_NETLINK;
    addr_sock.nl_pid = getpid();
    addr_sock.nl_groups = 0xffffffff;

    HWCEC_LOGD("Start to initialize, nl_pid(%d)", addr_sock.nl_pid);
    if (::bind(m_socket, (struct sockaddr *)&addr_sock, sizeof(addr_sock)) < 0)
    {
        HWCEC_LOGE("Failed to bind socket:%s(%d)",strerror(errno), errno);
        close(m_socket);
        return;
    }

    HWCEC_LOGD("Init uevent socket done");

}

void UEventCECThread::handleCECUEvents(const char *buff, int len)
{
    const char *s = buff;
    int change_cec= !strcmp(s, "change@/devices/virtual/switch/cec_hdmi");

    if (!change_cec)
       return;

    int state = 0;
    s += strlen(s) + 1;
	HWCEC_LOGD("handleCECUEvents");

    while (*s)
    {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
        {
            state = atoi(s + strlen("SWITCH_STATE="));
        }

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    if (change_cec)
    {
        HWCEC_LOGD("ucecevents: hdmi mPreState = %d", mPreState);
        if (state == 0x0)
        {
            HWCEC_LOGD("ucecevents: hdmi disconnecting...");
            if (state != mPreState)
                CECManager::getInstance().PlugOut();
        }
       else  if (state == 0x1)
        {
            HWCEC_LOGD("ucecevents: hdmi connecting...");
            //if (state != mPreState)
            CECManager::getInstance().PlugIn();
        }
        else if (state == 0x2)
        {
            HWCEC_LOGD("ucecevents: hdmi CEC_TX_STATUS callback...");
            if (state != mPreState)
                CECManager::getInstance().TXStatus();
        }
        else if (state == 0x3)
        {
            HWCEC_LOGD("ucecevents: hdmi CEC_RX_CMD callback...");
            if (state != mPreState)
                CECManager::getInstance().RXCmd();
        }
        else
        {
            //HWCEC_LOGE("uevents: hdmi invalid cmd");
        }

        mPreState = state;
    }

}

bool UEventCECThread::threadLoop()
{
     AutoMutex l(meventlock);
    static char ucecevent_desc[CEC_BUFFER_SIZE];
    struct pollfd cecfds;
    cecfds.fd = m_socket;
    cecfds.events = POLLIN;
    cecfds.revents = 0;
    int ret = poll(&cecfds, 1, -1);
    if (ret > 0 && (cecfds.revents & POLLIN))
    {
        /* keep last 2 zeroes to ensure double 0 termination */
        int count = recv(m_socket, ucecevent_desc, sizeof(ucecevent_desc) - 2, 0);
        if (count > 0)  {
            memset(ucecevent_desc + count, '\0' , sizeof(ucecevent_desc) - count);
            //HWCEC_LOGE("uevents: threadLoop %s, count:%d...",ucecevent_desc,count);
            handleCECUEvents(ucecevent_desc, count);
        }
    }
      return true;
}



