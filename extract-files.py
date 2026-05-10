#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    "device/xiaomi/camellia",
    "hardware/mediatek",
    "hardware/mediatek/libmtkperf_client",
    "hardware/xiaomi",
]


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f"{lib}_{partition}" if partition == "vendor" else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (): lib_fixup_vendor_suffix,
}


blob_fixups: blob_fixups_user_type = {
    ('vendor/bin/hw/android.hardware.gnss-service.mediatek',
     'vendor/lib64/hw/android.hardware.gnss-impl-mediatek.so'): blob_fixup()
        .replace_needed('android.hardware.gnss-V1-ndk_platform.so', 'android.hardware.gnss-V1-ndk.so'),

    ('vendor/lib/libnvram.so',
     'vendor/lib64/libnvram.so',
     'vendor/lib64/libsysenv.so'): blob_fixup()
        .add_needed('libbase_shim.so'),

    'vendor/bin/hw/mtkfusionrild': blob_fixup()
        .add_needed('libutils-v32.so'),

    ('vendor/bin/mnld',
     'vendor/lib64/libaalservice.so',
     'vendor/lib64/libcam.utils.sensorprovider.so'): blob_fixup()
        .replace_needed('libsensorndkbridge.so', 'android.hardware.sensors@1.0-convert-shared.so'),

    'vendor/etc/vintf/manifest/manifest_media_c2_V1_2_default.xml': blob_fixup()
        .regex_replace('1.1', '1.2'),

    'vendor/lib/hw/audio.primary.mt6833.so' : blob_fixup()
        .replace_needed('libalsautils.so', 'libalsautils-v31.so')
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),

    'vendor/lib/librt_extamp_intf.so': blob_fixup()
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),

    'vendor/lib/libvcodec_oal.so': blob_fixup()
        .clear_symbol_version('__aeabi_memcpy')
        .clear_symbol_version('__aeabi_memset')
        .clear_symbol_version('__gnu_Unwind_Find_exidx'),

    ('vendor/lib64/hw/android.hardware.camera.provider@2.6-impl-mediatek.so',
     'vendor/lib64/libmtkcam_stdutils.so'): blob_fixup()
        .replace_needed('libutils.so', 'libutils-v32.so'),

    'vendor/lib64/hw/hwcomposer.mt6833.so': blob_fixup()
        .add_needed('libprocessgroup_shim.so'),

    'vendor/lib64/hw/vendor.mediatek.hardware.pq@2.13-impl.so': blob_fixup()
        .replace_needed('libutils.so', 'libutils-v32.so')
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),

    ('vendor/lib64/libalLDC.so',
     'vendor/lib64/libalhLDC.so'): blob_fixup()
        .clear_symbol_version('AHardwareBuffer_allocate')
        .clear_symbol_version('AHardwareBuffer_describe')
        .clear_symbol_version('AHardwareBuffer_lock')
        .clear_symbol_version('AHardwareBuffer_release')
        .clear_symbol_version('AHardwareBuffer_unlock'),

    'vendor/lib64/libgoodixhwfingerprint.so': blob_fixup()
        .replace_needed('libvendor.goodix.hardware.biometrics.fingerprint@2.1.so', 'vendor.goodix.hardware.biometrics.fingerprint@2.1.so'),

    'vendor/lib64/libmnl.so' : blob_fixup()
        .add_needed('libcutils.so'),

    ('vendor/lib64/libteei_daemon_vfs.so',
     'vendor/lib64/libSQLiteModule_VER_ALL.so',
     'vendor/lib64/lib3a.flash.so',
     'vendor/lib64/lib3a.ae.stat.so',
     'vendor/lib64/lib3a.sensors.color.so',
     'vendor/lib64/lib3a.sensors.flicker.so',
     'vendor/lib64/libaaa_ltm.so'): blob_fixup()
        .add_needed('liblog.so'),

    ('vendor/lib64/libwvhidl.so',
     'vendor/lib64/mediadrm/libwvdrmengine.so'): blob_fixup()
        .replace_needed('libprotobuf-cpp-lite-3.9.1.so', 'libprotobuf-cpp-full-3.9.1.so')
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so'),

    'vendor/lib64/hw/sensors.mt6833.so': blob_fixup()
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so'),

    'vendor/bin/hw/android.hardware.media.c2@1.2-mediatek-64b': blob_fixup()
        .replace_needed('libavservices_minijail_vendor.so', 'libavservices_minijail.so')
        .replace_needed('libcodec2_hidl@1.0.so', 'libcodec2_hidl@1.0-v31.so')
        .replace_needed('libcodec2_hidl@1.1.so', 'libcodec2_hidl@1.1-v31.so')
        .replace_needed('libcodec2_hidl@1.2.so', 'libcodec2_hidl@1.2-v31.so')
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so'),

    'vendor/lib64/libcodec2_hidl@1.0-v31.so': blob_fixup()
        .replace_needed('libstagefright_bufferqueue_helper.so', 'libstagefright_bufferqueue_helper-v33.so')
        .replace_needed('libcodec2_hidl_plugin.so', 'libcodec2_hidl_plugin-v31.so')
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libui.so', 'libui-v34.so')
        .add_needed('libbase_shim.so'),

    'vendor/lib64/libcodec2_hidl@1.1-v31.so': blob_fixup()
        .replace_needed('libstagefright_bufferqueue_helper.so', 'libstagefright_bufferqueue_helper-v33.so')
        .replace_needed('libcodec2_hidl@1.0.so', 'libcodec2_hidl@1.0-v31.so')
        .replace_needed('libcodec2_hidl_plugin.so', 'libcodec2_hidl_plugin-v31.so')
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libui.so', 'libui-v34.so')
        .add_needed('libbase_shim.so'),

    'vendor/lib64/libcodec2_hidl@1.2-v31.so': blob_fixup()
        .replace_needed('libstagefright_bufferqueue_helper.so', 'libstagefright_bufferqueue_helper-v33.so')
        .replace_needed('libcodec2_hidl@1.0.so', 'libcodec2_hidl@1.0-v31.so')
        .replace_needed('libcodec2_hidl@1.1.so', 'libcodec2_hidl@1.1-v31.so')
        .replace_needed('libcodec2_hidl_plugin.so', 'libcodec2_hidl_plugin-v31.so')
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libui.so', 'libui-v34.so')
        .add_needed('libbase_shim.so'),

    ('vendor/lib64/libcodec2_hidl_plugin-v31.so',
     'vendor/lib64/libsfplugin_ccodec_utils-v31.so'): blob_fixup()
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so'),

    ('vendor/lib64/libcodec2_mtk_c2store.so',
     'vendor/lib64/libcodec2_mtk_vdec.so',
     'vendor/lib64/libcodec2_mtk_venc.so',
     'vendor/lib64/libcodec2_vpp_qt_plugin.so',
     'vendor/lib64/libcodec2_vpp_rs_plugin.so'): blob_fixup()
        .replace_needed('libcodec2_soft_common.so', 'libcodec2_soft_common-v31.so')
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libsfplugin_ccodec_utils.so', 'libsfplugin_ccodec_utils-v31.so')
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so')
        .replace_needed('libui.so', 'libui-v34.so'),

    'vendor/lib64/libcodec2_soft_common-v31.so': blob_fixup()
        .replace_needed('libcodec2_vndk.so', 'libcodec2_vndk-v31.so')
        .replace_needed('libsfplugin_ccodec_utils.so', 'libsfplugin_ccodec_utils-v31.so')
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so')
        .replace_needed('libui.so', 'libui-v34.so'),

    'vendor/lib64/libcodec2_vndk-v31.so': blob_fixup()
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v33.so')
        .replace_needed('libui.so', 'libui-v34.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    "camellia",
    "xiaomi",
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == "__main__":
    utils = ExtractUtils.device(module)
    utils.run()
