###############################################################
LOCAL_PATH := $(call my-dir)

common_SRC_FILES := xml.c \
	util.c \
	module.c \
	shm_buff.c \
	queue.c \
	module_file_input.c \
	module_file_output.c \
	module_v4l2.c \
	module_v4l2_mdp.c \
	module_v4l2_camera.c \
	module_ppm_logo.c \
	module_drm.c \
	drm_display.c \
	module_h264_logo.c \
	module_v4l2_list2va.c \
	module_v4l2_va2list.c \
	module_v4l2_codec.c \
	module_fourinone_camera.c \
	module_v4l2_nr.c \
	module_fake.c \
	RvcSocketEvent.cpp \
	RvcSocketListener.cpp \
	cpp_util.cpp \
	mtk_ovl_adapter.c \
	module_ovl.c \

###############################################################
# build initforfastrvc's executable
include $(CLEAR_VARS)

SYSTEM_INIT_PROGRAM := "/init"
FASTRVC_PROGRAM :="/initfastrvc"
FASTRVC_CONFIG :="/fastlogo_file/fastrvc_config.xml"

LOCAL_SRC_FILES := initforrvc.c \
	initdev.c \

LOCAL_C_INCLUDES += \
	system/core/base/include \
	system/core/include/utils \

LOCAL_STATIC_LIBRARIES := \
	libutils \
	libcutils

LOCAL_MODULE := initforfastrvc
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wall -Wunreachable-code
LOCAL_CFLAGS += -DFASTRVC_INIT_DEV
LOCAL_CFLAGS += -DSYSTEM_INIT_PROGRAM=\""/init\"" -DFASTRVC_PROGRAM=\""/initfastrvc\"" -DFASTRVC_CONFIG=\""/fastlogo_file/fastrvc_config.xml\""

include $(BUILD_EXECUTABLE)

###############################################################
# build fastrvc's executable
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(common_SRC_FILES) \
	fastrvc.c \
	initdev.c \

LOCAL_C_INCLUDES += \
	external/drm \
	external/drm/include/drm \
	external/icu/icu4c/source/common \
	external/libxml2/include \
	hardware/libhardware/include \
	system/core/base/include \
	system/core/include/utils \
	system/core/libsync \
	vendor/mediatek/mt2712/gpu_mali_lib/include

LOCAL_STATIC_LIBRARIES := \
    libsysutils \
	libdrm \
	libxml2 \
	liblog \
	libutils \
	libcutils \
	libbase


LOCAL_MODULE := initfastrvc
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wall -Wunreachable-code

include $(BUILD_EXECUTABLE)

###############################################################
# build rvc_ut's executable
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(common_SRC_FILES) \
	unit_test.c \

LOCAL_C_INCLUDES += \
	external/drm \
	external/drm/include/drm \
	external/icu/icu4c/source/common \
	external/libxml2/include \
	hardware/libhardware/include \
	system/core/base/include \
	system/core/include/utils \
	system/core/libsync \
	vendor/mediatek/mt2712/gpu_mali_lib/include

LOCAL_SHARED_LIBRARIES := \
	libdrm \
	libxml2 \
	liblog \
	libutils \
	libcutils \
	libbase \
	libsync \
	libhardware

LOCAL_MODULE := rvc_ut
LOCAL_MODULE_PATH := $(TARGET_OUT_BIN)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wall -Wunreachable-code

include $(BUILD_EXECUTABLE)

###############################################################
# build rvc_tool's executable
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(common_SRC_FILES) \
	rvc_tool.c \

LOCAL_C_INCLUDES += \
	external/drm \
	external/drm/include/drm \
	external/icu/icu4c/source/common \
	external/libxml2/include \
	hardware/libhardware/include \
	system/core/base/include \
	system/core/include/utils \
	system/core/libsync \
	vendor/mediatek/mt2712/gpu_mali_lib/include

LOCAL_SHARED_LIBRARIES := \
	libdrm \
	libxml2 \
	liblog \
	libutils \
	libcutils \
	libbase \
	libsync \
	libhardware

LOCAL_MODULE := rvc_tool
LOCAL_MODULE_PATH := $(TARGET_OUT_BIN)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wall -Wunreachable-code

include $(BUILD_EXECUTABLE)

$(warning $(TARGET_DEVICE))
$(warning $(TARGET_PRODUCT))
include $(CLEAR_VARS)
LOCAL_MODULE := fastrvc_config.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)/fastlogo_file
LOCAL_SRC_FILES := $(TARGET_PRODUCT)/config_file/fastrvc_config.xml
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE :=Panasonic_opening.h264
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)/fastlogo_file
LOCAL_SRC_FILES := $(TARGET_PRODUCT)/fastlogo_file/Panasonic_opening.h264
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := 720-1280.jpg.argb.bin
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)/fastlogo_file
LOCAL_SRC_FILES := $(TARGET_PRODUCT)/fastlogo_file/car.BGRA
include $(BUILD_PREBUILT)