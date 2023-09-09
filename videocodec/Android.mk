LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ExynosVideoInterface.c \
	osal/ExynosVideo_OSAL.c \
	dec/ExynosVideoDecoder.c \
	enc/ExynosVideoEncoder.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/osal/include \
	$(TOP)/hardware/samsung_slsi/exynos/include \

LOCAL_SHARED_LIBRARIES := libion_exynos

LOCAL_SHARED_LIBRARIES += liblog

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi/openmax/include/khronos
else
LOCAL_HEADER_LIBRARIES := media_plugin_headers
endif

LOCAL_CFLAGS :=
LOCAL_PROPRIETARY_MODULE := true

# only 3.4 kernel
ifeq ($(filter-out 3.4, $(TARGET_LINUX_KERNEL_VERSION)),)
LOCAL_CFLAGS += -DUSE_EXYNOS_MEDIA_EXT
endif

# since 3.10 kernel
ifneq ($(filter-out 3.4, $(TARGET_LINUX_KERNEL_VERSION)),)
LOCAL_CFLAGS += -DUSE_DEFINE_H264_SEI_TYPE
endif

# since 3.18 kernel
ifneq ($(filter-out 3.4 3.10, $(TARGET_LINUX_KERNEL_VERSION)),)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mfc_headers
LOCAL_CFLAGS += -DUSE_ORIGINAL_HEADER
LOCAL_CFLAGS += -DUSE_MFC_HEADER
endif

# since 4.19 kernel
ifneq ($(filter 4.19 5.4, $(TARGET_LINUX_KERNEL_VERSION)),)
LOCAL_CFLAGS += -DMAINLINE_FEATURE_IN_SINCE_4_19
endif

ifeq ($(BOARD_USE_HEVC_HWIP), true)
LOCAL_CFLAGS += -DUSE_HEVC_HWIP
endif

ifeq ($(BOARD_USE_DEINTERLACING_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_DEINTERLACING_SUPPORT
endif

ifeq ($(BOARD_USE_SINGLE_PLANE_IN_DRM), true)
LOCAL_CFLAGS += -DUSE_SINGLE_PALNE_SUPPORT
endif

ifneq ($(BOARD_USE_FRAMERATE_THRESH_HOLD),)
LOCAL_CFLAGS += -DFRAMERATE_THRESH_HOLD=$(BOARD_USE_FRAMERATE_THRESH_HOLD)
endif

LOCAL_MODULE := libExynosVideoApi
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-label -Wno-unused-function

include $(BUILD_STATIC_LIBRARY)
