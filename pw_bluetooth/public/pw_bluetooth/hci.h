// Copyright 2022 The Pigweed Authors
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

#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth {

// The Hci (Host Controller Interface) class is a shim for transferring packets
// between the Host and the Controller.
class Hci {
 public:
  using DataCallback = Function<void(span<const std::byte>)>;

  virtual ~Hci() = default;

  // Sends an HCI `command` packet to the controller.
  virtual void SendCommand(span<const std::byte> command) = 0;

  // Sets a callback that will be called with HCI event packets received from
  // the controller.
  virtual void SetEventCallback(DataCallback callback) = 0;

  // Sends an ACL data packet to the controller.
  virtual void SendAclData(span<const std::byte> data) = 0;

  // Sets a callback that will be called with ACL data packets received from the
  // controller.
  virtual void SetReceiveAclCallback(DataCallback callback) = 0;
};

}  // namespace pw::bluetooth
