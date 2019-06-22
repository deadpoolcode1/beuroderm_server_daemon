LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS) 
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils 
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary
#ini libs


# give module name
LOCAL_MODULE    := server_daemon
# list your C files to compile
LOCAL_SRC_FILES := server.c parson/parson.c inih/ini.c i2c.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

# give module name
LOCAL_MODULE    := server_daemon_send
# list your C files to compile
LOCAL_SRC_FILES := send.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

