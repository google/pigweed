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

Status GenericNanopbResponder::SendClientStreamOrResponse(
    const void* response, const Status* status) {
  if (!active()) {
    return Status::FailedPrecondition();
  }

  std::span<std::byte> payload_buffer = AcquirePayloadBuffer();

  // Cast the method to a NanopbMethod. Access the Nanopb
  // serializer/deserializer object and encode the response with it.
  StatusWithSize result = static_cast<const internal::NanopbMethod&>(method())
                              .serde()
                              .EncodeResponse(response, payload_buffer);

  if (!result.ok()) {
    return CloseAndSendServerError(Status::Internal());
  }

  payload_buffer = payload_buffer.first(result.size());

  if (status != nullptr) {
    return CloseAndSendResponse(payload_buffer, *status);
  }
  return SendPayloadBufferClientStream(payload_buffer);
}

void GenericNanopbResponder::DecodeRequest(ConstByteSpan payload,
                                           void* request_struct) const {
  static_cast<const internal::NanopbMethod&>(method()).serde().DecodeRequest(
      payload, request_struct);
}

}  // namespace pw::rpc::internal
