
LOCAL_PATH := $(call my-dir)
#------------------------------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wall -Werror -DLOG_NDEBUG=1 -DLOG_TAG=\"TV_HAL\"
LOCAL_SRC_FILES := tv_input.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := liblog libutils libgralloc_extra libhdmirx
LOCAL_HEADER_LIBRARIES := libhardware_headers

LOCAL_MODULE := tv_input.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(BUILD_SHARED_LIBRARY)
