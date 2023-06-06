# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.thermal@2.0-impl
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := \
    Thermal.cpp \
    thermal_watcher.cpp \
    thermal_helper.cpp

LOCAL_SHARED_LIBRARIES := \
    libc \
    liblog \
    libcutils \
    libhardware \
    libbase \
    libcutils \
    libutils \
    libhidlbase \
    android.hardware.thermal@1.0 \
    android.hardware.thermal@2.0

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.thermal@2.0-service.mtk
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := android.hardware.thermal@2.0-service.mtk.rc
LOCAL_VINTF_FRAGMENTS := android.hardware.thermal@2.0-service.mtk.xml
LOCAL_SRC_FILES := service.cpp

LOCAL_SHARED_LIBRARIES := \
    libc \
    liblog \
    libcutils \
    libhardware \
    libcutils \
    libbase \
    libhidlbase \
    libutils \
    android.hardware.thermal@2.0 \
    android.hardware.thermal@1.0

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := thermal_hal
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_SRC_FILES := thermal_hal.c
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware

include $(BUILD_SHARED_LIBRARY)

