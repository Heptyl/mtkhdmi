# build hwcec share library

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	hwcec.cpp

LOCAL_CFLAGS := \
	-DLOG_TAG=\"hdmi_cec\"

ifeq ($(MTK_HDMI_SUPPORT), yes)
LOCAL_CFLAGS += -DMTK_EXTERNAL_SUPPORT
endif


LOCAL_HEADER_LIBARIES += libhardware_headers

LOCAL_STATIC_LIBRARIES += \
	hdmi_cec.$(TARGET_BOARD_PLATFORM)

LOCAL_SHARED_LIBRARIES := \
	libui \
	libutils \
	libcutils \
	libhardware \
	liblog \

# HAL module implemenation stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
LOCAL_MODULE := hdmi_cec.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MULTILIB := both
include $(MTK_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))



