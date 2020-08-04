// Copyright 2020 The Pigweed Authors
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

#include "pw_rpc/nanopb_client_call.h"

namespace pw::rpc {
namespace internal {

Status BaseNanopbClientCall::SendRequest(const void* request_struct) {
  std::span<std::byte> buffer = AcquirePayloadBuffer();

  StatusWithSize sws = serde_.EncodeRequest(buffer, request_struct);
  if (!sws.ok()) {
    ReleasePayloadBuffer({});
    return sws.status();
  }

  return ReleasePayloadBuffer(buffer.first(sws.size()));
}

}  // namespace internal
}  // namespace pw::rpc
