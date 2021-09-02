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

#include "pw_rpc/nanopb/internal/method.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {

using std::byte;

namespace internal {

void NanopbMethod::CallSynchronousUnary(CallContext& call,
                                        const Packet& request,
                                        void* request_struct,
                                        void* response_struct) const {
  if (!DecodeRequest(call.channel(), request, request_struct)) {
    return;
  }

  const Status status =
      function_.synchronous_unary(call, request_struct, response_struct);
  SendResponse(call.channel(), request, response_struct, status);
}

void NanopbMethod::CallUnaryRequest(CallContext& call,
                                    MethodType type,
                                    const Packet& request,
                                    void* request_struct) const {
  if (!DecodeRequest(call.channel(), request, request_struct)) {
    return;
  }

  GenericNanopbResponder server_writer(call, type);
  function_.unary_request(call, request_struct, server_writer);
}

bool NanopbMethod::DecodeRequest(Channel& channel,
                                 const Packet& request,
                                 void* proto_struct) const {
  if (serde_.DecodeRequest(request.payload(), proto_struct)) {
    return true;
  }

  PW_LOG_WARN("Nanopb failed to decode request payload from channel %u",
              unsigned(channel.id()));
  channel.Send(Packet::ServerError(request, Status::DataLoss()));
  return false;
}

void NanopbMethod::SendResponse(Channel& channel,
                                const Packet& request,
                                const void* response_struct,
                                Status status) const {
  Channel::OutputBuffer response_buffer = channel.AcquireBuffer();
  std::span payload_buffer = response_buffer.payload(request);

  StatusWithSize encoded =
      serde_.EncodeResponse(response_struct, payload_buffer);

  if (encoded.ok()) {
    Packet response = Packet::Response(request);

    response.set_payload(payload_buffer.first(encoded.size()));
    response.set_status(status);
    pw::Status send_status = channel.Send(response_buffer, response);
    if (send_status.ok()) {
      return;
    }

    PW_LOG_WARN("Failed to send response packet for channel %u, status %u",
                unsigned(channel.id()),
                send_status.code());

    // Re-acquire the buffer to encode an error packet.
    response_buffer = channel.AcquireBuffer();
  } else {
    PW_LOG_WARN(
        "Nanopb failed to encode response packet for channel %u, status %u",
        unsigned(channel.id()),
        encoded.status().code());
  }
  channel.Send(response_buffer,
               Packet::ServerError(request, Status::Internal()));
}

}  // namespace internal
}  // namespace pw::rpc
