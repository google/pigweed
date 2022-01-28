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

#include <cstddef>
#include <cstring>

#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal {

void RawMethod::SynchronousUnaryInvoker(const CallContext& context,
                                        const Packet& request) {
  RawUnaryResponder responder(context);
  rpc_lock().unlock();
  // TODO(hepler): Remove support for raw synchronous unary methods. Unlike
  //     synchronous Nanopb methods, they provide little value compared to
  //     asynchronous unary methods. For now, just provide a fixed buffer on the
  //     stack.
  std::byte payload_buffer[64] = {};

  StatusWithSize sws =
      static_cast<const RawMethod&>(context.method())
          .function_.synchronous_unary(
              context.service(), request.payload(), std::span(payload_buffer));

  responder.Finish(std::span(payload_buffer, sws.size()), sws.status())
      .IgnoreError();
}

void RawMethod::AsynchronousUnaryInvoker(const CallContext& context,
                                         const Packet& request) {
  RawUnaryResponder responder(context);
  rpc_lock().unlock();
  static_cast<const RawMethod&>(context.method())
      .function_.asynchronous_unary(
          context.service(), request.payload(), responder);
}

void RawMethod::ServerStreamingInvoker(const CallContext& context,
                                       const Packet& request) {
  RawServerWriter server_writer(context);
  rpc_lock().unlock();
  static_cast<const RawMethod&>(context.method())
      .function_.server_streaming(
          context.service(), request.payload(), server_writer);
}

void RawMethod::ClientStreamingInvoker(const CallContext& context,
                                       const Packet&) {
  RawServerReader reader(context);
  rpc_lock().unlock();
  static_cast<const RawMethod&>(context.method())
      .function_.stream_request(context.service(), reader);
}

void RawMethod::BidirectionalStreamingInvoker(const CallContext& context,
                                              const Packet&) {
  RawServerReaderWriter reader_writer(context);
  rpc_lock().unlock();
  static_cast<const RawMethod&>(context.method())
      .function_.stream_request(context.service(), reader_writer);
}

}  // namespace pw::rpc::internal
