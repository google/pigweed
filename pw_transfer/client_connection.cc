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

#define PW_LOG_MODULE_NAME "TRN"

#include "pw_transfer/internal/client_connection.h"

#include "pw_log/log.h"
#include "pw_transfer/internal/chunk.h"

namespace pw::transfer::internal {

void ClientConnection::SendStatusChunk(TransferType type,
                                       uint32_t transfer_id,
                                       Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id;
  chunk.status = status.code();

  rpc::RawServerReaderWriter& destination = stream(type);

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, destination.PayloadBuffer());

  if (!result.ok()) {
    PW_LOG_ERROR("Failed to encode final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id));
    destination.ReleaseBuffer();
    return;
  }

  if (!destination.Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id));
    return;
  }
}

}  // namespace pw::transfer::internal
