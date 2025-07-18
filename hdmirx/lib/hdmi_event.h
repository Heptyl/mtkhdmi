#ifndef LIB_HDMI_EVENT_H__
#define LIB_HDMI_EVENT_H__

#include <hardware/tv_input.h>
#include <utils/threads.h>

using ::android::Mutex;
using ::android::Thread;

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

// ---------------------------------------------------------------------------

class HdmiUEventThread : public Thread {
 public:
    HdmiUEventThread();
    virtual ~HdmiUEventThread();

    void init(void);
    void regCallback(struct tv_input_device *dev,
                     int (*tv_cb)(struct tv_input_device *));

 protected:
    mutable Mutex m_lock;

 private:
    virtual bool threadLoop();

    void handleHdmiUEvents(const char *buff, int len);

    int m_socket;

    /* tv input callback */
    struct tv_input_device *_dev;
    int (*_tv_cb)(struct tv_input_device *);
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

#endif  // LIB_HDMI_EVENT_H__
