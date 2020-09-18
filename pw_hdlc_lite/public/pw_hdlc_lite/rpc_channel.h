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

#include <span>

#include "pw_rpc/channel.h"
#include "pw_stream/stream.h"

namespace pw::rpc {

// Custom HDLC ChannelOutput class to write and read data through serial using
// the HDLC-Lite protocol.
class HdlcChannelOutput : public ChannelOutput {
 public:
  // The HdlcChannelOutput class does not own the buffer it uses to store the
  // protobuf bytes. This buffer is specified at the time of creation along with
  // a writer object to which will be used to write and send the bytes.
  constexpr HdlcChannelOutput(stream::Writer& writer,
                              std::span<std::byte> buffer,
                              uint8_t address,
                              const char* channel_name)
      : ChannelOutput(channel_name),
        buffer_(buffer),
        address_(address),
        writer_(&writer) {}

  std::span<std::byte> AcquireBuffer() override { return buffer_; }

  // Any errors that may arise while encoding and writing the payload will be
  // logged.
  void SendAndReleaseBuffer(size_t size) override;

 private:
  std::span<std::byte> buffer_;
  uint8_t address_;
  stream::Writer* writer_;
};

}  // namespace pw::rpc
