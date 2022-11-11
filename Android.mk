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
LOCAL_SRC_FILES := server.c parson/parson.c inih/ini.c i2c.c common.c sha256/sha-256.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary

# give module name
LOCAL_MODULE    := server_daemon_fw
# list your C files to compile
LOCAL_SRC_FILES := server_fw.c parson/parson.c inih/ini.c i2c.c common.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary

# give module name
LOCAL_MODULE    := server_daemon_5v
# list your C files to compile
LOCAL_SRC_FILES := server_daemon_5v.c  i2c.c common.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary

# give module name
LOCAL_MODULE    := server_daemon_monitor_language
# list your C files to compile
LOCAL_SRC_FILES := server_daemon_monitor_language.c  common.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary

# give module name
LOCAL_MODULE    := server_daemon_monitor_language_create
# list your C files to compile
LOCAL_SRC_FILES := server_daemon_monitor_language_create.c  common.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
# Enable PIE manually. Will get reset on $(CLEAR_VARS). This
# is what enabling PIE translates to behind the scenes.
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie -llog -lcutils
LOCAL_LDLIBS := -ldl -llog
#ndlog
LOCAL_STATIC_LIBRARIES += ND_LogLibrary
LOCAL_C_INCLUDES += device/variscite/novodes/ND_LogLibrary
# give module name
LOCAL_MODULE    := server_daemon_nonvolotile
# list your C files to compile
LOCAL_SRC_FILES := server_daemon_nonvolotile.c  common.c inih/ini.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# give module name
LOCAL_MODULE    := server_daemon_send
# list your C files to compile
LOCAL_SRC_FILES := send.c
# this option will build executables instead of building library for android application.
include $(BUILD_EXECUTABLE)

