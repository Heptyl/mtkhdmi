
LOCAL_PATH := $(call my-dir)
#------------------------------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Werror
LOCAL_SRC_FILES := hdmirx_ut.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := libhdmirx libavsync
LOCAL_SHARED_LIBRARIES += libnativewindow libui libgralloc_extra
LOCAL_SHARED_LIBRARIES += libion libion_mtk libbinder
LOCAL_SHARED_LIBRARIES += libmtkbufferqueue libutils
LOCAL_HEADER_LIBRARIES := libhardware_headers libavsync_headers

LOCAL_MODULE := hdmirx_ut
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(BUILD_EXECUTABLE)
