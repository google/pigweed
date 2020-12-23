// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <lib/async/time.h>

#include "low_energy_connection_manager.h"

namespace bt::gap::internal {

LowEnergyConnection::LowEnergyConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link,
                                         async_dispatcher_t* dispatcher,
                                         fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
                                         fbl::RefPtr<l2cap::L2cap> l2cap,
                                         fxl::WeakPtr<gatt::GATT> gatt,
                                         LowEnergyConnectionRequest request)
    : peer_id_(peer_id),
      link_(std::move(link)),
      dispatcher_(dispatcher),
      conn_mgr_(conn_mgr),
      l2cap_(l2cap),
      gatt_(gatt),
      conn_pause_central_expiry_(zx::time(async_now(dispatcher_)) + kLEConnectionPauseCentral),
      request_(std::move(request)),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(peer_id_.IsValid());
  ZX_DEBUG_ASSERT(link_);
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(conn_mgr_);
  ZX_DEBUG_ASSERT(l2cap_);
  ZX_DEBUG_ASSERT(gatt_);

  link_->set_peer_disconnect_callback([conn_mgr](auto conn, auto reason) {
    if (conn_mgr) {
      conn_mgr->OnPeerDisconnect(conn, reason);
    }
  });
}

LowEnergyConnection::~LowEnergyConnection() {
  if (request_.has_value()) {
    bt_log(INFO, "gap-le",
           "destroying connection, notifying request callbacks of failure (handle %#.4x)",
           handle());
    request_->NotifyCallbacks(fit::error(HostError::kFailed));
    request_.reset();
  }

  // Unregister this link from the GATT profile and the L2CAP plane. This
  // invalidates all L2CAP channels that are associated with this link.
  gatt_->RemoveConnection(peer_id());
  l2cap_->RemoveConnection(link_->handle());

  // Notify all active references that the link is gone. This will
  // synchronously notify all refs.
  CloseRefs();
}

void LowEnergyConnection::AddRequestCallback(
    LowEnergyConnectionManager::ConnectionResultCallback cb) {
  if (request_.has_value()) {
    request_->AddCallback(std::move(cb));
  } else {
    cb(fit::ok(AddRef()));
  }
}

void LowEnergyConnection::NotifyRequestCallbacks() {
  if (request_.has_value()) {
    bt_log(TRACE, "gap-le", "notifying connection request callbacks (handle %#.4x)", handle());
    request_->NotifyCallbacks(fit::ok(std::bind(&LowEnergyConnection::AddRef, this)));
    request_.reset();
  }
}

std::unique_ptr<bt::gap::LowEnergyConnectionHandle> LowEnergyConnection::AddRef() {
  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_ref(
      new LowEnergyConnectionHandle(peer_id_, handle(), conn_mgr_));
  ZX_ASSERT(conn_ref);

  refs_.insert(conn_ref.get());

  bt_log(DEBUG, "gap-le", "added ref (handle %#.4x, count: %lu)", handle(), ref_count());

  return conn_ref;
}

void LowEnergyConnection::DropRef(LowEnergyConnectionHandle* ref) {
  ZX_DEBUG_ASSERT(ref);

  __UNUSED size_t res = refs_.erase(ref);
  ZX_DEBUG_ASSERT_MSG(res == 1u, "DropRef called with wrong connection reference");
  bt_log(DEBUG, "gap-le", "dropped ref (handle: %#.4x, count: %lu)", handle(), ref_count());
}

// Registers this connection with L2CAP and initializes the fixed channel
// protocols.
void LowEnergyConnection::InitializeFixedChannels(l2cap::LEConnectionParameterUpdateCallback cp_cb,
                                                  l2cap::LinkErrorCallback link_error_cb,
                                                  LowEnergyConnectionOptions connection_options) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto fixed_channels = l2cap_->AddLEConnection(
      link_->handle(), link_->role(), std::move(link_error_cb), std::move(cp_cb),
      [self](auto handle, auto level, auto cb) {
        if (self) {
          bt_log(DEBUG, "gap-le", "received security upgrade request on L2CAP channel");
          ZX_DEBUG_ASSERT(self->link_->handle() == handle);
          self->OnSecurityRequest(level, std::move(cb));
        }
      });

  OnL2capFixedChannelsOpened(std::move(fixed_channels.att), std::move(fixed_channels.smp),
                             connection_options);
}

// Used to respond to protocol/service requests for increased security.
void LowEnergyConnection::OnSecurityRequest(sm::SecurityLevel level, sm::StatusCallback cb) {
  ZX_ASSERT(sm_);
  sm_->UpgradeSecurity(level, [cb = std::move(cb)](sm::Status status, const auto& sp) {
    bt_log(INFO, "gap-le", "pairing status: %s, properties: %s", bt_str(status), bt_str(sp));
    cb(status);
  });
}

// Handles a pairing request (i.e. security upgrade) received from "higher levels", likely
// initiated from GAP. This will only be used by pairing requests that are initiated
// in the context of testing. May only be called on an already-established connection.
void LowEnergyConnection::UpgradeSecurity(sm::SecurityLevel level, sm::BondableMode bondable_mode,
                                          sm::StatusCallback cb) {
  ZX_ASSERT(sm_);
  sm_->set_bondable_mode(bondable_mode);
  OnSecurityRequest(level, std::move(cb));
}

// Cancels any on-going pairing procedures and sets up SMP to use the provided
// new I/O capabilities for future pairing procedures.
void LowEnergyConnection::ResetSecurityManager(sm::IOCapability ioc) { sm_->Reset(ioc); }

// Set callback that will be called after the kLEConnectionPausePeripheral timeout, or now if the
// timeout has already finished.
void LowEnergyConnection::on_peripheral_pause_timeout(
    fit::callback<void(LowEnergyConnection*)> callback) {
  // Check if timeout already completed.
  if (conn_pause_peripheral_timeout_.has_value() && !conn_pause_peripheral_timeout_->is_pending()) {
    callback(this);
    return;
  }
  conn_pause_peripheral_callback_ = std::move(callback);
}

// Should be called as soon as connection is established.
// Calls |conn_pause_peripheral_callback_| after kLEConnectionPausePeripheral.
void LowEnergyConnection::StartConnectionPausePeripheralTimeout() {
  ZX_ASSERT(!conn_pause_peripheral_timeout_.has_value());
  conn_pause_peripheral_timeout_.emplace([self = weak_ptr_factory_.GetWeakPtr()]() {
    if (!self) {
      return;
    }

    if (self->conn_pause_peripheral_callback_) {
      self->conn_pause_peripheral_callback_(self.get());
    }
  });
  conn_pause_peripheral_timeout_->PostDelayed(dispatcher_, kLEConnectionPausePeripheral);
}

void LowEnergyConnection::PostCentralPauseTimeoutCallback(fit::callback<void()> callback) {
  async::PostTaskForTime(
      dispatcher_,
      [self = weak_ptr_factory_.GetWeakPtr(), cb = std::move(callback)]() mutable {
        if (self) {
          cb();
        }
      },
      conn_pause_central_expiry_);
}

std::optional<LowEnergyConnectionRequest> LowEnergyConnection::take_request() {
  std::optional<LowEnergyConnectionRequest> returned_request;
  request_.swap(returned_request);
  return returned_request;
}

void LowEnergyConnection::OnL2capFixedChannelsOpened(
    fbl::RefPtr<l2cap::Channel> att, fbl::RefPtr<l2cap::Channel> smp,
    LowEnergyConnectionOptions connection_options) {
  if (!att || !smp) {
    bt_log(DEBUG, "gap-le", "link was closed before opening fixed channels");
    return;
  }

  bt_log(DEBUG, "gap-le", "ATT and SMP fixed channels open");

  // Obtain existing pairing data, if any.
  std::optional<sm::LTK> ltk;
  auto* peer = conn_mgr_->peer_cache()->FindById(peer_id());
  ZX_DEBUG_ASSERT_MSG(peer, "connected peer must be present in cache!");

  if (peer->le() && peer->le()->bond_data()) {
    // Legacy pairing allows both devices to generate and exchange LTKs. "The master device must
    // have the [...] (LTK, EDIV, and Rand) distributed by the slave device in LE legacy [...] to
    // setup an encrypted session" (V5.0 Vol. 3 Part H 2.4.4.2). For Secure Connections peer_ltk
    // and local_ltk will be equal, so this check is unnecessary but correct.
    ltk = (link()->role() == hci::Connection::Role::kMaster) ? peer->le()->bond_data()->peer_ltk
                                                             : peer->le()->bond_data()->local_ltk;
  }

  // Obtain the local I/O capabilities from the delegate. Default to
  // NoInputNoOutput if no delegate is available.
  auto io_cap = sm::IOCapability::kNoInputNoOutput;
  if (conn_mgr_->pairing_delegate()) {
    io_cap = conn_mgr_->pairing_delegate()->io_capability();
  }
  LeSecurityMode security_mode = conn_mgr_->security_mode();
  sm_ = conn_mgr_->sm_factory_func()(link_->WeakPtr(), std::move(smp), io_cap,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     connection_options.bondable_mode, security_mode);

  // Provide SMP with the correct LTK from a previous pairing with the peer, if it exists. This
  // will start encryption if the local device is the link-layer master.
  if (ltk) {
    bt_log(INFO, "gap-le", "assigning existing LTK");
    sm_->AssignLongTermKey(*ltk);
  }

  InitializeGatt(std::move(att), connection_options.service_uuid);
}

void LowEnergyConnection::InitializeGatt(fbl::RefPtr<l2cap::Channel> att,
                                         std::optional<UUID> service_uuid) {
  gatt_->AddConnection(peer_id(), std::move(att));

  std::vector<UUID> service_uuids;
  if (service_uuid) {
    // TODO(fxbug.dev/65592): De-duplicate services.
    service_uuids = {*service_uuid, kGenericAccessService};
  }
  gatt_->DiscoverServices(peer_id(), std::move(service_uuids));

  auto self = weak_ptr_factory_.GetWeakPtr();
  gatt_->ListServices(peer_id(), {kGenericAccessService}, [self](auto status, auto services) {
    if (self) {
      self->OnGattServicesResult(status, std::move(services));
    }
  });
}

void LowEnergyConnection::OnGattServicesResult(att::Status status, gatt::ServiceList services) {
  if (bt_is_error(status, INFO, "gap-le", "error discovering GAP service (peer: %s)",
                  bt_str(peer_id()))) {
    return;
  }

  if (services.empty()) {
    // The GAP service is mandatory for both central and peripheral, so this is unexpected.
    bt_log(INFO, "gap-le", "GAP service not found (peer: %s)", bt_str(peer_id()));
    return;
  }

  auto* peer = conn_mgr_->peer_cache()->FindById(peer_id());
  ZX_ASSERT_MSG(peer, "connected peer must be present in cache!");

  gap_service_client_.emplace(peer_id(), services.front());

  // TODO(fxbug.dev/65914): Read name and appearance characteristics.
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!peer->le()->preferred_connection_parameters().has_value()) {
    gap_service_client_->ReadPeripheralPreferredConnectionParameters([self](auto result) {
      if (!self) {
        return;
      }

      if (result.is_error()) {
        bt_log(INFO, "gap-le",
               "error reading peripheral preferred connection parameters (status:  %s, peer: %s)",
               bt_str(result.error()), bt_str(self->peer_id()));
        return;
      }
      auto params = result.value();

      auto* peer = self->conn_mgr_->peer_cache()->FindById(self->peer_id());
      ZX_ASSERT_MSG(peer, "connected peer must be present in cache!");
      peer->MutLe().SetPreferredConnectionParameters(params);
    });
  }
}

void LowEnergyConnection::CloseRefs() {
  for (auto* ref : refs_) {
    ref->MarkClosed();
  }

  refs_.clear();
}

void LowEnergyConnection::OnNewPairingData(const sm::PairingData& pairing_data) {
  const std::optional<sm::LTK> ltk =
      pairing_data.peer_ltk ? pairing_data.peer_ltk : pairing_data.local_ltk;
  // Consider the pairing temporary if no link key was received. This
  // means we'll remain encrypted with the STK without creating a bond and
  // reinitiate pairing when we reconnect in the future.
  if (!ltk.has_value()) {
    bt_log(INFO, "gap-le", "temporarily paired with peer (id: %s)", bt_str(peer_id()));
    return;
  }

  bt_log(INFO, "gap-le", "new %s pairing data [%s%s%s%s%s%sid: %s]",
         ltk->security().secure_connections() ? "secure connections" : "legacy",
         pairing_data.peer_ltk ? "peer_ltk " : "", pairing_data.local_ltk ? "local_ltk " : "",
         pairing_data.irk ? "irk " : "", pairing_data.cross_transport_key ? "ct_key " : "",
         pairing_data.identity_address
             ? fxl::StringPrintf("(identity: %s) ", bt_str(*pairing_data.identity_address)).c_str()
             : "",
         pairing_data.csrk ? "csrk " : "", bt_str(peer_id()));

  if (!conn_mgr_->peer_cache()->StoreLowEnergyBond(peer_id_, pairing_data)) {
    bt_log(ERROR, "gap-le", "failed to cache bonding data (id: %s)", bt_str(peer_id()));
  }
}

void LowEnergyConnection::OnPairingComplete(sm::Status status) {
  bt_log(DEBUG, "gap-le", "pairing complete: %s", status.ToString().c_str());

  auto delegate = conn_mgr_->pairing_delegate();
  if (delegate) {
    delegate->CompletePairing(peer_id_, status);
  }
}

void LowEnergyConnection::OnAuthenticationFailure(hci::Status status) {
  // TODO(armansito): Clear bonding data from the remote peer cache as any
  // stored link key is not valid.
  bt_log(ERROR, "gap-le", "link layer authentication failed: %s", status.ToString().c_str());
}

void LowEnergyConnection::OnNewSecurityProperties(const sm::SecurityProperties& sec) {
  bt_log(DEBUG, "gap-le", "new link security properties: %s", sec.ToString().c_str());
  // Update the data plane with the correct link security level.
  l2cap_->AssignLinkSecurityProperties(link_->handle(), sec);
}

std::optional<sm::IdentityInfo> LowEnergyConnection::OnIdentityInformationRequest() {
  if (!conn_mgr_->local_address_delegate()->irk()) {
    bt_log(TRACE, "gap-le", "no local identity information to exchange");
    return std::nullopt;
  }

  bt_log(DEBUG, "gap-le", "will distribute local identity information");
  sm::IdentityInfo id_info;
  id_info.irk = *conn_mgr_->local_address_delegate()->irk();
  id_info.address = conn_mgr_->local_address_delegate()->identity_address();

  return id_info;
}

void LowEnergyConnection::ConfirmPairing(ConfirmCallback confirm) {
  bt_log(DEBUG, "gap-le", "pairing delegate request for pairing confirmation w/ no passkey");

  auto* delegate = conn_mgr_->pairing_delegate();
  if (!delegate) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate!");
    confirm(false);
  } else {
    delegate->ConfirmPairing(peer_id(), std::move(confirm));
  }
}

void LowEnergyConnection::DisplayPasskey(uint32_t passkey, sm::Delegate::DisplayMethod method,
                                         ConfirmCallback confirm) {
  bt_log(TRACE, "gap-le", "pairing delegate request for %s",
         sm::util::DisplayMethodToString(method).c_str());

  auto* delegate = conn_mgr_->pairing_delegate();
  if (!delegate) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate!");
    confirm(false);
  } else {
    delegate->DisplayPasskey(peer_id(), passkey, method, std::move(confirm));
  }
}

void LowEnergyConnection::RequestPasskey(PasskeyResponseCallback respond) {
  bt_log(TRACE, "gap-le", "pairing delegate request for passkey entry");

  auto* delegate = conn_mgr_->pairing_delegate();
  if (!delegate) {
    bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate!");
    respond(-1);
  } else {
    delegate->RequestPasskey(peer_id(), std::move(respond));
  }
}

}  // namespace bt::gap::internal
