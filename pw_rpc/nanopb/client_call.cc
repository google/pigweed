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

#include "pw_rpc/nanopb/client_call.h"

namespace pw::rpc {
namespace internal {

Status BaseNanopbClientCall::SendRequest(const void* request_struct) {
  std::span<std::byte> buffer = AcquirePayloadBuffer();
  if (buffer.empty()) {
    // Getting an empty buffer means that either the call is inactive or the
    // channel does not exist.
    Unregister();
    return Status::Unavailable();
  }

  StatusWithSize sws = serde_.EncodeRequest(request_struct, buffer);
  Status status = sws.status();

  if (status.ok()) {
    status = ReleasePayloadBuffer(buffer.first(sws.size()));
  } else {
    ReleasePayloadBuffer({});
  }

  if (!status.ok()) {
    // Failing to send the initial request ends the RPC call.
    Unregister();
  }

  return status;
}

}  // namespace internal
}  // namespace pw::rpc
