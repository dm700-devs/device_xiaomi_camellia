/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THERMAL_MAP_TABLE_H
#define THERMAL_MAP_TABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <log/log.h>
#include <hardware/thermal.h>
#include <array>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <android/hardware/thermal/2.0/IThermal.h>
#include "thermal_map_table_type.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermal;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;

using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using TemperatureType_1_0 = ::android::hardware::thermal::V1_0::TemperatureType;
using TemperatureType_2_0 = ::android::hardware::thermal::V2_0::TemperatureType;

const char *CPU_ALL_LABEL[] = {"CPU0", "CPU1", "CPU2", "CPU3", "CPU4", "CPU5", "CPU6", "CPU7", "CPU8", "CPU9"};

TZ_DATA tz_data[TT_MAX] = {
	{CPU_TZ_NAME, "CPU", {-1, -1, -1, -1, -1}, 1},/*TT_CPU*/
	{GPU_TZ_NAME, "GPU", {-1, -1, -1, -1, -1}, 1},/*TT_GPU*/
	{BATTERY_TZ_NAME,"BATTERY", {-1, -1, -1, -1, -1}, 1},/*TT_BATTERY*/
	{SKIN_TZ_NAME,"SKIN", {-1, -1, -1, -1, -1}, 1},/*TT_SKIN*/
	{USB_PORT_TZ_NAME,"USB_PORT", {-1, -1, -1, -1, -1}, 1},/*TT_USB_PORT not support*/
	{POWER_AMPLIFIER_TZ_NAME,"POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 1},/*TT_POWER_AMPLIFIER*/
	{BCL_VOLTAGE_TZ_NAME,"BCL_VOLTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_VOLTAGE not support*/
	{BCL_CURRENT_TZ_NAME,"BCL_CURRENT", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_CURRENT not support*/
	{BCL_PERCENTAGE_TZ_NAME,"BCL_PERCENTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_PERCENTAGE not support*/
	{NPU_TZ_NAME,"NPU", {-1, -1, -1, -1, -1}, 1},/*TT_NPU*/
};
/*legancy thermal zone data*/
TZ_DATA tz_data_v1[TT_MAX] = {
	{CPU_TZ_NAME, "CPU", {-1, -1, -1, -1, -1}, 1},/*TT_CPU*/
	{GPU_TZ_NAME, "GPU", {-1, -1, -1, -1, -1}, 1},/*TT_GPU*/
	{BATTERY_TZ_NAME,"BATTERY", {-1, -1, -1, -1, -1}, 1},/*TT_BATTERY*/
	{SKIN_TZ_NAME,"SKIN", {-1, -1, -1, -1, -1}, 1},/*TT_SKIN*/
	{USB_PORT_TZ_NAME,"USB_PORT", {-1, -1, -1, -1, -1}, 1},/*TT_USB_PORT not support*/
	{POWER_AMPLIFIER_TZ_NAME,"POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 1},/*TT_POWER_AMPLIFIER*/
	{BCL_VOLTAGE_TZ_NAME,"BCL_VOLTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_VOLTAGE not support*/
	{BCL_CURRENT_TZ_NAME,"BCL_CURRENT", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_CURRENT not support*/
	{BCL_PERCENTAGE_TZ_NAME,"BCL_PERCENTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_PERCENTAGE not support*/
	{NPU_TZ_NAME,"NPU", {-1, -1, -1, -1, -1}, 1},/*TT_NPU*/
};
#define TZ_DATA_NUM_MAX 11
/*common kernel thermal zone data*/
TZ_DATA tz_data_v2[TZ_DATA_NUM_MAX] = {
	{"soc_max", "CPU", {-1, -1, -1, -1, -1}, 1},/*TT_CPU*/
	{"soc_max", "GPU", {-1, -1, -1, -1, -1}, 1},/*TT_GPU*/
	{"battery","BATTERY", {-1, -1, -1, -1, -1}, 1},/*TT_BATTERY*/
	{"ap_ntc","SKIN", {-1, -1, -1, -1, -1}, 1},/*TT_SKIN*/
	{"notsupport","USB_PORT", {-1, -1, -1, -1, -1}, 1},/*TT_USB_PORT not support*/
	{"nrpa_ntc","POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 2},/*TT_POWER_AMPLIFIER*/
	{"ltepa_ntc","POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 2},/*TT_POWER_AMPLIFIER*/	
	{"notsupport","BCL_VOLTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_VOLTAGE not support*/
	{"notsupport","BCL_CURRENT", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_CURRENT not support*/
	{"notsupport","BCL_PERCENTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_PERCENTAGE not support*/
	{"soc_max","NPU", {-1, -1, -1, -1, -1}, 1},/*TT_NPU*/
};
 
TZ_DATA tz_data_v3[TZ_DATA_NUM_MAX] = {
	{"soc_max", "CPU", {-1, -1, -1, -1, -1}, 1},/*TT_CPU*/
	{"soc_max", "GPU", {-1, -1, -1, -1, -1}, 1},/*TT_GPU*/
	{"battery","BATTERY", {-1, -1, -1, -1, -1}, 1},/*TT_BATTERY*/
	{"mtktsAP","SKIN", {-1, -1, -1, -1, -1}, 1},/*TT_SKIN*/
	{"notsupport","USB_PORT", {-1, -1, -1, -1, -1}, 1},/*TT_USB_PORT not support*/
	{"mtktsbtsnrpa","POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 2},/*TT_POWER_AMPLIFIER*/
	{"mtktsbtsmdpa","POWER_AMPLIFIER", {-1, -1, -1, -1, -1}, 2},/*TT_POWER_AMPLIFIER*/	
	{"notsupport","BCL_VOLTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_VOLTAGE not support*/
	{"notsupport","BCL_CURRENT", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_CURRENT not support*/
	{"notsupport","BCL_PERCENTAGE", {-1, -1, -1, -1, -1}, 1},/*TT_BCL_PERCENTAGE not support*/
	{"soc_max","NPU", {-1, -1, -1, -1, -1}, 1},/*TT_NPU*/
};
COOLING_DATA cdata[MAX_COOLING] = {
	{
		{
			.type = CoolingType::MODEM,
			.name = "cl-amutt-upper",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "cl-amutt-lower",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu_adaptive_0",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu_adaptive_1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu_adaptive_2",
 			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-backlight00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-backlight01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-backlight02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-backlight03",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "mtk-cl-bcct00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "mtk-cl-bcct01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "mtk-cl-bcct02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "abcct_lcmoff",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "abcct",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-cam00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-cam01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-cam02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-cam-urgent",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu03",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu04",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu05",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu06",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu07",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu08",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu09",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu10",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu11",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu12",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu13",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu14",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu15",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu16",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu17",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu18",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu19",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu20",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu21",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu22",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu23",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu24",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu25",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu26",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu27",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu28",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu29",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu30",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu31",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu32",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "cpu33",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown03",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown04",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown05",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown06",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-kshutdown07",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt03",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt04",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt05",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt06",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt07",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt08",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-noIMS",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mdoff",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-adp-mutt",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt00-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt01-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt02-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt03-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt04-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt05-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt06-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt07-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mutt08-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr00-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr01-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr02-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr03-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr04-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr05-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr06-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr07-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-tx-pwr08-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-noIMS-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-mdoff-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-adp-mutt-nr",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtk-cl-scg-off",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtkclNR",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-shutdown00",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-shutdown01",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtk-cl-shutdown02",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "mtktscpu-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsbuck-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsAP-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsAP-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsbif-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsbuck-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mt6359dcxo-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mt6359tsx-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mt6359vcore-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mt6359vgpu-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mt6359vproc-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::BATTERY,
			.name = "mtktsbattery-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktscharger-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktscharger2-rst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktsdram-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs0-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs1-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs2-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs3-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs4-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "tzimgs5-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mtktspa-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktspmic-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "sysrst.6357buck1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "sysrst.6357buck2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktswmt-sysrst",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktswmt-pa1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtktswmt-pa2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::COMPONENT,
			.name = "mtkclVR_FPS",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-cpufreq-0",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-cpufreq-1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mutt-pa1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mutt-pa2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mutt-pa1_no_ims",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "mutt-pa2_no_ims",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "tx-pwr-pa1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "tx-pwr-pa2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::MODEM,
			.name = "scg-off-pa2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-devfreq-0",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-devfreq-1",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-devfreq-2",
			.value = 0,
		},
		-1
	},
	{
		{
			.type = CoolingType::CPU,
			.name = "thermal-devfreq-3",
			.value = 0,
		},
		-1
	},
};
TemperatureThreshold kMtkTempThreshold[TT_MAX] = {
	{
		.type = TemperatureType::CPU,
		.name = "CPU",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 85, 90, 100, 117}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 85,
	},
	{
		.type = TemperatureType::GPU,
		.name = "GPU",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 85, 90, 100, 117}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 85,
	},
	{
		.type = TemperatureType::BATTERY,
		.name = "BATTERY",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 50, 55, 59, 60.0}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 50,
	},
	{
		.type = TemperatureType::SKIN,
		.name = "SKIN",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 50, 70, 80, 90}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 50,
	},
	{
		.type = TemperatureType::USB_PORT,
		.name = "USB_PORT",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = NAN,
	},
	{
		.type = TemperatureType::POWER_AMPLIFIER,
		.name = "POWER_AMPLIFIER",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 68, 90, 100, 110}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = NAN,
	},
	{
		.type = TemperatureType::BCL_VOLTAGE,
		.name = "BCL_VOLTAGE",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 50,
	},
	{
		.type = TemperatureType::BCL_CURRENT,
		.name = "BCL_CURRENT",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 50,
	},
	{
		.type = TemperatureType::BCL_PERCENTAGE,
		.name = "BCL_PERCENTAGE",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60.0}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = 50,
	},
	{
		.type = TemperatureType::NPU,
		.name = "NPU",
		.hotThrottlingThresholds = {{NAN, NAN, NAN, 85, 90, 100, 117}},
		.coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
		.vrThrottlingThreshold = NAN,
	},
};


}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif  // THERMAL_MAP_TABLE_H