// Copyright 2023 The Pigweed Authors
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

#include "pw_rpc_transport/rpc_transport.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/stream.h"

namespace pw::rpc {

// RpcFrameSender that wraps a stream::Writer.
template <size_t kMtu>
class StreamRpcFrameSender : public RpcFrameSender {
 public:
  StreamRpcFrameSender(stream::Writer& writer) : writer_(writer) {}

  size_t MaximumTransmissionUnit() const override { return kMtu; }

  Status Send(RpcFrame frame) override {
    PW_TRY(writer_.Write(frame.header));
    return writer_.Write(frame.payload);
  }

 private:
  stream::Writer& writer_;
};

}  // namespace pw::rpc
