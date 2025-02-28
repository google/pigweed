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

#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_function/function.h"

namespace pw::bluetooth::proxy {

// Contains the facilities for forwarding HCI packets to the host and
// controller.
class HciTransport {
 public:
  HciTransport(
      pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
      pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn)
      : outward_send_to_host_fn_(std::move(send_to_host_fn)),
        outward_send_to_controller_fn_(std::move(send_to_controller_fn)) {}

  // Send packet onwards to host.
  void SendToHost(H4PacketWithHci&& h4_packet) {
    PW_DCHECK(outward_send_to_host_fn_ != nullptr);
    outward_send_to_host_fn_(std::move(h4_packet));
  }

  // Send packet onwards to controller.
  void SendToController(H4PacketWithH4&& h4_packet) {
    PW_DCHECK(outward_send_to_controller_fn_ != nullptr);
    outward_send_to_controller_fn_(std::move(h4_packet));
  }

 private:
  // Function to call when proxy wants proxy container to pass a packet to the
  // host.
  pw::Function<void(H4PacketWithHci&& packet)> outward_send_to_host_fn_;

  // Function to call when proxy wants proxy container to pass a packet to the
  // controller.
  pw::Function<void(H4PacketWithH4&& packet)> outward_send_to_controller_fn_;
};

}  // namespace pw::bluetooth::proxy
