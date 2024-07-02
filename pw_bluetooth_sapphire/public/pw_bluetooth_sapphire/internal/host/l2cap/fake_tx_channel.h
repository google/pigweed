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

#include <lib/fit/function.h>

#include <queue>

#include "pw_bluetooth_sapphire/internal/host/l2cap/tx_engine.h"

namespace bt::l2cap::internal {

// A fake TxEngine::TxChannel, useful for testing.
class FakeTxChannel : public TxEngine::TxChannel {
 public:
  using SendFrameHandler = fit::function<void(ByteBufferPtr)>;

  ~FakeTxChannel() override = default;

  FakeTxChannel& HandleSendFrame(SendFrameHandler handler) {
    send_frame_cb_ = std::move(handler);
    return *this;
  }

  FakeTxChannel& QueueSdu(ByteBufferPtr sdu) {
    queue_.push(std::move(sdu));
    return *this;
  }

  void SendFrame(ByteBufferPtr frame) override {
    if (send_frame_cb_)
      send_frame_cb_(std::move(frame));
  }

  std::optional<ByteBufferPtr> GetNextQueuedSdu() override {
    if (queue_.empty())
      return std::nullopt;
    ByteBufferPtr next = std::move(queue_.front());
    queue_.pop();
    return next;
  }

 private:
  SendFrameHandler send_frame_cb_;
  std::queue<ByteBufferPtr> queue_;
};

}  // namespace bt::l2cap::internal
