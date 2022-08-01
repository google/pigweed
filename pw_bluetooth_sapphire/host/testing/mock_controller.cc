// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_controller.h"

#include <lib/async/cpp/task.h>
#include <zircon/status.h>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::testing {

Transaction::Transaction(const ByteBuffer& expected, const std::vector<const ByteBuffer*>& replies,
                         ExpectationMetadata meta)
    : expected_({DynamicByteBuffer(expected), meta}) {
  for (const auto* buffer : replies) {
    replies_.push(DynamicByteBuffer(*buffer));
  }
}

bool Transaction::Match(const ByteBuffer& packet) {
  return ContainersEqual(expected_.data, packet);
}

CommandTransaction::CommandTransaction(hci_spec::OpCode expected_opcode,
                                       const std::vector<const ByteBuffer*>& replies,
                                       ExpectationMetadata meta)
    : Transaction(DynamicByteBuffer(), replies, meta), prefix_(true) {
  hci_spec::OpCode le_opcode = htole16(expected_opcode);
  const BufferView expected(&le_opcode, sizeof(expected_opcode));
  set_expected({DynamicByteBuffer(expected), meta});
}

bool CommandTransaction::Match(const ByteBuffer& cmd) {
  return ContainersEqual(expected().data,
                         (prefix_ ? cmd.view(0, expected().data.size()) : cmd.view()));
}

MockController::MockController()
    : ControllerTestDoubleBase(),
      data_expectations_enabled_(false),
      data_dispatcher_(nullptr),
      transaction_dispatcher_(nullptr) {}

MockController::~MockController() {
  while (!cmd_transactions_.empty()) {
    auto& transaction = cmd_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound command packet (" << meta.expectation << ") {"
        << ByteContainerToString(transaction.expected().data) << "}";
    cmd_transactions_.pop();
  }

  while (!data_transactions_.empty()) {
    auto& transaction = data_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound data packet (" << meta.expectation << ") {"
        << ByteContainerToString(transaction.expected().data) << "}";
    data_transactions_.pop();
  }

  while (!sco_transactions_.empty()) {
    auto& transaction = sco_transactions_.front();
    auto meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound SCO packet (" << meta.expectation << ") {"
        << ByteContainerToString(transaction.expected().data) << "}";
    sco_transactions_.pop();
  }

  Stop();
}

void MockController::QueueCommandTransaction(CommandTransaction transaction) {
  cmd_transactions_.push(std::move(transaction));
}

void MockController::QueueCommandTransaction(const ByteBuffer& expected,
                                             const std::vector<const ByteBuffer*>& replies,
                                             ExpectationMetadata meta) {
  QueueCommandTransaction(CommandTransaction(DynamicByteBuffer(expected), replies, meta));
}

void MockController::QueueCommandTransaction(hci_spec::OpCode expected_opcode,
                                             const std::vector<const ByteBuffer*>& replies,
                                             ExpectationMetadata meta) {
  QueueCommandTransaction(CommandTransaction(expected_opcode, replies, meta));
}

void MockController::QueueDataTransaction(DataTransaction transaction) {
  ZX_ASSERT(data_expectations_enabled_);
  data_transactions_.push(std::move(transaction));
}

void MockController::QueueDataTransaction(const ByteBuffer& expected,
                                          const std::vector<const ByteBuffer*>& replies,
                                          ExpectationMetadata meta) {
  QueueDataTransaction(DataTransaction(DynamicByteBuffer(expected), replies, meta));
}

void MockController::QueueScoTransaction(const ByteBuffer& expected, ExpectationMetadata meta) {
  sco_transactions_.push(ScoTransaction(DynamicByteBuffer(expected), meta));
}

bool MockController::AllExpectedScoPacketsSent() const { return sco_transactions_.empty(); }

bool MockController::AllExpectedDataPacketsSent() const { return data_transactions_.empty(); }

bool MockController::AllExpectedCommandPacketsSent() const { return cmd_transactions_.empty(); }

void MockController::SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(dispatcher);
  ZX_DEBUG_ASSERT(!data_callback_);
  ZX_DEBUG_ASSERT(!data_dispatcher_);

  data_callback_ = std::move(callback);
  data_dispatcher_ = dispatcher;
}

void MockController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  data_callback_ = nullptr;
}

void MockController::SetTransactionCallback(fit::closure callback, async_dispatcher_t* dispatcher) {
  SetTransactionCallback([f = std::move(callback)](const auto&) { f(); }, dispatcher);
}

void MockController::SetTransactionCallback(TransactionCallback callback,
                                            async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(dispatcher);
  ZX_DEBUG_ASSERT(!transaction_callback_);
  ZX_DEBUG_ASSERT(!transaction_dispatcher_);

  transaction_callback_ = std::move(callback);
  transaction_dispatcher_ = dispatcher;
}

void MockController::ClearTransactionCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  transaction_callback_ = nullptr;
}

void MockController::OnCommandPacketReceived(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  uint16_t opcode = le16toh(command_packet.header().opcode);
  uint8_t ogf = hci_spec::GetOGF(opcode);
  uint16_t ocf = hci_spec::GetOCF(opcode);

  // Note: we upcast ogf to uint16_t so that it does not get interpreted as a
  // char for printing
  ASSERT_FALSE(cmd_transactions_.empty())
      << "Received unexpected command packet with OGF: 0x" << std::hex << static_cast<uint16_t>(ogf)
      << ", OCF: 0x" << ocf;

  auto& transaction = cmd_transactions_.front();
  const hci_spec::OpCode expected_opcode =
      le16toh(transaction.expected().data.To<hci_spec::OpCode>());
  uint8_t expected_ogf = hci_spec::GetOGF(expected_opcode);
  uint16_t expected_ocf = hci_spec::GetOCF(expected_opcode);

  if (!transaction.Match(command_packet.data())) {
    auto meta = transaction.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << " Expected command packet (" << meta.expectation << ") with OGF: 0x" << std::hex
        << static_cast<uint16_t>(expected_ogf) << ", OCF: 0x" << expected_ocf
        << ". Received command packet with OGF: 0x" << static_cast<uint16_t>(ogf) << ", OCF: 0x"
        << ocf;
  }

  while (!transaction.replies().empty()) {
    auto& reply = transaction.replies().front();
    auto status = SendCommandChannelPacket(reply);
    ASSERT_EQ(ZX_OK, status) << "Failed to send reply: " << zx_status_get_string(status);
    transaction.replies().pop();
  }
  cmd_transactions_.pop();

  if (transaction_callback_) {
    DynamicByteBuffer rx(command_packet.data());
    async::PostTask(transaction_dispatcher_,
                    [rx = std::move(rx), f = transaction_callback_.share()] { f(rx); });
  }
}

void MockController::OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) {
  if (data_expectations_enabled_) {
    ASSERT_FALSE(data_transactions_.empty()) << "Received unexpected acl data packet: { "
                                             << ByteContainerToString(acl_data_packet) << "}";

    auto& expected = data_transactions_.front();
    if (!expected.Match(acl_data_packet.view())) {
      auto meta = expected.expected().meta;
      GTEST_FAIL_AT(meta.file, meta.line) << "Expected data packet (" << meta.expectation << ")";
    }

    while (!expected.replies().empty()) {
      auto& reply = expected.replies().front();
      auto status = SendACLDataChannelPacket(reply);
      ASSERT_EQ(ZX_OK, status) << "Failed to send reply: " << zx_status_get_string(status);
      expected.replies().pop();
    }
    data_transactions_.pop();
  }

  if (data_callback_) {
    DynamicByteBuffer packet_copy(acl_data_packet);
    async::PostTask(data_dispatcher_, [packet_copy = std::move(packet_copy),
                                       cb = data_callback_.share()]() mutable { cb(packet_copy); });
  }
}

void MockController::OnScoDataPacketReceived(const ByteBuffer& sco_data_packet) {
  ASSERT_FALSE(sco_transactions_.empty())
      << "Received unexpected SCO data packet: { " << ByteContainerToString(sco_data_packet) << "}";

  auto& expected = sco_transactions_.front();
  if (!expected.Match(sco_data_packet.view())) {
    auto meta = expected.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line) << "Expected SCO packet (" << meta.expectation << ")";
  }

  sco_transactions_.pop();
}

}  // namespace bt::testing
