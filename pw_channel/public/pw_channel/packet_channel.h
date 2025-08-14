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

//         __      ___   ___ _  _ ___ _  _  ___
//         \ \    / /_\ | _ \ \| |_ _| \| |/ __|
//          \ \/\/ / _ \|   / .` || || .` | (_ |
//           \_/\_/_/ \_\_|_\_|\_|___|_|\_|\___|
//  _____  _____ ___ ___ ___ __  __ ___ _  _ _____ _   _
// | __\ \/ / _ \ __| _ \_ _|  \/  | __| \| |_   _/_\ | |
// | _| >  <|  _/ _||   /| || |\/| | _|| .` | | |/ _ \| |__
// |___/_/\_\_| |___|_|_\___|_|  |_|___|_|\_| |_/_/ \_\____|
//
// This module is in an early, experimental state. DO NOT USE THIS CLASS until
// this banner has been removed.

#include <cstddef>
#include <utility>

#include "lib/stdcompat/utility.h"
#include "pw_assert/assert.h"
#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_async2/waker.h"
#include "pw_channel/properties.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::channel {

/// Represents a pending write operation. Returned by
/// `pw::channel::PacketChannel::PendReadyToWrite`.
template <typename Packet>
class PendingWrite {
 public:
  PendingWrite(const PendingWrite&) = delete;
  PendingWrite& operator=(const PendingWrite&) = delete;

  constexpr PendingWrite(PendingWrite&& other)
      : channel_(other.channel_),
        num_packets_(cpp20::exchange(other.num_packets_, 0u)) {}
  constexpr PendingWrite& operator=(PendingWrite&& other) {
    channel_ = other.channel_;
    num_packets_ = cpp20::exchange(other.num_packets_, 0u);
  }

  ~PendingWrite() {
    // TODO: b/421961717 - Consider allowing staged writes to be discarded
    PW_ASSERT(num_packets_ == 0u);
  }

  /// Enqueues a packet to be written. Must be called before the `PendingWrite`
  /// goes out of scope.
  ///
  /// `Stage` may be called up to `num_packets()` times.
  void Stage(Packet&& packet) {
    PW_ASSERT(num_packets_ > 0u);
    channel_->DoStageWrite(std::move(packet));
    num_packets_ -= 1;
  }

  size_t num_packets() const { return num_packets_; }

 private:
  friend class AnyPacketChannel<Packet>;

  constexpr PendingWrite(AnyPacketChannel<Packet>& channel, size_t num_packets)
      : channel_(&channel), num_packets_(num_packets) {}

  AnyPacketChannel<Packet>* channel_;
  size_t num_packets_;
};

/// @defgroup pw_channel_packets
/// @{

/// If the number of available writes is set to this value, flow control is
/// disabled.
inline constexpr uint16_t kNoFlowControl = std::numeric_limits<uint16_t>::max();

/// Interface that supports reading and writing writing packets. The supported
/// operations are reflected in the type.
///
/// `PacketChannel` is typically referred to by one its aliases:
/// `pw::channel::PacketReader`,  `pw::channel::PacketWriter`, or
/// `pw::channel::PacketReaderWriter`.
///
/// To implement a packet channel, derive from `pw::channel::Implement<>` with
/// the desired `PacketChannel` type.
///
/// @warning This class is in an early, experimental state. Do not use it until
/// this warning is removed (https://pwbug.dev/421962771).
template <typename T, Property... kProperties>
class PacketChannel {
 public:
  using Packet = T;

  // @copydoc AnyPacketChannel::readable
  [[nodiscard]] static constexpr bool readable() {
    return ((kProperties == Property::kReadable) || ...);
  }
  // @copydoc AnyPacketChannel::writable
  [[nodiscard]] static constexpr bool writable() {
    return ((kProperties == Property::kWritable) || ...);
  }

  // @copydoc AnyPacketChannel::is_read_open
  [[nodiscard]] constexpr bool is_read_open() const;

  // @copydoc AnyPacketChannel::is_write_open
  [[nodiscard]] constexpr bool is_write_open() const;

  // @copydoc AnyPacketChannel::is_read_or_write_open
  [[nodiscard]] constexpr bool is_read_or_write_open() const {
    return is_read_open() || is_write_open();
  }

  // Read API

  // @copydoc AnyPacketChannel::PendRead
  async2::PollResult<Packet> PendRead(async2::Context& cx);

  // Write API

  // @copydoc AnyPacketChannel::PendReadyToWrite
  async2::PollResult<PendingWrite<Packet>> PendReadyToWrite(async2::Context& cx,
                                                            size_t num = 1);
  // @copydoc AnyPacketChannel::PendWrite
  async2::Poll<> PendWrite(async2::Context& cx);

  // @copydoc AnyPacketChannel::SetAvailableWrites
  void SetAvailableWrites(uint16_t available_writes);
  // @copydoc AnyPacketChannel::AcknowledgeWrites
  void AcknowledgeWrites(uint16_t num_completed);

  // @copydoc AnyPacketChannel::PendClose
  async2::Poll<Status> PendClose(async2::Context& cx);

  // Conversions

  /// Channels may be implicitly converted to other compatible channel types.
  template <typename Sibling,
            typename = internal::EnableIfConvertible<PacketChannel, Sibling>>
  constexpr operator Sibling&() {
    return as<Sibling>();
  }
  template <typename Sibling,
            typename = internal::EnableIfConvertible<PacketChannel, Sibling>>
  constexpr operator const Sibling&() const {
    return as<Sibling>();
  }
  constexpr operator AnyPacketChannel<Packet>&() {
    return static_cast<AnyPacketChannel<Packet>&>(*this);
  }
  constexpr operator const AnyPacketChannel<Packet>&() const {
    return static_cast<const AnyPacketChannel<Packet>&>(*this);
  }

  /// Returns a reference to this as another compatible packet channel type.
  template <typename Sibling>
  [[nodiscard]] Sibling& as() {
    internal::CheckPacketChannelConversion<PacketChannel, Sibling>();
    return static_cast<Sibling&>(static_cast<AnyPacketChannel<Packet>&>(*this));
  }
  template <typename Sibling>
  [[nodiscard]] const Sibling& as() const {
    internal::CheckPacketChannelConversion<PacketChannel, Sibling>();
    return static_cast<const Sibling&>(
        static_cast<const AnyPacketChannel<Packet>&>(*this));
  }

  /// Returns a reference to this channel as another channel with the specified
  /// properties, which must be compatible.
  template <Property... kOtherProperties>
  [[nodiscard]] auto& as() {
    return as<PacketChannel<Packet, kOtherProperties...>>();
  }
  template <Property... kOtherProperties>
  [[nodiscard]] const auto& as() const {
    return as<PacketChannel<Packet, kOtherProperties...>>();
  }

 protected:
  // @copydoc AnyPacketChannel::GetAvailableWrites
  uint16_t GetAvailableWrites() const;

 private:
  static_assert(internal::PacketChannelPropertiesAreValid<kProperties...>());

  template <typename>
  friend class AnyPacketChannel;

  explicit constexpr PacketChannel() = default;
};

/// `PacketChannel` that optionally supports reading and writing. Generally,
/// prefer `PacketChannel`, which expresses readability and writability in the
/// type.
///
/// @warning This class is in an early, experimental state. Do not use it until
/// this warning is removed (https://pwbug.dev/421962771).
template <typename T>
class AnyPacketChannel : private PacketChannel<T, kReadable>,
                         private PacketChannel<T, kWritable>,
                         private PacketChannel<T, kReadable, kWritable> {
 public:
  using Packet = T;

  virtual ~AnyPacketChannel() = default;

  /// Returns whether the channel implementation is readable.
  [[nodiscard]] constexpr bool readable() const {
    return (properties_ & kReadable) != 0u;
  }

  /// Returns whether the channel implementation is writable.
  [[nodiscard]] constexpr bool writable() const {
    return (properties_ & kWritable) != 0u;
  }

  /// True if the channel is open for reading. Always false for write-only
  /// channels.
  [[nodiscard]] constexpr bool is_read_open() const {
    return (read_write_open_ & kReadable) != 0u;
  }

  /// True if the channel is open for writing. Always false for read-only
  /// channels.
  [[nodiscard]] constexpr bool is_write_open() const {
    return (read_write_open_ & kWritable) != 0u;
  }

  /// True if the channel is open for either reading or writing.
  [[nodiscard]] constexpr bool is_read_or_write_open() const {
    return read_write_open_ != 0u;
  }

  /// Returns `async2::Ready` with a new packet when one arrives. If no packet
  /// is ready yet, returns `async2::Pending`. Returns `async2::Ready` with a
  /// non-OK status if there was an unrecoverable error.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: A packet was read
  ///
  ///    UNIMPLEMENTED: The channel does not support reading.
  ///
  ///    FAILED_PRECONDITION: The channel is closed for reading.
  ///
  ///    OUT_OF_RANGE: The end of the stream was reached and no further reads
  ///    will succeed.
  ///
  /// @endrst
  async2::PollResult<Packet> PendRead(async2::Context& cx) {
    // TODO: b/421962771 - if not readable, what to return (when called from
    // Any*)? The is_read_open() prevents you from getting to the DoPendRead()
    // that returns UNIMPLEMENTED.
    if (!is_read_open()) {
      return async2::Ready(Status::FailedPrecondition());
    }
    return DoPendRead(cx);
  }

  /// Returns `async2::Ready` if `num` packets can currently be staged, and
  /// `async2::Pending` otherwise. Returns `async2::Ready` with a non-OK status
  /// if there was an unrecoverable error.
  ///
  /// TODO: b/421961717 - Determine whether to keep this method.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The channel is currently writable. The returned `PendingWrite` may
  ///    be used to write a packet.
  ///
  ///    UNIMPLEMENTED: The channel does not support writing.
  ///
  ///    FAILED_PRECONDITION: The channel is closed for writing.
  ///
  /// @endrst
  async2::PollResult<PendingWrite<Packet>> PendReadyToWrite(async2::Context& cx,
                                                            size_t num = 1);

  /// Processes staged write packets. `PendWrite` must be called after a write
  /// is staged so the channel can complete the write. This could involve
  /// writing to physical hardware, pushing data into a queue, or be a no-op.
  ///
  /// `PendReadyToWrite` also allows the channel to send staged packets, but it
  /// is only called when there is a new outbound packet. `PendWrite` should be
  /// called after a write to avoid blocking outbound data until there is
  /// another packet to write.
  ///
  /// If the packets have a deallocator set, they will be automatically
  /// deallocated after they are written.
  ///
  /// @returns `async2::Ready` when the channel has completed the write
  /// operation for all outbound data. Returns `async2::Pending` otherwise.
  async2::Poll<> PendWrite(async2::Context& cx);

  /// Sets the number of packets the remote receiver can currently receive. This
  /// is typically set based on information from the receiver. Wakes any task
  /// waiting for `PendReadyToWrite` if the number of available writes
  /// increased.
  ///
  /// Set `available_writes` to `kNoFlowControl` to disable flow control and
  /// wake any pending task.
  void SetAvailableWrites(uint16_t available_writes);

  /// Increases the number of available writes and wakes. Equivalent to
  /// `SetAvailableWrites(GetAvailableWrites() + num_completed)`. Wakes a task
  /// waiting for `PendReadyToWrite`, if any.
  ///
  /// Crashes if `GetAvailableWrites()` is `kNoFlowControl`.
  void AcknowledgeWrites(uint16_t num_completed);

  /// Marks the channel as closed. Flushes any remaining data.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The channel was closed and all data was sent successfully.
  ///
  ///    DATA_LOSS: The channel was closed, but not all previously written
  ///    data was delivered.
  ///
  /// @endrst
  async2::Poll<Status> PendClose(async2::Context& cx);

 protected:
  /// Indicates how many additional packets can currently be sent to the remote
  /// receiver.
  uint16_t GetAvailableWrites() const { return available_writes_; }

  /// Marks the channel as closed for reading, but does nothing else.
  ///
  /// PendClose() always marks the channel closed when DoPendClose() returns
  /// Ready(), regardless of the status.
  void set_read_closed() { read_write_open_ &= ~uint8_t{kReadable}; }

  /// Marks the channel as closed for writing, but does nothing else.
  ///
  /// PendClose() always marks the channel closed when DoPendClose() returns
  /// Ready(), regardless of the status.
  void set_write_closed() { read_write_open_ &= ~uint8_t{kWritable}; }

  /// Marks the channel as close for both reading and writing, but does nothing
  /// else.
  ///
  /// PendClose() always marks the channel closed when DoPendClose() returns
  /// Ready(), regardless of the status.
  void set_read_write_closed() { read_write_open_ = 0; }

  /// Allows implementations to access the write waker.
  async2::Waker& write_waker() { return write_waker_; }

 private:
  template <typename, Property...>
  friend class PacketChannel;  // Allow static_casts to AnyPacketChannel

  template <typename, Property...>
  friend class internal::BasePacketChannelImpl;  // Allow inheritance

  friend PendingWrite<Packet>;

  explicit constexpr AnyPacketChannel(
      uint8_t properties, uint16_t available_writes = kNoFlowControl)
      : available_writes_(available_writes),
        properties_(properties),
        read_write_open_{uint8_t{kReadable} | uint8_t{kWritable}} {}

  virtual async2::PollResult<Packet> DoPendRead(async2::Context& cx) = 0;

  virtual async2::Poll<Status> DoPendReadyToWrite(async2::Context& cx,
                                                  size_t num) = 0;
  virtual void DoStageWrite(Packet&& packet) = 0;
  virtual async2::Poll<> DoPendWrite(async2::Context& cx) = 0;

  virtual async2::Poll<Status> DoPendClose(async2::Context& cx) = 0;

  async2::Waker write_waker_;
  uint16_t available_writes_;
  uint8_t properties_;
  uint8_t read_write_open_;
};

/// Asynchronous type that sends packets of a given type.
///
/// This type has built-in flow control, and will block when it exhausts its
/// available writes until some number of those writes have been acknowledged.
///
/// @tparam   Packet  Type to be written.
template <typename Packet>
using PacketWriter = PacketChannel<Packet, kWritable>;

/// Asynchronous type that receives packets of a given type.
///
/// @tparam   Packet  Type to be received.
template <typename Packet>
using PacketReader = PacketChannel<Packet, kReadable>;

/// Asynchronous type that sends and receives packets of a given type.
///
/// @tparam   Packet  Type to be received or written.
template <typename Packet>
using PacketReaderWriter = PacketChannel<Packet, kReadable, kWritable>;

/// @}

// Extend this implement a `pw::channel::Packet`.
template <typename ChannelType>
class Implement;

template <typename Packet, Property... kProperties>
class Implement<PacketChannel<Packet, kProperties...>>
    : public internal::PacketChannelImpl<Packet, kProperties...> {
 protected:
  explicit constexpr Implement() = default;
};

// Function implementations

template <typename Packet>
async2::PollResult<PendingWrite<Packet>>
AnyPacketChannel<Packet>::PendReadyToWrite(async2::Context& cx, size_t num) {
  PW_DASSERT(num > 0u);

  if (!is_write_open()) {
    return Status::FailedPrecondition();
  }

  if (GetAvailableWrites() < num) {
    PW_ASYNC_STORE_WAKER(cx, write_waker_, "waiting for available writes");
    return async2::Pending();
  }
  PW_TRY_READY_ASSIGN(Status ready, DoPendReadyToWrite(cx, num));
  if (!ready.ok()) {
    return ready;
  }
  return Result(PendingWrite<Packet>(*this, num));
}

template <typename Packet>
async2::Poll<> AnyPacketChannel<Packet>::PendWrite(async2::Context& cx) {
  if (!is_write_open()) {
    return async2::Ready();
  }

  if (GetAvailableWrites() == 0u) {
    PW_ASYNC_STORE_WAKER(
        cx, write_waker_, "waiting for writes to be acknowledged");
    return async2::Pending();
  }

  PW_TRY_READY(DoPendWrite(cx));
  if (GetAvailableWrites() != kNoFlowControl) {
    available_writes_ -= 1;
  }

  return async2::Ready();
}

template <typename Packet>
void AnyPacketChannel<Packet>::SetAvailableWrites(uint16_t available_writes) {
  if (available_writes > available_writes_) {
    std::move(write_waker_).Wake();
  }
  available_writes_ = available_writes;
}

template <typename Packet>
void AnyPacketChannel<Packet>::AcknowledgeWrites(uint16_t num_completed) {
  PW_DASSERT(num_completed > 0u);
  PW_DASSERT(GetAvailableWrites() != kNoFlowControl);

  uint32_t new_available_writes = uint32_t{available_writes_} + num_completed;
  PW_ASSERT(available_writes_ < kNoFlowControl);

  available_writes_ = static_cast<uint16_t>(new_available_writes);

  std::move(write_waker_).Wake();
}

template <typename Packet>
async2::Poll<Status> AnyPacketChannel<Packet>::PendClose(async2::Context& cx) {
  if (!is_read_or_write_open()) {
    return OkStatus();
  }
  auto result = DoPendClose(cx);
  if (result.IsReady()) {
    set_read_write_closed();
  }
  std::move(write_waker_).Wake();
  return result;
}

// pw::channel::PacketChannel<> function implementations. These down-cast to
// AnyPacketChannel and call its functions, so must be defined after it.

template <typename Packet, Property... kProperties>
constexpr bool PacketChannel<Packet, kProperties...>::is_read_open() const {
  return readable() &&
         static_cast<const AnyPacketChannel<Packet>*>(this)->is_read_open();
}

template <typename Packet, Property... kProperties>
constexpr bool PacketChannel<Packet, kProperties...>::is_write_open() const {
  return writable() &&
         static_cast<const AnyPacketChannel<Packet>*>(this)->is_write_open();
}

template <typename Packet, Property... kProperties>
async2::PollResult<Packet> PacketChannel<Packet, kProperties...>::PendRead(
    async2::Context& cx) {
  static_assert(readable(), "PendRead may only be called on readable channels");
  return static_cast<AnyPacketChannel<Packet>*>(this)->PendRead(cx);
}

template <typename Packet, Property... kProperties>
async2::PollResult<PendingWrite<Packet>>
PacketChannel<Packet, kProperties...>::PendReadyToWrite(async2::Context& cx,
                                                        size_t num) {
  static_assert(writable(),
                "PendReadyToWrite may only be called on writable channels");
  return static_cast<AnyPacketChannel<Packet>*>(this)->PendReadyToWrite(cx,
                                                                        num);
}
template <typename Packet, Property... kProperties>
async2::Poll<> PacketChannel<Packet, kProperties...>::PendWrite(
    async2::Context& cx) {
  static_assert(writable(),
                "PendWrite may only be called on writable channels");
  return static_cast<AnyPacketChannel<Packet>*>(this)->PendWrite(cx);
}
template <typename Packet, Property... kProperties>
uint16_t PacketChannel<Packet, kProperties...>::GetAvailableWrites() const {
  static_assert(writable(),
                "GetAvailableWrites may only be called on writable channels");
  return static_cast<const AnyPacketChannel<Packet>&>(*this)
      .GetAvailableWrites();
}
template <typename Packet, Property... kProperties>
void PacketChannel<Packet, kProperties...>::SetAvailableWrites(
    uint16_t available_writes) {
  static_assert(writable(),
                "SetAvailableWrites may only be called on writable channels");
  return static_cast<AnyPacketChannel<Packet>*>(this)->SetAvailableWrites(
      available_writes);
}
template <typename Packet, Property... kProperties>
void PacketChannel<Packet, kProperties...>::AcknowledgeWrites(
    uint16_t num_completed) {
  static_assert(writable(),
                "AcknowledgeWrites may only be called on writable channels");
  return static_cast<AnyPacketChannel<Packet>*>(this)->AcknowledgeWrites(
      num_completed);
}

template <typename Packet, Property... kProperties>
async2::Poll<Status> PacketChannel<Packet, kProperties...>::PendClose(
    async2::Context& cx) {
  return static_cast<AnyPacketChannel<Packet>*>(this)->PendClose(cx);
}

namespace internal {

/// Common class for channel implementations that associates a channel
/// implementation with its top-level typed PacketChannel.
template <typename Packet, Property... kProperties>
class BasePacketChannelImpl : public AnyPacketChannel<Packet> {
 public:
  using Channel = PacketChannel<Packet, kProperties...>;

  Channel& channel() { return *this; }
  const Channel& channel() const { return *this; }

  using Channel::as;

  // Use the base channel's version of the AnyPacketChannel API, so unsupported
  // methods are disabled.
  using Channel::PendRead;

  using Channel::AcknowledgeWrites;
  using Channel::GetAvailableWrites;
  using Channel::PendReadyToWrite;
  using Channel::PendWrite;
  using Channel::SetAvailableWrites;

  using Channel::PendClose;

 private:
  friend class PacketChannelImpl<Packet, kProperties...>;

  constexpr BasePacketChannelImpl()
      : AnyPacketChannel<Packet>((static_cast<uint8_t>(kProperties) | ...)) {}
};

template <typename Packet, Property... kProperties>
class PacketChannelImpl {
  static_assert(PacketChannelPropertiesAreValid<kProperties...>());
};

// PacketChannelImpl specialization with no write support.
template <typename Packet>
class PacketChannelImpl<Packet, kReadable>
    : public internal::BasePacketChannelImpl<Packet, kReadable> {
 private:
  async2::Poll<Status> DoPendReadyToWrite(async2::Context&, size_t) final {
    return async2::Ready(Status::Unimplemented());
  }
  void DoStageWrite(Packet&&) final { PW_ASSERT(false); }
  async2::Poll<> DoPendWrite(async2::Context&) final { return async2::Ready(); }
};

// PacketChannelImpl specialization with no read support.
template <typename Packet>
class PacketChannelImpl<Packet, kWritable>
    : public internal::BasePacketChannelImpl<Packet, kWritable> {
 private:
  async2::PollResult<Packet> DoPendRead(async2::Context&) final {
    return async2::Ready(Status::Unimplemented());
  }
};

// PacketChannelImpl specialization that supports reading and writing.
template <typename Packet>
class PacketChannelImpl<Packet, kReadable, kWritable>
    : public internal::BasePacketChannelImpl<Packet, kReadable, kWritable> {};

}  // namespace internal
}  // namespace pw::channel
