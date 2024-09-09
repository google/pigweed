// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <fuchsia/bluetooth/bredr/cpp/fidl.h>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"

namespace bthost {

// BrEdrConnectionServer relays packets and disconnections between the
// Connection FIDL protocol and a corresponding L2CAP channel.
class BrEdrConnectionServer : public ServerBase<fuchsia::bluetooth::Channel> {
 public:
  // The number of inbound packets to queue in this class.
  static constexpr size_t kDefaultReceiveQueueLimit = 20;

  // `channel` is the Channel that this Connection corresponds to.
  // BrEdrConnection server will activate and manage the lifetime of this
  // chanel. `closed_callback` will be called when either the Connection
  // protocol or the L2CAP channel closes. Returns nullptr on failure (failure
  // to activate the Channel).
  static std::unique_ptr<BrEdrConnectionServer> Create(
      fidl::InterfaceRequest<fuchsia::bluetooth::Channel> request,
      bt::l2cap::Channel::WeakPtr channel,
      fit::callback<void()> closed_callback);

  ~BrEdrConnectionServer() override;

 private:
  enum class State {
    kActivating,  // Default state.
    kActivated,
    kDeactivating,
    kDeactivated,
  };

  BrEdrConnectionServer(
      fidl::InterfaceRequest<fuchsia::bluetooth::Channel> request,
      bt::l2cap::Channel::WeakPtr channel,
      fit::callback<void()> closed_callback);

  // fuchsia::bluetooth::Channel overrides:
  void Send(std::vector<::fuchsia::bluetooth::Packet> packets,
            SendCallback callback) override;
  void Receive(ReceiveCallback callback) override;
  void WatchChannelParameters(WatchChannelParametersCallback callback) override;
  void handle_unknown_method(uint64_t ordinal,
                             bool method_has_response) override;

  bool Activate();
  void Deactivate();

  void OnChannelDataReceived(bt::ByteBufferPtr rx_data);
  void OnChannelClosed();
  void OnProtocolClosed();
  void DeactivateAndRequestDestruction();
  void ServiceReceiveQueue();

  bt::l2cap::Channel::WeakPtr channel_;

  // The maximum number of inbound packets to queue when the FIDL protocol is
  // full.
  const size_t receive_queue_max_frames_ = kDefaultReceiveQueueLimit;

  // We use a std::deque here to minimize the number dynamic memory allocations
  // (cf. std::list, which would require allocation on each SDU). This comes,
  // however, at the cost of higher memory usage when the number of SDUs is
  // small. (libc++ uses a minimum of 4KB per deque.)
  std::deque<bt::ByteBufferPtr> receive_queue_;

  // Client callback called when either FIDL protocol closes or L2CAP channel
  // closes.
  fit::callback<void()> closed_cb_;

  // Callback for pending Channel::Receive() call.
  ReceiveCallback receive_cb_ = nullptr;

  // Pending callback for a WatchChannelParameters call.
  std::optional<WatchChannelParametersCallback>
      pending_watch_channel_parameters_;

  State state_ = State::kActivating;

  WeakSelf<BrEdrConnectionServer> weak_self_;  // Keep last.
};

}  // namespace bthost
