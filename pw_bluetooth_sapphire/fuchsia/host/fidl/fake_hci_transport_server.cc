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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/fake_hci_transport_server.h"

#include "gtest/gtest.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace fhbt = ::fuchsia_hardware_bluetooth;

namespace bt::fidl::testing {

FakeHciTransportServer::FakeHciTransportServer(
    ::fidl::ServerEnd<fhbt::HciTransport> server_end,
    async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher),
      binding_(::fidl::BindServer(
          dispatcher_,
          std::move(server_end),
          this,
          std::mem_fn(&FakeHciTransportServer::OnUnbound))) {}

zx_status_t FakeHciTransportServer::SendEvent(const BufferView& event) {
  ::fit::result<::fidl::OneWayError> result =
      ::fidl::SendEvent(binding_)->OnReceive(
          ::fuchsia_hardware_bluetooth::ReceivedPacket::WithEvent(
              event.ToVector()));
  return result.is_ok() ? ZX_OK : result.error_value().status();
}

zx_status_t FakeHciTransportServer::SendAcl(const BufferView& buffer) {
  ::fit::result<::fidl::OneWayError> result =
      ::fidl::SendEvent(binding_)->OnReceive(
          ::fuchsia_hardware_bluetooth::ReceivedPacket::WithAcl(
              buffer.ToVector()));
  return result.is_ok() ? ZX_OK : result.error_value().status();
}

zx_status_t FakeHciTransportServer::SendSco(const BufferView& buffer) {
  if (!sco_server_) {
    return ZX_ERR_UNAVAILABLE;
  }
  return sco_server_->Send(buffer);
}

zx_status_t FakeHciTransportServer::SendIso(const BufferView& buffer) {
  ::fit::result<::fidl::OneWayError> result =
      ::fidl::SendEvent(binding_)->OnReceive(
          ::fuchsia_hardware_bluetooth::ReceivedPacket::WithIso(
              buffer.ToVector()));
  return result.is_ok() ? ZX_OK : result.error_value().status();
}

bool FakeHciTransportServer::UnbindSco() {
  if (!sco_server_) {
    return false;
  }
  sco_server_->Unbind();
  sco_server_.reset();
  return true;
}

FakeHciTransportServer::ScoConnectionServer::ScoConnectionServer(
    ::fidl::ServerEnd<fuchsia_hardware_bluetooth::ScoConnection> server_end,
    async_dispatcher_t* dispatcher,
    FakeHciTransportServer* hci_server)
    : hci_server_(hci_server),
      binding_(::fidl::BindServer(dispatcher, std::move(server_end), this)) {}

zx_status_t FakeHciTransportServer::ScoConnectionServer::Send(
    const BufferView& buffer) {
  ::fuchsia_hardware_bluetooth::ScoPacket packet(buffer.ToVector());
  fit::result<::fidl::OneWayError> result =
      ::fidl::SendEvent(binding_)->OnReceive(packet);
  return result.is_ok() ? ZX_OK : result.error_value().status();
}

void FakeHciTransportServer::ScoConnectionServer::Unbind() {
  binding_.Unbind();
}

void FakeHciTransportServer::ScoConnectionServer::Send(
    SendRequest& request, SendCompleter::Sync& completer) {
  hci_server_->sco_packets_received_.emplace_back(BufferView(request.packet()));
  completer.Reply();
}

void FakeHciTransportServer::ScoConnectionServer::AckReceive(
    AckReceiveCompleter::Sync& completer) {
  hci_server_->sco_ack_receive_count_++;
}

void FakeHciTransportServer::ScoConnectionServer::Stop(
    StopCompleter::Sync& completer) {
  binding_.Close(ZX_ERR_CANCELED);
  if (hci_server_->reset_sco_cb_) {
    hci_server_->reset_sco_cb_();
  }
  hci_server_->sco_server_.reset();
}

void FakeHciTransportServer::ScoConnectionServer::handle_unknown_method(
    ::fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::ScoConnection>
        metadata,
    ::fidl::UnknownMethodCompleter::Sync& completer) {
  FAIL();
}

void FakeHciTransportServer::ScoConnectionServer::OnUnbound(
    ::fidl::UnbindInfo info,
    ::fidl::ServerEnd<fuchsia_hardware_bluetooth::ScoConnection> server_end) {
  if (info.is_user_initiated()) {
    return;
  }
  if (info.is_peer_closed()) {
    FAIL() << "OnUnbound() called before Stop()";
  }

  hci_server_->sco_server_.reset();
}

void FakeHciTransportServer::Send(SendRequest& request,
                                  SendCompleter::Sync& completer) {
  switch (request.Which()) {
    case SendRequest::Tag::kIso:
      iso_packets_received_.emplace_back(BufferView(request.iso().value()));
      break;
    case SendRequest::Tag::kAcl:
      acl_packets_received_.emplace_back(BufferView(request.acl().value()));
      break;
    case SendRequest::Tag::kCommand:
      commands_received_.emplace_back(BufferView(request.command().value()));
      break;
    default:
      FAIL() << "Send(): unknown packet type";
  }
  completer.Reply();
}

void FakeHciTransportServer::AckReceive(AckReceiveCompleter::Sync& completer) {
  ack_receive_count_++;
}

void FakeHciTransportServer::ConfigureSco(
    ConfigureScoRequest& request, ConfigureScoCompleter::Sync& completer) {
  if (!request.connection() || !request.coding_format() ||
      !request.sample_rate() || !request.encoding()) {
    return;
  }

  ASSERT_FALSE(sco_server_);
  sco_server_.emplace(
      std::move(request.connection().value()), dispatcher_, this);

  if (check_configure_sco_) {
    check_configure_sco_(
        *request.coding_format(), *request.encoding(), *request.sample_rate());
  }
}

void FakeHciTransportServer::handle_unknown_method(
    ::fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::HciTransport>
        metadata,
    ::fidl::UnknownMethodCompleter::Sync& completer) {
  FAIL();
}

void FakeHciTransportServer::OnUnbound(
    ::fidl::UnbindInfo info,
    ::fidl::ServerEnd<fuchsia_hardware_bluetooth::HciTransport> server_end) {
  bound_ = false;
}

}  // namespace bt::fidl::testing
