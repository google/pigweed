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

#include "pw_channel/rp2_stdio_channel.h"

#include "pico/stdlib.h"
#include "pw_assert/check.h"
#include "pw_async2/dispatcher_base.h"
#include "pw_log/log.h"
#include "pw_multibuf/multibuf.h"
#include "pw_status/status.h"

namespace pw::channel {
namespace {

using pw::async2::Context;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::WaitReason;
using pw::async2::Waker;
using pw::multibuf::MultiBuf;

Waker global_chars_available_waker;

void InitStdio() {
  stdio_init_all();
  stdio_set_chars_available_callback(
      []([[maybe_unused]] void* arg) {
        std::move(global_chars_available_waker).Wake();
      },
      nullptr);
}

void WriteMultiBuf(const MultiBuf& buf) {
  for (std::byte b : buf) {
    putchar_raw(static_cast<int>(b));
  }
}

Poll<std::byte> PollReadByte(Context& cx) {
  int c = getchar_timeout_us(0);
  if (c == PICO_ERROR_TIMEOUT) {
    global_chars_available_waker = cx.GetWaker(WaitReason::Unspecified());
    // Read again to ensure that no race occurred.
    //
    // The concern is an interleaving like this
    // Thread one: get_char is called and times out
    // Thread two: char becomes available, Wake is called
    // Thread one: sets Waker
    //
    // In this interleaving, the task on Thread one is never awoken,
    // so we must check for available characters *after* setting the Waker.
    c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) {
      return Pending();
    }
  }
  return static_cast<std::byte>(c);
}

// Channel implementation which writes to and reads from rp2040's stdio.
//
// NOTE: only one Rp2StdioChannel may be in existence.
class Rp2StdioChannel final : public ByteReaderWriter {
 public:
  Rp2StdioChannel(multibuf::MultiBufAllocator& allocator)
      : write_token_(0),
        allocation_future_(std::nullopt),
        buffer_(std::nullopt),
        allocator_(&allocator) {}

  Rp2StdioChannel(const Rp2StdioChannel&) = delete;
  Rp2StdioChannel& operator=(const Rp2StdioChannel&) = delete;

  // This is a singleton static, so there's no need for move constructors.
  Rp2StdioChannel(Rp2StdioChannel&&) = delete;
  Rp2StdioChannel& operator=(Rp2StdioChannel&&) = delete;

 private:
  static constexpr size_t kMinimumReadSize = 64;
  static constexpr size_t kDesiredReadSize = 1024;

  async2::Poll<Status> PendGetBuffer(async2::Context& cx);

  async2::Poll<Result<multibuf::MultiBuf>> DoPendRead(
      async2::Context& cx) override;

  async2::Poll<Status> DoPendReadyToWrite(async2::Context& cx) override;

  multibuf::MultiBufAllocator& DoGetWriteAllocator() override {
    return *allocator_;
  }

  Result<channel::WriteToken> DoWrite(multibuf::MultiBuf&& data) override;

  async2::Poll<Result<channel::WriteToken>> DoPendFlush(
      async2::Context&) override {
    return CreateWriteToken(write_token_);
  }

  async2::Poll<Status> DoPendClose(async2::Context&) override {
    return async2::Ready(OkStatus());
  }

  uint32_t write_token_;
  std::optional<multibuf::MultiBufAllocationFuture> allocation_future_;
  std::optional<multibuf::MultiBuf> buffer_;
  multibuf::MultiBufAllocator* allocator_;
};

Poll<Status> Rp2StdioChannel::PendGetBuffer(async2::Context& cx) {
  if (buffer_.has_value()) {
    return OkStatus();
  }

  if (!allocation_future_.has_value()) {
    allocation_future_ =
        allocator_->AllocateContiguousAsync(kMinimumReadSize, kDesiredReadSize);
  }
  async2::Poll<std::optional<multibuf::MultiBuf>> maybe_multibuf =
      allocation_future_->Pend(cx);
  if (maybe_multibuf.IsPending()) {
    return Pending();
  }

  allocation_future_ = std::nullopt;

  if (!maybe_multibuf->has_value()) {
    PW_LOG_ERROR("Failed to allocate multibuf for reading");
    return Status::ResourceExhausted();
  }

  buffer_ = std::move(**maybe_multibuf);
  return OkStatus();
}

Poll<Result<MultiBuf>> Rp2StdioChannel::DoPendRead(Context& cx) {
  Poll<Status> buffer_ready = PendGetBuffer(cx);
  if (buffer_ready.IsPending() || !buffer_ready->ok()) {
    return buffer_ready;
  }

  size_t len = 0;
  for (std::byte& b : *buffer_) {
    Poll<std::byte> next = PollReadByte(cx);
    if (next.IsPending()) {
      break;
    }
    b = *next;
    ++len;
  }
  if (len == 0) {
    return Pending();
  }
  buffer_->Truncate(len);
  MultiBuf buffer = std::move(*buffer_);
  buffer_ = std::nullopt;
  return buffer;
}

async2::Poll<Status> Rp2StdioChannel::DoPendReadyToWrite(async2::Context&) {
  return OkStatus();
}

Result<channel::WriteToken> Rp2StdioChannel::DoWrite(
    multibuf::MultiBuf&& data) {
  WriteMultiBuf(data);
  const uint32_t token = write_token_++;
  return CreateWriteToken(token);
}

}  // namespace

ByteReaderWriter& Rp2StdioChannelInit(
    pw::multibuf::MultiBufAllocator& allocator) {
  static std::optional<Rp2StdioChannel> channel = std::nullopt;
  PW_CHECK(!channel.has_value());
  InitStdio();
  return channel.emplace(allocator);
}

}  // namespace pw::channel
