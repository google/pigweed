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

#include <fuchsia/bluetooth/le/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/channel_server.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/gatt2_client_server.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/iso_stream_server.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_connection_handle.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt.h"
#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"

namespace bthost {

class LowEnergyConnectionServer
    : public ServerBase<fuchsia::bluetooth::le::Connection> {
 public:
  // |closed_cb| will be called to signal the invalidation of this connection
  // instance. This can be called in response to the client closing its end of
  // the FIDL channel or when the LL connection is severed. |closed_cb| will be
  // called at most once in response to either of these events. The owner of the
  // LowEnergyConnectionServer instance is expected to destroy it.
  LowEnergyConnectionServer(
      bt::gap::Adapter::WeakPtr adapter,
      bt::gatt::GATT::WeakPtr gatt,
      std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection,
      zx::channel handle,
      fit::callback<void()> closed_cb);

  // Return a reference to the underlying connection ref. Expected to only be
  // used for testing.
  const bt::gap::LowEnergyConnectionHandle* conn() const { return conn_.get(); }

 private:
  void OnClosed();
  void ServeChannel(
      bt::l2cap::Channel::WeakPtr channel,
      fidl::InterfaceRequest<fuchsia::bluetooth::Channel> request);

  // fuchsia::bluetooth::le::Connection overrides:
  void RequestGattClient(
      ::fidl::InterfaceRequest<::fuchsia::bluetooth::gatt2::Client> client)
      override;
  void AcceptCis(
      fuchsia::bluetooth::le::ConnectionAcceptCisRequest parameters) override;
  void GetCodecLocalDelayRange(
      ::fuchsia::bluetooth::le::CodecDelayGetCodecLocalDelayRangeRequest
          CodecDelayGetCodecLocalDelayRangeRequest,
      GetCodecLocalDelayRangeCallback callback) override;
  void ConnectL2cap(
      fuchsia::bluetooth::le::ConnectionConnectL2capRequest request) override;

  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_;
  fit::callback<void()> closed_handler_;
  bt::PeerId peer_id_;
  bt::gap::Adapter::WeakPtr adapter_;
  bt::gatt::GATT::WeakPtr gatt_;
  std::optional<Gatt2ClientServer> gatt_client_server_;

  std::unordered_map<bt::iso::CigCisIdentifier,
                     std::unique_ptr<IsoStreamServer>>
      iso_streams_;
  std::unordered_map<bt::l2cap::Channel::UniqueId,
                     std::unique_ptr<ChannelServer>>
      channel_servers_;

  WeakSelf<LowEnergyConnectionServer> weak_self_;  // Keep last.

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LowEnergyConnectionServer);
};

}  // namespace bthost
