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

#include <fidl/fuchsia.hardware.bluetooth/cpp/fidl.h>
#include <lib/async/dispatcher.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"

namespace bt::fidl::testing {

class FakeHciTransportServer final
    : public ::fidl::Server<fuchsia_hardware_bluetooth::HciTransport> {
 public:
  FakeHciTransportServer(
      ::fidl::ServerEnd<fuchsia_hardware_bluetooth::HciTransport> server_end,
      async_dispatcher_t* dispatcher);

  void Unbind() {
    binding_.Unbind();
    bound_ = false;
  }

  bool bound() const { return bound_; }

  zx_status_t SendEvent(const BufferView& event);
  zx_status_t SendAcl(const BufferView& buffer);
  zx_status_t SendSco(const BufferView& buffer);
  zx_status_t SendIso(const BufferView& buffer);

  // Returns true if the SCO server was successfully unbound.
  bool UnbindSco();

  size_t acks_received() const { return ack_receive_count_; }
  size_t sco_acks_received() const { return sco_ack_receive_count_; }

  const std::vector<bt::DynamicByteBuffer>& commands_received() const {
    return commands_received_;
  }
  const std::vector<bt::DynamicByteBuffer>& acl_packets_received() const {
    return acl_packets_received_;
  }
  const std::vector<bt::DynamicByteBuffer>& sco_packets_received() const {
    return sco_packets_received_;
  }
  const std::vector<bt::DynamicByteBuffer>& iso_packets_received() const {
    return iso_packets_received_;
  }

  // Use custom |ConfigureScoTestCallback| to manually verify configuration
  // fields from tests
  using ConfigureScoTestCallback =
      fit::function<void(fuchsia_hardware_bluetooth::ScoCodingFormat,
                         fuchsia_hardware_bluetooth::ScoEncoding,
                         fuchsia_hardware_bluetooth::ScoSampleRate)>;
  void set_check_configure_sco(ConfigureScoTestCallback callback) {
    check_configure_sco_ = std::move(callback);
  }

  // Uee custom |ResetScoTestCallback| to manually perform reset actions from
  // tests
  using ResetScoTestCallback = fit::function<void()>;
  void set_reset_sco_callback(ResetScoTestCallback callback) {
    reset_sco_cb_ = std::move(callback);
  }

 private:
  class ScoConnectionServer
      : public ::fidl::Server<fuchsia_hardware_bluetooth::ScoConnection> {
   public:
    ScoConnectionServer(
        ::fidl::ServerEnd<fuchsia_hardware_bluetooth::ScoConnection> server_end,
        async_dispatcher_t* dispatcher,
        FakeHciTransportServer* hci_server);

    zx_status_t Send(const BufferView& buffer);

    void Unbind();

   private:
    // Server<ScoConnection> overrides:
    void Send(SendRequest& request, SendCompleter::Sync& completer) override;
    void AckReceive(AckReceiveCompleter::Sync& completer) override;
    void Stop(StopCompleter::Sync& completer) override;
    void handle_unknown_method(
        ::fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::ScoConnection>
            metadata,
        ::fidl::UnknownMethodCompleter::Sync& completer) override;

    void OnUnbound(::fidl::UnbindInfo info,
                   ::fidl::ServerEnd<fuchsia_hardware_bluetooth::ScoConnection>
                       server_end);

    FakeHciTransportServer* hci_server_;
    ::fidl::ServerBindingRef<fuchsia_hardware_bluetooth::ScoConnection>
        binding_;
  };

  // Server<HciTransport> overrides:
  void Send(SendRequest& request, SendCompleter::Sync& completer) override;
  void AckReceive(AckReceiveCompleter::Sync& completer) override;
  void ConfigureSco(ConfigureScoRequest& request,
                    ConfigureScoCompleter::Sync& completer) override;
  void handle_unknown_method(
      ::fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::HciTransport>
          metadata,
      ::fidl::UnknownMethodCompleter::Sync& completer) override;

  void OnUnbound(
      ::fidl::UnbindInfo info,
      ::fidl::ServerEnd<fuchsia_hardware_bluetooth::HciTransport> server_end);

  std::vector<bt::DynamicByteBuffer> commands_received_;

  std::vector<bt::DynamicByteBuffer> acl_packets_received_;

  std::vector<bt::DynamicByteBuffer> sco_packets_received_;
  ConfigureScoTestCallback check_configure_sco_;
  ResetScoTestCallback reset_sco_cb_;

  std::vector<bt::DynamicByteBuffer> iso_packets_received_;

  std::optional<ScoConnectionServer> sco_server_;

  size_t ack_receive_count_ = 0u;
  size_t sco_ack_receive_count_ = 0u;

  async_dispatcher_t* dispatcher_;
  bool bound_ = true;
  ::fidl::ServerBindingRef<fuchsia_hardware_bluetooth::HciTransport> binding_;
};

}  // namespace bt::fidl::testing
