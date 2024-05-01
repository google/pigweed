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

#include <cstdint>

#include "pw_bloat/bloat_this_binary.h"
#include "pw_bluetooth_proxy/common.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

namespace {

// Example for docs.rst.
void UsePassthroughProxy() {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, 20> h4_array_from_host{0};
  H4HciPacket h4_span_from_host = pw::span(h4_array_from_host);

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, 20> h4_array_from_controller{0};
  H4HciPacket h4_span_from_controller = pw::span(h4_array_from_controller);

  H4HciPacketSendFn containerSendToHostFn(
      []([[maybe_unused]] H4HciPacket packet) {});

  H4HciPacketSendFn containerSendToControllerFn(
      ([]([[maybe_unused]] H4HciPacket packet) {}));

  ProxyHost proxy = ProxyHost(std::move(containerSendToHostFn),
                              std::move(containerSendToControllerFn));

  proxy.HandleH4HciFromHost(h4_span_from_host);

  proxy.HandleH4HciFromController(h4_span_from_controller);
}

}  // namespace
}  // namespace pw::bluetooth::proxy

int main() {
  pw::bloat::BloatThisBinary();
  pw::bluetooth::proxy::UsePassthroughProxy();
}
