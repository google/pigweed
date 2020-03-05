// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_controller.h"

#include <lib/async/cpp/task.h>
#include <zircon/status.h>

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace testing {

Transaction::Transaction(const ByteBuffer& expected, const std::vector<const ByteBuffer*>& replies)
    : expected_({DynamicByteBuffer(expected)}) {
  for (const auto* buffer : replies) {
    replies_.push(DynamicByteBuffer(*buffer));
  }
}

bool Transaction::Match(const ByteBuffer& packet) {
  return ContainersEqual(expected_.data, packet);
}

CommandTransaction::CommandTransaction(hci::OpCode expected_opcode,
                                       const std::vector<const ByteBuffer*>& replies)
    : Transaction({DynamicByteBuffer()}, replies), prefix_(true) {
  hci::OpCode le_opcode = htole16(expected_opcode);
  const BufferView expected(&le_opcode, sizeof(expected_opcode));
  set_expected({DynamicByteBuffer(expected)});
}

bool CommandTransaction::Match(const ByteBuffer& cmd) {
  return ContainersEqual(expected().data,
                         (prefix_ ? cmd.view(0, expected().data.size()) : cmd.view()));
}

TestController::TestController()
    : FakeControllerBase(),
      data_expectations_enabled_(false),
      data_dispatcher_(nullptr),
      transaction_dispatcher_(nullptr) {}

TestController::~TestController() {
  while (!cmd_transactions_.empty()) {
    auto& transaction = cmd_transactions_.front();
    // TODO(46656): add file & line number to failure
    ADD_FAILURE() << "Didn't receive expected outbound command packet {"
                  << ByteContainerToString(transaction.expected().data) << "}";
    cmd_transactions_.pop();
  }

  while (!data_transactions_.empty()) {
    auto& transaction = data_transactions_.front();
    // TODO(46656): add file & line number to failure
    ADD_FAILURE() << "Didn't receive expected outbound data packet {"
                  << ByteContainerToString(transaction.expected().data) << "}";
    data_transactions_.pop();
  }
  Stop();
}

void TestController::QueueCommandTransaction(CommandTransaction transaction) {
  cmd_transactions_.push(std::move(transaction));
}

void TestController::QueueCommandTransaction(const ByteBuffer& expected,
                                             const std::vector<const ByteBuffer*>& replies) {
  QueueCommandTransaction(CommandTransaction({DynamicByteBuffer(expected)}, replies));
}

void TestController::QueueDataTransaction(DataTransaction transaction) {
  ZX_ASSERT(data_expectations_enabled_);
  data_transactions_.push(std::move(transaction));
}

void TestController::QueueDataTransaction(const ByteBuffer& expected,
                                          const std::vector<const ByteBuffer*>& replies) {
  QueueDataTransaction(DataTransaction({DynamicByteBuffer(expected)}, replies));
}

bool TestController::AllExpectedDataPacketsSent() const { return data_transactions_.empty(); }

void TestController::SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(dispatcher);
  ZX_DEBUG_ASSERT(!data_callback_);
  ZX_DEBUG_ASSERT(!data_dispatcher_);

  data_callback_ = std::move(callback);
  data_dispatcher_ = dispatcher;
}

void TestController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  data_callback_ = nullptr;
}

void TestController::SetTransactionCallback(fit::closure callback, async_dispatcher_t* dispatcher) {
  SetTransactionCallback([f = std::move(callback)](const auto&) { f(); }, dispatcher);
}

void TestController::SetTransactionCallback(TransactionCallback callback,
                                            async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(dispatcher);
  ZX_DEBUG_ASSERT(!transaction_callback_);
  ZX_DEBUG_ASSERT(!transaction_dispatcher_);

  transaction_callback_ = std::move(callback);
  transaction_dispatcher_ = dispatcher;
}

void TestController::ClearTransactionCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness.
  transaction_callback_ = nullptr;
}

void TestController::OnCommandPacketReceived(const PacketView<hci::CommandHeader>& command_packet) {
  uint16_t opcode = command_packet.header().opcode;
  uint8_t ogf = hci::GetOGF(opcode);
  uint16_t ocf = hci::GetOCF(opcode);

  // Note: we upcast ogf to uint16_t so that it does not get interpreted as a
  // char for printing
  ASSERT_FALSE(cmd_transactions_.empty())
      << "Received unexpected command packet with OGF: 0x" << std::hex << static_cast<uint16_t>(ogf)
      << ", OCF: 0x" << std::hex << ocf;

  auto& current = cmd_transactions_.front();
  ASSERT_TRUE(current.Match(command_packet.data()));

  while (!current.replies().empty()) {
    auto& reply = current.replies().front();
    auto status = SendCommandChannelPacket(reply);
    ASSERT_EQ(ZX_OK, status) << "Failed to send reply: " << zx_status_get_string(status);
    current.replies().pop();
  }
  cmd_transactions_.pop();

  if (transaction_callback_) {
    DynamicByteBuffer rx(command_packet.data());
    async::PostTask(transaction_dispatcher_,
                    [rx = std::move(rx), f = transaction_callback_.share()] { f(rx); });
  }
}

void TestController::OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) {
  if (data_expectations_enabled_) {
    ASSERT_FALSE(data_transactions_.empty()) << "Received unexpected acl data packet: { "
                                             << ByteContainerToString(acl_data_packet) << "}";

    auto& current = data_transactions_.front();
    ASSERT_TRUE(current.Match(acl_data_packet.view()));

    while (!current.replies().empty()) {
      auto& reply = current.replies().front();
      auto status = SendACLDataChannelPacket(reply);
      ASSERT_EQ(ZX_OK, status) << "Failed to send reply: " << zx_status_get_string(status);
      current.replies().pop();
    }
    data_transactions_.pop();
  }

  if (data_callback_) {
    DynamicByteBuffer packet_copy(acl_data_packet);
    async::PostTask(data_dispatcher_, [packet_copy = std::move(packet_copy),
                                       cb = data_callback_.share()]() mutable { cb(packet_copy); });
  }
}

}  // namespace testing
}  // namespace bt
