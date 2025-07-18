#define LOG_TAG "HdmirxCamDevSsn@3.4_DMABUF"



#include <fcntl.h>
#include <cutils/log.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>


#include <dma-buf.h>
#include <dma-heap.h>

#include "DMABUFDevice.h"

namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace dma_buf {


DMABUFDevice& DMABUFDevice::getInstance()
{
    static DMABUFDevice gInstance;
    return gInstance;
}

DMABUFDevice::DMABUFDevice()
{
    m_dev_fd = open("/dev/dma_heap/mtk_mm", O_RDONLY);
    if (m_dev_fd <= 0)
    {
        ALOGE("Failed to open dma_buf device: %s", strerror(errno));
    }
}

DMABUFDevice::~DMABUFDevice()
{
    if (m_dev_fd > 0)
        close(m_dev_fd);
}

int DMABUFDevice::getDeviceFd()
{
    return m_dev_fd;
}

int DMABUFDevice::dmabufCloseAndSet(int* share_fd, const int& value, const bool& log_on)
{
    int result = dmabufClose(*share_fd, log_on);
    if (result == 0)
    {
        *share_fd = value;
    }
    return result;
}


static int dma_buf_close(int fd, int share_fd)
{
    int ret = close(share_fd);
    if (ret < 0) {
        ALOGE("dma_buf_close failed fd = %d, share_fd = %d: %s", fd,
            share_fd, strerror(errno));
        return ret;
    }
    return 0;
}

int DMABUFDevice::dmabufClose(int share_fd, const bool& log_on)
{
    if (m_dev_fd <= 0) return -1;

    if (share_fd <= 0)
    {
        ALOGE("[mm_ionClose]Invalid Fd (%d)!", share_fd);
        return -1;
    }

    if (dma_buf_close(m_dev_fd, share_fd))
    {
        ALOGE("ion_share_close is failed: %s, share_fd(%d)", strerror(errno), share_fd);
        return -1;
    }

    if (log_on)
    {
        ALOGD("[dma_buf Close] share_fd(%d)", share_fd);
    }
    else
    {
        ALOGV("[dma_buf Close] share_fd(%d)", share_fd);
    }

    return 0;
}


static int dma_buf_alloc(int dma_fd, size_t length, int *shared_fd)
{
    struct dma_heap_allocation_data data = {
        .len = length,
        .fd_flags = O_RDWR | O_CLOEXEC,
        .heap_flags = DMA_HEAP_VALID_HEAP_FLAGS,
    };

    if (ioctl(dma_fd, DMA_HEAP_IOCTL_ALLOC, &data)) {
        ALOGE("DMA_HEAP_IOCTL_ALLOC fail\n");
        //close(m_dev_fd);
        return -1;
    }
    *shared_fd = (int)data.fd;


    return 0;
}

int DMABUFDevice::dmabufAlloc(int dma_fd, size_t length, int *shared_fd)
{
    return dma_buf_alloc(m_dev_fd, length, shared_fd);
}



static void* dma_buf_mmap(int dma_fd, size_t length, int prot, int flags, int shared_fd)
{
    void* ptr = NULL;

    ptr = mmap(NULL, length, prot, flags, shared_fd, 0);
    if (ptr == MAP_FAILED) {
        ALOGE("failed to mmap[dma_fd: %d share_fd:%d length: %zu]", dma_fd,
                shared_fd, length);
        return nullptr;
    }

    return ptr;
}

void* DMABUFDevice::dmabufMMap(int dma_fd, size_t length, int prot, int flags, int shared_fd)
{
    return dma_buf_mmap(dma_fd, length, prot, flags, shared_fd);
}

static int dma_buf_munmap(int dma_fd, void* addr, size_t length)
{
    int res = munmap(addr, length);
    if (res < 0) {
        ALOGE("failed to munmap[dma_fd:%d, addr:%p length:%zu]", dma_fd, addr, length);
        return res;
    }
    return 0;
}

int DMABUFDevice::dmabufMUnmap(int dma_fd, void* addr, size_t length)
{
    return dma_buf_munmap(dma_fd, addr, length);
}

}
}
}
}
}
