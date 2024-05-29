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

/// @defgroup pw_channel_epoll
/// @{

/// Channel implementation which writes to and reads from a file descriptor,
/// backed by Linux's epoll notification system.
///
/// This channel depends on APIs provided by the EpollDispatcher and cannot be
/// used with any other dispatcher backend.
///
/// An instantiated EpollChannel takes ownership of the file descriptor it is
/// given, and will close it if the channel is closed or destroyed. Users should
/// not close a channel's file descriptor from outside.
class EpollChannel : public ByteReaderWriter {
 public:
  EpollChannel(int channel_fd,
               async2::Dispatcher& dispatcher,
               multibuf::MultiBufAllocator& allocator)
      : channel_fd_(channel_fd),
        open_(false),
        ready_to_write_(false),
        write_token_(0),
        allocation_future_(std::nullopt),
        dispatcher_(&dispatcher),
        allocator_(&allocator) {}

  ~EpollChannel() override { Cleanup(); }

  EpollChannel(const EpollChannel&) = delete;
  EpollChannel& operator=(const EpollChannel&) = delete;

  EpollChannel(EpollChannel&&) = default;
  EpollChannel& operator=(EpollChannel&&) = default;

  /// Registers the channel's file descriptor on its EpollDisptacher.
  /// Must be called before any other channel operations.
  ///
  /// @return @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The channel has been registered and can be used.
  ///
  ///    INTERNAL: One of the underlying Linux syscalls failed. A log message
  ///      with the value of ``errno`` is sent.
  ///
  /// @endrst
  Status Register();

  constexpr bool is_open() const { return open_; }

 private:
  static constexpr size_t kMinimumReadSize = 64;
  static constexpr size_t kDesiredReadSize = 1024;

  async2::Poll<Result<multibuf::MultiBuf>> DoPendRead(
      async2::Context& cx) override;

  async2::Poll<Status> DoPendReadyToWrite(async2::Context& cx) final;

  multibuf::MultiBufAllocator& DoGetWriteAllocator() final {
    return *allocator_;
  }

  Result<channel::WriteToken> DoWrite(multibuf::MultiBuf&& data) final;

  async2::Poll<Result<channel::WriteToken>> DoPendFlush(
      async2::Context&) final {
    return CreateWriteToken(write_token_);
  }

  async2::Poll<Status> DoPendClose(async2::Context&) final {
    Cleanup();
    return async2::Ready(OkStatus());
  }

  void Cleanup();

  int channel_fd_;
  bool open_;
  bool ready_to_write_;
  uint32_t write_token_;
  std::optional<multibuf::MultiBufAllocationFuture> allocation_future_;
  async2::Dispatcher* dispatcher_;
  multibuf::MultiBufAllocator* allocator_;
  async2::Waker waker_;
};

/// @}

}  // namespace pw::channel
