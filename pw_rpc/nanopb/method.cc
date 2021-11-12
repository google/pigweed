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

// clang-format off
#include "pw_rpc/internal/log_config.h" // PW_LOG_* macros must be first.

#include "pw_rpc/nanopb/internal/method.h"
// clang-format on

#include "pb_decode.h"
#include "pb_encode.h"
#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {

namespace internal {

void NanopbMethod::CallSynchronousUnary(const CallContext& context,
                                        const Packet& request,
                                        void* request_struct,
                                        void* response_struct) const {
  if (!DecodeRequest(context.channel(), request, request_struct)) {
    return;
  }

  NanopbServerCall responder(context, MethodType::kUnary);
  const Status status = function_.synchronous_unary(
      context.service(), request_struct, response_struct);
  responder.SendUnaryResponse(response_struct, status).IgnoreError();
}

void NanopbMethod::CallUnaryRequest(const CallContext& context,
                                    MethodType type,
                                    const Packet& request,
                                    void* request_struct) const {
  if (!DecodeRequest(context.channel(), request, request_struct)) {
    return;
  }

  NanopbServerCall server_writer(context, type);
  function_.unary_request(context.service(), request_struct, server_writer);
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

}  // namespace internal
}  // namespace pw::rpc
