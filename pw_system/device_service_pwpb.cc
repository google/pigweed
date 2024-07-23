// Copyright 2024 The Pigweed Authors
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

#include "pw_system/device_service_pwpb.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_system/device_handler.h"

namespace pw::system {

DeviceServicePwpb::DeviceServicePwpb() {}

Status DeviceServicePwpb::Reboot(
    const proto::pwpb::RebootRequest::Message& /*request*/,
    proto::pwpb::RebootResponse::Message& /*response*/) {
  PW_LOG_CRITICAL("RPC triggered reboot");
  device_handler::RebootSystem();
  return OkStatus();
}

Status DeviceServicePwpb::Crash(
    const proto::pwpb::CrashRequest::Message& /*request*/,
    proto::pwpb::CrashResponse::Message& /*response*/) {
  const span msg = "RPC triggered crash";
  PW_LOG_CRITICAL("%s", msg.data());
  PW_CRASH("%s", msg.data());
  return OkStatus();
}

}  // namespace pw::system
