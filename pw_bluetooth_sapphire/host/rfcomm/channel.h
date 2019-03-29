// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_H_

#include <queue>

#include <fbl/ref_counted.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/frames.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/rfcomm.h"
#include "src/lib/fxl/macros.h"

namespace bt {
namespace rfcomm {

class Session;  // Break mutual dependency.

class Channel : public fbl::RefCounted<Channel> {
 public:
  using UniqueId = uint64_t;

  virtual ~Channel() = default;

  DLCI id() const { return dlci_; }
  size_t tx_mtu() const;
  // TODO(quiche): Provide lower-layer ID info for debugging purposes.
  hci::ConnectionHandle link_handle() const { return 0; }
  // TODO(NET-1763): Make this identifier unique across L2CAP channels and HCI
  // connections.
  UniqueId unique_id() const { return dlci_; }

  using RxCallback = fit::function<void(common::ByteBufferPtr)>;
  using ClosedCallback = fit::closure;
  // Activates this channel assigning |dispatcher| to execute |rx_callback| and
  // |closed_callback|. Returns true on success.
  virtual bool Activate(RxCallback rx_callback, ClosedCallback closed_callback,
                        async_dispatcher_t* dispatcher) = 0;
  // Cleans up resources associated with this channel.
  // TODO(NET-1756): Implement cleanup.
  void Deactivate() {}

  // Send a buffer of user data. Takes ownership of |data|. This method is
  // asynchronous, and there is no notification of delivery. We operate under
  // the assumption that the underlying transport is reliable. The channel must
  // be activated prior to sending. Returns true if the data was successfully
  // queued.
  virtual bool Send(common::ByteBufferPtr data) = 0;

 protected:
  friend class Session;

  Channel(DLCI dlci, Session* session);

  RxCallback rx_callback_;
  ClosedCallback closed_callback_;
  async_dispatcher_t* dispatcher_;

  const DLCI dlci_;
  // The Session owning this Channel. |session_| will always outlive |this|.
  Session* session_;

  // True if the channel is established (DLC Establishment has taken place)
  bool established_;

  // The negotiation state of this channel
  ParameterNegotiationState negotiation_state_;

  // The number of local and remote credits available on this channel.
  Credits local_credits_;
  Credits remote_credits_;

  // Frames waiting on this channel to receive credits to be sent (and
  // sent callbacks)
  std::queue<std::pair<std::unique_ptr<Frame>, fit::closure>> wait_queue_;

  // Called by |session_| when a new frame is received for this channel. If an
  // |rx_callback_| is registered, the frame is forwarded to the callback;
  // otherwise, the frame is buffered and is forwarded once a callback gets
  // registered.
  virtual void Receive(common::ByteBufferPtr data) = 0;

  FXL_DISALLOW_COPY_AND_ASSIGN(Channel);
};

namespace internal {

class ChannelImpl : public Channel {
 public:
  // Channel overrides
  bool Activate(RxCallback rx_callback, ClosedCallback closed_callback,
                async_dispatcher_t* dispatcher) override;
  bool Send(common::ByteBufferPtr data) override;

 private:
  friend class rfcomm::Session;

  // This should only be called from Session.
  ChannelImpl(DLCI dlci, Session* session);

  // This should only be called from Session.
  void Receive(common::ByteBufferPtr data) override;

  std::queue<common::ByteBufferPtr> pending_rxed_frames_;
};

}  //  namespace internal

}  // namespace rfcomm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_H_
