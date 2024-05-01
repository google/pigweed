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

#include <cinttypes>
#include <string_view>

#include "pw_bytes/span.h"
#include "pw_grpc/connection.h"
#include "pw_grpc/grpc_channel_output.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/server.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_status/status.h"
#include "pw_sync/inline_borrowable.h"

namespace pw::grpc {

class PwRpcHandler : public Connection::RequestCallbacks,
                     public GrpcChannelOutput::StreamCallbacks {
 public:
  PwRpcHandler(uint32_t channel_id, rpc::Server& server)
      : channel_id_(channel_id), server_(server) {}

  // GrpcChannelOutput::StreamCallbacks
  void OnClose(StreamId id) override;

  // Connection::RequestCallbacks
  void OnNewConnection() override;
  Status OnNew(StreamId id,
               InlineString<kMaxMethodNameSize> full_method_name) override;
  Status OnMessage(StreamId id, ByteSpan message) override;
  void OnHalfClose(StreamId id) override;

  void OnCancel(StreamId id) override;

 private:
  struct Stream {
    StreamId id;
    uint32_t service_id;
    uint32_t method_id;
    // Used for client streaming to determine whether initial request packet has
    // been sent on yet.
    bool sent_request = false;
  };

  // Returns copy of stream state so service/method id can be used unlocked.
  Result<Stream> LookupStream(StreamId id);
  void ResetAllStreams();
  void ResetStream(StreamId id);
  void MarkSentRequest(StreamId id);
  Status CreateStream(StreamId id, uint32_t service_id, uint32_t method_id);

  sync::InlineBorrowable<std::array<Stream, internal::kMaxConcurrentStreams>>
      streams_;
  const uint32_t channel_id_;
  rpc::Server& server_;
};

}  // namespace pw::grpc
