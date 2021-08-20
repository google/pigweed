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

#include "pw_rpc/nanopb/server_reader_writer.h"

#include "pw_rpc/nanopb/internal/method.h"

namespace pw::rpc::internal {

Status GenericNanopbResponder::SendClientStreamOrResponse(const void* response,
                                                          Status* status) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  std::span<std::byte> buffer = AcquirePayloadBuffer();

  // Cast the method to a NanopbMethod. Access the Nanopb
  // serializer/deserializer object and encode the response with it.
  auto result = static_cast<const internal::NanopbMethod&>(method())
                    .serde()
                    .EncodeResponse(response, buffer);
  if (!result.ok()) {
    ReleasePayloadBuffer();

    // If the Nanopb encode failed, the channel output may not have provided a
    // large enough buffer or something went wrong in Nanopb. Return INTERNAL to
    // indicate that the problem is internal to the server.
    return Status::Internal();
  }
  if (status != nullptr) {
    return CloseAndSendResponse(buffer.first(result.size()), *status);
  }
  return SendPayloadBufferClientStream(buffer.first(result.size()));
}

void GenericNanopbResponder::DecodeRequest(ConstByteSpan payload,
                                           void* request_struct) const {
  static_cast<const internal::NanopbMethod&>(method()).serde().DecodeRequest(
      payload, request_struct);
}

}  // namespace pw::rpc::internal
