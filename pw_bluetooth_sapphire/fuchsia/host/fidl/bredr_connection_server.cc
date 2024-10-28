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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/bredr_connection_server.h"

#include <algorithm>
namespace fidlbredr = fuchsia::bluetooth::bredr;
namespace fidlbt = fuchsia::bluetooth;

namespace bthost {

BrEdrConnectionServer::BrEdrConnectionServer(
    fidl::InterfaceRequest<fidlbt::Channel> request,
    bt::l2cap::Channel::WeakPtr channel,
    fit::callback<void()> closed_callback)
    : ServerBase(this, std::move(request)),
      channel_(std::move(channel)),
      closed_cb_(std::move(closed_callback)),
      weak_self_(this) {
  binding()->set_error_handler(
      [this](zx_status_t /*status*/) { OnProtocolClosed(); });
}

BrEdrConnectionServer::~BrEdrConnectionServer() {
  if (state_ != State::kDeactivated) {
    bt_log(TRACE, "fidl", "Deactivating channel %u in dtor", channel_->id());
    Deactivate();
  }
}

void BrEdrConnectionServer::Send(
    std::vector<::fuchsia::bluetooth::Packet> packets, SendCallback callback) {
  for (auto& fidl_packet : packets) {
    std::vector<uint8_t>& packet = fidl_packet.packet;
    if (packet.size() > channel_->max_tx_sdu_size()) {
      bt_log(TRACE,
             "fidl",
             "Dropping %zu bytes for channel %u as max TX SDU is %u ",
             packet.size(),
             channel_->id(),
             channel_->max_tx_sdu_size());
      continue;
    }

    // TODO(fxbug.dev/349653544): Avoid making a copy of `packet`, possibly by
    // making DynamicByteBuffer wrap a std::vector.
    auto buffer =
        std::make_unique<bt::DynamicByteBuffer>(bt::BufferView(packet));
    bool write_success = channel_->Send(std::move(buffer));
    if (!write_success) {
      bt_log(TRACE,
             "fidl",
             "Failed to write %zu bytes to channel %u",
             buffer->size(),
             channel_->id());
    }
  }

  fidlbt::Channel_Send_Response response;
  // NOLINTNEXTLINE(performance-move-const-arg)
  callback(fidlbt::Channel_Send_Result::WithResponse(std::move(response)));
}

void BrEdrConnectionServer::Receive(ReceiveCallback callback) {
  if (receive_cb_) {
    binding()->Close(ZX_ERR_BAD_STATE);
    OnProtocolClosed();
    return;
  }
  receive_cb_ = std::move(callback);
  ServiceReceiveQueue();
}

void BrEdrConnectionServer::WatchChannelParameters(
    WatchChannelParametersCallback callback) {
  PW_CHECK(
      !pending_watch_channel_parameters_.has_value(),
      "WatchChannelParameters called while there was already a pending call.");
  pending_watch_channel_parameters_ = std::move(callback);
}

void BrEdrConnectionServer::handle_unknown_method(uint64_t ordinal,
                                                  bool method_has_response) {
  bt_log(WARN,
         "fidl",
         "BrEdrConnectionServer: received unknown method (ordinal: %lu)",
         ordinal);
}

bool BrEdrConnectionServer::Activate() {
  PW_CHECK(state_ == State::kActivating);

  WeakPtr self = weak_self_.GetWeakPtr();
  bt::l2cap::ChannelId channel_id = channel_->id();
  bool activate_success = channel_->Activate(
      [self, channel_id](bt::ByteBufferPtr rx_data) {
        // Note: this lambda _may_ be invoked immediately for buffered packets.
        if (self.is_alive()) {
          self->OnChannelDataReceived(std::move(rx_data));
        } else {
          bt_log(
              TRACE,
              "fidl",
              "Ignoring data received on destroyed server (channel_id=%#.4x)",
              channel_id);
        }
      },
      [self, channel_id] {
        if (self.is_alive()) {
          self->OnChannelClosed();
        } else {
          bt_log(
              TRACE,
              "fidl",
              "Ignoring channel closure on destroyed server (channel_id=%#.4x)",
              channel_id);
        }
      });
  if (!activate_success) {
    return false;
  }

  state_ = State::kActivated;
  return true;
}

void BrEdrConnectionServer::Deactivate() {
  PW_CHECK(state_ != State::kDeactivated);
  state_ = State::kDeactivating;

  if (!receive_queue_.empty()) {
    bt_log(DEBUG,
           "fidl",
           "Dropping %zu packets from channel %u due to channel closure",
           receive_queue_.size(),
           channel_->id());
    receive_queue_.clear();
  }
  channel_->Deactivate();
  binding()->Close(ZX_ERR_CONNECTION_RESET);

  state_ = State::kDeactivated;
}

void BrEdrConnectionServer::OnChannelDataReceived(bt::ByteBufferPtr rx_data) {
  // Note: kActivating is deliberately permitted, as ChannelImpl::Activate()
  // will synchronously deliver any queued frames.
  PW_CHECK(state_ != State::kDeactivated);
  if (state_ == State::kDeactivating) {
    bt_log(DEBUG,
           "fidl",
           "Ignoring %s for channel %u while deactivating",
           __func__,
           channel_->id());
    return;
  }

  PW_CHECK(rx_data);
  if (rx_data->size() == 0) {
    bt_log(
        DEBUG, "fidl", "Ignoring empty rx_data for channel %u", channel_->id());
    return;
  }

  PW_CHECK(receive_queue_.size() <= receive_queue_max_frames_);
  // On a full queue, we drop the oldest element, on the theory that newer data
  // is more useful. This should be true, e.g., for real-time applications such
  // as voice calls. In the future, we may want to make the drop-head vs.
  // drop-tail choice configurable.
  if (receive_queue_.size() == receive_queue_max_frames_) {
    // TODO(fxbug.dev/42082614): Add a metric for number of dropped frames.
    receive_queue_.pop_front();
  }

  receive_queue_.push_back(std::move(rx_data));
  ServiceReceiveQueue();
}

void BrEdrConnectionServer::OnChannelClosed() {
  if (state_ == State::kDeactivating) {
    bt_log(DEBUG,
           "fidl",
           "Ignoring %s for channel %u while deactivating",
           __func__,
           channel_->id());
    return;
  }
  PW_CHECK(state_ == State::kActivated);
  DeactivateAndRequestDestruction();
}

void BrEdrConnectionServer::OnProtocolClosed() {
  DeactivateAndRequestDestruction();
}

void BrEdrConnectionServer::DeactivateAndRequestDestruction() {
  Deactivate();
  // closed_cb_ is expected to destroy `this`, so move the callback first.
  auto closed_cb = std::move(closed_cb_);
  closed_cb();
}

void BrEdrConnectionServer::ServiceReceiveQueue() {
  if (!receive_cb_ || receive_queue_.empty()) {
    return;
  }
  std::vector<uint8_t> buffer = receive_queue_.front()->ToVector();
  receive_queue_.pop_front();

  ::fuchsia::bluetooth::Channel_Receive_Response response(
      {fidlbt::Packet{std::move(buffer)}});
  receive_cb_(
      fidlbt::Channel_Receive_Result::WithResponse(std::move(response)));
  receive_cb_ = nullptr;
}

std::unique_ptr<BrEdrConnectionServer> BrEdrConnectionServer::Create(
    fidl::InterfaceRequest<fidlbt::Channel> request,
    bt::l2cap::Channel::WeakPtr channel,
    fit::callback<void()> closed_callback) {
  if (!channel.is_alive()) {
    return nullptr;
  }

  std::unique_ptr<BrEdrConnectionServer> server(new BrEdrConnectionServer(
      std::move(request), std::move(channel), std::move(closed_callback)));

  if (!server->Activate()) {
    return nullptr;
  }
  return server;
}
}  // namespace bthost
