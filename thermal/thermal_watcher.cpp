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
#include <cutils/uevent.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <chrono>
#include <fstream>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "thermal_watcher.h"
#include "thermal_map_table_type.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <sys/un.h>


#define THERMAL_HAL_SOCKET_NAME	"thermal_hal_socket"
#define THERMAL_HAL_SOCKET_IDENTIFY_WORD "TYPE"
#define BUFFER_SIZE 	(128)


namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using std::chrono_literals::operator""ms;
extern TZ_DATA tz_data[TT_MAX];
extern TemperatureThreshold kMtkTempThreshold[TT_MAX];

static int create_socket_server(void)
{
	int ret, socket_fd;

	socket_fd = android_get_control_socket(THERMAL_HAL_SOCKET_NAME);
	if (socket_fd == -1) {
		ALOGE("Failed to create a local socket\n");
		return -1;
	}
	ret = listen(socket_fd, 20);
	if (ret == -1) {
		ALOGE("Failed to listen socket\n");
		close(socket_fd);
		return -1;
	}

	return socket_fd;
}

void ThermalWatcher::initThermalWatcher(int tz_version) {

	if (tz_version == TZ_MAP_LEGANCY)
		event_fd_.reset((TEMP_FAILURE_RETRY(uevent_open_socket(64 * 1024, true))));
	else
		event_fd_.reset((TEMP_FAILURE_RETRY(create_socket_server())));

	if (event_fd_.get() < 0) {
		LOG(ERROR) << "failed to open thermal hal socket";
		is_polling_ = true;
		return;
	}

	fcntl(event_fd_, F_SETFL, O_NONBLOCK);

	looper_->addFd(event_fd_.get(), 0, Looper::EVENT_INPUT, nullptr, nullptr);
	is_polling_ = false;
	thermal_triggered_ = true;
	tz_map_version = tz_version;
}

bool ThermalWatcher::startThermalWatcher() {
	if (cb_) {
		auto ret = this->run("ThermalWatcherThread", PRIORITY_HIGHEST);
		if (ret != NO_ERROR) {
			LOG(ERROR) << "ThermalWatcherThread start fail";
			return false;
		} else {
			LOG(INFO) << "ThermalWatcherThread started";
			return true;
		}
	}
	return false;
}


void ThermalWatcher::parseMsg(std::set<std::string> *sensors_set) {
	bool thermal_event = false;
	bool thermal_update = false;
	char *cp;
	int ret, client_socket;
	char buffer[BUFFER_SIZE];

	while (true) {
		client_socket = accept(event_fd_.get(), NULL, NULL);
		if (client_socket == -1) {
			LOG(INFO) << "Wait to accept a connection";
			break;
		}

		ret = recv(client_socket, buffer, BUFFER_SIZE,0);
		if (ret == -1) {
			LOG(ERROR) << "Socket recv failed";
		} else {
			if (ret < BUFFER_SIZE)
				buffer[ret] = '\0';
			else
				buffer[ret - 1] = '\0';

			ALOGW("%s: Get from client %s", __func__, buffer);
			cp = buffer;
			while (*cp) {
				ret = strncmp(cp, THERMAL_HAL_SOCKET_IDENTIFY_WORD, 4);
				if (ret != 0){
					LOG(ERROR) << "Message format error\n";
					break;
				}
				std::string uevent = cp;
					if (uevent.find("TYPE=") == 0) {
						if (uevent.find("TYPE=thermal_hal_notify") != std::string::npos) {
							thermal_event = true;
						} else if (uevent.find("TYPE=thermal_hal_update") != std::string::npos) {
							thermal_update = true;
						} else {
							break;
						}
					}
				if (thermal_event) {
					auto start_pos = uevent.find("NAME=");
					if (start_pos != std::string::npos) {
						start_pos += 5;
						std::string name = uevent.substr(start_pos);
						ALOGW("%s:name.c_str():%s", __func__, name.c_str());
						for (int j = 0; j < TT_MAX; ++j) {
						if (strncmp(name.c_str(), tz_data[j].label, strlen(tz_data[j].label)) == 0) {
							ALOGW("%s:tz notify:%s", __func__, tz_data[j].label);
							sensors_set->insert(name);
							}
						}
						break;
					}
				}
				if (thermal_update) {
					auto start_pos = uevent.find("NAME=");
					auto threshold_pos = uevent.find("THRESHOLD=");
					auto critical_pos = uevent.find("CRITICAL=");
					auto emerency_pos = uevent.find("EMERGENCY=");
					auto shutdown_pos = uevent.find("SHUTDOWN=");
					if (start_pos != std::string::npos && threshold_pos != std::string::npos) {
						start_pos += 5;
						threshold_pos += 10;
						std::string name = uevent.substr(start_pos);
						std::string threshold = uevent.substr(threshold_pos);
						for (int j = 0; j < TT_MAX; ++j) {
						if (strncmp(name.c_str(), kMtkTempThreshold[j].name.c_str(), strlen(kMtkTempThreshold[j].name.c_str())) == 0) {
							kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)] = std::stoi(threshold);
							LOG(INFO) << "thermal update "
									<< " name: " << kMtkTempThreshold[j].name
									<< " hotThrottlingThresholds: " << kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
							}
						}
						break;
					} else if (start_pos != std::string::npos && critical_pos != std::string::npos) {
						start_pos += 5;
						critical_pos += 9;
						std::string name = uevent.substr(start_pos);
						std::string threshold = uevent.substr(critical_pos);
						for (int j = 0; j < TT_MAX; ++j) {
						if (strncmp(name.c_str(), kMtkTempThreshold[j].name.c_str(), strlen(kMtkTempThreshold[j].name.c_str())) == 0) {
							kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::CRITICAL)] = std::stoi(threshold);
							LOG(INFO) << "thermal update "
									<< " name: " << kMtkTempThreshold[j].name
									<< " hotThrottlingThresholds: " << kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::CRITICAL)];
							}
						}
						break;
					}
					else if (start_pos != std::string::npos && emerency_pos != std::string::npos) {
						start_pos += 5;
						emerency_pos += 10;
						std::string name = uevent.substr(start_pos);
						std::string threshold = uevent.substr(emerency_pos);
						for (int j = 0; j < TT_MAX; ++j) {
						if (strncmp(name.c_str(), kMtkTempThreshold[j].name.c_str(), strlen(kMtkTempThreshold[j].name.c_str())) == 0) {
							kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::EMERGENCY)] = std::stoi(threshold);
							LOG(INFO) << "thermal update "
									<< " name: " << kMtkTempThreshold[j].name
									<< " hotThrottlingThresholds: " << kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::EMERGENCY)];
							}
						}
						break;
					}
					else if (start_pos != std::string::npos && shutdown_pos != std::string::npos) {
						start_pos += 5;
						shutdown_pos += 9;
						std::string name = uevent.substr(start_pos);
						std::string threshold = uevent.substr(shutdown_pos);
						for (int j = 0; j < TT_MAX; ++j) {
						if (strncmp(name.c_str(), kMtkTempThreshold[j].name.c_str(), strlen(kMtkTempThreshold[j].name.c_str())) == 0) {
							kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)] = std::stoi(threshold);
							LOG(INFO) << "thermal update "
									<< " name: " << kMtkTempThreshold[j].name
									<< " hotThrottlingThresholds: " << kMtkTempThreshold[j].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)];
							}
						}
						break;
					}

				}
				cp++;
			}
		}
		close(client_socket);
	}

}
void ThermalWatcher::parseUevent(std::set<std::string> *sensors_set) {
	bool thermal_event = false;
	constexpr int kUeventMsgLen = 2048;
	char msg[kUeventMsgLen + 2];
	char *cp;

	while (true) {
		int n = uevent_kernel_multicast_recv(event_fd_.get(), msg, kUeventMsgLen);
		if (n <= 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				LOG(ERROR) << "Error reading from Uevent Fd";
			}
			break;
		}

		if (n >= kUeventMsgLen) {
			LOG(ERROR) << "Uevent overflowed buffer, discarding";
			continue;
		}

		msg[n] = '\0';
		msg[n + 1] = '\0';

		cp = msg;
		while (*cp) {
			std::string uevent = cp;
			if (!thermal_event) {
				if (uevent.find("SUBSYSTEM=") == 0) {
					if (uevent.find("SUBSYSTEM=thermal") != std::string::npos) {
						thermal_event = true;
					} else {
						break;
					}
				}
			} else {
				auto start_pos = uevent.find("NAME=");
				if (start_pos != std::string::npos) {
					start_pos += 5;
					std::string name = uevent.substr(start_pos);
					for (int j = 0; j < TT_MAX; ++j) {
					if (strncmp(name.c_str(), tz_data[j].label, strlen(tz_data[j].label)) == 0) {
						sensors_set->insert(name);
						}
					}
					break;
				}
			}
			while (*cp++) {
			}
		}
	}
}
void ThermalWatcher::wake() {
	looper_->wake();
}

bool ThermalWatcher::threadLoop() {
	// Polling interval 2s
	static constexpr int kMinPollIntervalMs = 2000;
	// Max event timeout 5mins
	static constexpr int keventPollTimeoutMs = 300000;
	int fd;
	std::set<std::string> sensors;

	int timeout = (thermal_triggered_ || is_polling_) ? kMinPollIntervalMs : keventPollTimeoutMs;
	if (looper_->pollOnce(timeout, &fd, nullptr, nullptr) >= 0) {
		if (fd != event_fd_.get()) {
			return true;
		}
		if (tz_map_version == TZ_MAP_LEGANCY)
			parseUevent(&sensors);
		else
			parseMsg(&sensors);
		// Ignore cb_ if uevent is not from monitored sensors
		if (sensors.size() == 0) {
		return true;
		}
	}
	thermal_triggered_ = cb_(sensors);
	return true;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
