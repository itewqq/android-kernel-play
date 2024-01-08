LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= user

LOCAL_SRC_FILES := user.c

include $(BUILD_EXECUTABLE)
