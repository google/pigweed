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

// Definitions for the static and dynamic versions of the pw_rpc encoding
// buffer. Both version are compiled rot, but only one is instantiated,
// depending on the PW_RPC_DYNAMIC_ALLOCATION config option.
#pragma once

#include <array>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_rpc/internal/config.h"
#include "pw_rpc/internal/lock.h"
#include "pw_rpc/internal/packet.h"
#include "pw_status/status_with_size.h"

#if PW_RPC_DYNAMIC_ALLOCATION

#include PW_RPC_DYNAMIC_CONTAINER_INCLUDE

#endif  // PW_RPC_DYNAMIC_ALLOCATION

namespace pw::rpc::internal {

constexpr ByteSpan ResizeForPayload(ByteSpan buffer) {
  return buffer.subspan(Packet::kMinEncodedSizeWithoutPayload);
}

// Wraps a statically allocated encoding buffer.
class StaticEncodingBuffer {
 public:
  constexpr StaticEncodingBuffer() : buffer_{} {}

  ByteSpan AllocatePayloadBuffer() { return ResizeForPayload(buffer_); }
  ByteSpan GetPacketBuffer(size_t /* payload_size */) { return buffer_; }

  void Release() {}
  void ReleaseIfAllocated() {}

 private:
  static_assert(MaxSafePayloadSize() > 0,
                "pw_rpc's encode buffer is too small to fit any data");

  std::array<std::byte, cfg::kEncodingBufferSizeBytes> buffer_;
};

#if PW_RPC_DYNAMIC_ALLOCATION

// Wraps a dynamically allocated encoding buffer.
class DynamicEncodingBuffer {
 public:
  DynamicEncodingBuffer() = default;

  ~DynamicEncodingBuffer() { PW_DASSERT(buffer_.empty()); }

  // Allocates a new buffer and returns a portion to use to encode the payload.
  ByteSpan AllocatePayloadBuffer(size_t payload_size) {
    Allocate(payload_size);
    return ResizeForPayload(buffer_);
  }

  // Returns the buffer into which to encode the packet, allocating a new buffer
  // if necessary.
  ByteSpan GetPacketBuffer(size_t payload_size) {
    if (buffer_.empty()) {
      Allocate(payload_size);
    }
    return buffer_;
  }

  // Frees the payload buffer, which MUST have been allocated previously.
  void Release() {
    PW_DASSERT(!buffer_.empty());
    buffer_.clear();
  }

  // Frees the payload buffer, if one was allocated.
  void ReleaseIfAllocated() {
    if (!buffer_.empty()) {
      Release();
    }
  }

 private:
  void Allocate(size_t payload_size) {
    const size_t buffer_size =
        payload_size + Packet::kMinEncodedSizeWithoutPayload;
    PW_DASSERT(buffer_.empty());
    buffer_.resize(buffer_size);
  }

  PW_RPC_DYNAMIC_CONTAINER(std::byte) buffer_;
};

using EncodingBuffer = DynamicEncodingBuffer;

#else

using EncodingBuffer = StaticEncodingBuffer;

#endif  // PW_RPC_DYNAMIC_ALLOCATION

// Instantiate the global encoding buffer variable, depending on whether dynamic
// allocation is enabled or not.
inline EncodingBuffer encoding_buffer PW_GUARDED_BY(rpc_lock());

// Successful calls to EncodeToPayloadBuffer MUST send the returned buffer,
// without releasing the RPC lock.
template <typename Proto, typename Encoder>
static Result<ByteSpan> EncodeToPayloadBuffer(Proto& payload,
                                              const Encoder& encoder)
    PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
  // If dynamic allocation is enabled, calculate the size of the encoded
  // protobuf and allocate a buffer for it.
#if PW_RPC_DYNAMIC_ALLOCATION
  StatusWithSize payload_size = encoder.EncodedSizeBytes(payload);
  if (!payload_size.ok()) {
    return Status::Internal();
  }

  ByteSpan buffer = encoding_buffer.AllocatePayloadBuffer(payload_size.size());
#else
  ByteSpan buffer = encoding_buffer.AllocatePayloadBuffer();
#endif  // PW_RPC_DYNAMIC_ALLOCATION

  StatusWithSize result = encoder.Encode(payload, buffer);
  if (!result.ok()) {
    encoding_buffer.ReleaseIfAllocated();
    return result.status();
  }
  return buffer.first(result.size());
}

}  // namespace pw::rpc::internal
