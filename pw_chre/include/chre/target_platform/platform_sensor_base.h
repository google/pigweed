// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once
#include "chre_api/chre/sensor.h"

namespace chre {

class PlatformSensorBase {
 public:
  void initBase(const struct chreSensorInfo* sensor_info,
                uint32_t sensor_handle) {
    sensor_info_ = sensor_info;
    sensor_handle_ = sensor_handle;
  }

  void setSensorInfo(const struct chreSensorInfo* sensor_info) {
    sensor_info_ = sensor_info;
  }

  void setSensorHandle(uint32_t sensor_handle) {
    sensor_handle_ = sensor_handle;
  }

  uint32_t getSensorHandle() const { return sensor_handle_; }

 protected:
  const struct chreSensorInfo* sensor_info_;
  uint32_t sensor_handle_;
};

}  // namespace chre
