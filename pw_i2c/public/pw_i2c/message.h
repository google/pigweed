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

#include <cstddef>
#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_i2c/address.h"

namespace pw::i2c {

/// A struct that represents I2C read and write messages.
///
/// Individual messages can be accumulated into a span and transmitted
/// in one atomic i2c transaction using an Initiator implementation.
///
/// @code{.cpp}
///   #include "pw_i2c/message.h"
///
///   constexpr auto kAddress = pw::i2c::Address::SevenBit<0x42>()
///   constexpr chrono::SystemClock::duration kTimeout =
///       std::chrono::duration_cast<chrono::SystemClock::duration>(100ms);
///   const std::array<std::byte, 2> tx_buffer = {std::byte{0xCD},
///                                               std::byte{0xEF}};
///   std::array<std::byte, 2> rx_buffer;
///   Vector<Message, 2> messages;
///   if (!tx_buffer.empty()) {
///     messages.push_back(Message::WriteMessage(kAddress, tx_buffer));
///   }
///   if (!rx_buffer.empty()) {
///     messages.push_back(Message::ReadMessage(kAddress, rx_buffer));
///   }
///   initiator.TransferFor(messages, kTimeout);
///
/// @endcode
class Message {
 public:
  /// Creates a `pw::i2c::Message` instance for an i2c write message.
  ///
  /// This Message can be passed to Initiator::TransferFor().
  ///
  /// @code{.cpp}
  ///   constexpr Message kMessage = Message::WriteMessage(
  ///       pw::i2c::Address::SevenBit<0x42>(),
  ///       data
  ///   );
  /// @endcode
  ///
  /// @returns A `pw::i2c::Message` instance.
  static constexpr Message WriteMessage(Address address, ConstByteSpan data) {
    return Message(address,
                   ByteSpan(const_cast<std::byte*>(data.data()), data.size()),
                   Direction::kWrite,
                   /*no_start=*/false);
  }

  /// Creates a `pw::i2c::Message` instance for an i2c write message without a
  /// start condition sent on the bus. Chaining one or more of these messages
  /// after a regular Write message allows the client to send non-contiguous
  /// blocks of memory as one single write message to the i2c target.
  ///
  /// Note: This message must follow another write message.
  ///
  /// Note: No addresses is needed and no address will be transmitted. The data
  /// should immediately follow the data from the previous write message.
  ///
  /// This Message can be passed to Initiator::TransferFor().
  ///
  /// @code{.cpp}
  ///   constexpr Message kMessage = Message::WriteMessageContinuation(data);
  /// @endcode
  ///
  /// @returns A `pw::i2c::Message` instance.
  static constexpr Message WriteMessageContinuation(ConstByteSpan data) {
    return Message(Address::SevenBit<1>(),
                   ByteSpan(const_cast<std::byte*>(data.data()), data.size()),
                   Direction::kWrite,
                   /*no_start=*/true);
  }

  /// Creates a `pw::i2c::Message` instance for an i2c read message.
  ///
  /// This Message can be passed to Initiator::TransferFor().
  ///
  /// @code{.cpp}
  ///   constexpr Message kMessage = Message::ReadMessage(
  ///       pw::i2c::Address::SevenBit<0x42>(),
  ///       data
  ///   );
  /// @endcode
  ///
  /// @returns A `pw::i2c::Message` instance.
  static constexpr Message ReadMessage(Address address, ByteSpan data) {
    return Message(address, data, Direction::kRead, /*no_start=*/false);
  }

  /// Getter for whether this object represents a read operation.
  ///
  /// @returns true if the message represents a read operation.
  bool IsRead() const { return direction_ == Direction::kRead; }

  /// Getter for whether this object represents an operation addressed with a
  /// ten-bit address.
  /// When true, communicate on the wire using the i2c 10-bit addressing
  /// protocol.
  ///
  /// @returns true if the message represents a 10-bit addressed operation.
  bool IsTenBit() const { return address_.IsTenBit(); }

  /// Getter for whether this object represents a continued write.
  ///
  /// @returns true if the message represents a continued write.
  bool IsWriteContinuation() const { return no_start_; }

  /// Getter for the address component.
  ///
  /// @returns the Address passed into one of the constructors or factories.
  Address GetAddress() const { return address_; }

  /// Getter for the data component.
  ///
  /// This method is only valid for Read messages and will runtime ASSERT on
  /// other messages.
  ///
  /// @returns the mutable variant of the data passed into one of the
  /// constructors or factories.
  ByteSpan GetMutableData() const {
    PW_ASSERT(IsRead());
    return data_;
  }

  /// Getter for the data component.
  ///
  /// @returns the data passed into one of the constructors or factories.
  ConstByteSpan GetData() const { return data_; }

 private:
  enum class Direction : uint8_t {
    kWrite,
    kRead,
  };

  constexpr Message(const Address& address,
                    ByteSpan data,
                    Direction direction,
                    bool no_start)
      : address_(address),
        data_(data),
        direction_(direction),
        no_start_(no_start) {}

  // I2c address of target device.
  Address address_;

  // Data buffer to send from or receive into.
  ByteSpan data_;

  // Message represents an i2c read.
  Direction direction_;

  // Message should be transmitted without an i2c start condition and without
  // an address.
  bool no_start_;
};

}  // namespace pw::i2c
