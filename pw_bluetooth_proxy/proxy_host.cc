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

#include "pw_bluetooth_proxy/proxy_host.h"

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth_proxy/common.h"

namespace pw::bluetooth::proxy {

ProxyHost::ProxyHost(H4HciPacketSendFn&& send_to_host_fn,
                     H4HciPacketSendFn&& send_to_controller_fn)
    : outward_send_to_host_fn_(std::move(send_to_host_fn)),
      outward_send_to_controller_fn_(std::move(send_to_controller_fn)) {}

void ProxyHost::ProcessH4HciFromHost(H4HciPacket packet) {
  SendToController(packet);
}

void ProxyHost::ProcessH4HciFromController(H4HciPacket packet) {
  SendToHost(packet);
}

void ProxyHost::SendToHost(H4HciPacket packet) {
  PW_DCHECK(outward_send_to_host_fn_ != nullptr);
  outward_send_to_host_fn_(packet);
}

void ProxyHost::SendToController(H4HciPacket packet) {
  PW_DCHECK(outward_send_to_controller_fn_ != nullptr);
  outward_send_to_controller_fn_(packet);
}

}  // namespace pw::bluetooth::proxy
