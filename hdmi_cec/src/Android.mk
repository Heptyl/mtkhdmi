# build hwcec static library

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := hdmi_cec.$(TARGET_BOARD_PLATFORM)

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_HEADER_LIBARIES += libhardware_headers

LOCAL_SHARED_LIBRARIES := \
	libui \
	libutils \
	libcutils \
	libhardware \
	liblog \

LOCAL_SRC_FILES := \
	hwcec.cpp \
	CecDevice.cpp



LOCAL_CFLAGS:= \
	-DLOG_TAG=\"hdmi_cec\"


ifneq ($(strip $(TARGET_BUILD_VARIANT)), eng)
LOCAL_CFLAGS += -DMTK_USER_BUILD
endif

LOCAL_CFLAGS += -DUSE_NATIVE_FENCE_SYNC

LOCAL_CFLAGS += -DUSE_SYSTRACE

$(warning MTK_NFX_SUPPORT is $(MTK_NFX_SUPPORT))
$(warning ANDROID_TV is $(ANDROID_TV))
ifeq ($(strip $(MTK_NFX_SUPPORT)), yes)
ifeq ($(strip $(ANDROID_TV)), yes)
LOCAL_CFLAGS += -DMTK_CEC_EXTRA_PROCESS
LOCAL_SRC_FILES += extra_process.cpp
endif
endif

include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := init.hdmicec.rc
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/etc/init
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(BUILD_PREBUILT)
