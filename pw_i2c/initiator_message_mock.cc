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
#include "pw_i2c/initiator_message_mock.h"

#include "pw_assert/check.h"
#include "pw_containers/algorithm.h"
#include "pw_unit_test/framework.h"

namespace pw::i2c {

Status MockMessageInitiator::DoTransferFor(
    span<const Message> messages, chrono::SystemClock::duration timeout) {
  PW_CHECK_INT_LT(expected_transaction_index_, expected_transactions_.size());

  const MockMessageTransaction& expected_transaction =
      expected_transactions_[expected_transaction_index_];

  auto expected_timeout = expected_transaction.timeout();
  if (expected_timeout.has_value()) {
    EXPECT_EQ(expected_timeout.value(), timeout);
  }

  const auto& expected_messages = expected_transaction.test_messages();

  PW_CHECK_INT_EQ(messages.size(), expected_messages.size());

  for (unsigned int i = 0; i < messages.size(); ++i) {
    EXPECT_EQ(messages[i].GetAddress().GetAddress(),
              expected_messages[i].address().GetAddress());

    EXPECT_EQ(messages[i].IsRead(),
              expected_messages[i].direction() == MockMessage::kMockRead);

    if (messages[i].IsRead()) {
      ConstByteSpan expected_rx_buffer = expected_messages[i].data_buffer();
      PW_CHECK_INT_EQ(messages[i].GetData().size(), expected_rx_buffer.size());
      std::copy(expected_rx_buffer.begin(),
                expected_rx_buffer.end(),
                messages[i].GetData().begin());
    } else {
      ConstByteSpan expected_tx_buffer = expected_messages[i].data_buffer();
      EXPECT_TRUE(
          pw::containers::Equal(messages[i].GetData(), expected_tx_buffer));
    }
  }

  // Do not directly return this value as expected_transaction_index_ should be
  // incremented.
  const Status expected_return_value = expected_transaction.return_value();

  expected_transaction_index_ += 1;

  return expected_return_value;
}

MockMessageInitiator::~MockMessageInitiator() {
  EXPECT_EQ(Finalize(), OkStatus());
}

}  // namespace pw::i2c
