// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_H_

#include <lib/fit/function.h>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

namespace bt::gap {

// ScoConnection is a wrapper around an owned SCO hci::Connection. It provides a
// high-level interface to the underlying connection. This class implements the required ChannelT
// template parameter methods of SocketChannelRelay and SocketFactory.
//
// This class is intended to be owned by a ScoConnectionManager.
class ScoConnection final : public fbl::RefCounted<ScoConnection> {
 public:
  static fbl::RefPtr<ScoConnection> Create(std::unique_ptr<hci::Connection> connection);

  hci::ConnectionHandle handle() const { return connection_->handle(); }

  // Called by ScoConnectionManager to notify a connection it can no longer process data and its
  // hci::Connection should be closed.
  void Close();

  // ChannelT implementation:

  // Returns a value that's unique for any SCO connection on this device.
  using UniqueId = hci::ConnectionHandle;
  UniqueId unique_id() const;
  UniqueId id() const;

  // Activates this channel. |rx_callback| and |closed_callback| are called as data is received and
  // the channel is closed, respectively.
  //
  // Returns false if the channel could not be activated.
  using RxCallback = fit::function<void(ByteBufferPtr packet)>;
  bool Activate(RxCallback rx_callback, fit::closure closed_callback);

  // Deactivates this channel. No more packets can be sent or received after
  // this is called. |rx_callback| may still be called if it has been already
  // dispatched to its task runner.
  void Deactivate();

  // Maximum outbound SDU payload size that will be accepted by |Send()|.
  uint16_t max_tx_sdu_size() const;

  // Queue the given SDU payload for transmission over this channel, taking
  // ownership of |sdu|. Returns true if the SDU was queued successfully, and
  // false otherwise.
  bool Send(ByteBufferPtr sdu);

 private:
  friend class fbl::RefPtr<ScoConnection>;

  // |connection| is the underlying connection and must have the link type kSCO or kESCO.
  explicit ScoConnection(std::unique_ptr<hci::Connection> connection);

  // Destroying this object will disconnect the underlying HCI connection.
  ~ScoConnection() = default;

  // Common clean up logic for Close() and Deactivate(). Marks connection as inactive and closes the
  // underlying connection.
  void CleanUp();

  // True if Activate() has been called and neither Close() or Deactivate() has been called yet.
  bool active_;

  std::unique_ptr<hci::Connection> connection_;

  fit::closure closed_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScoConnection);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_H_
