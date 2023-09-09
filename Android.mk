ifneq ($(filter exynos, $(TARGET_SOC_NAME)),)
openmax_dirs := \
	videocodec \
	openmax

include $(call all-named-subdir-makefiles,$(openmax_dirs))
else
PREFIX := $(shell echo $(TARGET_BOARD_PLATFORM) | head -c 6)
ifneq ($(filter exynos, $(PREFIX)),)
openmax_dirs := \
	videocodec \
	openmax

include $(call all-named-subdir-makefiles,$(openmax_dirs))
endif
endif
