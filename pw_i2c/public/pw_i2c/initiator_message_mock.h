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

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "pw_bytes/span.h"
#include "pw_containers/to_array.h"
#include "pw_i2c/initiator.h"

namespace pw::i2c {

/// Base class for creating expected individual Messages that make up a
/// MockMessageTransaction instance. For read-only, write-only, or probe
/// messages, improve code readability by using one of the following helpers
/// instead:
///
/// * `pw::i2c::MockReadMessage`
/// * `pw::i2c::MockWriteMessage`
/// * `pw::i2c::MockProbeMessage`
class MockMessage {
 public:
  enum Direction {
    kMockRead,
    kMockWrite,
  };
  constexpr MockMessage(Status expected_return_value,
                        Address address,
                        Direction direction,
                        ConstByteSpan data_buffer)
      : return_value_(expected_return_value),
        address_(address),
        direction_(direction),
        data_buffer_(data_buffer) {}

  /// Alternative constructor for creating probe transactions.
  constexpr MockMessage(Status expected_return_value, Address device_address)
      : MockMessage(expected_return_value,
                    device_address,
                    MockMessage::kMockRead,
                    ignored_buffer_) {}

  /// Gets the expected return value for the transaction.
  Status return_value() const { return return_value_; }

  /// Gets the I2C address that the I2C transaction is targeting.
  Address address() const { return address_; }

  Direction direction() const { return direction_; }

  /// Gets the buffer that is virtually read.
  ConstByteSpan data_buffer() const { return data_buffer_; }

 private:
  const Status return_value_;
  const Address address_;
  const Direction direction_;
  const ConstByteSpan data_buffer_;
  static constexpr std::array<std::byte, 1> ignored_buffer_ = {};
};

constexpr MockMessage MockReadMessage(Status expected_return_value,
                                      Address address,
                                      ConstByteSpan data_buffer) {
  return MockMessage(
      expected_return_value, address, MockMessage::kMockRead, data_buffer);
}

constexpr MockMessage MockWriteMessage(Status expected_return_value,
                                       Address address,
                                       ConstByteSpan data_buffer) {
  return MockMessage(
      expected_return_value, address, MockMessage::kMockWrite, data_buffer);
}

constexpr MockMessage MockProbeMessage(Status expected_return_value,
                                       Address address) {
  return MockMessage(expected_return_value, address);
}

// Makes a new i2c messages list.
template <size_t kSize>
constexpr std::array<MockMessage, kSize> MakeExpectedMessageArray(
    const MockMessage (&messages)[kSize]) {
  return containers::to_array(messages);
}

/// Represents a list of MockMessages that make up one i2c transaction.
/// An i2c transaction can consist of any arbitrary combination of i2c read
/// and write messages that are transmitted sequentially and without releasing
/// the bus with an i2c Stop condition.
class MockMessageTransaction {
 public:
  MockMessageTransaction(
      Status expected_return_value,
      span<const MockMessage> test_messages,
      std::optional<chrono::SystemClock::duration> timeout = std::nullopt)
      : return_value_(expected_return_value),
        test_messages_(test_messages.begin(), test_messages.end()),
        timeout_(timeout) {}
  /// Gets the minimum duration to wait for a blocking I2C transaction.
  std::optional<chrono::SystemClock::duration> timeout() const {
    return timeout_;
  }

  const std::vector<MockMessage>& test_messages() const {
    return test_messages_;
  }

  Status return_value() const { return return_value_; }

 private:
  const Status return_value_;
  const std::vector<MockMessage> test_messages_;
  const std::optional<chrono::SystemClock::duration> timeout_;
};

/// A generic mocked backend for `pw::i2c::Initiator` that's specifically
/// designed to make it easier to develop I2C device drivers.
/// `pw::i2c::MessageMockInitiator` compares actual I2C transactions against
/// expected transactions. The expected transactions are represented as a list
/// of `pw::i2c::MockMessageTransaction` instances that are passed as arguments
/// in the `pw::i2c::MessageMockInitiator` constructor. Each consecutive call to
/// `pw::i2c::MockMessageInitiator` iterates to the next expected transaction.
/// `pw::i2c::MockMessageInitiator::Finalize()` indicates whether the actual
/// transactions matched the expected transactions.
///
/// `pw::i2c::MessageMockInitiator` is intended to be used within GoogleTest
/// tests.
/// See @rstref{module-pw_unit_test}.
///
/// @code{.cpp}
///   #include <chrono>
///
///   #include "pw_bytes/array.h"
///   #include "pw_i2c/address.h"
///   #include "pw_i2c/initiator_message_mock.h"
///   #include "pw_i2c/message.h"
///   #include "pw_result/result.h"
///   #include "pw_unit_test/framework.h"
///
///   using namespace std::chrono_literals;
///
///   namespace {
///
///   TEST(I2CTestSuite, I2CReadRegisterTestCase) {
///     constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();
///     constexpr auto kExpectedWrite1 = bytes::Array<0x42>();
///     constexpr auto kExpectedRead1 = bytes::Array<1, 2>();
///
///     auto expected_transactions = MakeExpectedTransactionArray({
///         MockMessageTransaction(
///             OkStatus(),
///             MakeExpectedMessageArray({
///                 MockWriteMessage(OkStatus(), kTestDeviceAddress,
///                                  kExpectedWrite1),
///                 MockReadMessage(OkStatus(), kTestDeviceAddress,
///                                  kExpectedRead1),
///             }),
///             kI2cTransactionTimeout),
///     });
///     MockMessageInitiator initiator(expected_transactions);
///     std::array<std::byte, kExpectedRead1.size()> read1;
///     EXPECT_EQ(initiator.WriteReadFor(kTestDeviceAddress, kExpectedWrite1,
///               read1, kI2cTransactionTimeout), OkStatus());
///     EXPECT_TRUE(pw::containers::Equal(read1, kExpectedRead1));
///     EXPECT_EQ(initiator.Finalize(), OkStatus());
///   }
///
///   }
/// @endcode
class MockMessageInitiator : public Initiator {
 public:
  explicit MockMessageInitiator(span<MockMessageTransaction> transaction_list)
      : Initiator(Initiator::Feature::kStandard),
        expected_transactions_(transaction_list),
        expected_transaction_index_(0) {}

  /// Indicates whether the actual I2C transactions matched the expected
  /// transactions. Should be called at the end of the test.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The actual transactions matched the expected transactions.
  ///
  ///    OUT_OF_RANGE: The mocked set of transactions hasn't been exhausted.
  ///
  /// @endrst
  Status Finalize() const {
    if (expected_transaction_index_ != expected_transactions_.size()) {
      return Status::OutOfRange();
    }
    return Status();
  }

  /// Runs `pw::i2c::MessageMockInitiator::Finalize()` regardless of whether it
  /// was already optionally finalized.
  ~MockMessageInitiator() override;

 private:
  // Implements a mocked backend for the i2c initiator.
  //
  // Expects (via Gtest):
  // messages.size() == next_expected_transaction.messages.size()
  // For each element in messages, a corresponding and matching element
  // should exist in the next expected transaction's message list.
  //
  // Asserts:
  // When the number of calls to this method exceed the number of expected
  //    transactions.
  //
  // If the number of elements in the next expected transaction's message list
  // does not equal the number of elements in messages.
  //
  // The byte size of any individual expected transaction message does not
  // equal the byte size of the corresponding element in messages.
  //
  // Returns:
  // Specified transaction return type
  Status DoTransferFor(span<const Message> messages,
                       chrono::SystemClock::duration timeout) override;

  span<MockMessageTransaction> expected_transactions_;
  size_t expected_transaction_index_;
};

// Makes a new list of i2c transactions. Each transaction is made up of
// individual read and write messages transmitted together on the i2c bus.
template <size_t kSize>
constexpr std::array<MockMessageTransaction, kSize>
MakeExpectedTransactionArray(
    const MockMessageTransaction (&transactions)[kSize]) {
  return containers::to_array(transactions);
}

}  // namespace pw::i2c
