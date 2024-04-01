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

#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_channel/channel.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"

namespace pw::channel {

/// @defgroup pw_channel_loopback
/// @{

// Channel implementation which will read its own writes.
template <DataType kType>
class LoopbackChannel;

/// Alias for a loopback channel that sends and receives datagrams.
using LoopbackDatagramChannel = LoopbackChannel<DataType::kDatagram>;

/// Alias for a loopback channel that sends and receives bytes.
using LoopbackByteChannel = LoopbackChannel<DataType::kByte>;

/// @}

template <>
class LoopbackChannel<DataType::kDatagram>
    : public ReliableDatagramReaderWriter {
 public:
  LoopbackChannel(multibuf::MultiBufAllocator& write_allocator)
      : write_allocator_(&write_allocator) {}
  LoopbackChannel(const LoopbackChannel&) = delete;
  LoopbackChannel& operator=(const LoopbackChannel&) = delete;

  LoopbackChannel(LoopbackChannel&&) = default;
  LoopbackChannel& operator=(LoopbackChannel&&) = default;

 private:
  async2::Poll<Result<multibuf::MultiBuf>> DoPendRead(
      async2::Context& cx) override;

  async2::Poll<Status> DoPendReadyToWrite(async2::Context& cx) final;

  multibuf::MultiBufAllocator& DoGetWriteAllocator() final {
    return *write_allocator_;
  }

  Result<channel::WriteToken> DoWrite(multibuf::MultiBuf&& data) final;

  async2::Poll<Result<channel::WriteToken>> DoPendFlush(async2::Context&) final;

  async2::Poll<Status> DoPendClose(async2::Context&) final;

  multibuf::MultiBufAllocator* write_allocator_;
  std::optional<multibuf::MultiBuf> queue_;
  uint32_t write_token_;
  async2::Waker waker_;
};

template <>
class LoopbackChannel<DataType::kByte> : public ByteReaderWriter {
 public:
  LoopbackChannel(multibuf::MultiBufAllocator& write_allocator)
      : write_allocator_(&write_allocator) {}
  LoopbackChannel(const LoopbackChannel&) = delete;
  LoopbackChannel& operator=(const LoopbackChannel&) = delete;

  LoopbackChannel(LoopbackChannel&&) = default;
  LoopbackChannel& operator=(LoopbackChannel&&) = default;

 private:
  async2::Poll<Result<multibuf::MultiBuf>> DoPendRead(
      async2::Context& cx) override;

  async2::Poll<Status> DoPendReadyToWrite(async2::Context&) final {
    return async2::Ready(OkStatus());
  }

  multibuf::MultiBufAllocator& DoGetWriteAllocator() final {
    return *write_allocator_;
  }

  Result<channel::WriteToken> DoWrite(multibuf::MultiBuf&& data) final;

  async2::Poll<Result<channel::WriteToken>> DoPendFlush(async2::Context&) final;

  async2::Poll<Status> DoPendClose(async2::Context&) final;

  multibuf::MultiBufAllocator* write_allocator_;
  multibuf::MultiBuf queue_;
  uint32_t write_token_;
  async2::Waker read_waker_;
};

}  // namespace pw::channel
