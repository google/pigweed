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
#include <cstddef>
#include <optional>

#include "pw_bytes/span.h"
#include "pw_containers/to_array.h"
#include "pw_i2c/initiator.h"

namespace pw::i2c {

/// Base class for creating transaction instances. For read-only,
/// write-only, or probe transactions, improve code readability
/// by using one of the following helpers instead:
///
/// * `pw::i2c::ReadTransaction`
/// * `pw::i2c::WriteTransaction`
/// * `pw::i2c::ProbeTransaction`
///
/// If you need to create a write-then-read transaction, you can
/// use this class.
class Transaction {
 public:
  /// Constructor for creating write-only, read-only, or write-then-read
  /// transactions.
  constexpr Transaction(
      Status expected_return_value,
      Address device_address,
      ConstByteSpan write_buffer,
      ConstByteSpan read_buffer,
      std::optional<chrono::SystemClock::duration> timeout = std::nullopt)
      : return_value_(expected_return_value),
        read_buffer_(read_buffer),
        write_buffer_(write_buffer),
        address_(device_address),
        timeout_(timeout) {}

  /// Alternative constructor for creating probe transactions.
  constexpr Transaction(
      Status expected_return_value,
      Address device_address,
      std::optional<chrono::SystemClock::duration> timeout = std::nullopt)
      : Transaction(expected_return_value,
                    device_address,
                    ConstByteSpan(),
                    ignored_buffer_,
                    timeout) {}

  /// Gets the buffer that is virtually read.
  ConstByteSpan read_buffer() const { return read_buffer_; }

  /// Gets the buffer that the I2C device should write to.
  ConstByteSpan write_buffer() const { return write_buffer_; }

  /// Gets the minimum duration to wait for a blocking I2C transaction.
  std::optional<chrono::SystemClock::duration> timeout() const {
    return timeout_;
  }

  /// Gets the I2C address that the I2C transaction is targeting.
  Address address() const { return address_; }

  /// Gets the expected return value for the transaction.
  Status return_value() const { return return_value_; }

 private:
  const Status return_value_;
  const ConstByteSpan read_buffer_;
  const ConstByteSpan write_buffer_;
  static constexpr std::array<std::byte, 1> ignored_buffer_ = {};
  const Address address_;
  const std::optional<chrono::SystemClock::duration> timeout_;
};

/// A helper that constructs a read-only I2C transaction.
/// Used for testing read transactions with `pw::i2c::MockInitiator`.
constexpr Transaction ReadTransaction(
    Status expected_return_value,
    Address device_address,
    ConstByteSpan read_buffer,
    std::optional<chrono::SystemClock::duration> timeout = std::nullopt) {
  return Transaction(expected_return_value,
                     device_address,
                     ConstByteSpan(),
                     read_buffer,
                     timeout);
}

/// A helper that constructs a write-only I2C transaction.
/// Used for testing write transactions with `pw::i2c::MockInitiator`.
constexpr Transaction WriteTransaction(
    Status expected_return_value,
    Address device_address,
    ConstByteSpan write_buffer,
    std::optional<chrono::SystemClock::duration> timeout = std::nullopt) {
  return Transaction(expected_return_value,
                     device_address,
                     write_buffer,
                     ConstByteSpan(),
                     timeout);
}

/// A helper that constructs a one-byte read I2C transaction.
/// Used for testing probe transactions with `pw::i2c::MockInitiator`.
constexpr Transaction ProbeTransaction(
    Status expected_return_value,
    Address device_address,
    std::optional<chrono::SystemClock::duration> timeout = std::nullopt) {
  return Transaction(expected_return_value, device_address, timeout);
}

/// A generic mocked backend for `pw::i2c::Initiator` that's specifically
/// designed to make it easier to develop I2C device drivers.
/// `pw::i2c::MockInitiator` compares actual I2C transactions against expected
/// transactions. The expected transactions are represented as a list of
/// `pw::i2c::Transaction` instances that are passed as arguments in the
/// `pw::i2c::MockInitiator` constructor. Each consecutive call to
/// `pw::i2c::MockInitiator` iterates to the next expected transaction.
/// `pw::i2c::MockInitiator::Finalize()` indicates whether the actual
/// transactions matched the expected transactions.
///
/// `pw::i2c::MockInitiator` is intended to be used within GoogleTest tests.
/// See @rstref{module-pw_unit_test}.
///
/// @code{.cpp}
///   #include <chrono>
///
///   #include "pw_bytes/array.h"
///   #include "pw_i2c/address.h"
///   #include "pw_i2c/initiator_mock.h"
///   #include "pw_result/result.h"
///   #include "pw_unit_test/framework.h"
///
///   using namespace std::chrono_literals;
///
///   namespace {
///
///   TEST(I2CTestSuite, I2CWriteTestCase) {
///     constexpr pw::i2c::Address kAddress =
///     pw::i2c::Address::SevenBit<0x01>(); constexpr auto kExpectedWrite =
///     pw::bytes::Array<1, 2, 3>(); auto expected_transactions =
///     pw::i2c::MakeExpectedTransactionArray(
///       {pw::i2c::WriteTransaction(pw::OkStatus(), kAddress, kExpectedWrite,
///       1ms)}
///     );
///     pw::i2c::MockInitiator initiator(expected_transactions);
///     pw::ConstByteSpan kActualWrite = pw::bytes::Array<1, 2, 3>();
///     pw::Status status = initiator.WriteFor(kAddress, kActualWrite, 1ms);
///     EXPECT_EQ(initiator.Finalize(), pw::OkStatus());
///   }
///
///   }
/// @endcode
class MockInitiator : public Initiator {
 public:
  explicit constexpr MockInitiator(span<Transaction> transaction_list)
      : expected_transactions_(transaction_list),
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

  /// Runs `pw::i2c::MockInitiator::Finalize()` regardless of whether it was
  /// already optionally finalized.
  ~MockInitiator() override;

 private:
  // Implements a mocked backend for the i2c initiator.
  //
  // Expects (via Gtest):
  // tx_buffer == expected_transaction_tx_buffer
  // tx_buffer.size() == expected_transaction_tx_buffer.size()
  // rx_buffer.size() == expected_transaction_rx_buffer.size()
  //
  // Asserts:
  // When the number of calls to this method exceed the number of expected
  //    transactions.
  //
  // Returns:
  // Specified transaction return type
  Status DoWriteReadFor(Address device_address,
                        ConstByteSpan tx_buffer,
                        ByteSpan rx_buffer,
                        chrono::SystemClock::duration timeout) override;

  span<Transaction> expected_transactions_;
  size_t expected_transaction_index_;
};

// Makes a new i2c transactions list.
template <size_t kSize>
constexpr std::array<Transaction, kSize> MakeExpectedTransactionArray(
    const Transaction (&transactions)[kSize]) {
  return containers::to_array(transactions);
}

}  // namespace pw::i2c
