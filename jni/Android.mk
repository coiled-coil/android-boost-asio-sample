LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := list-downloader-test
LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_CPPFLAGS := -I$(BOOST_ROOT)
LOCAL_SRC_FILES += list-downloader-test.cpp
LOCAL_SRC_FILES += boost_system_impl.cpp
include $(BUILD_EXECUTABLE)

