/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <iterator>
#include <set>
#include <sstream>
#include <thread>
#include <vector>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>


#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <hidl/HidlTransportSupport.h>

#include "thermal_helper.h"
#include "thermal_map_table_type.h"
#include "thermal_map_table.h"


#define TH_LOG_TAG "thermal_hal"
#define TH_DLOG(_priority_, _fmt_, args...)  /*LOG_PRI(_priority_, TH_LOG_TAG, _fmt_, ##args)*/
#define TH_LOG(_priority_, _fmt_, args...)  LOG_PRI(_priority_, TH_LOG_TAG, _fmt_, ##args)

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using ::android::hardware::thermal::V2_0::implementation::kMtkTempThreshold;
using ::android::hardware::thermal::V2_0::implementation::tz_data;
using ::android::hardware::thermal::V2_0::implementation::tz_data_v1;
using ::android::hardware::thermal::V2_0::implementation::tz_data_v2;
using ::android::hardware::thermal::V2_0::implementation::tz_data_v3;
using ::android::hardware::thermal::V2_0::implementation::cdata;



ThermalHelper::ThermalHelper(const NotificationCallback &cb)
    : thermal_watcher_(new ThermalWatcher(
              std::bind(&ThermalHelper::thermalWatcherCallbackFunc, this, std::placeholders::_1))),
      cb_(cb) {
    thermal_zone_num = 0;
    cooling_device_num = 0;
    tz_map_version = get_tz_map_version();
    ALOGW("%s:tz_map_version %d", __func__, tz_map_version);
    thermal_watcher_->initThermalWatcher(tz_map_version);
    // Need start watching after status map initialized
    is_initialized_ = thermal_watcher_->startThermalWatcher();
    if (!is_initialized_) {
        LOG(FATAL) << "ThermalHAL could not start watching thread properly.";
    }
}

ThrottlingSeverity ThermalHelper::getSeverityFromThresholds(float value, TemperatureType_2_0 type) {
	ThrottlingSeverity ret_hot = ThrottlingSeverity::NONE;
	int typetoint = static_cast<int>(type);

	if (typetoint < 0)
		return ret_hot;

	for (size_t i = static_cast<size_t>(ThrottlingSeverity::SHUTDOWN);
		i > static_cast<size_t>(ThrottlingSeverity::NONE); --i) {
		if (!std::isnan(kMtkTempThreshold[typetoint].hotThrottlingThresholds[i]) && kMtkTempThreshold[typetoint].hotThrottlingThresholds[i] <= value &&
			ret_hot == ThrottlingSeverity::NONE) {
			ret_hot = static_cast<ThrottlingSeverity>(i);
		}
	}

	return ret_hot;
}

float ThermalHelper::get_max_temp(int type) {

	FILE *file;
	float temp = 0;
	char temp_path[TZPATH_LENGTH];
	float max_temp = INVALID_TEMP;
	int ret;

	if (type < 0 || type >= TT_MAX) {
		return INVALID_TEMP;
	}
	for (int i = 0; i < MAX_MUTI_TZ_NUM; i++) {
		ret = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/temp", tz_data[type].tz_idx[i]);
		if (ret < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, ret);
		}
		file = fopen(temp_path, "r");
		if (file == NULL) {
			//ALOGW("%s: failed to open type %d path %s", __func__, type, temp_path);
			break;
		} else {
			if ((fscanf(file, "%f", &temp) > 0) && (temp > max_temp)){
				max_temp = temp;
			}
		}

		ret = fclose(file);
		if (ret) {
			ALOGW("%s: fclose fail: %d", __func__, ret);
		}
	}

	return max_temp;
}

bool ThermalHelper::read_temperature(int type, Temperature_1_0 *ret_temp) {

	float temp;
	bool ret = false;

	if (type < 0 || type > TT_SKIN) {
		return ret;
	}
	temp = get_max_temp(type);
	ret_temp->name = tz_data[type].label;
	ret_temp->type = static_cast<TemperatureType_1_0>(type);
	ret_temp->currentValue = temp * 0.001;
	ret_temp->throttlingThreshold = kMtkTempThreshold[type].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
	ret_temp->shutdownThreshold = kMtkTempThreshold[type].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)];
	ret_temp->vrThrottlingThreshold = kMtkTempThreshold[type].vrThrottlingThreshold;
	ret = true;
	return ret;
}



bool ThermalHelper::read_temperature(int type, Temperature_2_0 *ret_temp) {

	float temp;
	bool ret = false;

	if (type < 0 || type >= TT_MAX) {
		return ret;
	}

	temp = get_max_temp(type);
	ret_temp->name = tz_data[type].label;
	ret_temp->type = static_cast<TemperatureType_2_0>(type);
	ret_temp->value = temp * 0.001;
	ret_temp->throttlingStatus = getSeverityFromThresholds(ret_temp->value, ret_temp->type);
	ret = true;
	return ret;
}

bool ThermalHelper::fillCpuUsages(hidl_vec<CpuUsage> *cpu_usages) {
	int vals, ret;
	ssize_t read;
	uint64_t user, nice, system, idle, active, total;
	char *line = NULL;
	size_t len = 0;
	int size = 0;
	FILE *file = NULL;
	unsigned int max_core_num, cpu_array;
	unsigned int cpu_num = 0;
	FILE *core_num_file = NULL;
	std::vector<CpuUsage> ret_cpu_usages;
	int i;

	/*======get device max core num=======*/
	core_num_file = fopen(CORENUM_PATH, "r");
	if (core_num_file == NULL) {
		ALOGW("thermal_hal: %s: failed to open:CORENUM_PATH %s", __func__, strerror(errno));
		return false;
	}

	if (fscanf(core_num_file, "%*d-%d", &max_core_num) != 1) {
		ALOGW("thermal_hal: %s: unable to parse CORENUM_PATH", __func__);
		ret = fclose(core_num_file);
		if (ret) {
			ALOGW("%s: fclose fail: %d", __func__, ret);
		}
		return false;
	}
	ret = fclose(core_num_file);
	if (ret) {
		ALOGW("%s: fclose fail: %d", __func__, ret);
	}

	cpu_array = sizeof(CPU_ALL_LABEL) / sizeof(CPU_ALL_LABEL[0]);

	if (((max_core_num + 1) > cpu_array) || ((max_core_num + 1) <= 0)) {
		ALOGW("thermal_hal: %s: max_core_num = %d, cpu_array = %d", __func__, max_core_num, cpu_array);
		return false;
	}
	max_core_num += 1;

	ALOGW("%s: max_core_num=%d", __func__, max_core_num);
	/*======get device max core num=======*/


	file = fopen(CPU_USAGE_FILE, "r");
	if (file == NULL) {
		ALOGW("thermal_hal: %s: failed to open: CPU_USAGE_FILE: %s", __func__, strerror(errno));
		return false;
	}

	while ((read = getline(&line, &len, file)) != -1) {
		CpuUsage cpu_usage;

		// Skip non "cpu[0-9]" lines.
		if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
			free(line);
			line = NULL;
			len = 0;
			continue;
		}


		vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                        &nice, &system, &idle);

		free(line);
		line = NULL;
		len = 0;


		if (vals != 5) {
			ALOGW("thermal_hal: %s: failed to read CPU information from file: %s", __func__, strerror(errno));
			ret = fclose(file);
			if (ret) {
				ALOGW("%s: fclose fail: %d", __func__, ret);
			}
			return false;
		}

		active = user + nice + system;
		total = active + idle;

		if (cpu_num < max_core_num) {
			cpu_usage.name = CPU_ALL_LABEL[cpu_num];
			cpu_usage.active = active;
			cpu_usage.total = total;
			cpu_usage.isOnline = 1;
			ret_cpu_usages.emplace_back(std::move(cpu_usage));
		} else {
			ALOGW("thermal_hal: %s: cpu_num %d > max_core_num %d", __func__, cpu_num, max_core_num);
			ret = fclose(file);
			if (ret) {
				ALOGW("%s: fclose fail: %d", __func__, ret);
			}
			return false;
		}

		size++;
	}
	/*if there are hotplug off CPUs, set cpu_usage.total = 0*/
	for (i = size; i < max_core_num; i++) {
		CpuUsage cpu_usage;
		cpu_usage.name = CPU_ALL_LABEL[i];
		cpu_usage.active = 0;
		cpu_usage.total = 0;
		cpu_usage.isOnline = 0;
		ret_cpu_usages.emplace_back(std::move(cpu_usage));
	}

	ALOGW("%s end loop, size %d, cpu_num = %d, max_core_num = %d", __func__, size, cpu_num, max_core_num);

	ret = fclose(file);
	if (ret) {
		ALOGW("%s: fclose fail: %d", __func__, ret);
	}
	*cpu_usages = ret_cpu_usages;
	return true;

}

bool ThermalHelper::fill_temperatures_1_0(hidl_vec<Temperature_1_0> *temperatures) {

	bool ret = false;
	std::vector<Temperature_1_0> ret_temps;
	int current_index = 0;

	for (int i = 0; i <= TT_SKIN; i++) {
		Temperature_1_0 ret_temp;
		if (!is_tz_path_valided(i))
			init_tz_path();

		if (tz_data[i].tz_idx[0] == -1) {
			continue;
		}
		if (read_temperature(i, &ret_temp))	{
			LOG(INFO) << "fill_temperatures_1_0 "
					<< " name: " << ret_temp.name
					<< " throttlingStatus: " << ret_temp.throttlingThreshold
					<< " value: " << ret_temp.currentValue;
			ret_temps.emplace_back(std::move(ret_temp));
			ret = true;
		} else {
			ALOGW("%s: read temp fail type:%d", __func__, i);
			return false;
		}
		++current_index;
	}
	*temperatures = ret_temps;
	return current_index > 0;
}

bool ThermalHelper::fill_temperatures(bool filterType, hidl_vec<Temperature_2_0> *temperatures, TemperatureType_2_0 type) {

	bool ret = false;
	std::vector<Temperature_2_0> ret_temps;
	int typetoint = static_cast<int>(type);

	if (!is_tz_path_valided(typetoint))
		init_tz_path();

	for (int i = 0; i < TT_MAX; i++) {
		Temperature_2_0 ret_temp;
		if ((filterType && i != typetoint) || (tz_data[i].tz_idx[0] == -1)) {
			continue;
		}
		if (read_temperature(i, &ret_temp))	{
			LOG(INFO) << "fill_temperatures "
					<< "filterType" << filterType
					<< " name: " << ret_temp.name
					<< " type: " << android::hardware::thermal::V2_0::toString(ret_temp.type)
					<< " throttlingStatus: " << android::hardware::thermal::V2_0::toString(ret_temp.throttlingStatus)
					<< " value: " << ret_temp.value
					<< " ret_temps size " << ret_temps.size();
			ret_temps.emplace_back(std::move(ret_temp));
			ret = true;
		} else {
			ALOGW("%s: read temp fail type:%d", __func__, i);
			return false;
		}
	}

	*temperatures = ret_temps;

	return ret;
}
bool ThermalHelper::fill_thresholds(bool filterType, hidl_vec<TemperatureThreshold> *Threshold, TemperatureType_2_0 type) {

	FILE *file;
	bool ret = false;
	std::vector<TemperatureThreshold> ret_thresholds;
	int typetoint = static_cast<int>(type);
	char temp_path[TZPATH_LENGTH];
	int r;

	for (int i = 0; i < TT_MAX; i++) {
		TemperatureThreshold ret_threshold;
		if (filterType && i != typetoint) {
			continue;
		}
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", tz_data[i].tz_idx[0]);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		file = fopen(temp_path, "r");
		if (file) {
			ret_threshold = {kMtkTempThreshold[i]};
			LOG(INFO) << "fill_thresholds "
					<< "filterType" << filterType
					<< " name: " << ret_threshold.name
					<< " type: " << android::hardware::thermal::V2_0::toString(ret_threshold.type)
					<< " hotThrottlingThresholds: " << ret_threshold.hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)]
					<< " vrThrottlingThreshold: " << ret_threshold.vrThrottlingThreshold
					<< " ret_thresholds size " << ret_thresholds.size();
			ret_thresholds.emplace_back(std::move(ret_threshold));
			ret = true;
			r = fclose(file);
			if (r) {
				ALOGW("%s: fclose fail: %d", __func__, r);
			}
		}
		else {
			ALOGW("%s: %s not support", __func__, kMtkTempThreshold[i].name.c_str());
			}
	}

	*Threshold = ret_thresholds;
	return ret;
}

bool ThermalHelper::fill_cooling_devices(bool filterType, std::vector<CoolingDevice_2_0> *CoolingDevice, CoolingType type) {

	std::vector<CoolingDevice_2_0> ret_coolings;
	bool ret = false;

	if (!is_cooling_path_valided())
		init_cl_path();

	for (int i = 0; i < MAX_COOLING; i++) {
		if (filterType && type != cdata[i].cl_2_0.type) {
			continue;
		}
		if (cdata[i].cl_idx != -1) {
			CoolingDevice_2_0 coolingdevice;
			coolingdevice.name = cdata[i].cl_2_0.name;
			coolingdevice.type = cdata[i].cl_2_0.type;
			coolingdevice.value = cdata[i].cl_2_0.value;
			LOG(INFO) << "fill_cooling_devices "
						<< "filterType" << filterType
						<< " name: " << coolingdevice.name
						<< " type: " << android::hardware::thermal::V2_0::toString(coolingdevice.type)
						<< " value: " << coolingdevice.value
						<< " ret_coolings size " << ret_coolings.size();
			ret_coolings.emplace_back(std::move(coolingdevice));
			ret = true;
		}
	}
	*CoolingDevice = ret_coolings;
	return ret;
}



bool ThermalHelper::init_cl_path() {

	char temp_path[CDPATH_LENGTH];
	char temp_value_path[CDPATH_LENGTH];
	char buf[CDNAME_SZ];
	int fd = -1;
	int fd_value = -1;
	int read_len = 0;
	int i = 0;
	bool ret = true;
	int r;

	/*initial cdata*/
	for (int j = 0; j < MAX_COOLING; ++j) {
		cdata[j].cl_2_0.value = 0;
		cdata[j].cl_idx = -1;
	}
	cooling_device_num = 0;
	while(1) {
		r = snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", i);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out cooling path", __func__);
			cooling_device_num = i;
			break;
		} else {
			CoolingDevice_2_0 coolingdevice;
			read_len = read(fd, buf, CDNAME_SZ);
			for (int j = 0; j < MAX_COOLING; ++j) {
				if (std::strncmp(buf, cdata[j].cl_2_0.name.c_str(), std::strlen(cdata[j].cl_2_0.name.c_str())) == 0) {
					cdata[j].cl_idx = i;
					r = snprintf(temp_value_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/cur_state", i);
					if (r < 0) {
						ALOGW("%s: snprintf fail: %d", __func__, r);
					}
					fd_value = open(temp_value_path, O_RDONLY);
					if (fd_value == -1) {
						ALOGW("%s:get value fail", __func__);
						ret = false;
						break;
					} else {
						read_len = read(fd_value, buf, CDNAME_SZ);
						cdata[j].cl_2_0.value = std::atoi(buf);
						LOG(INFO) << "init_cl_path"
									<< "cl_idx" << cdata[j].cl_idx
									<< " name: " << cdata[j].cl_2_0.name
									<< " value: " << cdata[j].cl_2_0.value;
					}
					close(fd_value);
				}
			}

		}
		i++;
		close(fd);
	}
	return ret;
}

bool ThermalHelper::is_cooling_path_valided() {
	char temp_path[CDPATH_LENGTH];
	char buf[CDNAME_SZ];
	int fd = -1;
	int read_len = 0;
	bool ret = true;
	int r;

	/*check if cooling device number are changed*/
	r = snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", (cooling_device_num - 1));
	if (r < 0) {
		ALOGW("%s: snprintf fail: %d", __func__, r);
	}
	fd = open(temp_path, O_RDONLY);
	if (fd == -1) {
		LOG(INFO) << "cl_num are changed" << cooling_device_num;
		return false;
	} else {
		close(fd);
	}

	r = snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", cooling_device_num);
	if (r < 0) {
		ALOGW("%s: snprintf fail: %d", __func__, r);
	}	
	fd = open(temp_path, O_RDONLY);
	if (fd != -1) {
		close(fd);
		LOG(INFO) << "cl_num are increased" << cooling_device_num;
		return false;
	}


	for (int i = 0; i < MAX_COOLING; i++) {
		if (cdata[i].cl_idx != -1) {
			r = snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", cdata[i].cl_idx);
			if (r < 0) {
				ALOGW("%s: snprintf fail: %d", __func__, r);
			}
			fd = open(temp_path, O_RDONLY);
			if (fd == -1) {
				ALOGW("%s:cl path error %d %s" , __func__, i, temp_path);
					ret = false;
				break;
			} else {
				read_len = read(fd, buf, CDNAME_SZ);
				if (std::strncmp(buf, cdata[i].cl_2_0.name.c_str(), std::strlen(cdata[i].cl_2_0.name.c_str())) != 0) {
					ret = false;
					close(fd);
					LOG(INFO) << " cl name mismatch "<< i << cdata[i].cl_2_0.name;
					break;
				}
			close(fd);
			}
		}
	}

	return ret;
}

bool ThermalHelper::is_tz_path_valided(int type) {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	bool ret = true;
	int r;

	if (type < 0 || type >= TT_MAX) {
		return false;
	}
	/*check if thermal zone number are changed*/
	r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", (thermal_zone_num - 1));
	if (r < 0) {
		ALOGW("%s: snprintf fail: %d", __func__, r);
	}
	fd = open(temp_path, O_RDONLY);
	if (fd == -1) {
		LOG(INFO) << "thermal_zone_num are changed" << thermal_zone_num;
		return false;
	} else {
		close(fd);
	}

	r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", thermal_zone_num);
	if (r < 0) {
		ALOGW("%s: snprintf fail: %d", __func__, r);
	}
	fd = open(temp_path, O_RDONLY);
	if (fd != -1) {
		close(fd);
		LOG(INFO) << "thermal_zone_num are increased" << thermal_zone_num;
		return false;
	}
	if (tz_data[type].tz_idx[0] != -1) {
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", tz_data[type].tz_idx[0]);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:tz path error %d %s" , __func__, type, temp_path);
			ret = false;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			/*/sys/class/thermal/thermal_zone{$tz_idx}/type should equal tzName*/
			if (std::strncmp(buf, tz_data[type].tzName, strlen(tz_data[type].tzName)) != 0) {
				ret = false;
				LOG(INFO) << " tz name mismatch "<< type << tz_data[type].tzName;
			}
			close(fd);
		}
	}

	return ret;
}

void ThermalHelper::get_tz_map() {
	int v2_idx = 0;
	int v3_idx = 0;
	int r;

	if (tz_map_version == TZ_MAP_LEGANCY) {
		for (int j = 0; j < TT_MAX; ++j) {
			r = snprintf(tz_data[j].tzName, sizeof(tz_data_v1[j].tzName), "%s", tz_data_v1[j].tzName);
			if (r < 0) {
				ALOGW("%s: snprintf fail: %d", __func__, r);
			}
			tz_data[j].muti_tz_num = 1;
			tz_data[j].tz_idx[0] = tz_data_v1[j].tz_idx[0];
		}
	} else if (tz_map_version == TZ_MAP_V2) {
		for (int j = 0; j < TT_MAX; ++j) {
			r = snprintf(tz_data[j].tzName, sizeof(tz_data_v2[v2_idx].tzName), "%s", tz_data_v2[v2_idx].tzName);
			if (r < 0) {
				ALOGW("%s: snprintf fail: %d", __func__, r);
			}
			tz_data[j].muti_tz_num = tz_data_v2[v2_idx].muti_tz_num;
			for (int l = 0; l < tz_data[j].muti_tz_num ; ++l)
				tz_data[j].tz_idx[l] = tz_data_v2[v2_idx + l].tz_idx[0];
			if (tz_data[j].muti_tz_num > 1)
				v2_idx += (tz_data[j].muti_tz_num - 1);
			v2_idx++;
		}
	} else if (tz_map_version == TZ_MAP_V3) {
		for (int j = 0; j < TT_MAX; ++j) {
			r = snprintf(tz_data[j].tzName, sizeof(tz_data_v3[v3_idx].tzName), "%s", tz_data_v3[v3_idx].tzName);
			if (r < 0) {
				ALOGW("%s: snprintf fail: %d", __func__, r);
			}
			tz_data[j].muti_tz_num = tz_data_v3[v3_idx].muti_tz_num;
			for (int l = 0; l < tz_data[j].muti_tz_num ; ++l)
				tz_data[j].tz_idx[l] = tz_data_v3[v3_idx + l].tz_idx[0];
			if (tz_data[j].muti_tz_num > 1)
				v3_idx += (tz_data[j].muti_tz_num - 1);
			v3_idx++;
		}
	}

	for (int j = 0; j < TT_MAX; ++j) {
		ALOGW("%s: tz%d, name=%s, label=%s, muti_tz_num=%d", __func__, j, tz_data[j].tzName, tz_data[j].label, tz_data[j].muti_tz_num);
		ALOGW("tz_idx0:%d, tz_idx1:%d,tz_idx2:%d,tz_idx3:%d,tz_idx4:%d,", tz_data[j].tz_idx[0], tz_data[j].tz_idx[1], tz_data[j].tz_idx[2], tz_data[j].tz_idx[3], tz_data[j].tz_idx[4]);
	}

}

int ThermalHelper::get_tz_map_version() {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	int i = 0;
	int version = 0;
	int soc_max_exist = 0;
	int ap_ntc_exist = 0;
	int r;

	while(1) {
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", i);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out tz path", __func__);
			break;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			if (std::strncmp(buf, tz_data_v1[0].tzName, strlen(tz_data_v1[0].tzName)) == 0) {
				version = TZ_MAP_LEGANCY;
				close(fd);
				break;
				ALOGW("support legancy tz data");
			} else {
				if (std::strncmp(buf, tz_data_v2[0].tzName, strlen(tz_data_v2[0].tzName)) == 0)
					soc_max_exist = 1;
				if (std::strncmp(buf, tz_data_v2[TT_SKIN].tzName, strlen(tz_data_v2[TT_SKIN].tzName)) == 0)
					ap_ntc_exist = 1;
			}
			i++;
			close(fd);
		}
	}

	if ((soc_max_exist == 1) && (ap_ntc_exist == 1))
		version = TZ_MAP_V2;
	else if ((soc_max_exist == 1) && (ap_ntc_exist == 0))
		version = TZ_MAP_V3;
	else
		version = TZ_MAP_LEGANCY;

	return version;
}

void ThermalHelper::init_tz_path() {

	if (tz_map_version == TZ_MAP_LEGANCY)
		init_tz_path_v1();
	else if (tz_map_version == TZ_MAP_V2)
		init_tz_path_v2();
	else if (tz_map_version == TZ_MAP_V3)
		init_tz_path_v3();
	get_tz_map();
}

void ThermalHelper::init_tz_path_v1() {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	int i = 0;
	int r;

	/*initial tz_data*/
	for (int j = 0; j < TT_MAX; ++j) {
		for (int k = 0; k < MAX_MUTI_TZ_NUM; ++k)
			tz_data[j].tz_idx[k] = -1;
	}
	thermal_zone_num = 0;

	while(1) {
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", i);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out tz path", __func__);
			thermal_zone_num = i;
			break;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			for (int j = 0; j < TT_MAX; ++j) {
				if (std::strncmp(buf, tz_data_v1[j].tzName, strlen(tz_data_v1[j].tzName)) == 0) {
					tz_data_v1[j].tz_idx[0] = i;
					ALOGW("tz_data_v1[%d].tz_idx:%d",j,i);
				}
			}
			i++;
			close(fd);
		}
	}

}

void ThermalHelper::init_tz_path_v2() {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	int i = 0;
	int r;

	/*initial tz_data*/
	for (int j = 0; j < TT_MAX; ++j) {
		for (int k = 0; k < MAX_MUTI_TZ_NUM; ++k)
			tz_data[j].tz_idx[k] = -1;
		tz_data_v2[j].tz_idx[0] = -1;
	}
	thermal_zone_num = 0;

	while(1) {
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", i);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out tz path", __func__);
			thermal_zone_num = i;
			break;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			for (int j = 0; j < TZ_DATA_NUM_MAX; ++j) {
				if (tz_data_v2[j].tz_idx[0] != -1)
					continue;
				if (std::strncmp(buf, tz_data_v2[j].tzName, strlen(tz_data_v2[j].tzName)) == 0) {
					tz_data_v2[j].tz_idx[0] = i;
					ALOGW("tz_data_v2[%d].tz_idx:%d",j,i);
				}
			}
			i++;
			close(fd);
		}
	}

}

void ThermalHelper::init_tz_path_v3() {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	int i = 0;
	int r;

	/*initial tz_data*/
	for (int j = 0; j < TT_MAX; ++j) {
		for (int k = 0; k < MAX_MUTI_TZ_NUM; ++k)
			tz_data[j].tz_idx[k] = -1;
		tz_data_v3[j].tz_idx[0] = -1;
	}
	thermal_zone_num = 0;

	while(1) {
		r = snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", i);
		if (r < 0) {
			ALOGW("%s: snprintf fail: %d", __func__, r);
		}
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out tz path", __func__);
			thermal_zone_num = i;
			break;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			for (int j = 0; j < TZ_DATA_NUM_MAX; ++j) {
				if (tz_data_v3[j].tz_idx[0] != -1)
					continue;
				if (std::strncmp(buf, tz_data_v3[j].tzName, strlen(tz_data_v3[j].tzName)) == 0) {
					tz_data_v3[j].tz_idx[0] = i;
					ALOGW("tz_data_v3[%d].tz_idx:%d",j,i);
				}
			}
			i++;
			close(fd);
		}
	}

}


// This is called in the different thread context and will update sensor_status
// uevent_sensors is the set of sensors which trigger uevent from thermal core driver.
bool ThermalHelper::thermalWatcherCallbackFunc(const std::set<std::string> &uevent_sensors) {
	std::vector<Temperature_2_0> temps;
	bool thermal_triggered = false;
	Temperature_2_0 temp;

	if (uevent_sensors.size() != 0) {
	// writer lock
	std::unique_lock<std::mutex> _lock(sensor_status_map_mutex_);
		for (const auto &name : uevent_sensors) {
			for (int i = 0; i < TT_MAX; i++) {
				if (strncmp(name.c_str(), tz_data[i].label, strlen(tz_data[i].label)) == 0) {
					if (!is_tz_path_valided(i))
						init_tz_path();
					if (read_temperature(i,&temp))
						temps.push_back(temp);
				}
			}
		}
		thermal_triggered = true;
	}
	if (!temps.empty() && cb_) {
		cb_(temps);
	}

	return thermal_triggered;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
