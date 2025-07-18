#ifndef LIB_INC_DMABUF_DEVICE_H__
#define LIB_INC_DMABUF_DEVICE_H__


namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace dma_buf {

class DMABUFDevice
{
public:
    static DMABUFDevice& getInstance();
    ~DMABUFDevice();

    int getDeviceFd();
    int dmabufAlloc(int dma_fd, size_t length, int *shared_fd);
    int dmabufClose(int share_fd, const bool& log_on = true);
    int dmabufCloseAndSet(int* share_fd, const int& value = -1, const bool& log_on = true);
    void* dmabufMMap(int dma_fd, size_t length, int prot, int flags, int shared_fd);
    int dmabufMUnmap(int fd, void* addr, size_t length);

private:
    DMABUFDevice();
    int m_dev_fd;
};

}
}
}
}
}
#endif
