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
  RawServerResponder responder(call);
  std::span payload_buffer = responder.AcquirePayloadBuffer();

  StatusWithSize sws =
      static_cast<const RawMethod&>(method).function_.synchronous_unary(
          call, request.payload(), payload_buffer);

  responder.Finish(payload_buffer.first(sws.size()), sws.status())
      .IgnoreError();
}

void RawMethod::AsynchronousUnaryInvoker(const Method& method,
                                         CallContext& call,
                                         const Packet& request) {
  RawServerResponder responder(call);
  static_cast<const RawMethod&>(method).function_.asynchronous_unary(
      call, request.payload(), responder);
}

void RawMethod::ServerStreamingInvoker(const Method& method,
                                       CallContext& call,
                                       const Packet& request) {
  RawServerWriter server_writer(call);
  static_cast<const RawMethod&>(method).function_.server_streaming(
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
