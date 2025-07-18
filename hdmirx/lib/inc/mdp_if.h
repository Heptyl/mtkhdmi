#ifndef LIB_INC_MDP_IF_H__
#define LIB_INC_MDP_IF_H__

#include <hardware/tv_input.h>

#include "hdmi_if.h"

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

int mdp_open(void);
void mdp_close(void);
void mdp_reg_done_callback(struct tv_input_device *dev,
                           int (*cb)(struct tv_input_device *dev,
                                     tv_input_event_t *event, int status));
int mdp_trigger(buffer_handle_t buffer, int32_t *fence,
                struct HDMIRX_VID_PARA *info = nullptr);
int mdp_capture(tv_input_event_t *event);

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

#endif  // LIB_INC_MDP_IF_H__
