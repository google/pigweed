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
#pragma once

#include <array>
#include <span>

#include "pw_assert/assert.h"
#include "pw_hdlc/encoder.h"
#include "pw_rpc/channel.h"
#include "pw_stream/stream.h"

namespace pw::hdlc {

// Custom HDLC ChannelOutput class to write and read data through serial using
// the HDLC protocol.
//
// WARNING: This ChannelOutput is not thread-safe. If thread-safety is required,
// create a similar class that adds a mtuex to Send.
class RpcChannelOutput : public rpc::ChannelOutput {
 public:
  // The RpcChannelOutput class does not own the buffer it uses to store the
  // protobuf bytes. This buffer is specified at the time of creation along with
  // a writer object to which will be used to write and send the bytes.
  constexpr RpcChannelOutput(stream::Writer& writer,
                             uint64_t address,
                             const char* channel_name)
      : ChannelOutput(channel_name), writer_(writer), address_(address) {}

  Status Send(std::span<const std::byte> buffer) override {
    return hdlc::WriteUIFrame(address_, buffer, writer_);
  }

 private:
  stream::Writer& writer_;
  const uint64_t address_;
};

}  // namespace pw::hdlc
