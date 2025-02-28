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

#include "pw_bluetooth_proxy/internal/h4_storage.h"

#include <mutex>

#include "pw_assert/check.h"

namespace pw::bluetooth::proxy {

std::array<containers::Pair<uint8_t*, bool>, H4Storage::kNumH4Buffs>
H4Storage::InitOccupiedMap() {
  std::lock_guard lock(storage_mutex_);
  std::array<containers::Pair<uint8_t*, bool>, kNumH4Buffs> arr;
  for (size_t i = 0; i < kNumH4Buffs; ++i) {
    arr[i] = {h4_buffs_[i].data(), false};
  }
  return arr;
}

H4Storage::H4Storage() : h4_buff_occupied_(InitOccupiedMap()) {}

std::optional<pw::span<uint8_t>> H4Storage::ReserveH4Buff() {
  std::lock_guard lock(storage_mutex_);
  for (const auto& [buff, occupied] : h4_buff_occupied_) {
    if (!occupied) {
      h4_buff_occupied_.at(buff) = true;
      pw::span<uint8_t> h4_buff = {buff, kH4BuffSize};
      std::fill(h4_buff.begin(), h4_buff.end(), 0);
      return h4_buff;
    }
  }
  return std::nullopt;
}

void H4Storage::ReleaseH4Buff(const uint8_t* buffer) {
  std::lock_guard lock(storage_mutex_);
  PW_CHECK(h4_buff_occupied_.contains(const_cast<uint8_t*>(buffer)),
           "Received release callback for invalid buffer address.");

  h4_buff_occupied_.at(const_cast<uint8_t*>(buffer)) = false;
}

}  // namespace pw::bluetooth::proxy
