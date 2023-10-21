/*
 * Copyright (C) 2021-2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libinit_dalvik_heap.h>
#include <libinit_variant.h>

#include "vendor_init.h"

static const variant_info_t camelliain_info = {
    .hwc_value = "camelliain",
    .sku_value = "camellia",

    .brand = "Redmi",
    .device = "camellia",
    .model = "Redmi Note 10T 5G",
    .build_fingerprint = "Redmi/camellia_in/camelliain:12/SP1A.210812.016/V14.0.6.0.TKSMIXM:user/release-keys",

    .nfc = false,
};

static const variant_info_t camelliacn_info = {
    .hwc_value = "camelliacn",
    .sku_value = "camelliar",

    .brand = "Redmi",
    .device = "camellia",
    .model = "Redmi Note 11SE",
    .build_fingerprint = "Redmi/camellia/camellia:12/SP1A.210812.016/V14.0.6.0.TKSMIXM:user/release-keys",

    .nfc = false,
};

static const variant_info_t camellia_info = {
    .hwc_value = "camelliacn",
    .sku_value = "camellia",

    .brand = "Redmi",
    .device = "camellia",
    .model = "Redmi Note 10 5G",
    .build_fingerprint = "Redmi/camellia/camellia:12/SP1A.210812.016/V14.0.6.0.TKSMIXM:user/release-keys",

    .nfc = false,
};

static const variant_info_t camelliagl_info = {
    .hwc_value = "camelliagl",
    .sku_value = "camellian",

    .brand = "Redmi",
    .device = "camellian",
    .model = "Redmi Note 10 5G",
    .build_fingerprint = "Redmi/camellian_global/camellian:12/SP1A.210812.016/V14.0.6.0.TKSMIXM:user/release-keys",

    .nfc = true,
};

static const variant_info_t camellianp_info = {
    .hwc_value = "camelliagl",
    .sku_value = "camellianp",

    .brand = "POCO",
    .device = "camellian",
    .model = "POCO M3 Pro 5G",
    .build_fingerprint = "POCO/camellian_p_global/camellian:12/SP1A.210812.016/V14.0.6.0.TKSMIXM:user/release-keys",

    .nfc = true,
};

static const std::vector<variant_info_t> variants = {
    camelliain_info,
    camelliacn_info,
    camellia_info,
    camelliagl_info,
    camellianp_info,
};

void vendor_load_properties() {
    search_variant(variants);
    set_dalvik_heap();
}
