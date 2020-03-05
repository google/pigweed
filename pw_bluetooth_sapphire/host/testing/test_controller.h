// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_CONTROLLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_CONTROLLER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <queue>
#include <vector>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_base.h"

namespace bt {
namespace testing {

// TODO(46656): add file name, line number, and expectation expression
struct PacketExpectation {
  DynamicByteBuffer data;
};

class Transaction {
 public:
  // The |expected| buffer and the buffers in |replies| will be copied, so their lifetime does not
  // need to extend past Transaction construction.
  Transaction(const ByteBuffer& expected, const std::vector<const ByteBuffer*>& replies);
  virtual ~Transaction() = default;
  Transaction(Transaction&& other) = default;
  Transaction& operator=(Transaction&& other) = default;

  // Returns true if the transaction matches the given HCI packet.
  virtual bool Match(const ByteBuffer& packet);

  const PacketExpectation& expected() { return expected_; }
  void set_expected(const PacketExpectation& expected) {
    expected_ = PacketExpectation{DynamicByteBuffer(expected.data)};
  }

  std::queue<DynamicByteBuffer>& replies() { return replies_; }

 private:
  PacketExpectation expected_;
  std::queue<DynamicByteBuffer> replies_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Transaction);
};

// A CommandTransaction is used to set up an expectation for a command channel
// packet and the events that should be sent back in response to it.
class CommandTransaction final : public Transaction {
 public:
  CommandTransaction(const ByteBuffer& expected, const std::vector<const ByteBuffer*>& replies)
      : Transaction(expected, replies), prefix_(false) {}

  // Match by opcode only.
  CommandTransaction(hci::OpCode expected_opcode, const std::vector<const ByteBuffer*>& replies);

  // Move constructor and assignment operator.
  CommandTransaction(CommandTransaction&& other) = default;
  CommandTransaction& operator=(CommandTransaction&& other) = default;

  // Returns true if the transaction matches the given HCI command packet.
  bool Match(const ByteBuffer& cmd) override;

 private:
  bool prefix_ = false;
};

// A DataTransaction is used to set up an expectation for an acl data channel
class DataTransaction final : public Transaction {
 public:
  DataTransaction(const ByteBuffer& expected, const std::vector<const ByteBuffer*>& replies)
      : Transaction(expected, replies) {}
  DataTransaction(DataTransaction&& other) = default;
  DataTransaction& operator=(DataTransaction&& other) = default;
};

// Helper macro for expecting a data packet and specifying a variable number of responses that the
// TestController should send in response to the expected packet.
// TODO(46656): add file & line number arguments
#define EXPECT_ACL_PACKET_OUT(device, packet_buffer, ...) \
  (device)->QueueDataTransaction((packet_buffer), {__VA_ARGS__})

// TestController allows unit tests to set up an expected sequence of HCI
// command packets and ACL data packets and any packets that should be sent back in response. The
// code internally verifies each received packet using gtest ASSERT_* macros.
class TestController : public FakeControllerBase {
 public:
  TestController();
  ~TestController() override;

  // Queues a transaction into the TestController's expected command queue. Each
  // packet received through the command channel endpoint will be verified
  // against the next expected transaction in the queue. A mismatch will cause a
  // fatal assertion. On a match, TestController will send back the replies
  // provided in the transaction.
  void QueueCommandTransaction(CommandTransaction transaction);
  void QueueCommandTransaction(const ByteBuffer& expected,
                               const std::vector<const ByteBuffer*>& replies);

  // Queues a transaction into the TestController's expected ACL data queue. Each
  // packet received through the ACL data channel endpoint will be verified
  // against the next expected transaction in the queue. A mismatch will cause a
  // fatal assertion. On a match, TestController will send back the replies
  // provided in the transaction.
  void QueueDataTransaction(DataTransaction transaction);
  void QueueDataTransaction(const ByteBuffer& expected,
                            const std::vector<const ByteBuffer*>& replies);

  // Returns true iff all transactions queued with QueueDataTransaction() have been received.
  bool AllExpectedDataPacketsSent() const;

  // TODO(benlawson): remove after all TestController tests have been refactored to use data
  // expectations
  void set_data_expectations_enabled(bool enabled) { data_expectations_enabled_ = enabled; }

  // Callback to invoke when a packet is received over the data channel. Care
  // should be taken to ensure that a callback with a reference to test case
  // variables is not invoked when tearing down.
  using DataCallback = fit::function<void(const ByteBuffer& packet)>;
  void SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher);
  void ClearDataCallback();

  // Callback invoked when a transaction completes. Care should be taken to
  // ensure that a callback with a reference to test case variables is not
  // invoked when tearing down.
  using TransactionCallback = fit::function<void(const ByteBuffer& rx)>;
  void SetTransactionCallback(TransactionCallback callback, async_dispatcher_t* dispatcher);
  void SetTransactionCallback(fit::closure callback, async_dispatcher_t* dispatcher);
  void ClearTransactionCallback();

 private:
  // FakeControllerBase overrides:
  void OnCommandPacketReceived(const PacketView<hci::CommandHeader>& command_packet) override;
  void OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) override;

  std::queue<CommandTransaction> cmd_transactions_;
  std::queue<DataTransaction> data_transactions_;
  bool data_expectations_enabled_;
  DataCallback data_callback_;
  async_dispatcher_t* data_dispatcher_;
  TransactionCallback transaction_callback_;
  async_dispatcher_t* transaction_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TestController);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_CONTROLLER_H_
