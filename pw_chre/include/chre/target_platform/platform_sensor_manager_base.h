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
#include <cstdint>

#include "chre/pal/sensor.h"

namespace chre {

class PlatformSensorManagerBase {
 public:
  // Note: these are load bearing names
  static const chrePalSensorCallbacks sSensorCallbacks;
  const chrePalSensorApi* mSensorApi;

 private:
  static void samplingStatusUpdateCallback(
      uint32_t sensor_handle, struct chreSensorSamplingStatus* status);
  static void dataEventCallback(uint32_t sensor_handle, void* data);
  static void biasEventCallback(uint32_t sensor_handle, void* bias_data);
  static void flushCompleteCallback(uint32_t sensor_handle,
                                    uint32_t flush_request_id,
                                    uint8_t error_code);
};

}  // namespace chre
