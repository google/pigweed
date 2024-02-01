// Copyright 2024 The Pigweed Authors
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
#pragma once

#include <cstdint>
#include <optional>

#include "pw_bytes/span.h"
#include "pw_grpc/connection.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/packet.h"

namespace pw::grpc {

class GrpcChannelOutput : public rpc::ChannelOutput {
 public:
  GrpcChannelOutput() : pw::rpc::ChannelOutput("grpc") {}

  class StreamCallbacks {
   public:
    // Called when a stream is completed from the server. Called on the same
    // thread as Send is called on.
    virtual void OnClose(StreamId) = 0;
  };

  void set_callbacks(StreamCallbacks& callbacks) { callbacks_ = callbacks; }

  void set_connection(Connection& conn) { connection_ = conn; }

  Status Send(ConstByteSpan data) override {
    using pw::rpc::internal::pwpb::PacketType;

    if (!connection_.has_value()) {
      return Status::FailedPrecondition();
    }
    // TODO: b/319162657 - Avoid this extra decode
    PW_TRY_ASSIGN(rpc::internal::Packet packet,
                  rpc::internal::Packet::FromBuffer(data));

    switch (packet.type()) {
      case PacketType::kResponse:
        if (packet.payload().size()) {
          PW_TRY(connection_->get().SendResponseMessage(packet.call_id(),
                                                        packet.payload()));
        }
        PW_TRY(connection_->get().SendResponseComplete(packet.call_id(),
                                                       packet.status()));
        if (callbacks_.has_value()) {
          callbacks_->get().OnClose(packet.call_id());
        }
        break;
      case PacketType::kServerStream:
        PW_TRY(connection_->get().SendResponseMessage(packet.call_id(),
                                                      packet.payload()));
        break;
      case PacketType::kServerError:
        PW_TRY(connection_->get().SendResponseComplete(packet.call_id(),
                                                       packet.status()));
        if (callbacks_.has_value()) {
          callbacks_->get().OnClose(packet.call_id());
        }
        break;
      default:
        return Status::FailedPrecondition();
    }

    return OkStatus();
  }

 private:
  std::optional<std::reference_wrapper<StreamCallbacks>> callbacks_;
  std::optional<std::reference_wrapper<Connection>> connection_;
};

}  // namespace pw::grpc
