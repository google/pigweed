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

#include "pw_rpc/raw/internal/method.h"

#include <cstring>

#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal {

void RawMethod::SynchronousUnaryInvoker(const Method& method,
                                        CallContext& call,
                                        const Packet& request) {
  Channel::OutputBuffer response_buffer = call.channel().AcquireBuffer();
  std::span payload_buffer = response_buffer.payload(request);

  StatusWithSize sws =
      static_cast<const RawMethod&>(method).function_.synchronous_unary(
          call, request.payload(), payload_buffer);
  Packet response = Packet::Response(request);

  response.set_payload(payload_buffer.first(sws.size()));
  response.set_status(sws.status());
  if (call.channel().Send(response_buffer, response).ok()) {
    return;
  }

  PW_LOG_WARN(
      "Failed to send response packet for channel %u; terminating RPC with "
      "INTERNAL error",
      unsigned(call.channel().id()));
  call.channel()
      .Send(Packet::ServerError(request, Status::Internal()))
      .IgnoreError();
}

void RawMethod::ServerStreamingInvoker(const Method& method,
                                       CallContext& call,
                                       const Packet& request) {
  RawServerWriter server_writer(call);
  static_cast<const RawMethod&>(method).function_.unary_request(
      call, request.payload(), server_writer);
}

void RawMethod::ClientStreamingInvoker(const Method& method,
                                       CallContext& call,
                                       const Packet&) {
  RawServerReader reader(call);
  static_cast<const RawMethod&>(method).function_.stream_request(call, reader);
}

void RawMethod::BidirectionalStreamingInvoker(const Method& method,
                                              CallContext& call,
                                              const Packet&) {
  RawServerReaderWriter reader_writer(call);
  static_cast<const RawMethod&>(method).function_.stream_request(call,
                                                                 reader_writer);
}

}  // namespace pw::rpc::internal
