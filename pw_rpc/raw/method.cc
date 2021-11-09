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

#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal {

void RawMethod::SynchronousUnaryInvoker(const CallContext& context,
                                        const Packet& request) {
  RawUnaryResponder responder(context);
  std::span payload_buffer = responder.PayloadBuffer();

  StatusWithSize sws =
      static_cast<const RawMethod&>(context.method())
          .function_.synchronous_unary(
              context.service(), request.payload(), payload_buffer);

  responder.Finish(payload_buffer.first(sws.size()), sws.status())
      .IgnoreError();
}

void RawMethod::AsynchronousUnaryInvoker(const CallContext& context,
                                         const Packet& request) {
  RawUnaryResponder responder(context);
  static_cast<const RawMethod&>(context.method())
      .function_.asynchronous_unary(
          context.service(), request.payload(), responder);
}

void RawMethod::ServerStreamingInvoker(const CallContext& context,
                                       const Packet& request) {
  RawServerWriter server_writer(context);
  static_cast<const RawMethod&>(context.method())
      .function_.server_streaming(
          context.service(), request.payload(), server_writer);
}

void RawMethod::ClientStreamingInvoker(const CallContext& context,
                                       const Packet&) {
  RawServerReader reader(context);
  static_cast<const RawMethod&>(context.method())
      .function_.stream_request(context.service(), reader);
}

void RawMethod::BidirectionalStreamingInvoker(const CallContext& context,
                                              const Packet&) {
  RawServerReaderWriter reader_writer(context);
  static_cast<const RawMethod&>(context.method())
      .function_.stream_request(context.service(), reader_writer);
}

}  // namespace pw::rpc::internal
