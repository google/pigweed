// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_CHANNEL_H_

#include <lib/zx/channel.h>

#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/weak_self.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/data_buffer_info.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_packet.h"

namespace bt::hci {

class Transport;

// Represents the Bluetooth SCO Data channel and manages the Host->Controller SCO data flow when SCO
// is not offloaded. ScoDataChannel uses a pull model, where packets are queued in the connections
// and only read by ScoDataChannel when controller buffer space is available.
// TODO(fxbug.dev/91560): Only 1 connection's bandwidth is configured with the transport driver at
// a time, so performance may be poor if multiple connections are registered. The connection used
// for the current configuration is selected randomly.
// TODO(fxbug.dev/89689): ScoDataChannel assumes that HCI flow control via
// HCI_Number_Of_Completed_Packets events is supported by the controller. Some controllers don't
// support this form of flow control.
class ScoDataChannel {
 public:
  // Registered SCO connections must implement this interface to send and receive packets.
  class ConnectionInterface {
   public:
    virtual ~ConnectionInterface() = default;

    virtual hci_spec::ConnectionHandle handle() const = 0;

    // These parameters must specify a data path of pw::bluetooth::emboss::ScoDataPath::HCI.
    virtual bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>
    parameters() = 0;

    // ScoDataChannel will call this method to get the next packet to send to the controller.
    // If no packet is available, return nullptr.
    virtual std::unique_ptr<ScoDataPacket> GetNextOutboundPacket() = 0;

    // This method will be called when a packet is received for this connection.
    virtual void ReceiveInboundPacket(std::unique_ptr<ScoDataPacket> packet) = 0;

    // Called when there is an internal error and this connection has been unregistered.
    // Unregistering this connection is unnecessary, but harmless.
    virtual void OnHciError() = 0;
  };

  static std::unique_ptr<ScoDataChannel> Create(const DataBufferInfo& buffer_info,
                                                CommandChannel* command_channel,
                                                pw::bluetooth::Controller* hci);
  virtual ~ScoDataChannel() = default;

  // Register a connection. The connection must have a data path of
  // pw::bluetooth::emboss::ScoDataPath::kHci.
  virtual void RegisterConnection(WeakPtr<ConnectionInterface> connection) = 0;

  // Unregister a connection when it is disconnected.
  // |UnregisterConnection| does not clear the controller packet count, so
  // |ClearControllerPacketCount| must be called after |UnregisterConnection| and the
  // HCI_Disconnection_Complete event has been received.
  virtual void UnregisterConnection(hci_spec::ConnectionHandle handle) = 0;

  // Resets controller packet count for |handle| so that controller buffer credits can be reused.
  // This must be called on the HCI_Disconnection_Complete event to notify ScoDataChannel that
  // packets in the controller's buffer for |handle| have been flushed. See Core Spec v5.1, Vol 2,
  // Part E, Section 4.3. This must be called after |UnregisterConnection|.
  virtual void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) = 0;

  // Called by connections when an outbound packet is available (via
  // ConnectionInterface::GetNextOutboundPacket).
  virtual void OnOutboundPacketReadable() = 0;

  // The controller's SCO max data length (not including header).
  virtual uint16_t max_data_length() const = 0;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_CHANNEL_H_
