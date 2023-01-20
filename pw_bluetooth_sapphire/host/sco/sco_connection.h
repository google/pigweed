// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_H_

#include <lib/fit/function.h>

#include <queue>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_packet.h"

namespace bt::sco {

// ScoConnection is a wrapper around an owned SCO hci::Connection. It provides a
// high-level interface to the underlying connection.
//
// This class is intended to be owned by a ScoConnectionManager.
class ScoConnection final : public hci::ScoDataChannel::ConnectionInterface {
 public:
  // |connection| is the underlying connection and must have the link type kSCO or kESCO.
  // |deactivated_cb| will be called when the connection has been Deactivated and should be
  // destroyed.
  ScoConnection(
      std::unique_ptr<hci::Connection> connection, fit::closure deactivated_cb,
      bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter> parameters,
      hci::ScoDataChannel* channel);

  // Destroying this object will disconnect the underlying HCI connection.
  ~ScoConnection() override = default;

  hci_spec::ConnectionHandle handle() const override { return handle_; }

  // Called by ScoConnectionManager to notify a connection it can no longer process data and its
  // hci::Connection should be closed.
  void Close();

  // Returns a value that's unique for any SCO connection on this device.
  using UniqueId = hci_spec::ConnectionHandle;
  UniqueId unique_id() const;
  UniqueId id() const;

  // Activates this channel. |rx_callback| and |closed_callback| are called as data is received and
  // the channel is closed, respectively. `Deactivate` should be called in `closed_callback`.
  //
  // Returns false if the channel could not be activated.
  bool Activate(fit::closure rx_callback, fit::closure closed_callback);

  // Deactivates this channel. No more packets can be sent or received after
  // this is called. |rx_callback| may still be called if it has been already
  // dispatched to its task runner.
  void Deactivate();

  // Maximum outbound SDU payload size that will be accepted by |Send()|.
  uint16_t max_tx_sdu_size() const;

  // Queue the given SCO payload for transmission over this channel, taking
  // ownership of |payload|. Returns true if the payload was queued successfully, and
  // false otherwise.
  bool Send(ByteBufferPtr payload);

  // If an inbound packet is ready to be read, returns the packet. Otherwise, returns nullptr.
  std::unique_ptr<hci::ScoDataPacket> Read();

  using WeakPtr = WeakSelf<ScoConnection>::WeakPtr;
  WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

  // ScoDataChannel overrides:
  bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter> parameters()
      override;
  std::unique_ptr<hci::ScoDataPacket> GetNextOutboundPacket() override;
  void ReceiveInboundPacket(std::unique_ptr<hci::ScoDataPacket> packet) override;
  void OnHciError() override;

 private:
  // Common clean up logic for Close() and Deactivate(). Marks connection as inactive and closes the
  // underlying connection.
  void CleanUp();

  // True if Activate() has been called and neither Close() or Deactivate() has been called yet.
  bool active_;

  hci_spec::ConnectionHandle handle_;

  std::unique_ptr<hci::Connection> connection_;

  // Called to notify the caller of Activate() that the connection was closed.
  fit::closure activator_closed_cb_;

  // Called to notify the owner that the connection was deactivated.
  fit::closure deactivated_cb_;

  // Notify caller of Activate() that an inbound packet has been received and may be read.
  fit::closure rx_callback_ = nullptr;

  // Contains outbound SCO payloads.
  std::queue<ByteBufferPtr> outbound_queue_;

  // Contains inbound SCO payloads.
  std::queue<std::unique_ptr<hci::ScoDataPacket>> inbound_queue_;

  // This will be null if HCI SCO is not supported.
  hci::ScoDataChannel* channel_ = nullptr;

  bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter> parameters_;

  WeakSelf<ScoConnection> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScoConnection);
};

}  // namespace bt::sco

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_H_
