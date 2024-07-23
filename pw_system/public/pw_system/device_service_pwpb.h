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
#pragma once

#include "pw_system_protos/device_service.rpc.pwpb.h"

namespace pw::system {

// See pw_system/pw_system_protos/device_service.proto for service details.
class DeviceServicePwpb final
    : public proto::pw_rpc::pwpb::DeviceService::Service<DeviceServicePwpb> {
 public:
  DeviceServicePwpb();

  Status Reboot(const proto::pwpb::RebootRequest::Message& request,
                proto::pwpb::RebootResponse::Message& response);

  Status Crash(const proto::pwpb::CrashRequest::Message& request,
               proto::pwpb::CrashResponse::Message& response);
};

}  // namespace pw::system
