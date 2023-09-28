#
# Copyright (C) 2023 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_DEVICE),camellia)
include $(call all-makefiles-under,$(LOCAL_PATH))

VENDOR_SYMLINKS := \
    $(TARGET_OUT_VENDOR)/lib/hw \
    $(TARGET_OUT_VENDOR)/lib64/hw

$(VENDOR_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	$(hide) echo "Making vendor symlinks"
	@mkdir -p $(TARGET_OUT_VENDOR)/lib/hw
	@mkdir -p $(TARGET_OUT_VENDOR)/lib64/hw
	@ln -sf $(TARGET_BOARD_PLATFORM)/libaiselector.so $(TARGET_OUT_VENDOR)/lib/libaiselector.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libdpframework.so $(TARGET_OUT_VENDOR)/lib/libdpframework.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libmtk_drvb.so $(TARGET_OUT_VENDOR)/lib/libmtk_drvb.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libpq_prot.so $(TARGET_OUT_VENDOR)/lib/libpq_prot.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libaiselector.so $(TARGET_OUT_VENDOR)/lib64/libaiselector.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libdpframework.so $(TARGET_OUT_VENDOR)/lib64/libdpframework.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libmtk_drvb.so $(TARGET_OUT_VENDOR)/lib64/libmtk_drvb.so
	@ln -sf $(TARGET_BOARD_PLATFORM)/libpq_prot.so $(TARGET_OUT_VENDOR)/lib64/libpq_prot.so
	$(hide) touch $@

ALL_DEFAULT_INSTALLED_MODULES += $(VENDOR_SYMLINKS)

endif
