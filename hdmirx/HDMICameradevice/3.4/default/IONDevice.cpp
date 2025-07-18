#define LOG_TAG "HdmirxCamDevSsn@3.4_ION"


//#include <linux/ion_drv.h>
#include <ion/ion.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>


#include "IONDevice.h"

namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace ion {


IONDevice& IONDevice::getInstance()
{
    static IONDevice gInstance;
    return gInstance;
}

IONDevice::IONDevice()
{
    m_dev_fd = open("/dev/ion", O_RDONLY);
    if (m_dev_fd <= 0)
    {
        ALOGE("Failed to open ION device: %s", strerror(errno));
    }
}

IONDevice::~IONDevice()
{
    if (m_dev_fd > 0)
        close(m_dev_fd);
}

int IONDevice::getDeviceFd()
{
    return m_dev_fd;
}

int IONDevice::ionCloseAndSet(int* share_fd, const int& value, const bool& log_on)
{
    int result = ionClose(*share_fd, log_on);
    if (result == 0)
    {
        *share_fd = value;
    }
    return result;
}

int IONDevice::ionImport(int* ion_fd, const bool& log_on)
{
    int dev_fd = m_dev_fd;
    if (dev_fd <= 0) return -1;

    ion_user_handle_t ion_hnd;
    if (ion_import(dev_fd, *ion_fd, &ion_hnd))
    {
        ALOGE("ion_import is failed: %s, ion_fd(%d)", strerror(errno), *ion_fd);
        return -1;
    }

    int share_fd;
    if (ion_share(dev_fd, ion_hnd, &share_fd))
    {
        ALOGE("ion_share is failed: %s, ion_fd(%d)", strerror(errno), *ion_fd);
        return -1;
    }

    if (ion_free(dev_fd, ion_hnd))
    {
        ALOGE("ion_free is failed: %s, ion_fd(%d)", strerror(errno), *ion_fd);
        return -1;
    }

    if (log_on)
    {
        ALOGD("[mm_ionImport] ion_fd(%d) -> share_fd(%d)", *ion_fd, share_fd);
    }
    else
    {
        ALOGV("[mm_ionImport] ion_fd(%d) -> share_fd(%d)", *ion_fd, share_fd);
    }

    *ion_fd = share_fd;

    return 0;
}

int IONDevice::ionImport(const int32_t& ion_fd, int32_t* new_ion_fd, const char* dbg_name)
{
    if (m_dev_fd <= 0)
    {
        ALOGE("ion_import is failed because dev_fd is not initialized");
        return -1;
    }

    ion_user_handle_t ion_hnd;
    if (ion_import(m_dev_fd, ion_fd, &ion_hnd))
    {
        ALOGE("ion_import is failed: %s, ion_fd(%d)", strerror(errno), ion_fd);
        return -1;
    }

    if (ion_share(m_dev_fd, ion_hnd, new_ion_fd))
    {
        ALOGE("ion_share is failed: %s, ion_fd(%d)", strerror(errno), ion_fd);
        return -1;
    }

    if (ion_free(m_dev_fd, ion_hnd))
    {
        ALOGE("ion_free is failed: %s, ion_fd(%d)", strerror(errno), ion_fd);
        return -1;
    }
   
    if (dbg_name != nullptr)
    {
        ALOGD("[mm_ionImport] ion_fd(%d) -> new_ion_fd(%d)", ion_fd, *new_ion_fd);
    }
    else
    {
        ALOGV("[mm_ionImport] ion_fd(%d) -> new_ion_fd(%d)", ion_fd, *new_ion_fd);
    }

    return 0;
}

static int ion_share_close(int fd, int share_fd)
{
    int ret = close(share_fd);
    if (ret < 0) {
        ALOGE("ion_share_close failed fd = %d, share_fd = %d: %s", fd, 
            share_fd, strerror(errno));
        return ret;
    }
    return 0;
}

int IONDevice::ionClose(int share_fd, const bool& log_on)
{
    if (m_dev_fd <= 0) return -1;

    if (share_fd <= 0)
    {
        ALOGE("[mm_ionClose]Invalid Fd (%d)!", share_fd);
        return -1;
    }

    if (ion_share_close(m_dev_fd, share_fd))
    {
        ALOGE("ion_share_close is failed: %s, share_fd(%d)", strerror(errno), share_fd);
        return -1;
    }

    if (log_on)
    {
        ALOGD("[mm_ionClose] share_fd(%d)", share_fd);
    }
    else
    {
        ALOGV("[mm_ionClose] share_fd(%d)", share_fd);
    }

    return 0;
}

static void* ion_mmap(int ion_fd, size_t length, int prot, int flags, int shared_fd)
{
    void* ptr = NULL;
    ptr = mmap(NULL, length, prot, flags, shared_fd, 0);
    if (ptr == MAP_FAILED) {
        ALOGE("failed to mmap[ion_fd: %d share_fd:%d length: %zu]", ion_fd, 
                shared_fd, length);
        return nullptr;
    }
    return ptr;
}

void* IONDevice::ionMMap(int ion_fd, size_t length, int prot, int flags, int shared_fd)
{
    return ion_mmap(ion_fd, length, prot, flags, shared_fd);
}

static int ion_munmap(int fd, void* addr, size_t length)
{
    int res = munmap(addr, length);
    if (res < 0) {
        ALOGE("failed to munmap[fd:%d, addr:%p length:%zu]", fd, addr, length);
        return res;
    }
    return 0;
}

int IONDevice::ionMUnmap(int fd, void* addr, size_t length)
{
    return ion_munmap(fd, addr, length);
}

}
}
}
}
}
