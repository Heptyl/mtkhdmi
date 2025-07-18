#ifndef LIB_INC_ION_DEVICE_H__
#define LIB_INC_ION_DEVICE_H__


namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace ion {

class IONDevice
{
public:
    static IONDevice& getInstance();
    ~IONDevice();

    int getDeviceFd();
    int ionImport(int* ion_fd, const bool& log_on = true);
    int ionImport(const int32_t& ion_fd, int32_t* new_ion_fd, const char* = nullptr);
    int ionClose(int share_fd, const bool& log_on = true);
    int ionCloseAndSet(int* share_fd, const int& value = -1, const bool& log_on = true);
    void* ionMMap(int ion_fd, size_t length, int prot, int flags, int shared_fd);
    int ionMUnmap(int fd, void* addr, size_t length);

private:
    IONDevice();
    int m_dev_fd;
};

}
}
}
}
}
#endif
