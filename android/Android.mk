#
# Copyright (C) 2021 The Android-x86 Open Source Project
#
# Licensed under the GNU Lesser General Public License Version 2.1.
# You may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.gnu.org/licenses/lgpl.html
#

LOCAL_PATH := $(dir $(call my-dir))
include $(CLEAR_VARS)

LOCAL_MODULE := libasound
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_CFLAGS := -DPIC -Wno-absolute-value -Wno-address-of-packed-member \
	-Wno-missing-braces -Wno-missing-field-initializers \
	-Wno-pointer-arith -Wno-sign-compare -Wno-unused-function \
	-Wno-unused-const-variable -Wno-unused-parameter -Wno-unused-variable \
	-finline-limit=300 -finline-functions -fno-inline-functions-called-once

# list of files to be excluded
EXCLUDE_SRC_FILES := \
	src/alisp/alisp_snd.c \
	src/compat/hsearch_r.c \
	src/control/control_shm.c \
	src/pcm/pcm_d%.c \
	src/pcm/pcm_ladspa.c \
	src/pcm/pcm_shm.c \
	src/pcm/scopes/level.c \

LOCAL_SRC_FILES := $(filter-out $(EXCLUDE_SRC_FILES),$(call all-c-files-under,src))

intermediates := $(call local-generated-sources-dir)

GEN := $(intermediates)/alsa/asoundlib.h
$(GEN): $(LOCAL_PATH)configure.ac
	$(hide) rm -rf $(dir $(@D)); mkdir -p $(@D)
	ver=$$(sed -n "/^AC_INIT.* \([0-9.]*\))/s//\1/p" $<); vers=($${ver//./ }); \
	echo -e "#define SND_LIB_MAJOR $${vers[0]}\n#define SND_LIB_MINOR $${vers[1]}\n#define SND_LIB_SUBMINOR $${vers[2]}\n#define SND_LIB_EXTRAVER 1000000" > $(@D)/version.h; \
	echo -e "#define SND_LIB_VER(maj, min, sub) (((maj)<<16)|((min)<<8)|(sub))\n#define SND_LIB_VERSION SND_LIB_VER(SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR)" >> $(@D)/version.h; \
	echo "#define SND_LIB_VERSION_STR \"$$ver\"" >> $(@D)/version.h
	cat $(<D)/include/asoundlib-head.h > $@; \
	sed -n "/.*\(#include <[ae].*.h>\).*/s//\1/p" $< >> $@; \
	cat $(<D)/include/asoundlib-tail.h >> $@

LOCAL_GENERATED_SOURCES := $(GEN)

LOCAL_C_INCLUDES := $(LOCAL_PATH)include $(LOCAL_PATH)android $(dir $(GEN))
LOCAL_EXPORT_C_INCLUDE_DIRS := $(intermediates) $(LOCAL_PATH)android
LOCAL_SHARED_LIBRARIES := libdl

include $(BUILD_SHARED_LIBRARY)
