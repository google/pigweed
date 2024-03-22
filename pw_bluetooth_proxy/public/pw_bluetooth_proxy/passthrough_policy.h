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

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth_proxy/common.h"
#include "pw_bluetooth_proxy/proxy_policy.h"

namespace pw::bluetooth::proxy {

/// Simple `pw::bluetooth::proxy::ProxyPolicy` that just passes packets onward.
/// Intended for testing and as a simple example.
class PassthroughPolicy : public ProxyPolicy {
 public:
  PassthroughPolicy() : ProxyPolicy{} {}

  ~PassthroughPolicy() override = default;
  PassthroughPolicy(const PassthroughPolicy&) = delete;
  PassthroughPolicy& operator=(const PassthroughPolicy&) = delete;
  PassthroughPolicy(PassthroughPolicy&&) = delete;
  PassthroughPolicy& operator=(PassthroughPolicy&&) = delete;

  void ProcessH4HciFromHost(H4HciPacket packet) override {
    PW_DCHECK(send_to_controller_fn_ != nullptr);
    send_to_controller_fn_(packet);
  }

  void ProcessH4HciFromController(H4HciPacket packet) override {
    PW_DCHECK(send_to_host_fn_ != nullptr);
    send_to_host_fn_(packet);
  }
};

}  // namespace pw::bluetooth::proxy
