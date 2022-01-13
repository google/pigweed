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

#include "pw_rpc/internal/client_call.h"

namespace pw::rpc::internal {

void ClientCall::CloseClientCall() {
  if (client_stream_open()) {
    // TODO(pwbug/597): Ensure the call object is locked before releasing the
    //     RPC mutex.
    CloseClientStreamLocked();
    rpc_lock().lock();  // Reacquire after sending the packet
  }
  CloseAndReleasePayloadBuffer();
  rpc_lock().lock();  // Reacquire releasing the buffer
}

}  // namespace pw::rpc::internal
