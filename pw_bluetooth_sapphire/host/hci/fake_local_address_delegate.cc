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

#include "pw_bluetooth_sapphire/internal/host/hci/fake_local_address_delegate.h"

namespace bt::hci {

void FakeLocalAddressDelegate::EnsureLocalAddress(AddressCallback callback) {
  BT_DEBUG_ASSERT(callback);
  if (!async_) {
    callback(local_address_);
    return;
  }
  (void)heap_dispatcher_.Post(
      [callback = std::move(callback), addr = local_address_](
          pw::async::Context /*ctx*/, pw::Status status) {
        if (status.ok()) {
          callback(addr);
        }
      });
}

}  // namespace bt::hci
