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

#include "pw_bluetooth_sapphire/internal/host/transport/iso_data_channel.h"

#include <pw_assert/check.h>
#include <pw_bluetooth/hci_data.emb.h>

namespace bt::hci {

class IsoDataChannelImpl final : public IsoDataChannel {
 public:
  IsoDataChannelImpl(const DataBufferInfo& buffer_info,
                     CommandChannel* command_channel,
                     pw::bluetooth::Controller* hci);

  // IsoDataChannel overrides:
  virtual bool RegisterConnection(
      hci_spec::ConnectionHandle handle,
      WeakPtr<ConnectionInterface> connection) override;
  virtual bool UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  virtual void SendData(DynamicByteBuffer packet) override;
  virtual const DataBufferInfo& buffer_info() const override {
    return buffer_info_;
  }

 private:
  void OnRxPacket(pw::span<const std::byte> buffer);
  void TrySendPackets();

  CommandChannel* command_channel_ __attribute__((unused));
  pw::bluetooth::Controller* hci_;
  DataBufferInfo buffer_info_;

  size_t available_buffers_;

  // Stores connections registered by RegisterConnection()
  std::unordered_map<hci_spec::ConnectionHandle, WeakPtr<ConnectionInterface>>
      connections_;

  // Stores queued packets ready for sending
  std::deque<DynamicByteBuffer> outbound_queue_;
};

IsoDataChannelImpl::IsoDataChannelImpl(const DataBufferInfo& buffer_info,
                                       CommandChannel* command_channel,
                                       pw::bluetooth::Controller* hci)
    : command_channel_(command_channel),
      hci_(hci),
      buffer_info_(buffer_info),
      available_buffers_(buffer_info.max_num_packets()) {
  // IsoDataChannel shouldn't be used if the buffer is unavailable (implying the
  // controller doesn't support isochronous channels).
  PW_CHECK(buffer_info_.IsAvailable());

  hci_->SetReceiveIsoFunction(
      fit::bind_member<&IsoDataChannelImpl::OnRxPacket>(this));
}

void IsoDataChannelImpl::OnRxPacket(pw::span<const std::byte> buffer) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  auto header_view =
      pw::bluetooth::emboss::MakeIsoDataFrameHeaderView(data, buffer.size());

  hci_spec::ConnectionHandle handle = header_view.connection_handle().Read();
  if (connections_.count(handle) == 0) {
    bt_log(WARN,
           "hci",
           "ISO data packet received for unrecognized handle 0x%x",
           handle);
    return;
  }
  WeakPtr<ConnectionInterface> stream = connections_[handle];
  PW_CHECK(stream.is_alive());
  stream->ReceiveInboundPacket(buffer);
}

bool IsoDataChannelImpl::RegisterConnection(
    hci_spec::ConnectionHandle handle,
    WeakPtr<ConnectionInterface> connection) {
  bt_log(INFO, "hci", "registering ISO connection for handle 0x%x", handle);
  if (connections_.count(handle) != 0) {
    bt_log(ERROR,
           "hci",
           "Attempt to re-register connection for handle 0x%x",
           handle);
    return false;
  }
  connections_[handle] = std::move(connection);
  return true;
}

bool IsoDataChannelImpl::UnregisterConnection(
    hci_spec::ConnectionHandle handle) {
  bt_log(INFO, "hci", "unregistering ISO connection for handle 0x%x", handle);
  if (connections_.count(handle) == 0) {
    bt_log(ERROR,
           "hci",
           "Attempt to de-register connection for unrecognized handle 0x%x",
           handle);
    return false;
  }
  connections_.erase(handle);
  return true;
}

void IsoDataChannelImpl::SendData(DynamicByteBuffer packet) {
  PW_CHECK(packet.size() <= buffer_info_.max_data_length(),
           "Unfragmented packet received, cannot send.");
  outbound_queue_.push_back(std::move(packet));
  TrySendPackets();
}

void IsoDataChannelImpl::TrySendPackets() {
  while (available_buffers_ && !outbound_queue_.empty()) {
    hci_->SendIsoData(outbound_queue_.front().view().subspan());
    outbound_queue_.pop_front();
    --available_buffers_;
  }
}

std::unique_ptr<IsoDataChannel> IsoDataChannel::Create(
    const DataBufferInfo& buffer_info,
    CommandChannel* command_channel,
    pw::bluetooth::Controller* hci) {
  bt_log(DEBUG, "hci", "Creating a new IsoDataChannel");
  return std::make_unique<IsoDataChannelImpl>(
      buffer_info, command_channel, hci);
}

}  // namespace bt::hci
