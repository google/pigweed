// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_manager.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <lib/async/time.h>
#include <zircon/assert.h>
#include <zircon/syscalls.h>

#include <optional>

#include "pairing_delegate.h"
#include "peer.h"
#include "peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_state.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/strings/string_printf.h"

using bt::sm::BondableMode;

namespace bt {
namespace gap {

namespace {

static const hci::LEPreferredConnectionParameters kDefaultPreferredConnectionParameters(
    hci::defaults::kLEConnectionIntervalMin, hci::defaults::kLEConnectionIntervalMax,
    /*max_latency=*/0, hci::defaults::kLESupervisionTimeout);

}  // namespace

namespace internal {

// Represents the state of an active connection. Each instance is owned
// and managed by a LowEnergyConnectionManager and is kept alive as long as
// there is at least one LowEnergyConnectionRef that references it.
class LowEnergyConnection final : public sm::PairingState::Delegate {
 public:
  LowEnergyConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link,
                      async_dispatcher_t* dispatcher,
                      fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
                      fbl::RefPtr<data::Domain> data_domain, fbl::RefPtr<gatt::GATT> gatt)
      : peer_id_(peer_id),
        link_(std::move(link)),
        dispatcher_(dispatcher),
        conn_mgr_(conn_mgr),
        data_domain_(data_domain),
        gatt_(gatt),
        conn_pause_central_expiry_(zx::time(async_now(dispatcher_)) + kLEConnectionPauseCentral),
        weak_ptr_factory_(this) {
    ZX_DEBUG_ASSERT(peer_id_.IsValid());
    ZX_DEBUG_ASSERT(link_);
    ZX_DEBUG_ASSERT(dispatcher_);
    ZX_DEBUG_ASSERT(conn_mgr_);
    ZX_DEBUG_ASSERT(data_domain_);
    ZX_DEBUG_ASSERT(gatt_);

    link_->set_peer_disconnect_callback([conn_mgr](auto conn) {
      if (conn_mgr) {
        conn_mgr->OnPeerDisconnect(conn);
      }
    });
  }

  ~LowEnergyConnection() override {
    // Unregister this link from the GATT profile and the L2CAP plane. This
    // invalidates all L2CAP channels that are associated with this link.
    gatt_->RemoveConnection(peer_id());
    data_domain_->RemoveConnection(link_->handle());

    // Notify all active references that the link is gone. This will
    // synchronously notify all refs.
    CloseRefs();
  }

  LowEnergyConnectionRefPtr AddRef() {
    LowEnergyConnectionRefPtr conn_ref(new LowEnergyConnectionRef(peer_id_, handle(), conn_mgr_));
    ZX_ASSERT(conn_ref);

    refs_.insert(conn_ref.get());

    bt_log(DEBUG, "gap-le", "added ref (handle %#.4x, count: %lu)", handle(), ref_count());

    return conn_ref;
  }

  void DropRef(LowEnergyConnectionRef* ref) {
    ZX_DEBUG_ASSERT(ref);

    __UNUSED size_t res = refs_.erase(ref);
    ZX_DEBUG_ASSERT_MSG(res == 1u, "DropRef called with wrong connection reference");
    bt_log(DEBUG, "gap-le", "dropped ref (handle: %#.4x, count: %lu)", handle(), ref_count());
  }

  // Registers this connection with L2CAP and initializes the fixed channel
  // protocols.
  void InitializeFixedChannels(l2cap::LEConnectionParameterUpdateCallback cp_cb,
                               l2cap::LinkErrorCallback link_error_cb, BondableMode bondable_mode) {
    auto self = weak_ptr_factory_.GetWeakPtr();
    auto fixed_channels = data_domain_->AddLEConnection(
        link_->handle(), link_->role(), std::move(link_error_cb), std::move(cp_cb),
        [self](auto handle, auto level, auto cb) {
          if (self) {
            bt_log(DEBUG, "gap-le", "received security upgrade request on L2CAP channel");
            ZX_DEBUG_ASSERT(self->link_->handle() == handle);
            self->OnSecurityRequest(level, std::move(cb));
          }
        });

    OnL2capFixedChannelsOpened(std::move(fixed_channels.att), std::move(fixed_channels.smp),
                               bondable_mode);
  }

  // Tells the connection's pairing state to UpgradeSecurity to the desired level.
  void OnSecurityRequest(sm::SecurityLevel level, sm::StatusCallback cb) {
    ZX_ASSERT(pairing_);
    pairing_->UpgradeSecurity(level, [cb = std::move(cb)](sm::Status status, const auto& sp) {
      bt_log(INFO, "gap-le", "pairing status: %s, properties: %s", bt_str(status), bt_str(sp));
      cb(status);
    });
  }

  // Handles a pairing request (i.e. security upgrade) received from "higher levels", likely
  // initiated from GAP. This will only be used by pairing requests that are initiated
  // in the context of testing. May only be called on an already-established connection.
  void UpgradeSecurity(sm::SecurityLevel level, sm::BondableMode bondable_mode,
                       sm::StatusCallback cb) {
    ZX_ASSERT(pairing_);
    pairing_->set_bondable_mode(bondable_mode);
    OnSecurityRequest(level, std::move(cb));
  }

  // Cancels any on-going pairing procedures and sets up SMP to use the provided
  // new I/O capabilities for future pairing procedures.
  void ResetPairingState(sm::IOCapability ioc) { pairing_->Reset(ioc); }

  // Set callback that will be called after the kLEConnectionPausePeripheral timeout, or now if the
  // timeout has already finished.
  void on_peripheral_pause_timeout(fit::callback<void()> callback) {
    // Check if timeout already completed.
    if (conn_pause_peripheral_timeout_.has_value() &&
        !conn_pause_peripheral_timeout_->is_pending()) {
      callback();
      return;
    }
    conn_pause_peripheral_callback_ = std::move(callback);
  }

  // Should be called as soon as connection is established.
  // Calls |conn_pause_peripheral_callback_| after kLEConnectionPausePeripheral.
  void StartConnectionPausePeripheralTimeout() {
    ZX_ASSERT(!conn_pause_peripheral_timeout_.has_value());
    conn_pause_peripheral_timeout_.emplace([self = weak_ptr_factory_.GetWeakPtr()]() {
      if (!self) {
        return;
      }

      if (self->conn_pause_peripheral_callback_) {
        self->conn_pause_peripheral_callback_();
      }
    });
    conn_pause_peripheral_timeout_->PostDelayed(dispatcher_, kLEConnectionPausePeripheral);
  }

  // Posts |callback| to be called kLEConnectionPauseCentral after this connection was established.
  void PostCentralPauseTimeoutCallback(fit::callback<void()> callback) {
    async::PostTaskForTime(
        dispatcher_,
        [self = weak_ptr_factory_.GetWeakPtr(), cb = std::move(callback)]() mutable {
          if (self) {
            cb();
          }
        },
        conn_pause_central_expiry_);
  }

  size_t ref_count() const { return refs_.size(); }

  PeerId peer_id() const { return peer_id_; }
  hci::ConnectionHandle handle() const { return link_->handle(); }
  hci::Connection* link() const { return link_.get(); }
  BondableMode bondable_mode() const {
    ZX_DEBUG_ASSERT(pairing_);
    return pairing_->bondable_mode();
  }

 private:
  // Called by the L2CAP layer once the link has been registered and the fixed
  // channels have been opened.
  void OnL2capFixedChannelsOpened(fbl::RefPtr<l2cap::Channel> att, fbl::RefPtr<l2cap::Channel> smp,
                                  BondableMode bondable_mode) {
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
      // |ltk| will remain as std::nullopt if bonding data contains no LTK.
      ltk = peer->le()->bond_data()->ltk;
    }

    // Obtain the local I/O capabilities from the delegate. Default to
    // NoInputNoOutput if no delegate is available.
    auto io_cap = sm::IOCapability::kNoInputNoOutput;
    if (conn_mgr_->pairing_delegate()) {
      io_cap = conn_mgr_->pairing_delegate()->io_capability();
    }

    pairing_ = std::make_unique<sm::PairingState>(link_->WeakPtr(), std::move(smp), io_cap,
                                                  weak_ptr_factory_.GetWeakPtr(), bondable_mode);

    // Encrypt the link with the current LTK if it exists.
    if (ltk) {
      bt_log(INFO, "gap-le", "assigning existing LTK");
      pairing_->AssignLongTermKey(*ltk);
    }

    // Initialize the GATT layer.
    gatt_->AddConnection(peer_id(), std::move(att));
    gatt_->DiscoverServices(peer_id());
  }

  // sm::PairingState::Delegate override:
  void OnNewPairingData(const sm::PairingData& pairing_data) override {
    // Consider the pairing temporary if no link key was received. This
    // means we'll remain encrypted with the STK without creating a bond and
    // reinitiate pairing when we reconnect in the future.
    // TODO(armansito): Support bonding with just the CSRK for LE security mode
    // 2.
    if (!pairing_data.ltk) {
      bt_log(INFO, "gap-le", "temporarily paired with peer (id: %s)", bt_str(peer_id()));
      return;
    }

    bt_log(INFO, "gap-le", "new pairing data [%s%s%s%sid: %s]", pairing_data.ltk ? "ltk " : "",
           pairing_data.irk ? "irk " : "",
           pairing_data.identity_address
               ? fxl::StringPrintf("(identity: %s) ",
                                   pairing_data.identity_address->ToString().c_str())
                     .c_str()
               : "",
           pairing_data.csrk ? "csrk " : "", bt_str(peer_id()));

    if (!conn_mgr_->peer_cache()->StoreLowEnergyBond(peer_id_, pairing_data)) {
      bt_log(ERROR, "gap-le", "failed to cache bonding data (id: %s)", bt_str(peer_id()));
    }
  }

  // sm::PairingState::Delegate override:
  void OnPairingComplete(sm::Status status) override {
    bt_log(DEBUG, "gap-le", "pairing complete: %s", status.ToString().c_str());

    auto delegate = conn_mgr_->pairing_delegate();
    if (delegate) {
      delegate->CompletePairing(peer_id_, status);
    }
  }

  // sm::PairingState::Delegate override:
  void OnAuthenticationFailure(hci::Status status) override {
    // TODO(armansito): Clear bonding data from the remote peer cache as any
    // stored link key is not valid.
    bt_log(ERROR, "gap-le", "link layer authentication failed: %s", status.ToString().c_str());
  }

  // sm::PairingState::Delegate override:
  void OnNewSecurityProperties(const sm::SecurityProperties& sec) override {
    bt_log(DEBUG, "gap-le", "new link security properties: %s", sec.ToString().c_str());
    // Update the data plane with the correct link security level.
    data_domain_->AssignLinkSecurityProperties(link_->handle(), sec);
  }

  // sm::PairingState::Delegate override:
  std::optional<sm::IdentityInfo> OnIdentityInformationRequest() override {
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

  // sm::PairingState::Delegate override:
  void OnTemporaryKeyRequest(sm::PairingMethod method,
                             sm::PairingState::Delegate::TkResponse responder) override {
    bt_log(DEBUG, "gap-le", "TK request - method: %s",
           sm::util::PairingMethodToString(method).c_str());

    auto delegate = conn_mgr_->pairing_delegate();
    if (!delegate) {
      bt_log(ERROR, "gap-le", "rejecting pairing without a PairingDelegate!");
      responder(false, 0);
      return;
    }

    if (method == sm::PairingMethod::kPasskeyEntryInput) {
      // The TK will be provided by the user.
      delegate->RequestPasskey(peer_id(), [responder = std::move(responder)](int64_t passkey) {
        if (passkey < 0) {
          responder(false, 0);
        } else {
          responder(true, static_cast<uint32_t>(passkey));
        }
      });
      return;
    }

    if (method == sm::PairingMethod::kPasskeyEntryDisplay) {
      // Randomly generate a 6 digit passkey.
      // TODO(armansito): Use a uniform prng.
      uint32_t passkey;
      zx_cprng_draw(&passkey, sizeof(passkey));
      passkey = passkey % 1000000;
      delegate->DisplayPasskey(peer_id(), passkey, PairingDelegate::DisplayMethod::kPeerEntry,
                               [passkey, responder = std::move(responder)](bool confirm) {
                                 responder(confirm, passkey);
                               });
      return;
    }

    // TODO(armansito): Support providing a TK out of band.
    // OnTKRequest() should only be called for legacy pairing.
    ZX_DEBUG_ASSERT(method == sm::PairingMethod::kJustWorks);

    delegate->ConfirmPairing(peer_id(), [responder = std::move(responder)](bool confirm) {
      // The TK for Just Works pairing is 0 (Vol 3,
      // Part H, 2.3.5.2).
      responder(confirm, 0);
    });
  }

  void CloseRefs() {
    for (auto* ref : refs_) {
      ref->MarkClosed();
    }

    refs_.clear();
  }

  PeerId peer_id_;
  std::unique_ptr<hci::Connection> link_;
  async_dispatcher_t* dispatcher_;
  fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr_;

  // Reference to the data plane is used to update the L2CAP layer to
  // reflect the correct link security level.
  fbl::RefPtr<data::Domain> data_domain_;

  // Reference to the GATT profile layer is used to initiate service discovery
  // and register the link.
  fbl::RefPtr<gatt::GATT> gatt_;

  // SMP pairing manager.
  std::unique_ptr<sm::PairingState> pairing_;

  // Called after kLEConnectionPausePeripheral.
  std::optional<async::TaskClosure> conn_pause_peripheral_timeout_;

  // Called by |conn_pause_peripheral_timeout_|.
  fit::callback<void()> conn_pause_peripheral_callback_;

  // Set to the time when connection parameters should be sent as LE central.
  const zx::time conn_pause_central_expiry_;

  // LowEnergyConnectionManager is responsible for making sure that these
  // pointers are always valid.
  std::unordered_set<LowEnergyConnectionRef*> refs_;

  fxl::WeakPtrFactory<LowEnergyConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnection);
};

}  // namespace internal

LowEnergyConnectionRef::LowEnergyConnectionRef(PeerId peer_id, hci::ConnectionHandle handle,
                                               fxl::WeakPtr<LowEnergyConnectionManager> manager)
    : active_(true), peer_id_(peer_id), handle_(handle), manager_(manager) {
  ZX_DEBUG_ASSERT(peer_id_.IsValid());
  ZX_DEBUG_ASSERT(manager_);
  ZX_DEBUG_ASSERT(handle_);
}

LowEnergyConnectionRef::~LowEnergyConnectionRef() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  if (active_) {
    Release();
  }
}

void LowEnergyConnectionRef::Release() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(active_);
  active_ = false;
  if (manager_) {
    manager_->ReleaseReference(this);
  }
}

void LowEnergyConnectionRef::MarkClosed() {
  active_ = false;
  if (closed_cb_) {
    // Move the callback out of |closed_cb_| to prevent it from deleting itself
    // by deleting |this|.
    auto f = std::move(closed_cb_);
    f();
  }
}

BondableMode LowEnergyConnectionRef::bondable_mode() const {
  ZX_DEBUG_ASSERT(manager_);
  auto conn_iter = manager_->connections_.find(peer_id_);
  ZX_DEBUG_ASSERT(conn_iter != manager_->connections_.end());
  return conn_iter->second->bondable_mode();
}

LowEnergyConnectionManager::PendingRequestData::PendingRequestData(
    const DeviceAddress& address, ConnectionResultCallback first_callback,
    BondableMode bondable_mode)
    : address_(address), bondable_mode_(bondable_mode) {
  callbacks_.push_back(std::move(first_callback));
}

void LowEnergyConnectionManager::PendingRequestData::NotifyCallbacks(hci::Status status,
                                                                     const RefFunc& func) {
  ZX_DEBUG_ASSERT(!callbacks_.empty());
  for (const auto& callback : callbacks_) {
    callback(status, func());
  }
}

LowEnergyConnectionManager::LowEnergyConnectionManager(fxl::WeakPtr<hci::Transport> hci,
                                                       hci::LocalAddressDelegate* addr_delegate,
                                                       hci::LowEnergyConnector* connector,
                                                       PeerCache* peer_cache,
                                                       fbl::RefPtr<data::Domain> data_domain,
                                                       fbl::RefPtr<gatt::GATT> gatt)
    : hci_(std::move(hci)),
      request_timeout_(kLECreateConnectionTimeout),
      dispatcher_(async_get_default_dispatcher()),
      peer_cache_(peer_cache),
      data_domain_(data_domain),
      gatt_(gatt),
      connector_(connector),
      local_address_delegate_(addr_delegate),
      interrogator_(peer_cache, hci_, dispatcher_),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(peer_cache_);
  ZX_DEBUG_ASSERT(data_domain_);
  ZX_DEBUG_ASSERT(gatt_);
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(connector_);
  ZX_DEBUG_ASSERT(local_address_delegate_);

  auto self = weak_ptr_factory_.GetWeakPtr();

  conn_update_cmpl_handler_id_ = hci_->command_channel()->AddLEMetaEventHandler(
      hci::kLEConnectionUpdateCompleteSubeventCode, [self](const auto& event) {
        if (self) {
          return self->OnLEConnectionUpdateComplete(event);
        }
        return hci::CommandChannel::EventCallbackResult::kRemove;
      });
}

LowEnergyConnectionManager::~LowEnergyConnectionManager() {
  hci_->command_channel()->RemoveEventHandler(conn_update_cmpl_handler_id_);

  bt_log(DEBUG, "gap-le", "connection manager shutting down");

  weak_ptr_factory_.InvalidateWeakPtrs();

  // This will cancel any pending request.
  if (connector_->request_pending()) {
    connector_->Cancel();
  }

  // Clear |pending_requests_| and notify failure.
  for (auto& iter : pending_requests_) {
    iter.second.NotifyCallbacks(hci::Status(HostError::kFailed), [] { return nullptr; });
  }
  pending_requests_.clear();

  // Clean up all connections.
  for (auto& iter : connections_) {
    CleanUpConnection(std::move(iter.second));
  }

  connections_.clear();
}

bool LowEnergyConnectionManager::Connect(PeerId peer_id, ConnectionResultCallback callback,
                                         BondableMode bondable_mode) {
  if (!connector_) {
    bt_log(WARN, "gap-le", "connect called during shutdown!");
    return false;
  }

  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(WARN, "gap-le", "peer not found (id: %s)", bt_str(peer_id));
    return false;
  }

  if (peer->technology() == TechnologyType::kClassic) {
    bt_log(ERROR, "gap-le", "peer does not support LE: %s", peer->ToString().c_str());
    return false;
  }

  if (!peer->connectable()) {
    bt_log(ERROR, "gap-le", "peer not connectable: %s", peer->ToString().c_str());
    return false;
  }

  // If we are already waiting to connect to |peer_id| then we store
  // |callback| to be processed after the connection attempt completes (in
  // either success of failure).
  auto pending_iter = pending_requests_.find(peer_id);
  if (pending_iter != pending_requests_.end()) {
    // TODO(52283): add connection state asserts for invariants
    pending_iter->second.AddCallback(std::move(callback));
    return true;
  }

  // If there is already an active connection then we add a new reference and
  // succeed.
  auto conn_ref = AddConnectionRef(peer_id);
  if (conn_ref) {
    async::PostTask(dispatcher_,
                    [conn_ref = std::move(conn_ref), callback = std::move(callback)]() mutable {
                      // Do not report success if the link has been disconnected (e.g. via
                      // Disconnect() or other circumstances).
                      if (!conn_ref->active()) {
                        bt_log(DEBUG, "gap-le", "link disconnected, ref is inactive");
                        callback(hci::Status(HostError::kFailed), nullptr);
                      } else {
                        callback(hci::Status(), std::move(conn_ref));
                      }
                    });

    return true;
  }

  peer->MutLe().SetConnectionState(Peer::ConnectionState::kInitializing);
  pending_requests_[peer_id] =
      PendingRequestData(peer->address(), std::move(callback), bondable_mode);

  TryCreateNextConnection();

  return true;
}

bool LowEnergyConnectionManager::Disconnect(PeerId peer_id) {
  // TODO(BT-873): When connection requests can be canceled, do so here.
  if (pending_requests_.find(peer_id) != pending_requests_.end()) {
    bt_log(WARN, "gap-le", "Can't disconnect peer %s because it's being connected to",
           bt_str(peer_id));
    return false;
  }

  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "peer not connected (id: %s)", bt_str(peer_id));
    return true;
  }

  // Remove the connection state from the internal map right away.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  // Since this was an intentional disconnect, update the auto-connection behavior
  // appropriately.
  peer_cache_->SetAutoConnectBehaviorForIntentionalDisconnect(peer_id);

  bt_log(INFO, "gap-le", "disconnecting link: %s", bt_str(*conn->link()));
  CleanUpConnection(std::move(conn));
  return true;
}

void LowEnergyConnectionManager::Pair(PeerId peer_id, sm::SecurityLevel pairing_level,
                                      sm::BondableMode bondable_mode, sm::StatusCallback cb) {
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "cannot pair: peer not connected (id: %s)", bt_str(peer_id));
    cb(bt::sm::Status(bt::HostError::kNotFound));
    return;
  }
  bt_log(DEBUG, "gap-le", "pairing with security level: %d", pairing_level);
  iter->second->UpgradeSecurity(pairing_level, bondable_mode, std::move(cb));
}

void LowEnergyConnectionManager::RegisterRemoteInitiatedLink(hci::ConnectionPtr link,
                                                             BondableMode bondable_mode,
                                                             ConnectionResultCallback callback) {
  ZX_DEBUG_ASSERT(link);
  bt_log(DEBUG, "gap-le", "new remote-initiated link (local addr: %s): %s",
         bt_str(link->local_address()), bt_str(*link));

  Peer* peer = UpdatePeerWithLink(*link);
  auto peer_id = peer->identifier();

  // TODO(armansito): Use own address when storing the connection (NET-321).
  // Currently this will refuse the connection and disconnect the link if |peer|
  // is already connected to us by a different local address.
  InitializeConnection(peer_id, std::move(link), bondable_mode,
                       [peer_id, cb = std::move(callback), this](
                           hci::Status status, LowEnergyConnectionRefPtr conn_ref) {
                         auto peer = peer_cache_->FindById(peer_id);
                         if (conn_ref && peer) {
                           peer->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);
                         }
                         cb(status, std::move(conn_ref));
                       });
}

void LowEnergyConnectionManager::SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) {
  // TODO(armansito): Add a test case for this once NET-1179 is done.
  pairing_delegate_ = delegate;

  // Tell existing connections to abort ongoing pairing procedures. The new
  // delegate will receive calls to PairingDelegate::CompletePairing, unless it
  // is null.
  for (auto& iter : connections_) {
    iter.second->ResetPairingState(delegate ? delegate->io_capability()
                                            : sm::IOCapability::kNoInputNoOutput);
  }
}

void LowEnergyConnectionManager::SetConnectionParametersCallbackForTesting(
    ConnectionParametersCallback callback) {
  test_conn_params_cb_ = std::move(callback);
}

void LowEnergyConnectionManager::SetDisconnectCallbackForTesting(DisconnectCallback callback) {
  test_disconn_cb_ = std::move(callback);
}

void LowEnergyConnectionManager::ReleaseReference(LowEnergyConnectionRef* conn_ref) {
  ZX_DEBUG_ASSERT(conn_ref);

  auto iter = connections_.find(conn_ref->peer_identifier());
  ZX_DEBUG_ASSERT(iter != connections_.end());

  iter->second->DropRef(conn_ref);
  if (iter->second->ref_count() != 0u)
    return;

  // Move the connection object before erasing the entry.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  bt_log(INFO, "gap-le", "all refs dropped on connection: %s", conn->link()->ToString().c_str());
  CleanUpConnection(std::move(conn));
}

void LowEnergyConnectionManager::TryCreateNextConnection() {
  // There can only be one outstanding LE Create Connection request at a time.
  if (connector_->request_pending()) {
    bt_log(DEBUG, "gap-le", "HCI_LE_Create_Connection command pending");
    return;
  }

  // TODO(armansito): Perform either the General or Auto Connection
  // Establishment procedure here (see NET-187).

  if (pending_requests_.empty()) {
    bt_log(TRACE, "gap-le", "no pending requests remaining");

    // TODO(armansito): Unpause discovery and disable background scanning if
    // there aren't any peers to auto-connect to.
    return;
  }

  for (auto& iter : pending_requests_) {
    const auto& next_peer_addr = iter.second.address();
    Peer* peer = peer_cache_->FindByAddress(next_peer_addr);
    if (peer) {
      RequestCreateConnection(peer, iter.second.bondable_mode());
      break;
    }

    bt_log(DEBUG, "gap-le", "deferring connection attempt for peer: %s",
           next_peer_addr.ToString().c_str());

    // TODO(armansito): For now the requests for this peer won't complete
    // until the next peer discovery. This will no longer be an issue when we
    // use background scanning (see NET-187).
  }
}

void LowEnergyConnectionManager::RequestCreateConnection(Peer* peer, BondableMode bondable_mode) {
  ZX_DEBUG_ASSERT(peer);

  // During the initial connection to a peripheral we use the initial high
  // duty-cycle parameters to ensure that initiating procedures (bonding,
  // encryption setup, service discovery) are completed quickly. Once these
  // procedures are complete, we will change the connection interval to the
  // peripheral's preferred connection parameters (see v5.0, Vol 3, Part C,
  // Section 9.3.12).

  // TODO(armansito): Initiate the connection using the cached preferred
  // connection parameters if we are bonded.
  hci::LEPreferredConnectionParameters initial_params(kLEInitialConnIntervalMin,
                                                      kLEInitialConnIntervalMax, 0,
                                                      hci::defaults::kLESupervisionTimeout);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [bondable_mode, self, peer_id = peer->identifier()](hci::Status status,
                                                                       auto link) {
    if (self)
      self->OnConnectResult(peer_id, status, std::move(link), bondable_mode);
  };

  // We set the scan window and interval to the same value for continuous
  // scanning.
  connector_->CreateConnection(false /* use_whitelist */, peer->address(), kLEScanFastInterval,
                               kLEScanFastInterval, initial_params, status_cb, request_timeout_);
}

void LowEnergyConnectionManager::InitializeConnection(PeerId peer_id,
                                                      std::unique_ptr<hci::Connection> link,
                                                      BondableMode bondable_mode,
                                                      ConnectionResultCallback callback) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(link->ll_type() == hci::Connection::LinkType::kLE);

  auto handle = link->handle();
  auto role = link->role();

  // TODO(armansito): For now reject having more than one link with the same
  // peer. This should change once this has more context on the local
  // destination for remote initiated connections (see NET-321).
  if (connections_.find(peer_id) != connections_.end()) {
    bt_log(DEBUG, "gap-le", "multiple links from peer; connection refused");
    callback(hci::Status(HostError::kFailed), nullptr);
    return;
  }

  // Add the connection to the L2CAP table. Incoming data will be buffered until
  // the channels are open.
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto conn_param_update_cb = [self, handle, peer_id](const auto& params) {
    if (self) {
      self->OnNewLEConnectionParams(peer_id, handle, params);
    }
  };

  auto link_error_cb = [self, peer_id] {
    bt_log(DEBUG, "gap", "link error received from L2CAP");
    if (self) {
      self->Disconnect(peer_id);
    }
  };

  // Initialize connection.
  auto conn = std::make_unique<internal::LowEnergyConnection>(peer_id, std::move(link), dispatcher_,
                                                              self, data_domain_, gatt_);
  conn->InitializeFixedChannels(std::move(conn_param_update_cb), std::move(link_error_cb),
                                bondable_mode);
  conn->StartConnectionPausePeripheralTimeout();
  auto first_ref = conn->AddRef();
  connections_[peer_id] = std::move(conn);

  // TODO(armansito): Should complete a few more things before returning the
  // connection:
  //    1. If this is the first time we connected to this peer:
  //      a. If master, obtain Peripheral Preferred Connection Parameters via
  //         GATT if available
  //      b. Initiate name discovery over GATT if complete name is unknown
  //      c. If master, allow slave to initiate procedures (service discovery,
  //         encryption setup, etc) for kLEConnectionPauseCentral before
  //         updating the connection parameters to the slave's preferred values.

  if (role == hci::Connection::Role::kMaster) {
    // After the Central device has no further pending actions to perform and the
    // Peripheral device has not initiated any other actions within
    // kLEConnectionPauseCentral, then the Central device should update the connection parameters to
    // either the Peripheral Preferred Connection Parameters or self-determined values (Core Spec
    // v5.2, Vol 3, Part C, Sec 9.3.12).
    connections_[peer_id]->PostCentralPauseTimeoutCallback([this, handle]() {
      UpdateConnectionParams(handle, kDefaultPreferredConnectionParameters);
    });
  }

  interrogator_.Start(peer_id, handle,
                      [peer_id, conn_ref = std::move(first_ref), cb = std::move(callback),
                       self](hci::Status status) mutable {
                        if (!self) {
                          return;
                        }

                        if (!status.is_success()) {
                          // Releasing ref will disconnect.
                          conn_ref.release();
                          cb(status, nullptr);
                          return;
                        }

                        self->OnInterrogationComplete(peer_id);

                        cb(status, std::move(conn_ref));
                      });
}

void LowEnergyConnectionManager::OnInterrogationComplete(PeerId peer_id) {
  auto it = connections_.find(peer_id);
  if (it == connections_.end()) {
    bt_log(INFO, "gap", "OnInterrogationComplete called for non-connected peer");
    return;
  }
  auto& conn = it->second;

  if (conn->link()->role() == hci::Connection::Role::kSlave) {
    // "The peripheral device should not perform a connection parameter update procedure within
    // kLEConnectionPausePeripheral after establishing a connection." (Core Spec v5.2, Vol 3, Part
    // C, Sec 9.3.12).
    conn->on_peripheral_pause_timeout([&conn, peer_id, this]() {
      RequestConnectionParameterUpdate(peer_id, *conn, kDefaultPreferredConnectionParameters);
    });
  }
}

LowEnergyConnectionRefPtr LowEnergyConnectionManager::AddConnectionRef(PeerId peer_id) {
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end())
    return nullptr;

  return iter->second->AddRef();
}

void LowEnergyConnectionManager::CleanUpConnection(
    std::unique_ptr<internal::LowEnergyConnection> conn) {
  ZX_DEBUG_ASSERT(conn);

  // Mark the peer peer as no longer connected.
  Peer* peer = peer_cache_->FindById(conn->peer_id());
  ZX_DEBUG_ASSERT_MSG(peer, "A connection was active for an unknown peer! (id: %s)",
                      bt_str(conn->peer_id()));
  peer->MutLe().SetConnectionState(Peer::ConnectionState::kNotConnected);

  conn.reset();
}

void LowEnergyConnectionManager::RegisterLocalInitiatedLink(std::unique_ptr<hci::Connection> link,
                                                            BondableMode bondable_mode) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(link->ll_type() == hci::Connection::LinkType::kLE);
  bt_log(INFO, "gap-le", "new connection %s", bt_str(*link));

  Peer* peer = UpdatePeerWithLink(*link);

  // Initialize the connection  and obtain the initial reference.
  // On successful initialization, this reference lasts until this method returns to prevent it from
  // dropping to 0 due to an unclaimed reference while notifying pending callbacks and listeners
  // below.
  InitializeConnection(peer->identifier(), std::move(link), bondable_mode,
                       [this, peer_id = peer->identifier()](hci::Status status,
                                                            LowEnergyConnectionRefPtr first_ref) {
                         OnLocalInitiatedLinkInitialized(status, std::move(first_ref), peer_id);
                       });
}

void LowEnergyConnectionManager::OnLocalInitiatedLinkInitialized(hci::Status status,
                                                                 LowEnergyConnectionRefPtr conn_ref,
                                                                 PeerId peer_id) {
  if (!status.is_success()) {
    ZX_ASSERT(!conn_ref);

    auto iter = pending_requests_.find(peer_id);
    if (iter != pending_requests_.end()) {
      // Remove the entry from |pending_requests_| before notifying the
      // callbacks.
      auto pending_req_data = std::move(iter->second);
      pending_requests_.erase(iter);

      pending_req_data.NotifyCallbacks(status, [] { return nullptr; });
    }
  } else {
    // We take care never to initiate more than one connection to the same
    // peer.
    ZX_ASSERT(conn_ref);

    auto conn_iter = connections_.find(peer_id);
    ZX_ASSERT(conn_iter != connections_.end());

    auto peer = peer_cache_->FindById(peer_id);
    ZX_ASSERT(peer);

    // For now, jump to the initialized state.
    peer->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);

    auto iter = pending_requests_.find(peer_id);
    if (iter != pending_requests_.end()) {
      // Remove the entry from |pending_requests_| before notifying the
      // callbacks.
      auto pending_req_data = std::move(iter->second);
      pending_requests_.erase(iter);

      pending_req_data.NotifyCallbacks(status,
                                       [&conn_iter] { return conn_iter->second->AddRef(); });
    }

    // Release the extra reference before attempting the next connection.
    // This will disconnect the link if no callback retained its reference.
    conn_ref = nullptr;
  }

  ZX_ASSERT(!connector_->request_pending());
  TryCreateNextConnection();
}

Peer* LowEnergyConnectionManager::UpdatePeerWithLink(const hci::Connection& link) {
  Peer* peer = peer_cache_->FindByAddress(link.peer_address());
  if (!peer) {
    peer = peer_cache_->NewPeer(link.peer_address(), true /* connectable */);
  }
  peer->MutLe().SetConnectionParameters(link.low_energy_parameters());
  peer_cache_->SetAutoConnectBehaviorForSuccessfulConnection(peer->identifier());

  return peer;
}

void LowEnergyConnectionManager::OnConnectResult(PeerId peer_id, hci::Status status,
                                                 hci::ConnectionPtr link,
                                                 BondableMode bondable_mode) {
  ZX_DEBUG_ASSERT(connections_.find(peer_id) == connections_.end());

  if (status) {
    bt_log(DEBUG, "gap-le", "connection request successful");
    RegisterLocalInitiatedLink(std::move(link), bondable_mode);
    return;
  }

  // The request failed or timed out.
  bt_log(ERROR, "gap-le", "failed to connect to peer (id: %s)", bt_str(peer_id));
  Peer* peer = peer_cache_->FindById(peer_id);
  ZX_ASSERT(peer);
  peer->MutLe().SetConnectionState(Peer::ConnectionState::kNotConnected);

  // Notify the matching pending callbacks about the failure.
  auto iter = pending_requests_.find(peer_id);
  ZX_DEBUG_ASSERT(iter != pending_requests_.end());

  // Remove the entry from |pending_requests_| before notifying callbacks.
  auto pending_req_data = std::move(iter->second);
  pending_requests_.erase(iter);
  pending_req_data.NotifyCallbacks(status, [] { return nullptr; });

  // Process the next pending attempt.
  ZX_DEBUG_ASSERT(!connector_->request_pending());
  TryCreateNextConnection();
}

void LowEnergyConnectionManager::OnPeerDisconnect(const hci::Connection* connection) {
  auto handle = connection->handle();
  if (test_disconn_cb_) {
    test_disconn_cb_(handle);
  }

  // See if we can find a connection with a matching handle by walking the
  // connections list.
  auto iter = FindConnection(handle);
  if (iter == connections_.end()) {
    bt_log(TRACE, "gap-le", "disconnect from unknown connection handle: %#.4x", handle);
    return;
  }

  // Found the connection. Remove the entry from |connections_| before notifying
  // the "closed" handlers.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  bt_log(INFO, "gap-le", "peer %s disconnected (handle: %#.4x)", bt_str(conn->peer_id()), handle);
  ZX_DEBUG_ASSERT(conn->ref_count());

  CleanUpConnection(std::move(conn));
}

hci::CommandChannel::EventCallbackResult LowEnergyConnectionManager::OnLEConnectionUpdateComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLEMetaEventCode);
  ZX_DEBUG_ASSERT(event.params<hci::LEMetaEventParams>().subevent_code ==
                  hci::kLEConnectionUpdateCompleteSubeventCode);

  auto payload = event.le_event_params<hci::LEConnectionUpdateCompleteSubeventParams>();
  ZX_ASSERT(payload);
  hci::ConnectionHandle handle = le16toh(payload->connection_handle);

  // This event may be the result of the LE Connection Update command.
  if (le_conn_update_complete_command_callback_) {
    le_conn_update_complete_command_callback_(handle, payload->status);
  }

  if (payload->status != hci::StatusCode::kSuccess) {
    bt_log(WARN, "gap-le",
           "HCI LE Connection Update Complete event with error "
           "(status: %#.2x, handle: %#.4x)",
           payload->status, handle);

    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto iter = FindConnection(handle);
  if (iter == connections_.end()) {
    bt_log(DEBUG, "gap-le", "conn. parameters received for unknown link (handle: %#.4x)", handle);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  const auto& conn = *iter->second;
  ZX_DEBUG_ASSERT(conn.handle() == handle);

  bt_log(INFO, "gap-le", "conn. parameters updated (id: %s, handle: %#.4x)", bt_str(conn.peer_id()),
         handle);
  hci::LEConnectionParameters params(le16toh(payload->conn_interval),
                                     le16toh(payload->conn_latency),
                                     le16toh(payload->supervision_timeout));
  conn.link()->set_low_energy_parameters(params);

  Peer* peer = peer_cache_->FindById(conn.peer_id());
  if (!peer) {
    bt_log(ERROR, "gap-le", "conn. parameters updated for unknown peer!");
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  peer->MutLe().SetConnectionParameters(params);

  if (test_conn_params_cb_)
    test_conn_params_cb_(*peer);

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

void LowEnergyConnectionManager::OnNewLEConnectionParams(
    PeerId peer_id, hci::ConnectionHandle handle,
    const hci::LEPreferredConnectionParameters& params) {
  bt_log(DEBUG, "gap-le", "conn. parameters received (handle: %#.4x)", handle);

  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(ERROR, "gap-le", "conn. parameters received from unknown peer!");
    return;
  }

  peer->MutLe().SetPreferredConnectionParameters(params);

  // Use the new parameters if we're not performing service discovery or
  // bonding.
  if (peer->le()->connected()) {
    UpdateConnectionParams(handle, params);
  }
}

void LowEnergyConnectionManager::RequestConnectionParameterUpdate(
    PeerId peer_id, const internal::LowEnergyConnection& conn,
    const hci::LEPreferredConnectionParameters& params) {
  ZX_ASSERT_MSG(conn.link()->role() == hci::Connection::Role::kSlave,
                "tried to send connection parameter update request as master");

  Peer* peer = peer_cache_->FindById(peer_id);
  // Ensure interrogation has completed.
  ZX_ASSERT(peer->le()->features().has_value());

  // TODO(49714): check local controller support for LL Connection Parameters Request procedure
  // (mask is currently in Adapter le state, consider propagating down)
  bool ll_connection_parameters_req_supported =
      peer->le()->features()->le_features &
      static_cast<uint64_t>(hci::LESupportedFeature::kConnectionParametersRequestProcedure);

  bt_log(TRACE, "gap-le", "ll connection parameters req procedure supported: %s",
         ll_connection_parameters_req_supported ? "true" : "false");

  if (ll_connection_parameters_req_supported) {
    auto status_cb = [self = weak_ptr_factory_.GetWeakPtr(), peer_id, params](hci::Status status) {
      if (!self) {
        return;
      }

      auto it = self->connections_.find(peer_id);
      if (it == self->connections_.end()) {
        bt_log(TRACE, "gap-le",
               "connection update command status for non-connected peer (peer id: %s)",
               bt_str(peer_id));
        return;
      }
      auto& conn = it->second;

      // The next LE Connection Update complete event is for this command iff the command status
      // is success.
      if (status.is_success()) {
        self->le_conn_update_complete_command_callback_ = [self, params, peer_id,
                                                           expected_handle = conn->handle()](
                                                              hci::ConnectionHandle handle,
                                                              hci::StatusCode status) {
          if (!self) {
            return;
          }

          if (handle != expected_handle) {
            bt_log(WARN, "gap-le",
                   "handle in conn update complete command callback (%#.4x) does not match handle "
                   "in command (%#.4x)",
                   handle, expected_handle);
            return;
          }

          auto it = self->connections_.find(peer_id);
          if (it == self->connections_.end()) {
            bt_log(TRACE, "gap-le",
                   "connection update complete event for non-connected peer (peer id: %s)",
                   bt_str(peer_id));
            return;
          }
          auto& conn = it->second;

          // Retry connection parameter update with l2cap if the peer doesn't support LL procedure.
          if (status == hci::StatusCode::kUnsupportedRemoteFeature) {
            bt_log(TRACE, "gap-le",
                   "peer does not support HCI LE Connection Update command, trying l2cap request");
            self->L2capRequestConnectionParameterUpdate(*conn, params);
          }
        };

      } else if (status.protocol_error() == hci::StatusCode::kUnsupportedRemoteFeature) {
        // Retry connection parameter update with l2cap if the peer doesn't support LL procedure.
        bt_log(TRACE, "gap-le",
               "peer does not support HCI LE Connection Update command, trying l2cap request");
        self->L2capRequestConnectionParameterUpdate(*conn, params);
      }
    };

    UpdateConnectionParams(conn.handle(), params, std::move(status_cb));
  } else {
    L2capRequestConnectionParameterUpdate(conn, params);
  }
}

void LowEnergyConnectionManager::UpdateConnectionParams(
    hci::ConnectionHandle handle, const hci::LEPreferredConnectionParameters& params,
    StatusCallback status_cb) {
  bt_log(DEBUG, "gap-le", "updating connection parameters (handle: %#.4x)", handle);
  auto command = hci::CommandPacket::New(hci::kLEConnectionUpdate,
                                         sizeof(hci::LEConnectionUpdateCommandParams));
  auto event_params = command->mutable_payload<hci::LEConnectionUpdateCommandParams>();

  event_params->connection_handle = htole16(handle);
  event_params->conn_interval_min = htole16(params.min_interval());
  event_params->conn_interval_max = htole16(params.max_interval());
  event_params->conn_latency = htole16(params.max_latency());
  event_params->supervision_timeout = htole16(params.supervision_timeout());
  event_params->minimum_ce_length = 0x0000;
  event_params->maximum_ce_length = 0x0000;

  auto status_cb_wrapper = [handle, cb = std::move(status_cb)](
                               auto id, const hci::EventPacket& event) mutable {
    ZX_ASSERT(event.event_code() == hci::kCommandStatusEventCode);
    hci_is_error(event, TRACE, "gap-le",
                 "controller rejected connection parameters (handle: %#.4x)", handle);
    if (cb) {
      cb(event.ToStatus());
    }
  };

  hci_->command_channel()->SendCommand(std::move(command), std::move(status_cb_wrapper),
                                       hci::kCommandStatusEventCode);
}

void LowEnergyConnectionManager::L2capRequestConnectionParameterUpdate(
    const internal::LowEnergyConnection& conn, const hci::LEPreferredConnectionParameters& params) {
  ZX_ASSERT_MSG(conn.link()->role() == hci::Connection::Role::kSlave,
                "tried to send l2cap connection parameter update request as master");

  bt_log(DEBUG, "gap-le", "sending l2cap connection parameter update request");

  auto handle = conn.handle();
  auto response_cb = [handle](bool accepted) {
    bt_log(DEBUG, "gap-le", "peer %s l2cap connection parameter update request (handle: %#.4x)",
           accepted ? "accepted" : "rejected", handle);
  };

  // TODO(49717): don't send request until after kLEConnectionParameterTimeout of an l2cap conn
  // parameter update response being received (Core Spec v5.2, Vol 3, Part C, Sec 9.3.9).
  data_domain_->RequestConnectionParameterUpdate(handle, params, std::move(response_cb));
}

LowEnergyConnectionManager::ConnectionMap::iterator LowEnergyConnectionManager::FindConnection(
    hci::ConnectionHandle handle) {
  auto iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    const auto& conn = *iter->second;
    if (conn.handle() == handle)
      break;
  }
  return iter;
}

}  // namespace gap
}  // namespace bt
