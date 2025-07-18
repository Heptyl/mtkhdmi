#ifndef LIB_INC_FBM_IF_H__
#define LIB_INC_FBM_IF_H__

#include "hdmi_if.h"

namespace vendor {
namespace mediatek {
namespace hardware {
namespace hdmirx {
namespace V1_0 {
namespace implementation {

int fbm_open(int pip = 0, struct HDMIRX_VID_PARA *info = nullptr);
void fbm_close(void);
native_handle_t *fbm_create_sideband_handle(void);
int fbm_get_display(uint32_t *w, uint32_t *h);
/* for debug purpose */
void fbm_set_display(int fps, int w = FULL_WIDTH, int p = FULL_WIDTH,
                     int h = FULL_HEIGHT);

}  // namespace implementation
}  // namespace V1_0
}  // namespace hdmirx
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

#endif  // LIB_INC_FBM_IF_H__
