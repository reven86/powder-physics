LOCAL_PATH := $(call my-dir)/../../../source
include $(CLEAR_VARS)
 

LOCAL_MODULE     := powder-physics          # name of your module
LOCAL_LDLIBS     := -L$(SYSROOT)/usr/lib    # libraries to link against
# LOCAL_CPP_EXTENSION := .c

# LOCAL_CFLAGS := -v
LOCAL_SRC_FILES := \
	shared/utils.c \
	solver/api.c \
	solver/cpu_st/solver_cpu_st.c

# LOCAL_C_INCLUDES := 

#include $(BUILD_SHARED_LIBRARY)            # uncomment this line to build a shared library
include $(BUILD_STATIC_LIBRARY)           # here, we are building a static library