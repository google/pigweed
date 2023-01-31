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

#include "pw_bluetooth_profiles/device_info_service.h"

#include "pw_assert/check.h"

namespace pw::bluetooth_profiles {

void DeviceInfoServiceImpl::PublishService(
    bluetooth::gatt::Server* gatt_server,
    Callback<
        void(bluetooth::Result<bluetooth::gatt::Server::PublishServiceError>)>
        result_callback) {
  PW_CHECK(publish_service_callback_ == nullptr);
  publish_service_callback_ = std::move(result_callback);
  this->delegate_.SetServicePtr(nullptr);
  return gatt_server->PublishService(
      service_info_,
      &delegate_,
      [this](bluetooth::gatt::Server::PublishServiceResult result) {
        if (result.ok()) {
          this->publish_service_callback_({});
          this->delegate_.SetServicePtr(std::move(result.value()));
        } else {
          this->publish_service_callback_(result.error());
        }
      });
}

void DeviceInfoServiceImpl::Delegate::OnError(
    bluetooth::gatt::Error /* error */) {
  local_service_.reset();
}

void DeviceInfoServiceImpl::Delegate::ReadValue(
    bluetooth::PeerId /* peer_id */,
    bluetooth::gatt::Handle handle,
    uint32_t offset,
    Function<void(
        bluetooth::Result<bluetooth::gatt::Error, span<const std::byte>>)>&&
        result_callback) {
  const uint32_t value_index = static_cast<uint32_t>(handle);
  if (value_index >= values_.size()) {
    result_callback(bluetooth::gatt::Error::kInvalidHandle);
    return;
  }
  span<const std::byte> value = values_[value_index];
  if (offset > value.size()) {
    result_callback(bluetooth::gatt::Error::kInvalidOffset);
    return;
  }
  result_callback({std::in_place, value.subspan(offset)});
}

}  // namespace pw::bluetooth_profiles
