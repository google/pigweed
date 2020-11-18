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

#include "pw_rpc/internal/raw_method.h"

#include <cstring>

#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {

RawServerWriter::~RawServerWriter() {
  if (!buffer().empty()) {
    ReleasePayloadBuffer();
  }
}

Status RawServerWriter::Write(ConstByteSpan response) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  if (buffer().Contains(response)) {
    return ReleasePayloadBuffer(response);
  }

  std::span<std::byte> buffer = AcquirePayloadBuffer();

  if (response.size() > buffer.size()) {
    ReleasePayloadBuffer();
    return Status::OutOfRange();
  }

  std::memcpy(buffer.data(), response.data(), response.size());
  return ReleasePayloadBuffer(buffer.first(response.size()));
}

namespace internal {

void RawMethod::CallUnary(ServerCall& call, const Packet& request) const {
  Channel::OutputBuffer response_buffer = call.channel().AcquireBuffer();
  std::span payload_buffer = response_buffer.payload(request);

  StatusWithSize sws = function_.unary(call, request.payload(), payload_buffer);
  Packet response = Packet::Response(request);

  response.set_payload(payload_buffer.first(sws.size()));
  response.set_status(sws.status());
  if (call.channel().Send(response_buffer, response).ok()) {
    return;
  }

  PW_LOG_WARN("Failed to send response packet for channel %u",
              unsigned(call.channel().id()));
  call.channel().Send(response_buffer,
                      Packet::ServerError(request, Status::Internal()));
}

void RawMethod::CallServerStreaming(ServerCall& call,
                                    const Packet& request) const {
  internal::BaseServerWriter server_writer(call);
  function_.server_streaming(call, request.payload(), server_writer);
}

}  // namespace internal
}  // namespace pw::rpc
