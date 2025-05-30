// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"

#include <pw_assert/check.h>
#include <pw_bytes/endian.h>

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/testing/gtest_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace bt::testing {

Transaction::Transaction(const ByteBuffer& expected,
                         const std::vector<const ByteBuffer*>& replies,
                         ExpectationMetadata meta)
    : expected_({DynamicByteBuffer(expected), meta}) {
  for (const auto* buffer : replies) {
    replies_.push(DynamicByteBuffer(*buffer));
  }
}

bool Transaction::Match(const ByteBuffer& packet) {
  return ContainersEqual(expected_.data, packet);
}

CommandTransaction::CommandTransaction(
    hci_spec::OpCode expected_opcode,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta)
    : Transaction(DynamicByteBuffer(), replies, meta), prefix_(true) {
  hci_spec::OpCode le_opcode =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, expected_opcode);
  const BufferView expected(&le_opcode, sizeof(expected_opcode));
  set_expected({DynamicByteBuffer(expected), meta});
}

bool CommandTransaction::Match(const ByteBuffer& cmd) {
  return ContainersEqual(
      expected().data,
      (prefix_ ? cmd.view(0, expected().data.size()) : cmd.view()));
}

MockController::MockController(pw::async::Dispatcher& pw_dispatcher)
    : ControllerTestDoubleBase(pw_dispatcher), WeakSelf(this) {}

MockController::~MockController() {
  while (!cmd_transactions_.empty()) {
    auto& transaction = cmd_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound command packet ("
        << meta.expectation << ") {"
        << ByteContainerToString(transaction.expected().data) << "}";
    cmd_transactions_.pop();
  }

  while (!data_transactions_.empty()) {
    auto& transaction = data_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound data packet (" << meta.expectation
        << ") {" << ByteContainerToString(transaction.expected().data) << "}";
    data_transactions_.pop();
  }

  while (!sco_transactions_.empty()) {
    auto& transaction = sco_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound SCO packet (" << meta.expectation
        << ") {" << ByteContainerToString(transaction.expected().data) << "}";
    sco_transactions_.pop();
  }
}

void MockController::QueueCommandTransaction(CommandTransaction transaction) {
  cmd_transactions_.push(std::move(transaction));
}

void MockController::QueueCommandTransaction(
    const ByteBuffer& expected,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta) {
  QueueCommandTransaction(
      CommandTransaction(DynamicByteBuffer(expected), replies, meta));
}

void MockController::QueueCommandTransaction(
    hci_spec::OpCode expected_opcode,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta) {
  QueueCommandTransaction(CommandTransaction(expected_opcode, replies, meta));
}

void MockController::QueueDataTransaction(DataTransaction transaction) {
  data_transactions_.push(std::move(transaction));
}

void MockController::QueueDataTransaction(
    const ByteBuffer& expected,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta) {
  QueueDataTransaction(
      DataTransaction(DynamicByteBuffer(expected), replies, meta));
}

void MockController::QueueScoTransaction(const ByteBuffer& expected,
                                         ExpectationMetadata meta) {
  sco_transactions_.push(ScoTransaction(DynamicByteBuffer(expected), meta));
}

void MockController::QueueIsoTransaction(const ByteBuffer& expected,
                                         ExpectationMetadata meta) {
  iso_transactions_.push(IsoTransaction(DynamicByteBuffer(expected), meta));
}

bool MockController::AllExpectedScoPacketsSent() const {
  return sco_transactions_.empty();
}

bool MockController::AllExpectedDataPacketsSent() const {
  return data_transactions_.empty();
}

bool MockController::AllExpectedCommandPacketsSent() const {
  return cmd_transactions_.empty();
}

bool MockController::AllExpectedIsoPacketsSent() const {
  return iso_transactions_.empty();
}

void MockController::SetDataCallback(DataCallback callback) {
  PW_DCHECK(callback);
  PW_DCHECK(!data_callback_);

  data_callback_ = std::move(callback);
}

void MockController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  data_callback_ = nullptr;
}

void MockController::SetTransactionCallback(fit::closure callback) {
  SetTransactionCallback([f = std::move(callback)](const auto&) { f(); });
}

void MockController::SetTransactionCallback(TransactionCallback callback) {
  PW_DCHECK(callback);
  PW_DCHECK(!transaction_callback_);
  transaction_callback_ = std::move(callback);
}

void MockController::ClearTransactionCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  transaction_callback_ = nullptr;
}

void MockController::OnCommandReceived(const ByteBuffer& data) {
  ASSERT_GE(data.size(), sizeof(hci_spec::OpCode));
  const hci_spec::OpCode opcode = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, data.To<hci_spec::OpCode>());
  const uint8_t ogf = hci_spec::GetOGF(opcode);
  const uint16_t ocf = hci_spec::GetOCF(opcode);

  // Note: we upcast ogf to uint16_t so that it does not get interpreted as a
  // char for printing
  ASSERT_FALSE(cmd_transactions_.empty())
      << "Received unexpected command packet with OGF: 0x" << std::hex
      << static_cast<uint16_t>(ogf) << ", OCF: 0x" << ocf;

  auto& transaction = cmd_transactions_.front();
  const hci_spec::OpCode expected_opcode = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      transaction.expected().data.To<hci_spec::OpCode>());
  const uint8_t expected_ogf = hci_spec::GetOGF(expected_opcode);
  const uint16_t expected_ocf = hci_spec::GetOCF(expected_opcode);

  if (!transaction.Match(data)) {
    auto meta = transaction.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << " Expected command packet (" << meta.expectation << ") with OGF: 0x"
        << std::hex << static_cast<uint16_t>(expected_ogf) << ", OCF: 0x"
        << expected_ocf << ". Received command packet with OGF: 0x"
        << static_cast<uint16_t>(ogf) << ", OCF: 0x" << ocf;
  }

  while (!transaction.replies().empty()) {
    DynamicByteBuffer& reply = transaction.replies().front();
    ASSERT_TRUE(SendCommandChannelPacket(reply)) << "Failed to send reply";
    transaction.replies().pop();
  }
  cmd_transactions_.pop();

  if (transaction_callback_) {
    DynamicByteBuffer rx(data);
    (void)heap_dispatcher().Post(
        [rx = std::move(rx), f = transaction_callback_.share()](
            auto, pw::Status status) {
          if (status.ok()) {
            f(rx);
          }
        });
  }
}

void MockController::OnACLDataPacketReceived(
    const ByteBuffer& acl_data_packet) {
  ASSERT_FALSE(data_transactions_.empty())
      << "Received unexpected acl data packet: { "
      << ByteContainerToString(acl_data_packet) << "}";

  auto& expected = data_transactions_.front();
  if (!expected.Match(acl_data_packet.view())) {
    auto meta = expected.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << "Expected data packet (" << meta.expectation << ")";
  }

  while (!expected.replies().empty()) {
    auto& reply = expected.replies().front();
    ASSERT_TRUE(SendACLDataChannelPacket(reply)) << "Failed to send reply";
    expected.replies().pop();
  }
  data_transactions_.pop();

  if (data_callback_) {
    DynamicByteBuffer packet_copy(acl_data_packet);
    (void)heap_dispatcher().Post(
        [packet_copy = std::move(packet_copy), cb = data_callback_.share()](
            auto, pw::Status status) mutable {
          if (status.ok()) {
            cb(packet_copy);
          }
        });
  }
}

void MockController::OnScoDataPacketReceived(
    const ByteBuffer& sco_data_packet) {
  ASSERT_FALSE(sco_transactions_.empty())
      << "Received unexpected SCO data packet: { "
      << ByteContainerToString(sco_data_packet) << "}";

  auto& expected = sco_transactions_.front();
  if (!expected.Match(sco_data_packet.view())) {
    auto meta = expected.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << "Expected SCO packet (" << meta.expectation << ")";
  }

  sco_transactions_.pop();
}

void MockController::OnIsoDataPacketReceived(
    const ByteBuffer& iso_data_packet) {
  ASSERT_FALSE(iso_transactions_.empty())
      << "Received unexpected ISO data packet: { "
      << ByteContainerToString(iso_data_packet) << "}";

  auto& expected = iso_transactions_.front();
  if (!expected.Match(iso_data_packet.view())) {
    auto meta = expected.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << "Expected ISO packet (" << meta.expectation << ")";
  }

  iso_transactions_.pop();
}

void MockController::SendCommand(pw::span<const std::byte> data) {
  // Post task to simulate async
  DynamicByteBuffer buffer(BufferView(data.data(), data.size()));
  (void)heap_dispatcher().Post(
      [this, buffer = std::move(buffer)](pw::async::Context /*ctx*/,
                                         pw::Status status) {
        if (status.ok()) {
          OnCommandReceived(buffer);
        }
      });
}
void MockController::SendAclData(pw::span<const std::byte> data) {
  // Post task to simulate async
  DynamicByteBuffer buffer(BufferView(data.data(), data.size()));
  (void)heap_dispatcher().Post(
      [this, buffer = std::move(buffer)](pw::async::Context /*ctx*/,
                                         pw::Status status) {
        if (status.ok()) {
          OnACLDataPacketReceived(buffer);
        }
      });
}
void MockController::SendScoData(pw::span<const std::byte> data) {
  // Post task to simulate async
  DynamicByteBuffer buffer(BufferView(data.data(), data.size()));
  (void)heap_dispatcher().Post(
      [this, buffer = std::move(buffer)](pw::async::Context /*ctx*/,
                                         pw::Status status) {
        if (status.ok()) {
          OnScoDataPacketReceived(buffer);
        }
      });
}
void MockController::SendIsoData(pw::span<const std::byte> data) {
  // Post task to simulate async
  DynamicByteBuffer buffer(BufferView(data.data(), data.size()));
  (void)heap_dispatcher().Post(
      [this, buffer = std::move(buffer)](pw::async::Context /*ctx*/,
                                         pw::Status status) {
        if (status.ok()) {
          OnIsoDataPacketReceived(buffer);
        }
      });
}

}  // namespace bt::testing
