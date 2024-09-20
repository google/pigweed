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

#include "emulated_peer.h"

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/helpers.h"

namespace fbt = fuchsia_bluetooth;

namespace bt_hci_virtual {
namespace {

bt::DeviceAddress::Type LeAddressTypeFromFidl(fbt::AddressType type) {
  return (type == fbt::AddressType::kRandom)
             ? bt::DeviceAddress::Type::kLERandom
             : bt::DeviceAddress::Type::kLEPublic;
}

bt::DeviceAddress LeAddressFromFidl(const fbt::Address& address) {
  return bt::DeviceAddress(LeAddressTypeFromFidl(address.type()),
                           address.bytes());
}

pw::bluetooth::emboss::ConnectionRole ConnectionRoleFromFidl(
    fbt::ConnectionRole role) {
  switch (role) {
    case fbt::ConnectionRole::kLeader:
      return pw::bluetooth::emboss::ConnectionRole::CENTRAL;
    case fbt::ConnectionRole::kFollower:
      [[fallthrough]];
    default:
      break;
  }
  return pw::bluetooth::emboss::ConnectionRole::PERIPHERAL;
}

}  // namespace

// static
EmulatedPeer::Result EmulatedPeer::NewLowEnergy(
    fuchsia_hardware_bluetooth::PeerParameters parameters,
    bt::testing::FakeController* fake_controller,
    async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(parameters.channel().has_value());
  ZX_DEBUG_ASSERT(fake_controller);

  if (!parameters.address().has_value()) {
    bt_log(ERROR, "virtual", "A fake peer address is mandatory!\n");
    return fpromise::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kParametersInvalid);
  }

  auto address = LeAddressFromFidl(parameters.address().value());
  bool connectable =
      parameters.connectable().has_value() && parameters.connectable().value();

  // TODO(armansito): We should consider splitting bt::testing::FakePeer into
  // separate types for BR/EDR and LE transport emulation logic.
  auto peer = std::make_unique<bt::testing::FakePeer>(
      address, fake_controller->pw_dispatcher(), connectable);

  if (!fake_controller->AddPeer(std::move(peer))) {
    bt_log(ERROR,
           "virtual",
           "A fake LE peer with given address already exists: %s\n",
           address.ToString().c_str());
    return fpromise::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kAddressRepeated);
  }

  return fpromise::ok(std::unique_ptr<EmulatedPeer>(
      new EmulatedPeer(address,
                       std::move(parameters.channel().value()),
                       fake_controller,
                       dispatcher)));
}

// static
EmulatedPeer::Result EmulatedPeer::NewBredr(
    fuchsia_hardware_bluetooth::PeerParameters parameters,
    bt::testing::FakeController* fake_controller,
    async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(parameters.channel().has_value());
  ZX_DEBUG_ASSERT(fake_controller);

  if (!parameters.address().has_value()) {
    bt_log(ERROR, "virtual", "A fake peer address is mandatory!\n");
    return fpromise::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kParametersInvalid);
  }

  auto address = bt::DeviceAddress(bt::DeviceAddress::Type::kBREDR,
                                   parameters.address()->bytes());
  bool connectable =
      parameters.connectable().has_value() && parameters.connectable().value();

  // TODO(armansito): We should consider splitting bt::testing::FakePeer into
  // separate types for BR/EDR and LE transport emulation logic.
  auto peer = std::make_unique<bt::testing::FakePeer>(
      address, fake_controller->pw_dispatcher(), connectable, false);

  if (!fake_controller->AddPeer(std::move(peer))) {
    bt_log(ERROR,
           "virtual",
           "A fake BR/EDR peer with given address already exists: %s\n",
           address.ToString().c_str());
    return fpromise::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kAddressRepeated);
  }

  return fpromise::ok(std::unique_ptr<EmulatedPeer>(
      new EmulatedPeer(address,
                       std::move(parameters.channel().value()),
                       fake_controller,
                       dispatcher)));
}

EmulatedPeer::EmulatedPeer(
    bt::DeviceAddress address,
    fidl::ServerEnd<fuchsia_hardware_bluetooth::Peer> request,
    bt::testing::FakeController* fake_controller,
    async_dispatcher_t* dispatcher)
    : address_(address),
      fake_controller_(fake_controller),
      binding_(dispatcher,
               std::move(request),
               this,
               std::mem_fn(&EmulatedPeer::OnChannelClosed)) {
  ZX_DEBUG_ASSERT(fake_controller_);
}

EmulatedPeer::~EmulatedPeer() { CleanUp(); }

void EmulatedPeer::AssignConnectionStatus(
    AssignConnectionStatusRequest& request,
    AssignConnectionStatusCompleter::Sync& completer) {
  bt_log(TRACE, "virtual", "EmulatedPeer.AssignConnectionStatus\n");

  auto peer = fake_controller_->FindPeer(address_);
  if (peer) {
    peer->set_connect_response(
        bthost::fidl_helpers::FidlHciErrorToStatusCode(request.status()));
  }

  completer.Reply();
}

void EmulatedPeer::EmulateLeConnectionComplete(
    EmulateLeConnectionCompleteRequest& request,
    EmulateLeConnectionCompleteCompleter::Sync& completer) {
  bt_log(TRACE, "virtual", "EmulatedPeer.EmulateLeConnectionComplete\n");
  fake_controller_->ConnectLowEnergy(address_,
                                     ConnectionRoleFromFidl(request.role()));
}

void EmulatedPeer::EmulateDisconnectionComplete(
    EmulateDisconnectionCompleteCompleter::Sync& completer) {
  bt_log(TRACE, "virtual", "EmulatedPeer.EmulateDisconnectionComplete\n");
  fake_controller_->Disconnect(address_);
}

void EmulatedPeer::WatchConnectionStates(
    WatchConnectionStatesCompleter::Sync& completer) {
  bt_log(TRACE, "virtual", "EmulatedPeer.WatchConnectionState\n");

  {
    std::lock_guard<std::mutex> lock(connection_states_lock_);
    connection_states_completers_.emplace(completer.ToAsync());
  }
  MaybeUpdateConnectionStates();
}

void EmulatedPeer::SetDeviceClass(SetDeviceClassRequest& request,
                                  SetDeviceClassCompleter::Sync& completer) {
  auto peer = fake_controller_->FindPeer(address_);
  if (!peer) {
    bt_log(WARN,
           "virtual",
           "Peer with address %s not found",
           address_.ToString().c_str());
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    return;
  }
  if (!peer->supports_bredr()) {
    bt_log(WARN, "virtual", "Expected fake BR/EDR peer");
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    return;
  }

  peer->set_class_of_device(bt::DeviceClass(request.value()));

  completer.Reply();
}

void EmulatedPeer::SetServiceDefinitions(
    SetServiceDefinitionsRequest& request,
    SetServiceDefinitionsCompleter::Sync& completer) {
  auto peer = fake_controller_->FindPeer(address_);
  if (!peer) {
    bt_log(WARN,
           "virtual",
           "Peer with address %s not found",
           address_.ToString().c_str());
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    return;
  }
  if (!peer->supports_bredr()) {
    bt_log(WARN, "virtual", "Expected fake BR/EDR peer");
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    return;
  }

  std::vector<bt::sdp::ServiceRecord> recs;
  for (const auto& defn : request.service_definitions()) {
    auto rec = bthost::fidl_helpers::ServiceDefinitionToServiceRecord(defn);
    if (rec.is_ok()) {
      recs.emplace_back(std::move(rec.value()));
    }
  }
  bt::l2cap::ChannelParameters params;
  auto NopConnectCallback = [](auto /*channel*/, const bt::sdp::DataElement&) {
  };
  peer->sdp_server()->server()->RegisterService(
      std::move(recs), params, NopConnectCallback);

  completer.Reply();
}

void EmulatedPeer::SetLeAdvertisement(
    SetLeAdvertisementRequest& request,
    SetLeAdvertisementCompleter::Sync& completer) {
  auto peer = fake_controller_->FindPeer(address_);
  if (!peer) {
    bt_log(WARN,
           "virtual",
           "Peer with address %s not found",
           address_.ToString().c_str());
    completer.Reply(fit::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kParametersInvalid));
    return;
  }
  if (!peer->supports_le()) {
    bt_log(WARN, "virtual", "Expected fake LE peer");
    completer.Reply(fit::error(
        fuchsia_hardware_bluetooth::EmulatorPeerError::kParametersInvalid));
    return;
  }

  if (request.le_address().has_value()) {
    bt::DeviceAddress le_address =
        LeAddressFromFidl(request.le_address().value());
    auto le_peer = fake_controller_->FindPeer(le_address);
    if (le_peer != peer) {
      bt_log(ERROR,
             "virtual",
             "A fake LE peer with given address already exists: %s\n",
             le_address.ToString().c_str());
      completer.Reply(fit::error(
          fuchsia_hardware_bluetooth::EmulatorPeerError::kAddressRepeated));
      return;
    }
    peer->set_le_advertising_address(le_address);
  }

  if (request.advertisement().has_value() &&
      request.advertisement().value().data().has_value()) {
    peer->set_advertising_data(
        bt::BufferView(request.advertisement().value().data().value()));
  }

  if (request.scan_response().has_value() &&
      request.scan_response().value().data().has_value()) {
    bt::BufferView scan_rsp =
        bt::BufferView(request.scan_response().value().data().value());
    peer->set_scannable(true);
    peer->set_scan_response(scan_rsp);
  }
  completer.Reply(fit::success());
}

void EmulatedPeer::handle_unknown_method(
    fidl::UnknownMethodMetadata<fuchsia_hardware_bluetooth::Peer> metadata,
    fidl::UnknownMethodCompleter::Sync& completer) {
  bt_log(WARN,
         "virtual",
         "Unknown method in Peer request, closing with ZX_ERR_NOT_SUPPORTED");
  completer.Close(ZX_ERR_NOT_SUPPORTED);
}

void EmulatedPeer::UpdateConnectionState(bool connected) {
  fuchsia_hardware_bluetooth::ConnectionState state =
      connected ? fuchsia_hardware_bluetooth::ConnectionState::kConnected
                : fuchsia_hardware_bluetooth::ConnectionState::kDisconnected;

  connection_states_.emplace_back(state);
  MaybeUpdateConnectionStates();
}

void EmulatedPeer::MaybeUpdateConnectionStates() {
  std::lock_guard<std::mutex> lock(connection_states_lock_);
  if (connection_states_.empty() || connection_states_completers_.empty()) {
    return;
  }
  while (!connection_states_completers_.empty()) {
    connection_states_completers_.front().Reply(connection_states_);
    connection_states_completers_.pop();
  }
  connection_states_.clear();
}

void EmulatedPeer::OnChannelClosed(fidl::UnbindInfo info) {
  bt_log(TRACE, "virtual", "EmulatedPeer channel closed\n");
  NotifyChannelClosed();
}

void EmulatedPeer::CleanUp() { fake_controller_->RemovePeer(address_); }

void EmulatedPeer::NotifyChannelClosed() {
  if (closed_callback_) {
    closed_callback_();
  }
}

}  // namespace bt_hci_virtual
