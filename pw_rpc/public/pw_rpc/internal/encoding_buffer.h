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

#include "pw_bytes/span.h"
#include "pw_rpc/internal/lock.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc::internal {

ByteSpan GetPayloadBuffer() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

// Successful calls to EncodeToPayloadBuffer MUST send the returned buffer,
// without releasing the RPC lock.
template <typename Proto, typename Encoder>
static Result<ByteSpan> EncodeToPayloadBuffer(Proto& payload,
                                              const Encoder& encoder)
    PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
  ByteSpan buffer = GetPayloadBuffer();
  StatusWithSize result = encoder.Encode(payload, buffer);
  if (!result.ok()) {
    return result.status();
  }
  return buffer.first(result.size());
}

}  // namespace pw::rpc::internal
