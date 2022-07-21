// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_FACTORY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_FACTORY_H_

#include <lib/async/dispatcher.h>
#include <lib/zx/socket.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include <memory>
#include <unordered_map>

#include "socket_channel_relay.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::socket {

// A SocketFactory vends zx::socket objects that an IPC peer can use to
// communicate with l2cap::Channels.
//
// Over time, the factory may grow more responsibility and intelligence. For
// example, the factory might manage QoS by configuring the number of packets a
// SocketChannelRelay can process before yielding control back to the
// dispatcher.
//
// THREAD-SAFETY: This class is thread-hostile. An instance must be
// created and destroyed on a single thread. Said thread must have a
// single-threaded dispatcher. Failure to follow those rules may cause the
// program to abort.
template <typename ChannelT>
class SocketFactory final {
 public:
  SocketFactory();
  ~SocketFactory();

  // Creates a zx::socket which can be used to read from, and write to,
  // |channel|.
  //
  // |channel| will automatically be Deactivated() when the zx::socket is
  // closed, or the creation thread's dispatcher shuts down.
  //
  // Similarly, the local end corresponding to the returned zx::socket will
  // automatically be closed when |channel| is closed, or the creation thread's
  // dispatcher  shuts down.
  //
  // It is an error to call MakeSocketForChannel() multiple times for
  // the same Channel.
  //
  // Returns the new socket on success, and an invalid socket otherwise
  // (including if |channel| is nullptr).
  zx::socket MakeSocketForChannel(fxl::WeakPtr<ChannelT> channel);

 private:
  using RelayT = SocketChannelRelay<ChannelT>;
  using ChannelIdT = typename ChannelT::UniqueId;

  // TODO(fxbug.dev/671): Figure out what we need to do handle the possibility that a
  // channel id is recycled. (See comment in LogicalLink::HandleRxPacket.)
  std::unordered_map<ChannelIdT, std::unique_ptr<RelayT>> channel_to_relay_;

  fxl::WeakPtrFactory<SocketFactory> weak_ptr_factory_;  // Keep last.

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SocketFactory);
};

template <typename ChannelT>
SocketFactory<ChannelT>::SocketFactory() : weak_ptr_factory_(this) {}

template <typename ChannelT>
SocketFactory<ChannelT>::~SocketFactory() {}

template <typename ChannelT>
zx::socket SocketFactory<ChannelT>::MakeSocketForChannel(fxl::WeakPtr<ChannelT> channel) {
  if (!channel) {
    return zx::socket();
  }

  const auto unique_id = channel->unique_id();
  if (channel_to_relay_.find(unique_id) != channel_to_relay_.end()) {
    bt_log(ERROR, "l2cap", "channel %u is already bound to a socket", channel->id());
    return zx::socket();
  }

  zx::socket local_socket, remote_socket;
  const auto status = zx::socket::create(ZX_SOCKET_DATAGRAM, &local_socket, &remote_socket);
  if (status != ZX_OK) {
    bt_log(ERROR, "data", "Failed to create socket for channel %u: %s", channel->unique_id(),
           zx_status_get_string(status));
    return zx::socket();
  }

  auto relay = std::make_unique<RelayT>(
      std::move(local_socket), channel,
      typename RelayT::DeactivationCallback(
          [self = weak_ptr_factory_.GetWeakPtr(), id = unique_id]() mutable {
            ZX_DEBUG_ASSERT_MSG(self, "(unique_id=%u)", id);
            size_t n_erased = self->channel_to_relay_.erase(id);
            ZX_DEBUG_ASSERT_MSG(n_erased == 1, "(n_erased=%zu, unique_id=%u)", n_erased, id);
          }));

  // Note: Activate() may abort, if |channel| has been Activated() without
  // going through this SocketFactory.
  if (!relay->Activate()) {
    bt_log(ERROR, "l2cap", "Failed to Activate() relay for channel %u", channel->id());
    return zx::socket();
  }

  channel_to_relay_.emplace(unique_id, std::move(relay));
  return remote_socket;
}

}  // namespace bt::socket

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_FACTORY_H_
