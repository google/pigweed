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

#include <optional>

#include "pw_containers/flat_map.h"
#include "pw_span/span.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

// Contains a configurable array of buffers to hold H4 packets.
class H4Storage {
 public:
  H4Storage();

  // Returns a free H4 buffer and marks it as occupied. If all H4 buffers are
  // occupied, returns std::nullopt.
  //
  // TODO: https://pwbug.dev/369849508 - Take a variable size.
  std::optional<pw::span<uint8_t>> ReserveH4Buff();

  // Marks an H4 buffer as unoccupied.
  void ReleaseH4Buff(const uint8_t* buffer);

  // Marks all H4 buffers as unoccupied.
  void Reset();

  // Returns the number of slots in `h4_buffs_`.
  static constexpr size_t GetNumH4Buffs() { return kNumH4Buffs; }

  // Returns the size of a slot in `h4_buffs_`.
  static constexpr uint16_t GetH4BuffSize() { return kH4BuffSize; }

 private:
  // TODO: https://pwbug.dev/353734827 - Allow container to specify constants.
  // To pass the unit tests, kNumH4Buffs >= L2capCoc::QueueCapacity().
  static constexpr size_t kNumH4Buffs = 10;

  // Support big enough HCI packets to handle 3-DH5 baseband packets. Note this
  // doesn't guarantee packets will fit since the controller can combine
  // multiple baseband packets, but in practice that hasn't been observed.
  // Max 3-DH5 payload (1021 bytes) + ACL header (4 bytes) + H4 type (1 byte)
  // TODO: https://pwbug.dev/369849508 - Support variable size buffers with
  // an allocator & replace this constant with total memory pool size.
  static constexpr uint16_t kH4BuffSize = 1026;

  // Returns an initializer list for `h4_buff_occupied_` with each buffer
  // address in `h4_buffs_` mapped to false.
  std::array<containers::Pair<uint8_t*, bool>, kNumH4Buffs> InitOccupiedMap();

  sync::Mutex storage_mutex_;

  // Each buffer is meant to hold one H4 packet containing an ACL PDU.
  std::array<std::array<uint8_t, kH4BuffSize>, kNumH4Buffs> h4_buffs_
      PW_GUARDED_BY(storage_mutex_);

  // Maps each H4 buffer to a flag that is set when the buffer holds an H4
  // packet being sent through `acl_data_channel_` and cleared in that H4
  // packet's release function to indicate that the H4 buffer is safe to
  // overwrite.
  containers::FlatMap<uint8_t*, bool, kNumH4Buffs> h4_buff_occupied_
      PW_GUARDED_BY(storage_mutex_);
};

}  // namespace pw::bluetooth::proxy
