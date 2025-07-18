
LOCAL_PATH := $(call my-dir)
#------------------------------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Werror -DLOG_NDEBUG=1 -DLOG_TAG=\"HDMI_RX\"
LOCAL_SRC_FILES := hdmi_event.cpp hdmi_ctl.cpp mdp_ctl.cpp stc_ctl.cpp fbm_ctl.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libsync libutils libgralloc_extra
LOCAL_SHARED_LIBRARIES += libbinder libui libcutils
LOCAL_SHARED_LIBRARIES += libdpframework libavsync libmtkbufferqueue
LOCAL_HEADER_LIBRARIES := libhardware_headers libavsync_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc

LOCAL_MODULE := libhdmirx
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(BUILD_SHARED_LIBRARY)
