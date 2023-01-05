// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <lib/async/time.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"

namespace bt::gap::internal {

namespace {

constexpr const char* kInspectPeerIdPropertyName = "peer_id";
constexpr const char* kInspectPeerAddressPropertyName = "peer_address";
constexpr const char* kInspectRefsPropertyName = "ref_count";

// Connection parameters to use when the peer's preferred connection parameters are not known.
static const hci_spec::LEPreferredConnectionParameters kDefaultPreferredConnectionParameters(
    hci_spec::defaults::kLEConnectionIntervalMin, hci_spec::defaults::kLEConnectionIntervalMax,
    /*max_latency=*/0, hci_spec::defaults::kLESupervisionTimeout);

}  // namespace

std::unique_ptr<LowEnergyConnection> LowEnergyConnection::Create(
    Peer::WeakPtr peer, std::unique_ptr<hci::LowEnergyConnection> link,
    LowEnergyConnectionOptions connection_options, PeerDisconnectCallback peer_disconnect_cb,
    ErrorCallback error_cb, WeakSelf<LowEnergyConnectionManager>::WeakPtr conn_mgr,
    l2cap::ChannelManager* l2cap, gatt::GATT::WeakPtr gatt,
    hci::CommandChannel::WeakPtr cmd_channel) {
  // Catch any errors/disconnects during connection initialization so that they are reported by
  // returning a nullptr. This is less error-prone than calling the user's callbacks during
  // initialization.
  bool error = false;
  auto peer_disconnect_cb_temp = [&error](auto) { error = true; };
  auto error_cb_temp = [&error] { error = true; };
  std::unique_ptr<LowEnergyConnection> connection(
      new LowEnergyConnection(std::move(peer), std::move(link), connection_options,
                              std::move(peer_disconnect_cb_temp), std::move(error_cb_temp),
                              std::move(conn_mgr), l2cap, std::move(gatt), std::move(cmd_channel)));

  // This looks strange, but it is possible for InitializeFixedChannels() to trigger an error and
  // still return true, so |error| can change between the first and last check.
  if (error || !connection->InitializeFixedChannels() || error) {
    return nullptr;
  }

  // Now it is safe to set the user's callbacks, as no more errors/disconnects can be signaled
  // before returning.
  connection->set_peer_disconnect_callback(std::move(peer_disconnect_cb));
  connection->set_error_callback(std::move(error_cb));
  return connection;
}

LowEnergyConnection::LowEnergyConnection(Peer::WeakPtr peer,
                                         std::unique_ptr<hci::LowEnergyConnection> link,
                                         LowEnergyConnectionOptions connection_options,
                                         PeerDisconnectCallback peer_disconnect_cb,
                                         ErrorCallback error_cb,
                                         WeakSelf<LowEnergyConnectionManager>::WeakPtr conn_mgr,
                                         l2cap::ChannelManager* l2cap, gatt::GATT::WeakPtr gatt,
                                         hci::CommandChannel::WeakPtr cmd_channel)
    : peer_(std::move(peer)),
      link_(std::move(link)),
      connection_options_(connection_options),
      conn_mgr_(std::move(conn_mgr)),
      l2cap_(l2cap),
      gatt_(std::move(gatt)),
      cmd_(std::move(cmd_channel)),
      peer_disconnect_callback_(std::move(peer_disconnect_cb)),
      error_callback_(std::move(error_cb)),
      refs_(/*convert=*/[](const auto& refs) { return refs.size(); }),
      weak_self_(this),
      weak_delegate_(this) {
  BT_ASSERT(peer_.is_alive());
  BT_ASSERT(link_);
  BT_ASSERT(conn_mgr_.is_alive());
  BT_ASSERT(gatt_.is_alive());
  BT_ASSERT(cmd_.is_alive());
  BT_ASSERT(peer_disconnect_callback_);
  BT_ASSERT(error_callback_);

  link_->set_peer_disconnect_callback(
      [this](auto, auto reason) { peer_disconnect_callback_(reason); });

  RegisterEventHandlers();
  StartConnectionPauseTimeout();
}

LowEnergyConnection::~LowEnergyConnection() {
  cmd_->RemoveEventHandler(conn_update_cmpl_handler_id_);

  // Unregister this link from the GATT profile and the L2CAP plane. This
  // invalidates all L2CAP channels that are associated with this link.
  gatt_->RemoveConnection(peer_id());
  l2cap_->RemoveConnection(link_->handle());

  // Notify all active references that the link is gone. This will
  // synchronously notify all refs.
  CloseRefs();
}

std::unique_ptr<bt::gap::LowEnergyConnectionHandle> LowEnergyConnection::AddRef() {
  auto self = GetWeakPtr();
  auto release_cb = [self](LowEnergyConnectionHandle* handle) {
    if (self.is_alive()) {
      self->conn_mgr_->ReleaseReference(handle);
    }
  };
  auto bondable_cb = [self] {
    BT_ASSERT(self.is_alive());
    return self->bondable_mode();
  };
  auto security_cb = [self] {
    BT_ASSERT(self.is_alive());
    return self->security();
  };
  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_ref(new LowEnergyConnectionHandle(
      peer_id(), handle(), std::move(release_cb), std::move(bondable_cb), std::move(security_cb)));
  BT_ASSERT(conn_ref);

  refs_.Mutable()->insert(conn_ref.get());

  bt_log(DEBUG, "gap-le", "added ref (peer: %s, handle %#.4x, count: %lu)", bt_str(peer_id()),
         handle(), ref_count());

  return conn_ref;
}

void LowEnergyConnection::DropRef(LowEnergyConnectionHandle* ref) {
  BT_DEBUG_ASSERT(ref);

  size_t res = refs_.Mutable()->erase(ref);
  BT_ASSERT_MSG(res == 1u, "DropRef called with wrong connection reference");
  bt_log(DEBUG, "gap-le", "dropped ref (peer: %s, handle: %#.4x, count: %lu)", bt_str(peer_id()),
         handle(), ref_count());
}

// Registers this connection with L2CAP and initializes the fixed channel
// protocols.
[[nodiscard]] bool LowEnergyConnection::InitializeFixedChannels() {
  auto self = GetWeakPtr();
  // Ensure error_callback_ is only called once if link_error_cb is called multiple times.
  auto link_error_cb = [self]() {
    if (self.is_alive() && self->error_callback_) {
      self->error_callback_();
    }
  };
  auto update_conn_params_cb = [self](auto params) {
    if (self.is_alive()) {
      self->OnNewLEConnectionParams(params);
    }
  };
  auto security_upgrade_cb = [self](auto handle, auto level, auto cb) {
    if (!self.is_alive()) {
      return;
    }

    bt_log(
        INFO, "gap-le",
        "received security upgrade request on L2CAP channel (level: %s, peer: %s, handle: %#.4x)",
        sm::LevelToString(level), bt_str(self->peer_id()), handle);
    BT_ASSERT(self->handle() == handle);
    self->OnSecurityRequest(level, std::move(cb));
  };
  l2cap::ChannelManager::LEFixedChannels fixed_channels =
      l2cap_->AddLEConnection(link_->handle(), link_->role(), std::move(link_error_cb),
                              update_conn_params_cb, security_upgrade_cb);

  return OnL2capFixedChannelsOpened(std::move(fixed_channels.att), std::move(fixed_channels.smp),
                                    connection_options_);
}

// Used to respond to protocol/service requests for increased security.
void LowEnergyConnection::OnSecurityRequest(sm::SecurityLevel level, sm::ResultFunction<> cb) {
  BT_ASSERT(sm_);
  sm_->UpgradeSecurity(level, [cb = std::move(cb), peer_id = peer_id(), handle = handle()](
                                  sm::Result<> status, const auto& sp) {
    bt_log(INFO, "gap-le", "pairing status: %s, properties: %s (peer: %s, handle: %#.4x)",
           bt_str(status), bt_str(sp), bt_str(peer_id), handle);
    cb(status);
  });
}

// Handles a pairing request (i.e. security upgrade) received from "higher levels", likely
// initiated from GAP. This will only be used by pairing requests that are initiated
// in the context of testing. May only be called on an already-established connection.
void LowEnergyConnection::UpgradeSecurity(sm::SecurityLevel level, sm::BondableMode bondable_mode,
                                          sm::ResultFunction<> cb) {
  BT_ASSERT(sm_);
  sm_->set_bondable_mode(bondable_mode);
  OnSecurityRequest(level, std::move(cb));
}

// Cancels any on-going pairing procedures and sets up SMP to use the provided
// new I/O capabilities for future pairing procedures.
void LowEnergyConnection::ResetSecurityManager(sm::IOCapability ioc) { sm_->Reset(ioc); }

void LowEnergyConnection::OnInterrogationComplete() {
  BT_ASSERT(!interrogation_completed_);
  interrogation_completed_ = true;
  MaybeUpdateConnectionParameters();
}

void LowEnergyConnection::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  inspect_properties_.peer_id =
      inspect_node_.CreateString(kInspectPeerIdPropertyName, peer_id().ToString());
  inspect_properties_.peer_address = inspect_node_.CreateString(
      kInspectPeerAddressPropertyName, link_.get() ? link_->peer_address().ToString() : "");
  refs_.AttachInspect(inspect_node_, kInspectRefsPropertyName);
}

void LowEnergyConnection::StartConnectionPauseTimeout() {
  if (link_->role() == hci_spec::ConnectionRole::CENTRAL) {
    StartConnectionPauseCentralTimeout();
  } else {
    StartConnectionPausePeripheralTimeout();
  }
}

void LowEnergyConnection::RegisterEventHandlers() {
  auto self = GetWeakPtr();
  conn_update_cmpl_handler_id_ = cmd_->AddLEMetaEventHandler(
      hci_spec::kLEConnectionUpdateCompleteSubeventCode, [self](const auto& event) {
        if (self.is_alive()) {
          self->OnLEConnectionUpdateComplete(event);
          return hci::CommandChannel::EventCallbackResult::kContinue;
        }
        return hci::CommandChannel::EventCallbackResult::kRemove;
      });
}

// Connection parameter updates by the peripheral are not allowed until the central has been idle
// for kLEConnectionPauseCentral and kLEConnectionPausePeripheral has passed since the connection
// was established (Core Spec v5.2, Vol 3, Part C, Sec 9.3.12).
// TODO(fxbug.dev/79491): Wait to update connection parameters until all initialization
// procedures have completed.
void LowEnergyConnection::StartConnectionPausePeripheralTimeout() {
  BT_ASSERT(!conn_pause_peripheral_timeout_.has_value());
  conn_pause_peripheral_timeout_.emplace([this]() {
    // Destroying this task will invalidate the capture list, so we need to save a self pointer.
    auto self = this;
    conn_pause_peripheral_timeout_.reset();
    self->MaybeUpdateConnectionParameters();
  });
  conn_pause_peripheral_timeout_->PostDelayed(async_get_default_dispatcher(),
                                              kLEConnectionPausePeripheral);
}

// Connection parameter updates by the central are not allowed until the central is idle and the
// peripheral has been idle for kLEConnectionPauseCentral (Core Spec v5.2, Vol 3, Part
// C, Sec 9.3.12).
// TODO(fxbug.dev/79491): Wait to update connection parameters until all initialization
// procedures have completed.
void LowEnergyConnection::StartConnectionPauseCentralTimeout() {
  BT_ASSERT(!conn_pause_central_timeout_.has_value());
  conn_pause_central_timeout_.emplace([this]() {
    // Destroying this task will invalidate the capture list, so we need to save a self pointer.
    auto self = this;
    conn_pause_central_timeout_.reset();
    self->MaybeUpdateConnectionParameters();
  });
  conn_pause_central_timeout_->PostDelayed(async_get_default_dispatcher(),
                                           kLEConnectionPauseCentral);
}

bool LowEnergyConnection::OnL2capFixedChannelsOpened(
    l2cap::Channel::WeakPtr att, l2cap::Channel::WeakPtr smp,
    LowEnergyConnectionOptions connection_options) {
  bt_log(DEBUG, "gap-le", "ATT and SMP fixed channels open (peer: %s)", bt_str(peer_id()));

  // Obtain existing pairing data, if any.
  std::optional<sm::LTK> ltk;

  if (peer_->le() && peer_->le()->bond_data()) {
    // Legacy pairing allows both devices to generate and exchange LTKs. "The Central must have the
    // security information (LTK, EDIV, and Rand) distributed by the Peripheral in LE legacy [...]
    // to setup an encrypted session" (v5.3, Vol. 3 Part H 2.4.4.2). For Secure Connections peer_ltk
    // and local_ltk will be equal, so this check is unnecessary but correct.
    ltk = (link()->role() == hci_spec::ConnectionRole::CENTRAL)
              ? peer_->le()->bond_data()->peer_ltk
              : peer_->le()->bond_data()->local_ltk;
  }

  // Obtain the local I/O capabilities from the delegate. Default to
  // NoInputNoOutput if no delegate is available.
  auto io_cap = sm::IOCapability::kNoInputNoOutput;
  if (conn_mgr_->pairing_delegate().is_alive()) {
    io_cap = conn_mgr_->pairing_delegate()->io_capability();
  }
  LESecurityMode security_mode = conn_mgr_->security_mode();
  sm_ = conn_mgr_->sm_factory_func()(link_->GetWeakPtr(), std::move(smp), io_cap,
                                     weak_delegate_.GetWeakPtr(), connection_options.bondable_mode,
                                     security_mode);

  // Provide SMP with the correct LTK from a previous pairing with the peer, if it exists. This
  // will start encryption if the local device is the link-layer central.
  if (ltk) {
    bt_log(INFO, "gap-le", "assigning existing LTK (peer: %s, handle: %#.4x)", bt_str(peer_id()),
           handle());
    sm_->AssignLongTermKey(*ltk);
  }

  return InitializeGatt(std::move(att), connection_options.service_uuid);
}

void LowEnergyConnection::OnNewLEConnectionParams(
    const hci_spec::LEPreferredConnectionParameters& params) {
  bt_log(INFO, "gap-le", "LE connection parameters received (peer: %s, handle: %#.4x)",
         bt_str(peer_id()), link_->handle());

  BT_ASSERT(peer_.is_alive());

  peer_->MutLe().SetPreferredConnectionParameters(params);

  UpdateConnectionParams(params);
}

void LowEnergyConnection::RequestConnectionParameterUpdate(
    const hci_spec::LEPreferredConnectionParameters& params) {
  BT_ASSERT_MSG(link_->role() == hci_spec::ConnectionRole::PERIPHERAL,
                "tried to send connection parameter update request as central");

  BT_ASSERT(peer_.is_alive());
  // Ensure interrogation has completed.
  BT_ASSERT(peer_->le()->features().has_value());

  // TODO(fxbug.dev/49714): check local controller support for LL Connection Parameters Request
  // procedure (mask is currently in Adapter le state, consider propagating down)
  bool ll_connection_parameters_req_supported =
      peer_->le()->features()->le_features &
      static_cast<uint64_t>(hci_spec::LESupportedFeature::kConnectionParametersRequestProcedure);

  bt_log(TRACE, "gap-le", "ll connection parameters req procedure supported: %s",
         ll_connection_parameters_req_supported ? "true" : "false");

  if (ll_connection_parameters_req_supported) {
    auto self = weak_self_.GetWeakPtr();
    auto status_cb = [self, params](hci::Result<> status) {
      if (!self.is_alive()) {
        return;
      }

      self->HandleRequestConnectionParameterUpdateCommandStatus(params, status);
    };

    UpdateConnectionParams(params, std::move(status_cb));
  } else {
    L2capRequestConnectionParameterUpdate(params);
  }
}

void LowEnergyConnection::HandleRequestConnectionParameterUpdateCommandStatus(
    hci_spec::LEPreferredConnectionParameters params, hci::Result<> status) {
  // The next LE Connection Update complete event is for this command iff the command |status|
  // is success.
  if (status.is_error()) {
    if (status == ToResult(hci_spec::StatusCode::UNSUPPORTED_REMOTE_FEATURE)) {
      // Retry connection parameter update with l2cap if the peer doesn't support LL procedure.
      bt_log(
          INFO, "gap-le",
          "peer does not support HCI LE Connection Update command, trying l2cap request (peer: %s)",
          bt_str(peer_id()));
      L2capRequestConnectionParameterUpdate(params);
    }
    return;
  }

  // Note that this callback is for the Connection Update Complete event, not the Connection Update
  // status event, which is handled by the above code (see v5.2, Vol. 4, Part E 7.7.15 / 7.7.65.3).
  le_conn_update_complete_command_callback_ = [this, params](hci_spec::StatusCode status) {
    // Retry connection parameter update with l2cap if the peer doesn't support LL procedure.
    if (status == hci_spec::StatusCode::UNSUPPORTED_REMOTE_FEATURE) {
      bt_log(INFO, "gap-le",
             "peer does not support HCI LE Connection Update command, trying l2cap request "
             "(peer: %s)",
             bt_str(peer_id()));
      L2capRequestConnectionParameterUpdate(params);
    }
  };
}

void LowEnergyConnection::L2capRequestConnectionParameterUpdate(
    const hci_spec::LEPreferredConnectionParameters& params) {
  BT_ASSERT_MSG(link_->role() == hci_spec::ConnectionRole::PERIPHERAL,
                "tried to send l2cap connection parameter update request as central");

  bt_log(DEBUG, "gap-le", "sending l2cap connection parameter update request (peer: %s)",
         bt_str(peer_id()));

  auto response_cb = [handle = handle(), peer_id = peer_id()](bool accepted) {
    if (accepted) {
      bt_log(DEBUG, "gap-le",
             "peer accepted l2cap connection parameter update request (peer: %s, handle: %#.4x)",
             bt_str(peer_id), handle);
    } else {
      bt_log(INFO, "gap-le",
             "peer rejected l2cap connection parameter update request (peer: %s, handle: %#.4x)",
             bt_str(peer_id), handle);
    }
  };

  // TODO(fxbug.dev/49717): don't send request until after kLEConnectionParameterTimeout of an
  // l2cap conn parameter update response being received (Core Spec v5.2, Vol 3, Part C,
  // Sec 9.3.9).
  l2cap_->RequestConnectionParameterUpdate(handle(), params, std::move(response_cb));
}

void LowEnergyConnection::UpdateConnectionParams(
    const hci_spec::LEPreferredConnectionParameters& params, StatusCallback status_cb) {
  bt_log(DEBUG, "gap-le", "updating connection parameters (peer: %s)", bt_str(peer_id()));
  auto command = hci::CommandPacket::New(hci_spec::kLEConnectionUpdate,
                                         sizeof(hci_spec::LEConnectionUpdateCommandParams));
  auto event_params = command->mutable_payload<hci_spec::LEConnectionUpdateCommandParams>();

  event_params->connection_handle = htole16(handle());
  event_params->conn_interval_min = htole16(params.min_interval());
  event_params->conn_interval_max = htole16(params.max_interval());
  event_params->conn_latency = htole16(params.max_latency());
  event_params->supervision_timeout = htole16(params.supervision_timeout());
  event_params->minimum_ce_length = 0x0000;
  event_params->maximum_ce_length = 0x0000;

  auto status_cb_wrapper = [handle = handle(), cb = std::move(status_cb)](
                               auto id, const hci::EventPacket& event) mutable {
    BT_ASSERT(event.event_code() == hci_spec::kCommandStatusEventCode);
    hci_is_error(event, TRACE, "gap-le",
                 "controller rejected connection parameters (handle: %#.4x)", handle);
    if (cb) {
      cb(event.ToResult());
    }
  };

  cmd_->SendCommand(std::move(command), std::move(status_cb_wrapper),
                    hci_spec::kCommandStatusEventCode);
}

void LowEnergyConnection::OnLEConnectionUpdateComplete(const hci::EventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_ASSERT(event.params<hci_spec::LEMetaEventParams>().subevent_code ==
            hci_spec::kLEConnectionUpdateCompleteSubeventCode);

  auto payload = event.subevent_params<hci_spec::LEConnectionUpdateCompleteSubeventParams>();
  BT_ASSERT(payload);
  hci_spec::ConnectionHandle handle = le16toh(payload->connection_handle);

  // Ignore events for other connections.
  if (handle != link_->handle()) {
    return;
  }

  // This event may be the result of the LE Connection Update command.
  if (le_conn_update_complete_command_callback_) {
    le_conn_update_complete_command_callback_(payload->status);
  }

  if (payload->status != hci_spec::StatusCode::SUCCESS) {
    bt_log(WARN, "gap-le",
           "HCI LE Connection Update Complete event with error "
           "(peer: %s, status: %#.2hhx, handle: %#.4x)",
           bt_str(peer_id()), payload->status, handle);

    return;
  }

  bt_log(INFO, "gap-le", "conn. parameters updated (peer: %s)", bt_str(peer_id()));

  hci_spec::LEConnectionParameters params(le16toh(payload->conn_interval),
                                          le16toh(payload->conn_latency),
                                          le16toh(payload->supervision_timeout));
  link_->set_low_energy_parameters(params);

  BT_ASSERT(peer_.is_alive());
  peer_->MutLe().SetConnectionParameters(params);
}

void LowEnergyConnection::MaybeUpdateConnectionParameters() {
  if (connection_parameters_update_requested_ || conn_pause_central_timeout_ ||
      conn_pause_peripheral_timeout_ || !interrogation_completed_) {
    return;
  }

  connection_parameters_update_requested_ = true;

  if (link_->role() == hci_spec::ConnectionRole::CENTRAL) {
    // If the GAP service preferred connection parameters characteristic has not been read by now,
    // just use the default parameters.
    // TODO(fxbug.dev/66031): Wait for preferred connection parameters to be read.
    BT_ASSERT(peer_.is_alive());
    auto conn_params = peer_->le()->preferred_connection_parameters().value_or(
        kDefaultPreferredConnectionParameters);
    UpdateConnectionParams(conn_params);
  } else {
    RequestConnectionParameterUpdate(kDefaultPreferredConnectionParameters);
  }
}

bool LowEnergyConnection::InitializeGatt(l2cap::Channel::WeakPtr att_channel,
                                         std::optional<UUID> service_uuid) {
  att_bearer_ = att::Bearer::Create(std::move(att_channel));
  if (!att_bearer_) {
    // This can happen if the link closes before the Bearer activates the
    // channel.
    bt_log(WARN, "gatt", "failed to initialize ATT bearer");
    return false;
  }

  // The att::Bearer object is owned by LowEnergyConnection, so it outlives the gatt::Server and
  // Client objects. As such, they can safely take WeakPtrs to the Bearer.
  auto server_factory = [att_bearer = att_bearer_->GetWeakPtr()](
                            PeerId peer_id,
                            gatt::LocalServiceManager::WeakPtr local_services) mutable {
    return gatt::Server::Create(peer_id, std::move(local_services), std::move(att_bearer));
  };
  std::unique_ptr<gatt::Client> gatt_client = gatt::Client::Create(att_bearer_->GetWeakPtr());
  gatt_->AddConnection(peer_id(), std::move(gatt_client), std::move(server_factory));

  std::vector<UUID> service_uuids;
  if (service_uuid) {
    // TODO(fxbug.dev/65592): De-duplicate services.
    service_uuids = {*service_uuid, kGenericAccessService};
  }
  gatt_->InitializeClient(peer_id(), std::move(service_uuids));

  auto self = weak_self_.GetWeakPtr();
  gatt_->ListServices(peer_id(), {kGenericAccessService}, [self](auto status, auto services) {
    if (self.is_alive()) {
      self->OnGattServicesResult(status, std::move(services));
    }
  });

  return true;
}

void LowEnergyConnection::OnGattServicesResult(att::Result<> status, gatt::ServiceList services) {
  if (bt_is_error(status, INFO, "gap-le", "error discovering GAP service (peer: %s)",
                  bt_str(peer_id()))) {
    return;
  }

  if (services.empty()) {
    // The GAP service is mandatory for both central and peripheral, so this is unexpected.
    bt_log(INFO, "gap-le", "GAP service not found (peer: %s)", bt_str(peer_id()));
    return;
  }

  gap_service_client_.emplace(peer_id(), services.front());
  auto self = weak_self_.GetWeakPtr();

  gap_service_client_->ReadDeviceName([self](att::Result<std::string> result) {
    if (!self.is_alive() || result.is_error()) {
      return;
    }

    self->peer_->RegisterName(result.value(), Peer::NameSource::kGenericAccessService);
  });

  gap_service_client_->ReadAppearance([self](att::Result<uint16_t> result) {
    if (!self.is_alive() || result.is_error()) {
      return;
    }

    self->peer_->SetAppearance(result.value());
  });

  if (!peer_->le()->preferred_connection_parameters().has_value()) {
    gap_service_client_->ReadPeripheralPreferredConnectionParameters(
        [self](att::Result<hci_spec::LEPreferredConnectionParameters> result) {
          if (!self.is_alive()) {
            return;
          }

          if (result.is_error()) {
            bt_log(
                INFO, "gap-le",
                "error reading peripheral preferred connection parameters (status:  %s, peer: %s)",
                ::bt::internal::ToString(result).c_str(), bt_str(self->peer_id()));
            return;
          }

          auto params = result.value();
          self->peer_->MutLe().SetPreferredConnectionParameters(params);
        });
  }
}

void LowEnergyConnection::CloseRefs() {
  for (auto* ref : *refs_.Mutable()) {
    ref->MarkClosed();
  }

  refs_.Mutable()->clear();
}

void LowEnergyConnection::OnNewPairingData(const sm::PairingData& pairing_data) {
  const std::optional<sm::LTK> ltk =
      pairing_data.peer_ltk ? pairing_data.peer_ltk : pairing_data.local_ltk;
  // Consider the pairing temporary if no link key was received. This
  // means we'll remain encrypted with the STK without creating a bond and
  // reinitiate pairing when we reconnect in the future.
  if (!ltk.has_value()) {
    bt_log(INFO, "gap-le", "temporarily paired with peer (peer: %s)", bt_str(peer_id()));
    return;
  }

  bt_log(INFO, "gap-le", "new %s pairing data: [%s%s%s%s%s%s] (peer: %s)",
         ltk->security().secure_connections() ? "secure connections" : "legacy",
         pairing_data.peer_ltk ? "peer_ltk " : "", pairing_data.local_ltk ? "local_ltk " : "",
         pairing_data.irk ? "irk " : "", pairing_data.cross_transport_key ? "ct_key " : "",
         pairing_data.identity_address
             ? bt_lib_cpp_string::StringPrintf("(identity: %s) ",
                                               bt_str(*pairing_data.identity_address))
                   .c_str()
             : "",
         pairing_data.csrk ? "csrk " : "", bt_str(peer_id()));

  if (!peer_->MutLe().StoreBond(pairing_data)) {
    bt_log(ERROR, "gap-le", "failed to cache bonding data (id: %s)", bt_str(peer_id()));
  }
}

void LowEnergyConnection::OnPairingComplete(sm::Result<> status) {
  bt_log(INFO, "gap-le", "pairing complete (status: %s, peer: %s)", bt_str(status),
         bt_str(peer_id()));

  auto delegate = conn_mgr_->pairing_delegate();
  if (delegate.is_alive()) {
    delegate->CompletePairing(peer_id(), status);
  }
}

void LowEnergyConnection::OnAuthenticationFailure(hci::Result<> status) {
  // TODO(armansito): Clear bonding data from the remote peer cache as any
  // stored link key is not valid.
  bt_log(WARN, "gap-le", "link layer authentication failed (status: %s, peer: %s)", bt_str(status),
         bt_str(peer_id()));
}

void LowEnergyConnection::OnNewSecurityProperties(const sm::SecurityProperties& sec) {
  bt_log(INFO, "gap-le", "new link security properties (properties: %s, peer: %s)", bt_str(sec),
         bt_str(peer_id()));
  // Update the data plane with the correct link security level.
  l2cap_->AssignLinkSecurityProperties(link_->handle(), sec);
}

std::optional<sm::IdentityInfo> LowEnergyConnection::OnIdentityInformationRequest() {
  if (!conn_mgr_->local_address_delegate()->irk()) {
    bt_log(TRACE, "gap-le", "no local identity information to exchange");
    return std::nullopt;
  }

  bt_log(DEBUG, "gap-le", "will distribute local identity information (peer: %s)",
         bt_str(peer_id()));
  sm::IdentityInfo id_info;
  id_info.irk = *conn_mgr_->local_address_delegate()->irk();
  id_info.address = conn_mgr_->local_address_delegate()->identity_address();

  return id_info;
}

void LowEnergyConnection::ConfirmPairing(ConfirmCallback confirm) {
  bt_log(INFO, "gap-le",
         "pairing delegate request for pairing confirmation w/ no passkey (peer: %s)",
         bt_str(peer_id()));

  auto delegate = conn_mgr_->pairing_delegate();
  if (!delegate.is_alive()) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate! (peer: %s)",
           bt_str(peer_id()));
    confirm(false);
  } else {
    delegate->ConfirmPairing(peer_id(), std::move(confirm));
  }
}

void LowEnergyConnection::DisplayPasskey(uint32_t passkey, sm::Delegate::DisplayMethod method,
                                         ConfirmCallback confirm) {
  bt_log(INFO, "gap-le", "pairing delegate request (method: %s, peer: %s)",
         sm::util::DisplayMethodToString(method).c_str(), bt_str(peer_id()));

  auto delegate = conn_mgr_->pairing_delegate();
  if (!delegate.is_alive()) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate!");
    confirm(false);
  } else {
    delegate->DisplayPasskey(peer_id(), passkey, method, std::move(confirm));
  }
}

void LowEnergyConnection::RequestPasskey(PasskeyResponseCallback respond) {
  bt_log(INFO, "gap-le", "pairing delegate request for passkey entry (peer: %s)",
         bt_str(peer_id()));

  auto delegate = conn_mgr_->pairing_delegate();
  if (!delegate.is_alive()) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate! (peer: %s)",
           bt_str(peer_id()));
    respond(-1);
  } else {
    delegate->RequestPasskey(peer_id(), std::move(respond));
  }
}

}  // namespace bt::gap::internal
