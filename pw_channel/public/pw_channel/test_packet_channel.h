// Copyright 2025 The Pigweed Authors
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

#include <iterator>

#include "pw_allocator/allocator.h"
#include "pw_assert/assert.h"
#include "pw_async2/waker.h"
#include "pw_channel/packet_channel.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_containers/dynamic_vector.h"
#include "pw_span/span.h"

namespace pw::channel {

/// `pw::channel::PacketReadWriter` implementation for testing use.
template <typename Packet>
class TestPacketReaderWriter final
    : public Implement<PacketReaderWriter<Packet>> {
 public:
  explicit constexpr TestPacketReaderWriter(allocator::Allocator& allocator)
      : read_queue_(allocator), staged_(allocator), written_(allocator) {}

  /// Returns all packets that have been written to this packet channel.
  span<const Packet> written_packets() const { return written_; }

  /// Enqueues packets to be returned from future `PendRead` calls.
  void EnqueueReadPacket(Packet&& packet) {
    read_queue_.push_back(std::move(packet));
    std::move(read_waker_).Wake();
  }

 private:
  using AnyPacketChannel<Packet>::write_waker;

  async2::Poll<Result<Packet>> DoPendRead(async2::Context& cx) override {
    if (read_queue_.empty()) {
      PW_ASYNC_STORE_WAKER(
          cx, read_waker_, "TestPacketReaderWriter::DoPendRead");
      return async2::Pending();
    }
    async2::Poll<Result<Packet>> result =
        async2::Ready(std::move(read_queue_.front()));
    read_queue_.pop_front();
    return result;
  }

  async2::Poll<Status> DoPendReadyToWrite(async2::Context& cx,
                                          size_t count) override {
    // The staged_ queue capacity represents a write reservation.
    if (staged_.capacity() != 0u) {
      PW_ASYNC_STORE_WAKER(
          cx, write_waker(), "TestPacketReaderWriter::DoPendReadyToWrite");
      return async2::Pending();
    }
    staged_.reserve_exact(
        static_cast<typename decltype(staged_)::size_type>(count));
    return async2::Ready(OkStatus());
  }

  void DoStageWrite(Packet&& packet) override {
    PW_ASSERT(staged_.size() < staged_.capacity());
    staged_.push_back(std::move(packet));
  }

  async2::Poll<> DoPendWrite(async2::Context&) override {
    FlushStaged();
    return async2::Ready();
  }

  async2::Poll<Status> DoPendClose(async2::Context&) override {
    FlushStaged();
    return async2::Ready(OkStatus());
  }

  void FlushStaged() {
    // TODO: b/424613355 - use `insert` instead of repeated `push_back`
    auto it = std::make_move_iterator(staged_.begin());
    const auto end = std::make_move_iterator(staged_.end());
    while (it != end) {
      written_.push_back(std::move(*it++));
    }
    staged_.clear();
    staged_.shrink_to_fit();
    std::move(write_waker()).Wake();
  }

  async2::Waker read_waker_;
  DynamicDeque<Packet> read_queue_;
  DynamicDeque<Packet> staged_;
  DynamicVector<Packet> written_;
};

}  // namespace pw::channel
