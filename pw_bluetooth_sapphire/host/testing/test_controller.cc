// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_controller.h"

#include <lib/async/cpp/task.h>
#include <zircon/status.h>

#include "gtest/gtest.h"

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace btlib {
namespace testing {

CommandTransaction::CommandTransaction(
    const common::ByteBuffer& expected,
    const std::vector<const common::ByteBuffer*>& replies)
    : prefix_(false), expected_(expected) {
  for (const auto* buffer : replies) {
    replies_.push(common::DynamicByteBuffer(*buffer));
  }
}

CommandTransaction::CommandTransaction(
    hci::OpCode expected_opcode,
    const std::vector<const common::ByteBuffer*>& replies)
    : prefix_(true) {
  hci::OpCode le_opcode = htole16(expected_opcode);
  const common::BufferView expected(&le_opcode, sizeof(expected_opcode));
  expected_ = common::DynamicByteBuffer(expected);
  for (const auto* buffer : replies) {
    replies_.push(common::DynamicByteBuffer(*buffer));
  }
}

bool CommandTransaction::Match(const common::BufferView& cmd) {
  return ContainersEqual(expected_,
                         (prefix_ ? cmd.view(0, expected_.size()) : cmd));
}

TestController::TestController()
    : FakeControllerBase(),
      data_dispatcher_(nullptr),
      transaction_dispatcher_(nullptr) {}

TestController::~TestController() {
  EXPECT_EQ(0u, cmd_transactions_.size()) << "Not all transactions resolved!";
  Stop();
}

void TestController::QueueCommandTransaction(CommandTransaction transaction) {
  cmd_transactions_.push(std::move(transaction));
}

void TestController::QueueCommandTransaction(
    const common::ByteBuffer& expected,
    const std::vector<const common::ByteBuffer*>& replies) {
  QueueCommandTransaction(CommandTransaction(expected, replies));
}

void TestController::SetDataCallback(DataCallback callback,
                                     async_dispatcher_t* dispatcher) {
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

void TestController::SetTransactionCallback(fit::closure callback,
                                            async_dispatcher_t* dispatcher) {
  SetTransactionCallback([f = std::move(callback)](const auto&) { f(); },
                         dispatcher);
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

void TestController::OnCommandPacketReceived(
    const common::PacketView<hci::CommandHeader>& command_packet) {
  uint16_t opcode = command_packet.header().opcode;
  uint8_t ogf = hci::GetOGF(opcode);
  uint16_t ocf = hci::GetOCF(opcode);

  // Note: we upcast ogf to uint16_t so that it does not get interpreted as a
  // char for printing
  ASSERT_FALSE(cmd_transactions_.empty())
      << "Received unexpected command packet with OGF: 0x" << std::hex
      << static_cast<uint16_t>(ogf) << ", OCF: 0x" << std::hex << ocf;

  auto& current = cmd_transactions_.front();
  ASSERT_TRUE(current.Match(command_packet.data()));

  while (!current.replies_.empty()) {
    auto& reply = current.replies_.front();
    auto status = SendCommandChannelPacket(reply);
    ASSERT_EQ(ZX_OK, status)
        << "Failed to send reply: " << zx_status_get_string(status);
    current.replies_.pop();
  }
  cmd_transactions_.pop();

  if (transaction_callback_) {
    common::DynamicByteBuffer rx(command_packet.data());
    async::PostTask(
        transaction_dispatcher_,
        [rx = std::move(rx), f = transaction_callback_.share()] { f(rx); });
  }
}

void TestController::OnACLDataPacketReceived(
    const common::ByteBuffer& acl_data_packet) {
  if (!data_callback_)
    return;

  common::DynamicByteBuffer packet_copy(acl_data_packet);
  async::PostTask(data_dispatcher_,
                  [packet_copy = std::move(packet_copy),
                   cb = data_callback_.share()]() mutable { cb(packet_copy); });
}

}  // namespace testing
}  // namespace btlib
