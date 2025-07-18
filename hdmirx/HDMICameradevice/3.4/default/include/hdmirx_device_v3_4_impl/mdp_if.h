#ifndef LIB_INC_MDP_IF_H__
#define LIB_INC_MDP_IF_H__

#include <android/hardware/graphics/common/1.0/types.h>


#include "hdmi_if.h"

enum HDMIRX_CS {
    MDP_PROFILE_AUTO  = 1,
    MDP_PROFILE_LIMIT = 2,
    MDP_PROFILE_FULL  = 3,
};


namespace android {
namespace hardware {
namespace camera {

namespace hdmirx {
namespace mdp {

using ::android::hardware::camera::hdmirx::hdmi::HDMIRX_VID_PARA;
using ::android::hardware::graphics::common::V1_0::PixelFormat;

//#ifdef __cplusplus
//extern "C" {
//#endif


int mdp_open(void);
void mdp_close(void);
int mdp_trigger(buffer_handle_t buffer, int32_t *fence, int32_t mdp_profile, HDMIRX_VID_PARA *info = nullptr);
int mdp_1in1out(buffer_handle_t input_handle, int input_width, int input_height,
                buffer_handle_t output_handle, int output_width, int output_height);
int mdp_1in2out(buffer_handle_t input_handle, int input_width, int input_height,
                buffer_handle_t output0_handle, int output0_width, int output0_height,
                buffer_handle_t output1_handle, int output1_width, int output1_height);
int mdp_parse_hdr_info(struct hdmirx::hdmi::hdr10InfoPkt *hdr10info);

//#ifdef __cplusplus
//}
//#endif


}
}
}
}
}

#endif  // LIB_INC_MDP_IF_H__
