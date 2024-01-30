// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/sco/sco_connection.h"

namespace bt::sco {

ScoConnection::ScoConnection(
    std::unique_ptr<hci::Connection> connection,
    fit::closure deactivated_cb,
    bt::StaticPacket<
        pw::bluetooth::emboss::SynchronousConnectionParametersWriter>
        parameters,
    hci::ScoDataChannel* channel)
    : active_(false),
      connection_(std::move(connection)),
      deactivated_cb_(std::move(deactivated_cb)),
      channel_(channel),
      parameters_(std::move(parameters)),
      weak_self_(this) {
  BT_ASSERT(connection_);
  BT_ASSERT(!channel_ || channel_->max_data_length() <=
                             hci_spec::kMaxSynchronousDataPacketPayloadSize);

  handle_ = connection_->handle();

  connection_->set_peer_disconnect_callback([this](const auto&, auto) {
    // Notifies activator that this connection has been disconnected.
    // Activator will call Deactivate().
    Close();
  });
}

ScoConnection::UniqueId ScoConnection::unique_id() const {
  // HCI connection handles are unique per controller.
  return handle();
}

ScoConnection::UniqueId ScoConnection::id() const { return unique_id(); }

void ScoConnection::Close() {
  bt_log(TRACE, "gap-sco", "closing sco connection (handle: %.4x)", handle());

  bool active = active_;
  CleanUp();

  if (!active) {
    return;
  }

  BT_ASSERT(activator_closed_cb_);
  // Move cb out of this, since cb may destroy this.
  auto cb = std::move(activator_closed_cb_);
  cb();
}

bool ScoConnection::Activate(fit::closure rx_callback,
                             fit::closure closed_callback) {
  // TODO(fxbug.dev/42136417): Handle Activate() called on a connection that has
  // been closed already.
  BT_ASSERT(closed_callback);
  BT_ASSERT(!active_);
  BT_ASSERT(rx_callback);
  activator_closed_cb_ = std::move(closed_callback);
  rx_callback_ = std::move(rx_callback);
  active_ = true;
  if (channel_ && parameters_.view().input_data_path().Read() ==
                      pw::bluetooth::emboss::ScoDataPath::HCI) {
    channel_->RegisterConnection(GetWeakPtr());
  }
  return true;
}

void ScoConnection::Deactivate() {
  bt_log(
      TRACE, "gap-sco", "deactivating sco connection (handle: %.4x)", handle());
  CleanUp();
  if (deactivated_cb_) {
    // Move cb out of this, since cb may destroy this.
    auto cb = std::move(deactivated_cb_);
    cb();
  }
}

uint16_t ScoConnection::max_tx_sdu_size() const {
  return channel_ ? channel_->max_data_length() : 0u;
}

bool ScoConnection::Send(ByteBufferPtr payload) {
  if (!active_) {
    bt_log(WARN,
           "gap-sco",
           "dropping SCO packet for inactive connection (handle: %#.4x)",
           handle_);
    return false;
  }

  if (!channel_) {
    bt_log(
        WARN,
        "gap-sco",
        "dropping SCO packet because HCI SCO is not supported (handle: %#.4x)",
        handle_);
    return false;
  }

  if (payload->size() > channel_->max_data_length()) {
    bt_log(WARN,
           "gap-sco",
           "dropping SCO packet larger than the buffer data packet length "
           "(packet size: %zu, max "
           "data length: "
           "%hu)",
           payload->size(),
           channel_->max_data_length());
    return false;
  }

  outbound_queue_.push(std::move(payload));

  // Notify ScoDataChannel that a packet is available. This is only necessary
  // for the first packet of an empty queue (flow control will poll this
  // connection otherwise).
  if (outbound_queue_.size() == 1u) {
    channel_->OnOutboundPacketReadable();
  }
  return true;
}

std::unique_ptr<hci::ScoDataPacket> ScoConnection::Read() {
  if (inbound_queue_.empty()) {
    return nullptr;
  }
  std::unique_ptr<hci::ScoDataPacket> packet =
      std::move(inbound_queue_.front());
  inbound_queue_.pop();
  return packet;
}

bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>
ScoConnection::parameters() {
  return parameters_;
}

std::unique_ptr<hci::ScoDataPacket> ScoConnection::GetNextOutboundPacket() {
  if (outbound_queue_.empty()) {
    return nullptr;
  }

  std::unique_ptr<hci::ScoDataPacket> out = hci::ScoDataPacket::New(
      handle(), static_cast<uint8_t>(outbound_queue_.front()->size()));
  if (!out) {
    bt_log(ERROR, "gap-sco", "failed to allocate SCO data packet");
    return nullptr;
  }
  out->mutable_view()->mutable_payload_data().Write(
      outbound_queue_.front()->view());
  outbound_queue_.pop();
  return out;
}

void ScoConnection::ReceiveInboundPacket(
    std::unique_ptr<hci::ScoDataPacket> packet) {
  BT_ASSERT(packet->connection_handle() == handle_);

  if (!active_ || !rx_callback_) {
    bt_log(TRACE, "gap-sco", "dropping inbound SCO packet");
    return;
  }

  inbound_queue_.push(std::move(packet));
  // It's only necessary to notify activator of the first packet queued (flow
  // control will poll this connection otherwise).
  if (inbound_queue_.size() == 1u) {
    rx_callback_();
  }
}

void ScoConnection::OnHciError() {
  // Notify activator that this connection should be deactivated.
  Close();
}

void ScoConnection::CleanUp() {
  if (active_ && channel_ &&
      parameters_.view().input_data_path().Read() ==
          pw::bluetooth::emboss::ScoDataPath::HCI) {
    channel_->UnregisterConnection(handle_);
  }
  active_ = false;
  connection_ = nullptr;
}

}  // namespace bt::sco
