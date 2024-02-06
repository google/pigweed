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

//         __      ___   ___ _  _ ___ _  _  ___
//         \ \    / /_\ | _ \ \| |_ _| \| |/ __|
//          \ \/\/ / _ \|   / .` || || .` | (_ |
//           \_/\_/_/ \_\_|_\_|\_|___|_|\_|\___|
//  _____  _____ ___ ___ ___ __  __ ___ _  _ _____ _   _
// | __\ \/ / _ \ __| _ \_ _|  \/  | __| \| |_   _/_\ | |
// | _| >  <|  _/ _||   /| || |\/| | _|| .` | | |/ _ \| |__
// |___/_/\_\_| |___|_|_\___|_|  |_|___|_|\_| |_/_/ \_\____|
//
// This module is in an early, experimental state. The APIs are in flux and may
// change without notice. Please do not rely on it in production code, but feel
// free to explore and share feedback with the Pigweed team!

#include <cstddef>
#include <cstdint>
#include <limits>

#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_bytes/span.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::channel {

/// @defgroup pw_channel
/// @{

/// Basic properties of a `Channel`. A `Channel` type can convert to any other
/// `Channel` for which it supports the required properties. For example, a
/// `kReadable` and `kWritable` channel may be passed to an API that only
/// requires `kReadable`.
enum Property : uint8_t {
  kReliable = 1 << 0,
  kReadable = 1 << 1,
  kWritable = 1 << 2,
  kSeekable = 1 << 3,
};

/// The type of data exchanged in `Channel` read and write calls. Unlike
/// `Property`, `Channels` with different `DataType`s cannot be used
/// interchangeably.
enum class DataType : uint8_t { kByte = 0, kDatagram = 1 << 4 };

/// Positions from which to seek.
enum Whence : uint8_t {
  /// Seek from the beginning of the channel. The offset is a direct offset
  /// into the data.
  kBeginning = 0b001,

  /// Seek from the current position in the channel. The offset is added to
  /// the current position. Use a negative offset to seek backwards.
  ///
  /// Implementations may only support seeking within a limited range from the
  /// current position.
  kCurrent = 0b010,

  /// Seek from the end of the channel. The offset is added to the end
  /// position. Use a negative offset to seek backwards from the end.
  kEnd = 0b100,
};

/// Represents a write operation. `WriteToken` can be used to track whether a
/// particular write has been flushed.
class [[nodiscard]] WriteToken {
 public:
  constexpr WriteToken() : token_(0) {}

  constexpr WriteToken(const WriteToken&) = default;
  constexpr WriteToken& operator=(const WriteToken&) = default;

  constexpr bool operator==(const WriteToken& other) const {
    return token_ == other.token_;
  }
  constexpr bool operator!=(const WriteToken& other) const {
    return token_ != other.token_;
  }
  constexpr bool operator<(const WriteToken& other) const {
    return token_ < other.token_;
  }
  constexpr bool operator>(const WriteToken& other) const {
    return token_ > other.token_;
  }
  constexpr bool operator<=(const WriteToken& other) const {
    return token_ <= other.token_;
  }
  constexpr bool operator>=(const WriteToken& other) const {
    return token_ >= other.token_;
  }

 private:
  friend class Channel;

  constexpr WriteToken(uint32_t value) : token_(value) {}

  uint32_t token_;
};

/// A generic data channel that may support reading or writing bytes or
/// datagrams.
///
/// Note that this channel should be used from only one ``pw::async::Task``
/// at a time, as the ``Poll`` methods are only required to remember the
/// latest ``pw::async2::Context`` that was provided.
class Channel {
 public:
  virtual ~Channel() = default;

  // Returned by Position() if getting the position is not supported.
  // TODO: b/323622630 - `Seek` and `Position` are not yet implemented.
  // static constexpr size_t kUnknownPosition =
  //     std::numeric_limits<size_t>::max();

  // Channel properties

  [[nodiscard]] constexpr DataType data_type() const {
    return (type_reliable_seekable_ &
            static_cast<uint8_t>(DataType::kDatagram)) == 0
               ? DataType::kDatagram
               : DataType::kByte;
  }

  [[nodiscard]] constexpr bool reliable() const {
    return (type_reliable_seekable_ & Property::kReliable) != 0;
  }

  [[nodiscard]] constexpr bool seekable() const {
    return (type_reliable_seekable_ & Property::kSeekable) != 0;
  }

  [[nodiscard]] constexpr bool readable() const { return readable_; }

  [[nodiscard]] constexpr bool writable() const { return writable_; }

  [[nodiscard]] constexpr bool is_open() const { return open_; }

  /// Read API

  /// Returns a MultiBuf read data, if available. If data is not available,
  /// invokes cx.waker() when it becomes available.
  ///
  /// For datagram channels, each successful read yields one complete
  /// datagram. For byte stream channels, each successful read yields some
  /// number of bytes.
  ///
  /// Channels only support one read operation / waker at a time.
  ///
  /// * OK - Data was read into a MultiBuf.
  /// * UNIMPLEMENTED - The channel does not support reading.
  /// * FAILED_PRECONDITION - The channel is closed.
  /// * OUT_OF_RANGE - The end of the stream was reached. This may be though
  ///   of as reaching the end of a file. Future reads may succeed after
  ///   ``Seek`` ing backwards, but no more new data will be produced. The
  ///   channel is still open; writes and seeks may succeed.
  async2::Poll<Result<multibuf::MultiBuf>> PollRead(async2::Context& cx) {
    return PollRead(cx, std::numeric_limits<size_t>::max());
  }

  /// On byte stream channels, reads up to max_bytes from the channel.
  /// This function is hidden on datagram-oriented channels.
  async2::Poll<Result<multibuf::MultiBuf>> PollRead(async2::Context& cx,
                                                    size_t max_bytes) {
    return DoPollRead(cx, max_bytes);
  }

  /// Write API

  /// Checks whether a writeable channel is *currently* writeable.
  ///
  /// This should be called before attempting to ``Write``, and may be called
  /// before allocating a write buffer if trying to reduce memory pressure.
  ///
  /// If ``Ready`` is returned, a *single* caller may proceed to ``Write``.
  ///
  /// If ``Pending`` is returned, ``cx`` will be awoken when the channel
  /// becomes writeable again.
  ///
  /// Note: this method will always return ``Ready`` for non-writeable
  /// channels.
  async2::Poll<> PollReadyToWrite(pw::async2::Context& cx) {
    return DoPollReadyToWrite(cx);
  }

  /// Gives access to an allocator for write buffers. The MultiBufAllocator
  /// provides an asynchronous API for obtaining a buffer.
  ///
  /// This allocator must *only* be used to allocate the next argument to
  /// ``Write``. The allocator must be used at most once per call to
  /// ``Write``, and the returned ``MultiBuf`` must not be combined with
  /// any other ``MultiBuf`` s or ``Chunk`` s.
  ///
  /// Write allocation attempts will always return ``std::nullopt`` for
  /// channels that do not support writing.
  multibuf::MultiBufAllocator& GetWriteAllocator();

  /// Writes using a previously allocated MultiBuf. Returns a token that
  /// refers to this write. These tokens are monotonically increasing, and
  /// FlushPoll() returns the value of the latest token it has flushed.
  ///
  /// The ``MultiBuf`` argument to ``Write`` may consist of either:
  ///   (1) A single ``MultiBuf`` allocated by ``GetWriteAllocator()``
  ///       that has not been combined with any other ``MultiBuf`` s
  ///       or ``Chunk``s OR
  ///   (2) A ``MultiBuf`` containing any combination of buffers from sources
  ///       other than ``GetWriteAllocator``.
  ///
  /// This requirement allows for more efficient use of memory in case (1).
  /// For example, a ring-buffer implementation of a ``Channel`` may
  /// specialize ``GetWriteAllocator`` to return the next section of the
  /// buffer available for writing.
  ///
  /// May fail with the following error codes:
  ///
  /// * OK - Data was accepted by the channel
  /// * UNIMPLEMENTED - The channel does not support writing.
  /// * UNAVAILABLE - The write failed due to a transient error (only applies
  ///   to unreliable channels).
  /// * FAILED_PRECONDITION - The channel is closed.
  Result<WriteToken> Write(multibuf::MultiBuf&& data) {
    return DoWrite(std::move(data));
  }

  /// Flushes pending writes.
  ///
  /// Returns a ``async2::Poll`` indicating whether or not flushing has
  /// completed.
  ///
  /// * Ready(OK) - All data has been successfully flushed.
  /// * Ready(UNIMPLEMENTED) - The channel does not support writing.
  /// * Ready(FAILED_PRECONDITION) - The channel is closed.
  /// * Pending - Data remains to be flushed.
  async2::Poll<Result<WriteToken>> PollFlush(async2::Context& cx) {
    return DoPollFlush(cx);
  }

  /// Seek changes the position in the stream.
  ///
  /// TODO: b/323622630 - `Seek` and `Position` are not yet implemented.
  ///
  /// Any ``PollRead`` or ``Write`` calls following a call to ``Seek`` will be
  /// relative to the new position. Already-written data still being flushed
  /// will be output relative to the old position.
  ///
  /// * OK - The current position was successfully changed.
  /// * UNIMPLEMENTED - The channel does not support seeking.
  /// * FAILED_PRECONDITION - The channel is closed.
  /// * NOT_FOUND - The seek was to a valid position, but the channel is no
  ///   longer capable of seeking to this position (partially seekable
  ///   channels only).
  /// * OUT_OF_RANGE - The seek went beyond the end of the stream.
  async2::Poll<Status> Seek(async2::Context& cx,
                            ptrdiff_t position,
                            Whence whence);

  /// Returns the current position in the stream, or `kUnknownPosition` if
  /// unsupported.
  ///
  /// TODO: b/323622630 - `Seek` and `Position` are not yet implemented.
  size_t Position() const;

  /// Closes the channel, flushing any data.
  ///
  /// * OK - The channel was closed and all data was sent successfully.
  /// * DATA_LOSS - The channel was closed, but not all previously written
  ///   data was delivered.
  /// * FAILED_PRECONDITION - Channel was already closed, which can happen
  ///   out-of-band due to errors.
  async2::Poll<pw::Status> PollClose(async2::Context& cx) {
    auto result = DoPollClose(cx);
    open_ = false;
    return result;
  }

 protected:
  static constexpr WriteToken CreateWriteToken(uint32_t value) {
    return WriteToken(value);
  }

 private:
  template <Property...>
  friend class ByteChannel;

  template <Property...>
  friend class DatagramChannel;

  template <Property kLhs, Property kRhs, Property... kProperties>
  static constexpr bool PropertiesAreInOrderWithoutDuplicates() {
    return (kLhs < kRhs) &&
           PropertiesAreInOrderWithoutDuplicates<kRhs, kProperties...>();
  }

  template <Property>
  static constexpr bool PropertiesAreInOrderWithoutDuplicates() {
    return true;
  }

  template <Property... kProperties>
  static constexpr uint8_t GetProperties() {
    return (static_cast<uint8_t>(kProperties) | ...);
  }

  template <Property... kProperties>
  static constexpr bool PropertiesAreValid() {
    static_assert(((kProperties != kSeekable) && ...),
                  "Seekable channels are not yet implemented; see b/323624921");

    static_assert(((kProperties == kReadable) || ...) ||
                      ((kProperties == kWritable) || ...),
                  "At least one of kReadable or kWritable must be provided");
    static_assert(sizeof...(kProperties) <= 4,
                  "Too many properties given; no more than 4 may be specified "
                  "(kReliable, kReadable, kWritable, kSeekable)");
    static_assert(
        PropertiesAreInOrderWithoutDuplicates<kProperties...>(),
        "Properties must be specified in the following order, without "
        "duplicates: kReliable, kReadable, kWritable, kSeekable");
    return true;
  }

  constexpr Channel(DataType data_type, uint8_t properties)
      : open_(true),
        readable_((properties & kReadable) != 0),
        writable_((properties & Property::kWritable) != 0),
        type_reliable_seekable_(static_cast<uint8_t>(data_type) |
                                (properties & kReliable) |
                                (properties & kSeekable)) {}

  // Virtual interface

  // Read functions

  // The max_bytes argument is ignored for datagram-oriented channels.
  virtual async2::Poll<Result<multibuf::MultiBuf>> DoPollRead(
      async2::Context& cx, size_t max_bytes) = 0;

  // Write functions

  // TODO: b/323624921 - Implement when MultiBufAllocator exists.
  // virtual multibuf::MultiBufAllocator& DoGetWriteBufferAllocator() = 0;

  virtual async2::Poll<> DoPollReadyToWrite(async2::Context& cx) = 0;

  virtual Result<WriteToken> DoWrite(multibuf::MultiBuf&& buffer) = 0;

  virtual async2::Poll<Result<WriteToken>> DoPollFlush(async2::Context& cx) = 0;

  // Seek functions
  /// TODO: b/323622630 - `Seek` and `Position` are not yet implemented.

  // virtual Status DoSeek(ptrdiff_t position, Whence whence) = 0;

  // virtual size_t DoPosition() const = 0;

  // Common functions
  virtual async2::Poll<Status> DoPollClose(async2::Context& cx) = 0;

  bool open_;

  bool readable_;
  bool writable_;

  // Bits 0-4 are used for properties (reliable & seekable)
  // Bit 5 is the data type (byte or datagram)
  uint8_t type_reliable_seekable_;
};

/// A `ByteChannel` exchanges data as a stream of bytes.
template <Property... kProperties>
class ByteChannel : public Channel {
 protected:
  static_assert(PropertiesAreValid<kProperties...>());

  constexpr ByteChannel()
      : Channel(DataType::kByte, GetProperties<kProperties...>()) {}
};

/// A `DatagramChannel` exchanges data as a series of datagrams.
template <Property... kProperties>
class DatagramChannel : public Channel {
 protected:
  static_assert(PropertiesAreValid<kProperties...>());

  constexpr DatagramChannel()
      : Channel(DataType::kDatagram, GetProperties<kProperties...>()) {}
};

// Aliases for common ByteChannel and DatagramChannel types.

/// Alias for a reliable `ByteChannel` that supports reading.
using ByteReader = ByteChannel<kReliable, kReadable>;
/// Alias for a reliable `ByteChannel` that supports writing.
using ByteWriter = ByteChannel<kReliable, kWritable>;
/// Alias for a reliable `ByteChannel` that supports reading and writing.
using ByteReaderWriter = ByteChannel<kReliable, kReadable, kWritable>;

/// Alias for a reliable `DatagramChannel` that supports reading.
using ReliableDatagramReader = DatagramChannel<kReliable, kReadable>;
/// Alias for a reliable `DatagramChannel` that supports writing.
using ReliableDatagramWriter = DatagramChannel<kReliable, kWritable>;
/// Alias for a reliable `DatagramChannel` that supports reading and writing.
using ReliableDatagramReaderWriter =
    DatagramChannel<kReliable, kReadable, kWritable>;
/// Alias for an unreliable `DatagramChannel` that supports reading.
using DatagramReader = DatagramChannel<kReadable>;
/// Alias for an unreliable `DatagramChannel` that supports writing.
using DatagramWriter = DatagramChannel<kWritable>;
/// Alias for an unreliable `DatagramChannel` that supports reading and writing.
using DatagramReaderWriter = DatagramChannel<kReadable, kWritable>;

/// @}

}  // namespace pw::channel

// Include specializations for supported Channel types.
#include "pw_channel/internal/channel_specializations.h"
