LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	Exynos_OMX_HEVCdec.c \
	library_register.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libOMX.Exynos.HEVC.Decoder
LOCAL_MODULE_RELATIVE_PATH := omx
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS :=
LOCAL_CFLAGS += -DUSE_HEVC_SUPPORT

ifeq ($(BOARD_USE_DMA_BUF), true)
LOCAL_CFLAGS += -DUSE_DMA_BUF
endif

ifeq ($(BOARD_USE_S3D_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_S3D_SUPPORT
endif

ifeq ($(BOARD_USE_CSC_HW), true)
LOCAL_CFLAGS += -DUSE_CSC_HW
endif

ifeq ($(BOARD_USE_CUSTOM_COMPONENT_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_CUSTOM_COMPONENT_SUPPORT
endif

ifeq ($(BOARD_USE_SINGLE_PLANE_IN_DRM), true)
LOCAL_CFLAGS += -DUSE_SINGLE_PLANE_IN_DRM
endif

ifeq ($(BOARD_USE_FULL_ST2094_40), true)
LOCAL_CFLAGS += -DUSE_FULL_ST2094_40
endif

ifdef BOARD_EXYNOS_S10B_FORMAT_ALIGN
LOCAL_CFLAGS += -DS10B_FORMAT_8B_ALIGNMENT=$(BOARD_EXYNOS_S10B_FORMAT_ALIGN)
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := libExynosOMX_Vdec libVendorVideoApi libExynosOMX_OSAL libExynosOMX_Basecomponent \
	libExynosVideoApi
LOCAL_SHARED_LIBRARIES := \
    libc \
    libcutils \
    libutils \
    libdl \
    liblog \
    libhardware \
    libhidlbase \
    libui \
    libexynosgraphicbuffer \
    libstagefright_foundation \
    libexynosv4l2 \
    libion_exynos \
    libcsc \
    libExynosOMX_Resourcemanager \
    libepicoperator

LOCAL_C_INCLUDES := \
	$(EXYNOS_OMX_INC)/exynos \
	$(EXYNOS_OMX_TOP)/osal \
	$(EXYNOS_OMX_TOP)/core \
	$(EXYNOS_OMX_COMPONENT)/common \
	$(EXYNOS_OMX_COMPONENT)/video/dec \
	$(EXYNOS_VIDEO_CODEC)/include \
	$(TOP)/hardware/samsung_slsi/exynos/include \

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_CFLAGS += -DUSE_KHRONOS_OMX_HEADER
LOCAL_C_INCLUDES += $(EXYNOS_OMX_INC)/khronos
else
ifeq ($(BOARD_USE_ANDROID), true)
LOCAL_HEADER_LIBRARIES := media_plugin_headers
LOCAL_CFLAGS += -DUSE_ANDROID
endif
endif

ifeq ($(BOARD_USE_SMALL_SECURE_MEMORY), true)
LOCAL_CFLAGS += -DUSE_SMALL_SECURE_MEMORY
endif

ifeq ($(BOARD_USE_COMPRESSED_COLOR), true)
LOCAL_CFLAGS += -DUSE_COMPRESSED_COLOR
endif

LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-label

include $(BUILD_SHARED_LIBRARY)
