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

#include "pw_bluetooth_proxy/hci_proxy.h"

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth_proxy/common.h"
#include "pw_bluetooth_proxy/proxy_policy.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

HciProxy::HciProxy(H4HciPacketSendFn&& send_to_host_fn,
                   H4HciPacketSendFn&& send_to_controller_fn,
                   pw::span<ProxyPolicy*> policies)
    : outward_send_to_host_fn_(std::move(send_to_host_fn)),
      outward_send_to_controller_fn_(std::move(send_to_controller_fn)),
      policies_(std::move(policies)) {
  // Currently the proxy just passes packets across the policies. So here
  // we just set up `pw::Function` call chains that pass the packets to each
  // policy in order.

  // Set the downward call chain for handling packets from host through to
  // controller.
  // this::firstProcessH4HciFromHostFn_ -> policy[0]::ProcessH4HciFromHost
  // policy[0]::setSendToControllerFn -> policy[0+1]::ProcessH4HciFromHost
  // ...
  // policy[N]::setSendToControllerFn -> this::sendToController
  pw::Function<void(H4HciPacketSendFn && fn)> upper_set_process_from_host_fn =
      pw::bind_member<&HciProxy::setFirstProcessH4HciFromHostFn>(this);
  for (ProxyPolicy* policy : policies_) {
    upper_set_process_from_host_fn(
        pw::bind_member<&ProxyPolicy::ProcessH4HciFromHost>(policy));
    upper_set_process_from_host_fn =
        pw::bind_member<&ProxyPolicy::setSendToControllerFn>(policy);
  }
  upper_set_process_from_host_fn(
      pw::bind_member<&HciProxy::sendToController>(this));

  // Set the upward call chain for handling packets from controller through to
  // host.
  // this::firstProcessH4HciFromControllerFn_ ->
  // policy[N]::ProcessH4HciFromController
  // policy[N]::setSendToHostFn -> policy[N-1]::ProcessH4HciFromController
  // ...
  // policy[0]::setSendToHostFn -> this::sendToHost
  pw::Function<void(H4HciPacketSendFn && fn)>
      lower_set_process_from_controller_fn =
          pw::bind_member<&HciProxy::setFirstProcessH4HciFromControllerFn>(
              this);
  for (auto ri = policies_.rend(); ri != policies_.rend(); ri--) {
    auto policy = *ri;
    lower_set_process_from_controller_fn(
        pw::bind_member<&ProxyPolicy::ProcessH4HciFromController>(policy));
    lower_set_process_from_controller_fn =
        pw::bind_member<&ProxyPolicy::setSendToHostFn>(policy);
  }
  lower_set_process_from_controller_fn(
      pw::bind_member<&HciProxy::sendToHost>(this));
}

}  // namespace pw::bluetooth::proxy
