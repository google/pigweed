// Copyright 2021 The Pigweed Authors
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

#include <cinttypes>
#include <cstddef>
#include <limits>

namespace pw::transfer {

// TODO(hepler): Remove this forward declaration whent the friend statement is
// no longer needed.
class Client;

namespace internal {

// Information about a single transfer.
class Context {
 public:
  constexpr Context()
      : transfer_id_(0),
        offset_(0),
        pending_bytes_(0),
        max_chunk_size_bytes_(std::numeric_limits<uint32_t>::max()) {}

  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;

  constexpr uint32_t transfer_id() const { return transfer_id_; }

 protected:
  // TODO(hepler): Temporarily friend the Client code until it is refactored to
  // no longer need it.
  friend class transfer::Client;

  constexpr uint32_t offset() const { return offset_; }
  constexpr void set_offset(size_t offset) { offset_ = offset; }
  constexpr void advance_offset(size_t size) { offset_ += size; }

  constexpr uint32_t pending_bytes() const { return pending_bytes_; }
  constexpr void set_pending_bytes(size_t pending_bytes) {
    pending_bytes_ = pending_bytes;
  }

  constexpr uint32_t max_chunk_size_bytes() const {
    return max_chunk_size_bytes_;
  }
  constexpr void set_max_chunk_size_bytes(size_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
  }

  constexpr Context(uint32_t transfer_id)
      : transfer_id_(transfer_id),
        offset_(0),
        pending_bytes_(0),
        max_chunk_size_bytes_(std::numeric_limits<uint32_t>::max()) {}

  constexpr void set_transfer_id(uint32_t transfer_id) {
    transfer_id_ = transfer_id;
  }

  // Calculates the maximum size of actual data that can be sent within a single
  // client write transfer chunk, accounting for the overhead of the transfer
  // protocol and RPC system.
  //
  // Note: This function relies on RPC protocol internals. This is generally a
  // *bad* idea, but is necessary here due to limitations of the RPC system and
  // its asymmetric ingress and egress paths.
  //
  // TODO(frolv): This should be investigated further and perhaps addressed
  // within the RPC system, at the least through a helper function.
  uint32_t MaxWriteChunkSize(uint32_t max_chunk_size_bytes,
                             uint32_t channel_id) const;

 private:
  uint32_t transfer_id_;
  size_t offset_;
  size_t pending_bytes_;
  size_t max_chunk_size_bytes_;
};

}  // namespace internal
}  // namespace pw::transfer
