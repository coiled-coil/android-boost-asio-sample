APP_ABI := x86
APP_STL := gnustl_static
NDK_TOOLCHAIN_VERSION=clang3.2
APP_CPPFLAGS += -std=c++11
APP_CPPFLAGS += -DBOOST_SYSTEM_NO_LIB
APP_PLATFORM := android-8
