// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_

#include <lib/async/cpp/wait.h>
#include <lib/zx/channel.h>
#include <zircon/device/bt-hci.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_packet.h"

namespace bt::testing {

// Abstract base for implementing a fake HCI controller endpoint. This can
// directly send ACL data and event packets on request and forward outgoing ACL
// data packets to subclass implementations.
class ControllerTestDoubleBase {
 public:
  ControllerTestDoubleBase();
  virtual ~ControllerTestDoubleBase();

  // Sets a callback that will be called when an error is signaled with SignalError().
  void set_error_callback(fit::callback<void(zx_status_t)> callback) {
    error_cb_ = std::move(callback);
  }

  // Closes all open channels and signals |status| to the error callback.
  void Stop(zx_status_t status = ZX_ERR_PEER_CLOSED);

  // Sends the given packet over this FakeController's command channel endpoint.
  // Returns the result of the write operation on the command channel.
  zx_status_t SendCommandChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this FakeController's ACL data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  zx_status_t SendACLDataChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this ControllerTestDouble's SCO data channel
  // endpoint.
  // Returns the result of the write operation on the channel.
  zx_status_t SendScoDataChannelPacket(const ByteBuffer& packet);

  // Sends the given packet over this FakeController's Snoop channel endpoint.
  // Returns the result of the write operation on the channel.
  void SendSnoopChannelPacket(const ByteBuffer& packet, bt_hci_snoop_type_t packet_type,
                              bool is_received);

  // Immediately closes the Snoop channel endpoint.
  void CloseSnoopChannel();

  // Starts listening for event packets with the given callback.
  void StartCmdChannel(fit::function<void(std::unique_ptr<hci::EventPacket>)> packet_cb);

  // Starts listening for ACL packets with the given callback.
  void StartAclChannel(fit::function<void(std::unique_ptr<hci::ACLDataPacket>)> packet_cb);

  // Starts listening for SCO packets with the given callback.
  void StartScoChannel(fit::function<void(std::unique_ptr<hci::ScoDataPacket>)> packet_cb);

  // Starts listening for snoop packets on the given channel.
  // Returns false if already listening on a snoop channel
  bool StartSnoopChannel(zx::channel chan);

  // Called by test fixtures to send packets:
  void HandleCommandPacket(std::unique_ptr<hci::CommandPacket> packet);
  void HandleACLPacket(std::unique_ptr<hci::ACLDataPacket> packet);
  void HandleScoPacket(std::unique_ptr<hci::ScoDataPacket> packet);

  void SignalError(zx_status_t status) {
    if (error_cb_) {
      error_cb_(status);
    }
  }

 protected:
  const zx::channel& snoop_channel() const { return snoop_channel_; }

  // Called when there is an outgoing command packet.
  virtual void OnCommandPacketReceived(
      const PacketView<hci_spec::CommandHeader>& command_packet) = 0;

  // Called when there is an outgoing ACL data packet.
  virtual void OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) = 0;

  // Called when there is an outgoing SCO data packet.
  virtual void OnScoDataPacketReceived(const ByteBuffer& sco_data_packet) = 0;

 private:
  zx::channel snoop_channel_;

  // Send inbound packets to the host stack:
  fit::function<void(std::unique_ptr<hci::EventPacket>)> send_event_;
  fit::function<void(std::unique_ptr<hci::ACLDataPacket>)> send_acl_packet_;
  fit::function<void(std::unique_ptr<hci::ScoDataPacket>)> send_sco_packet_;

  fit::callback<void(zx_status_t)> error_cb_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTestDoubleBase);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_DOUBLE_BASE_H_
