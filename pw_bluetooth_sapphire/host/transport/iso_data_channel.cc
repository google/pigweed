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

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"

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

 private:
  void OnRxPacket(pw::span<const std::byte> buffer);

  CommandChannel* command_channel_ __attribute__((unused));
  pw::bluetooth::Controller* hci_;
  DataBufferInfo buffer_info_;

  // Stores connections registered by RegisterConnection()
  std::unordered_map<hci_spec::ConnectionHandle, WeakPtr<ConnectionInterface>>
      connections_;
};

IsoDataChannelImpl::IsoDataChannelImpl(const DataBufferInfo& buffer_info,
                                       CommandChannel* command_channel,
                                       pw::bluetooth::Controller* hci)
    : command_channel_(command_channel), hci_(hci), buffer_info_(buffer_info) {
  // IsoDataChannel shouldn't be used if the buffer is unavailable (implying the
  // controller doesn't support isochronous channels).
  BT_ASSERT(buffer_info_.IsAvailable());

  hci_->SetReceiveIsoFunction(
      fit::bind_member<&IsoDataChannelImpl::OnRxPacket>(this));
}

void IsoDataChannelImpl::OnRxPacket(pw::span<const std::byte> buffer) {}

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

std::unique_ptr<IsoDataChannel> IsoDataChannel::Create(
    const DataBufferInfo& buffer_info,
    CommandChannel* command_channel,
    pw::bluetooth::Controller* hci) {
  bt_log(DEBUG, "hci", "Creating a new IsoDataChannel");
  return std::make_unique<IsoDataChannelImpl>(
      buffer_info, command_channel, hci);
}

}  // namespace bt::hci
