// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_manager.h"

#include <lib/async/time.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/expiring_set.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/status.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::gap {

using std::unique_ptr;
using ConnectionState = Peer::ConnectionState;

namespace {

const char* const kInspectRequestsNodeName = "connection_requests";
const char* const kInspectRequestNodeNamePrefix = "request_";
const char* const kInspectConnectionsNodeName = "connections";
const char* const kInspectConnectionNodeNamePrefix = "connection_";
const char* const kInspectLastDisconnectedListName = "last_disconnected";
const char* const kInspectLastDisconnectedItemDurationPropertyName = "duration_s";
const char* const kInspectLastDisconnectedItemPeerPropertyName = "peer_id";
const char* const kInspectTimestampPropertyName = "@time";

std::string ReasonAsString(DisconnectReason reason) {
  switch (reason) {
    case DisconnectReason::kApiRequest:
      return "ApiRequest";
    case DisconnectReason::kInterrogationFailed:
      return "InterrogationFailed";
    case DisconnectReason::kPairingFailed:
      return "PairingFailed";
    case DisconnectReason::kAclLinkError:
      return "AclLinkError";
    default:
      return "<Unknown Reason>";
  }
}

// This procedure can continue to operate independently of the existence of an
// BrEdrConnectionManager instance, which will begin to disable Page Scan as it shuts down.
void SetPageScanEnabled(bool enabled, fxl::WeakPtr<hci::Transport> hci,
                        async_dispatcher_t* dispatcher, hci::StatusCallback cb) {
  ZX_DEBUG_ASSERT(cb);
  auto read_enable = hci::CommandPacket::New(hci_spec::kReadScanEnable);
  auto finish_enable_cb = [enabled, hci, finish_cb = std::move(cb)](
                              auto, const hci::EventPacket& event) mutable {
    if (hci_is_error(event, WARN, "gap-bredr", "read scan enable failed")) {
      finish_cb(event.ToStatus());
      return;
    }

    auto params = event.return_params<hci_spec::ReadScanEnableReturnParams>();
    uint8_t scan_type = params->scan_enable;
    if (enabled) {
      scan_type |= static_cast<uint8_t>(hci_spec::ScanEnableBit::kPage);
    } else {
      scan_type &= ~static_cast<uint8_t>(hci_spec::ScanEnableBit::kPage);
    }
    auto write_enable = hci::CommandPacket::New(hci_spec::kWriteScanEnable,
                                                sizeof(hci_spec::WriteScanEnableCommandParams));
    write_enable->mutable_payload<hci_spec::WriteScanEnableCommandParams>()->scan_enable =
        scan_type;
    hci->command_channel()->SendCommand(
        std::move(write_enable),
        [cb = std::move(finish_cb)](auto, const hci::EventPacket& event) { cb(event.ToStatus()); });
  };
  hci->command_channel()->SendCommand(std::move(read_enable), std::move(finish_enable_cb));
}

}  // namespace

// An event signifying that a connection was completed by the controller
BrEdrConnectionManager::ConnectionComplete::ConnectionComplete(const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci_spec::kConnectionCompleteEventCode);
  const auto& params = event.params<hci_spec::ConnectionCompleteEventParams>();
  handle = letoh16(params.connection_handle);
  addr = DeviceAddress(DeviceAddress::Type::kBREDR, params.bd_addr);
  status = hci::Status(params.status);
  link_type = params.link_type;
}

// An event signifying that an incoming connection is being requested by a peer
BrEdrConnectionManager::ConnectionRequestEvent::ConnectionRequestEvent(
    const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci_spec::kConnectionRequestEventCode);
  const auto& params = event.params<hci_spec::ConnectionRequestEventParams>();
  addr = DeviceAddress(DeviceAddress::Type::kBREDR, params.bd_addr);
  link_type = params.link_type;
  class_of_device = params.class_of_device;
}

hci::CommandChannel::EventHandlerId BrEdrConnectionManager::AddEventHandler(
    const hci_spec::EventCode& code, hci::CommandChannel::EventCallback cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto event_id = hci_->command_channel()->AddEventHandler(
      code, [self, callback = std::move(cb)](const auto& event) {
        if (self) {
          return callback(event);
        }
        return hci::CommandChannel::EventCallbackResult::kRemove;
      });
  ZX_DEBUG_ASSERT(event_id);
  event_handler_ids_.push_back(event_id);
  return event_id;
}

BrEdrConnectionManager::BrEdrConnectionManager(fxl::WeakPtr<hci::Transport> hci,
                                               PeerCache* peer_cache, DeviceAddress local_address,
                                               fbl::RefPtr<l2cap::L2cap> l2cap,
                                               bool use_interlaced_scan)
    : hci_(std::move(hci)),
      cache_(peer_cache),
      local_address_(local_address),
      l2cap_(l2cap),
      interrogator_(cache_, hci_),
      page_scan_interval_(0),
      page_scan_window_(0),
      use_interlaced_scan_(use_interlaced_scan),
      request_timeout_(kBrEdrCreateConnectionTimeout),
      dispatcher_(async_get_default_dispatcher()),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(cache_);
  ZX_DEBUG_ASSERT(l2cap_);
  ZX_DEBUG_ASSERT(dispatcher_);

  hci_cmd_runner_ = std::make_unique<hci::SequentialCommandRunner>(dispatcher_, hci_);

  // Register event handlers
  AddEventHandler(hci_spec::kAuthenticationCompleteEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnAuthenticationComplete));
  AddEventHandler(hci_spec::kConnectionCompleteEventCode, [this](const hci::EventPacket& event) {
    OnConnectionComplete(ConnectionComplete(event));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  });
  AddEventHandler(hci_spec::kConnectionRequestEventCode, [this](const hci::EventPacket& event) {
    OnConnectionRequest(ConnectionRequestEvent(event));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  });
  AddEventHandler(hci_spec::kIOCapabilityRequestEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnIoCapabilityRequest));
  AddEventHandler(hci_spec::kIOCapabilityResponseEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnIoCapabilityResponse));
  AddEventHandler(hci_spec::kLinkKeyRequestEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnLinkKeyRequest));
  AddEventHandler(hci_spec::kLinkKeyNotificationEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnLinkKeyNotification));
  AddEventHandler(hci_spec::kSimplePairingCompleteEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnSimplePairingComplete));
  AddEventHandler(hci_spec::kUserConfirmationRequestEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnUserConfirmationRequest));
  AddEventHandler(hci_spec::kUserPasskeyRequestEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnUserPasskeyRequest));
  AddEventHandler(hci_spec::kUserPasskeyNotificationEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnUserPasskeyNotification));
  AddEventHandler(hci_spec::kRoleChangeEventCode,
                  fit::bind_member(this, &BrEdrConnectionManager::OnRoleChange));

  // Set the timeout for outbound connections explicitly to the spec default.
  WritePageTimeout(hci_spec::kDefaultPageTimeoutDuration, [](const hci::Status status) {
    [[maybe_unused]] bool _ = bt_is_error(status, WARN, "gap-bredr", "write page timeout failed");
  });
}

BrEdrConnectionManager::~BrEdrConnectionManager() {
  if (pending_request_ && pending_request_->Cancel())
    SendCreateConnectionCancelCommand(pending_request_->peer_address());

  // Disconnect any connections that we're holding.
  connections_.clear();
  // Become unconnectable
  SetPageScanEnabled(/*enabled=*/false, hci_, dispatcher_, [](const auto) {});
  // Remove all event handlers
  for (auto handler_id : event_handler_ids_) {
    hci_->command_channel()->RemoveEventHandler(handler_id);
  }
}

void BrEdrConnectionManager::SetConnectable(bool connectable, hci::StatusCallback status_cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!connectable) {
    auto not_connectable_cb = [self, cb = std::move(status_cb)](const auto& status) {
      if (self) {
        self->page_scan_interval_ = 0;
        self->page_scan_window_ = 0;
      } else if (status) {
        cb(hci::Status(HostError::kFailed));
        return;
      }
      cb(status);
    };
    SetPageScanEnabled(/*enabled=*/false, hci_, dispatcher_, std::move(not_connectable_cb));
    return;
  }

  WritePageScanSettings(
      hci_spec::kPageScanR1Interval, hci_spec::kPageScanR1Window, use_interlaced_scan_,
      [self, cb = std::move(status_cb)](const auto& status) mutable {
        if (bt_is_error(status, WARN, "gap-bredr", "Write Page Scan Settings failed")) {
          cb(status);
          return;
        }
        if (!self) {
          cb(hci::Status(HostError::kFailed));
          return;
        }
        SetPageScanEnabled(/*enabled=*/true, self->hci_, self->dispatcher_, std::move(cb));
      });
}

void BrEdrConnectionManager::SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) {
  pairing_delegate_ = std::move(delegate);
  for (auto& [handle, connection] : connections_) {
    connection.pairing_state().SetPairingDelegate(pairing_delegate_);
  }
}

PeerId BrEdrConnectionManager::GetPeerId(hci_spec::ConnectionHandle handle) const {
  auto it = connections_.find(handle);
  if (it == connections_.end()) {
    return kInvalidPeerId;
  }

  auto* peer = cache_->FindByAddress(it->second.link().peer_address());
  ZX_DEBUG_ASSERT_MSG(peer, "Couldn't find peer for handle %#.4x", handle);
  return peer->identifier();
}

void BrEdrConnectionManager::Pair(PeerId peer_id, BrEdrSecurityRequirements security,
                                  hci::StatusCallback callback) {
  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "can't pair to peer_id %s: connection not found", bt_str(peer_id));
    callback(hci::Status(HostError::kNotFound));
    return;
  }
  auto& [handle, connection] = *conn_pair;
  auto pairing_callback = [pair_callback = std::move(callback)](auto, hci::Status status) {
    pair_callback(status);
  };
  connection->pairing_state().InitiatePairing(security, std::move(pairing_callback));
}

void BrEdrConnectionManager::OpenL2capChannel(PeerId peer_id, l2cap::PSM psm,
                                              BrEdrSecurityRequirements security_reqs,
                                              l2cap::ChannelParameters params,
                                              l2cap::ChannelCallback cb) {
  auto pairing_cb = [self = weak_ptr_factory_.GetWeakPtr(), peer_id, psm, params,
                     cb = std::move(cb)](auto status) mutable {
    bt_log(TRACE, "gap-bredr", "got pairing status %s, %sreturning socket to %s", bt_str(status),
           status ? "" : "not ", bt_str(peer_id));
    if (!status || !self) {
      // Report the failure to the user with a null channel.
      cb(nullptr);
      return;
    }

    auto conn_pair = self->FindConnectionById(peer_id);
    if (!conn_pair) {
      bt_log(INFO, "gap-bredr", "can't open l2cap channel: connection not found (peer: %s)",
             bt_str(peer_id));
      cb(nullptr);
      return;
    }
    auto& [handle, connection] = *conn_pair;

    connection->OpenL2capChannel(psm, params,
                                 [cb = std::move(cb)](auto chan) { cb(std::move(chan)); });
  };

  Pair(peer_id, security_reqs, std::move(pairing_cb));
}

BrEdrConnectionManager::SearchId BrEdrConnectionManager::AddServiceSearch(
    const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
    BrEdrConnectionManager::SearchCallback callback) {
  auto on_service_discovered = [self = weak_ptr_factory_.GetWeakPtr(), uuid,
                                client_cb = std::move(callback)](PeerId peer_id, auto& attributes) {
    if (self) {
      Peer* const peer = self->cache_->FindById(peer_id);
      ZX_ASSERT(peer);
      peer->MutBrEdr().AddService(uuid);
    }
    client_cb(peer_id, attributes);
  };
  return discoverer_.AddSearch(uuid, std::move(attributes), std::move(on_service_discovered));
}

bool BrEdrConnectionManager::RemoveServiceSearch(SearchId id) {
  return discoverer_.RemoveSearch(id);
}

std::optional<BrEdrConnectionManager::ScoRequestHandle> BrEdrConnectionManager::OpenScoConnection(
    PeerId peer_id, hci_spec::SynchronousConnectionParameters parameters,
    sco::ScoConnectionManager::OpenConnectionCallback callback) {
  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "Can't open SCO connection to unconnected peer (peer: %s)",
           bt_str(peer_id));
    callback(fpromise::error(HostError::kNotFound));
    return std::nullopt;
  };
  return conn_pair->second->OpenScoConnection(parameters, std::move(callback));
}

std::optional<BrEdrConnectionManager::ScoRequestHandle> BrEdrConnectionManager::AcceptScoConnection(
    PeerId peer_id, std::vector<hci_spec::SynchronousConnectionParameters> parameters,
    sco::ScoConnectionManager::AcceptConnectionCallback callback) {
  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "Can't accept SCO connection from unconnected peer (peer: %s)",
           bt_str(peer_id));
    callback(fpromise::error(HostError::kNotFound));
    return std::nullopt;
  };
  return conn_pair->second->AcceptScoConnection(std::move(parameters), std::move(callback));
}

bool BrEdrConnectionManager::Disconnect(PeerId peer_id, DisconnectReason reason) {
  bt_log_scope("peer: %s", bt_str(peer_id));
  bt_log(INFO, "gap-bredr", "Disconnect Requested (reason %hhu - %s)", reason,
         ReasonAsString(reason).c_str());

  // TODO(fxbug.dev/65157) - If a disconnect request is received when we have a pending connection,
  // we should instead abort the connection, by either:
  //   * removing the request if it has not yet been processed
  //   * sending a cancel command to the controller and waiting for it to be processed
  //   * sending a cancel command, and if we already complete, then beginning a disconnect procedure
  if (connection_requests_.find(peer_id) != connection_requests_.end()) {
    bt_log(WARN, "gap-bredr", "Can't disconnect because it's being connected to");
    return false;
  }

  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(INFO, "gap-bredr", "No need to disconnect: It is not connected");
    return true;
  }

  auto [handle, connection] = *conn_pair;

  const DeviceAddress& peer_addr = connection->link().peer_address();
  bt_log_scope("addr: %s", bt_str(peer_addr));
  if (reason == DisconnectReason::kApiRequest) {
    bt_log(DEBUG, "gap-bredr", "requested disconnect from peer, cooldown for %lds",
           kLocalDisconnectCooldownDuration.to_secs());
    deny_incoming_.add_until(
        peer_addr, async::Now(async_get_default_dispatcher()) + kLocalDisconnectCooldownDuration);
  }

  CleanUpConnection(handle, std::move(connections_.extract(handle).mapped()));
  return true;
}

void BrEdrConnectionManager::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);

  inspect_properties_.connections_node_ = inspect_node_.CreateChild(kInspectConnectionsNodeName);
  inspect_properties_.last_disconnected_list.AttachInspect(inspect_node_,
                                                           kInspectLastDisconnectedListName);

  inspect_properties_.requests_node_ = inspect_node_.CreateChild(kInspectRequestsNodeName);
  for (auto& [_, req] : connection_requests_) {
    req.AttachInspect(inspect_properties_.requests_node_,
                      inspect_properties_.requests_node_.UniqueName(kInspectRequestNodeNamePrefix));
  }
}

void BrEdrConnectionManager::WritePageTimeout(zx::duration page_timeout, hci::StatusCallback cb) {
  ZX_ASSERT(page_timeout >= hci_spec::kMinPageTimeoutDuration);
  ZX_ASSERT(page_timeout <= hci_spec::kMaxPageTimeoutDuration);

  const int64_t raw_page_timeout = page_timeout / hci_spec::kDurationPerPageTimeoutUnit;
  ZX_ASSERT(raw_page_timeout >= hci_spec::kMinPageTimeoutCommandParameterValue);
  ZX_ASSERT(raw_page_timeout <= hci_spec::kMaxPageTimeoutCommandParameterValue);

  auto write_page_timeout_cmd = hci::CommandPacket::New(
      hci_spec::kWritePageTimeout, sizeof(hci_spec::WritePageTimeoutCommandParams));
  auto& params =
      *write_page_timeout_cmd->mutable_payload<hci_spec::WritePageTimeoutCommandParams>();
  params.page_timeout = static_cast<uint16_t>(raw_page_timeout);

  hci_->command_channel()->SendCommand(
      std::move(write_page_timeout_cmd),
      [cb = std::move(cb)](auto id, const hci::EventPacket& event) { cb(event.ToStatus()); });
}

void BrEdrConnectionManager::WritePageScanSettings(uint16_t interval, uint16_t window,
                                                   bool interlaced, hci::StatusCallback cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!hci_cmd_runner_->IsReady()) {
    // TODO(jamuraa): could run the three "settings" commands in parallel and
    // remove the sequence runner.
    cb(hci::Status(HostError::kInProgress));
    return;
  }

  auto write_activity = hci::CommandPacket::New(
      hci_spec::kWritePageScanActivity, sizeof(hci_spec::WritePageScanActivityCommandParams));
  auto* activity_params =
      write_activity->mutable_payload<hci_spec::WritePageScanActivityCommandParams>();
  activity_params->page_scan_interval = htole16(interval);
  activity_params->page_scan_window = htole16(window);

  hci_cmd_runner_->QueueCommand(
      std::move(write_activity), [self, interval, window](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr", "write page scan activity failed")) {
          return;
        }

        self->page_scan_interval_ = interval;
        self->page_scan_window_ = window;

        bt_log(TRACE, "gap-bredr", "page scan activity updated");
      });

  auto write_type = hci::CommandPacket::New(hci_spec::kWritePageScanType,
                                            sizeof(hci_spec::WritePageScanTypeCommandParams));
  auto* type_params = write_type->mutable_payload<hci_spec::WritePageScanTypeCommandParams>();
  type_params->page_scan_type = (interlaced ? hci_spec::PageScanType::kInterlacedScan
                                            : hci_spec::PageScanType::kStandardScan);

  hci_cmd_runner_->QueueCommand(
      std::move(write_type), [self, interlaced](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr", "write page scan type failed")) {
          return;
        }

        self->page_scan_type_ = (interlaced ? hci_spec::PageScanType::kInterlacedScan
                                            : hci_spec::PageScanType::kStandardScan);

        bt_log(TRACE, "gap-bredr", "page scan type updated");
      });

  hci_cmd_runner_->RunCommands(std::move(cb));
}

std::optional<std::pair<hci_spec::ConnectionHandle, BrEdrConnection*>>
BrEdrConnectionManager::FindConnectionById(PeerId peer_id) {
  auto it = std::find_if(connections_.begin(), connections_.end(),
                         [peer_id](const auto& c) { return c.second.peer_id() == peer_id; });

  if (it == connections_.end()) {
    return std::nullopt;
  }

  auto& [handle, conn] = *it;
  ZX_ASSERT(conn.link().ll_type() != bt::LinkType::kLE);

  return std::pair(handle, &conn);
}

std::optional<std::pair<hci_spec::ConnectionHandle, BrEdrConnection*>>
BrEdrConnectionManager::FindConnectionByAddress(const DeviceAddressBytes& bd_addr) {
  auto* const peer = cache_->FindByAddress(DeviceAddress(DeviceAddress::Type::kBREDR, bd_addr));
  if (!peer) {
    return std::nullopt;
  }

  return FindConnectionById(peer->identifier());
}

Peer* BrEdrConnectionManager::FindOrInitPeer(DeviceAddress addr) {
  Peer* peer = cache_->FindByAddress(addr);
  if (!peer) {
    peer = cache_->NewPeer(addr, /*connectable*/ true);
  }
  return peer;
}

// Build connection state for a new connection and begin interrogation. L2CAP is not enabled for
// this link but pairing is allowed before interrogation completes.
void BrEdrConnectionManager::InitializeConnection(DeviceAddress addr,
                                                  hci_spec::ConnectionHandle connection_handle,
                                                  hci_spec::ConnectionRole role) {
  hci::Connection::Role conn_role = role == hci_spec::ConnectionRole::kMaster
                                        ? hci::Connection::Role::kMaster
                                        : hci::Connection::Role::kPeripheral;
  auto link = hci::Connection::CreateACL(connection_handle, conn_role, local_address_, addr, hci_);
  Peer* const peer = FindOrInitPeer(addr);
  auto peer_id = peer->identifier();
  bt_log(INFO, "gap-bredr", "Beginning interrogation for peer %s", bt_str(peer_id));

  // We should never have more than one link to a given peer
  ZX_DEBUG_ASSERT(!FindConnectionById(peer_id));

  // The controller has completed the HCI connection procedure, so the connection request can no
  // longer be failed by a lower layer error. Now tie error reporting of the request to the lifetime
  // of the connection state object (BrEdrConnection RAII).
  auto node = connection_requests_.extract(peer_id);
  auto request = node ? std::optional(std::move(node.mapped())) : std::nullopt;

  const hci_spec::ConnectionHandle handle = link->handle();
  auto send_auth_request_cb = [this, handle]() {
    this->SendAuthenticationRequested(handle, [handle](auto status) {
      bt_is_error(status, WARN, "gap-bredr", "authentication requested command failed for %#.4x",
                  handle);
    });
  };
  auto disconnect_cb = [this, peer_id] { Disconnect(peer_id, DisconnectReason::kPairingFailed); };
  auto on_peer_disconnect_cb = [this, link = link.get()] { OnPeerDisconnect(link); };
  auto [conn_iter, success] = connections_.try_emplace(
      handle, peer->GetWeakPtr(), std::move(link), std::move(send_auth_request_cb),
      std::move(disconnect_cb), std::move(on_peer_disconnect_cb), l2cap_, hci_, std::move(request));
  ZX_ASSERT(success);

  BrEdrConnection& connection = conn_iter->second;
  connection.pairing_state().SetPairingDelegate(pairing_delegate_);
  connection.AttachInspect(
      inspect_properties_.connections_node_,
      inspect_properties_.connections_node_.UniqueName(kInspectConnectionNodeNamePrefix));

  // Interrogate this peer to find out its version/capabilities.
  auto self = weak_ptr_factory_.GetWeakPtr();
  interrogator_.Start(peer->identifier(), handle, [peer, self, handle](auto status) {
    if (!self) {
      return;
    }
    bt_log_scope("peer: %s, handle: %#.4x", bt_str(peer->identifier()), handle);
    if (bt_is_error(status, WARN, "gap-bredr", "interrogation failed, dropping connection")) {
      // If this connection was locally requested, requester(s) are notified by the disconnection.
      self->Disconnect(peer->identifier(), DisconnectReason::kInterrogationFailed);
      return;
    }
    bt_log(INFO, "gap-bredr", "interrogation complete");
    self->CompleteConnectionSetup(peer, handle);
  });

  // If this was our in-flight request, close it
  if (pending_request_.has_value() && addr == pending_request_->peer_address()) {
    pending_request_.reset();
  }

  TryCreateNextConnection();
}

// Finish connection setup after a successful interrogation.
void BrEdrConnectionManager::CompleteConnectionSetup(Peer* peer,
                                                     hci_spec::ConnectionHandle handle) {
  auto self = weak_ptr_factory_.GetWeakPtr();

  bt_log_scope("peer: %s, handle: %#.4x", bt_str(peer->identifier()), handle);
  auto connections_iter = connections_.find(handle);
  if (connections_iter == connections_.end()) {
    bt_log(WARN, "gap-bredr", "Connection to complete not found");
    return;
  }
  BrEdrConnection& conn_state = connections_iter->second;
  if (conn_state.peer_id() != peer->identifier()) {
    bt_log(WARN, "gap-bredr",
           "Connection switched peers! (now to %s), ignoring interrogation result",
           bt_str(conn_state.peer_id()));
    return;
  }
  hci::Connection* const connection = &conn_state.link();

  auto error_handler = [self, log_ctx = capture_log_context(), peer_id = peer->identifier(),
                        connection = connection->WeakPtr()] {
    if (!self || !connection)
      return;
    add_parent_context(log_ctx);
    bt_log(WARN, "gap-bredr", "Link error received, closing connection");
    self->Disconnect(peer_id, DisconnectReason::kAclLinkError);
  };

  // TODO(fxbug.dev/37650): Implement this callback as a call to InitiatePairing().
  auto security_callback = [log_ctx = capture_log_context()](hci_spec::ConnectionHandle handle,
                                                             sm::SecurityLevel level, auto cb) {
    add_parent_context(log_ctx);
    bt_log(INFO, "gap-bredr", "Ignoring security upgrade request; not implemented");
    cb(sm::Status(HostError::kNotSupported));
  };

  // Register with L2CAP to handle services on the ACL signaling channel.
  l2cap_->AddACLConnection(handle, connection->role(), error_handler, std::move(security_callback));

  // Remove from the denylist if we successfully connect.
  deny_incoming_.remove(peer->address());

  if (discoverer_.search_count()) {
    l2cap_->OpenL2capChannel(handle, l2cap::kSDP, l2cap::ChannelParameters(),
                             [self, peer_id = peer->identifier()](auto channel) {
                               if (!self)
                                 return;
                               if (!channel) {
                                 bt_log_scope("peer: %s", bt_str(peer_id));
                                 bt_log(ERROR, "gap", "failed to create l2cap channel for SDP");
                                 return;
                               }

                               auto client = sdp::Client::Create(std::move(channel));
                               self->discoverer_.StartServiceDiscovery(peer_id, std::move(client));
                             });
  }

  conn_state.OnInterrogationComplete();
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnAuthenticationComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kAuthenticationCompleteEventCode);
  const auto& params = event.params<hci_spec::AuthenticationCompleteEventParams>();

  auto iter = connections_.find(params.connection_handle);
  if (iter == connections_.end()) {
    bt_log(INFO, "gap-bredr",
           "ignoring authentication complete (status: %s) for unknown connection handle %#.04x",
           bt_str(hci::Status(params.status)), params.connection_handle);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::StatusCode status_code;
  event.ToStatusCode(&status_code);
  iter->second.pairing_state().OnAuthenticationComplete(status_code);
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

bool BrEdrConnectionManager::ExistsIncomingRequest(PeerId id) {
  auto request = connection_requests_.find(id);
  return (request != connection_requests_.end() && request->second.HasIncoming());
}

void BrEdrConnectionManager::OnConnectionRequest(ConnectionRequestEvent event) {
  if (deny_incoming_.contains(event.addr)) {
    bt_log(INFO, "gap-bredr", "rejecting incoming from peer (addr: %s) on cooldown",
           bt_str(event.addr));
    SendRejectConnectionRequest(event.addr, hci_spec::StatusCode::kConnectionRejectedBadBdAddr);
    return;
  }
  // Initialize the peer if it doesn't exist, to ensure we have allocated a PeerId
  auto peer = FindOrInitPeer(event.addr);
  auto peer_id = peer->identifier();
  bt_log_scope("peer: %s, addr: %s, link_type: %s, class: %s", bt_str(peer_id), bt_str(event.addr),
               hci_spec::LinkTypeToString(event.link_type).c_str(), bt_str(event.class_of_device));

  // In case of concurrent incoming requests from the same peer, reject all but the first
  if (ExistsIncomingRequest(peer_id)) {
    bt_log(WARN, "gap-bredr", "rejecting duplicate incoming connection request");
    SendRejectConnectionRequest(event.addr, hci_spec::StatusCode::kConnectionRejectedBadBdAddr);
    return;
  }

  if (event.link_type == hci_spec::LinkType::kACL) {
    // If we happen to be already connected (for example, if our outgoing raced, or we received
    // duplicate requests), we reject the request with 'ConnectionAlreadyExists'
    if (FindConnectionById(peer_id)) {
      bt_log(WARN, "gap-bredr", "rejecting incoming connection request; already connected");
      SendRejectConnectionRequest(event.addr, hci_spec::StatusCode::kConnectionAlreadyExists);
      return;
    }

    // Accept the connection, performing a role switch. We receive a Connection Complete event
    // when the connection is complete, and finish the link then.
    bt_log(INFO, "gap-bredr", "accepting incoming connection");

    // Register that we're in the middle of an incoming request for this peer - create a new
    // request if one doesn't already exist
    auto [request, _ignore] = connection_requests_.try_emplace(
        peer_id, event.addr, peer_id, peer->MutBrEdr().RegisterInitializingConnection());
    request->second.BeginIncoming();
    request->second.AttachInspect(
        inspect_properties_.requests_node_,
        inspect_properties_.requests_node_.UniqueName(kInspectRequestNodeNamePrefix));

    SendAcceptConnectionRequest(
        event.addr.value(),
        [addr = event.addr, self = weak_ptr_factory_.GetWeakPtr(), peer_id](auto status) {
          if (self && !status)
            self->CompleteRequest(peer_id, addr, status, /*handle=*/0);
        });

    return;
  }

  if (event.link_type == hci_spec::LinkType::kSCO ||
      event.link_type == hci_spec::LinkType::kExtendedSCO) {
    auto conn_pair = FindConnectionByAddress(event.addr.value());
    if (conn_pair) {
      // The ScoConnectionManager owned by the BrEdrConnection will respond.
      bt_log(INFO, "gap-bredr", "delegating incoming SCO connection to ScoConnectionManager");
      return;
    }
    bt_log(WARN, "gap-bredr", "rejecting (e)SCO connection request for peer that is not connected");
    SendRejectSynchronousRequest(event.addr,
                                 hci_spec::StatusCode::kUnacceptableConnectionParameters);
  } else {
    auto link_type = static_cast<unsigned int>(event.link_type);
    bt_log(WARN, "gap-bredr", "reject unsupported connection type %u", link_type);
    SendRejectConnectionRequest(event.addr, hci_spec::StatusCode::kUnsupportedFeatureOrParameter);
  }
}

void BrEdrConnectionManager::OnConnectionComplete(ConnectionComplete event) {
  if (event.link_type != hci_spec::LinkType::kACL) {
    // Only ACL links are processed
    return;
  }

  // Initialize the peer if it doesn't exist, to ensure we have allocated a PeerId (we should
  // usually have a peer by this point)
  auto peer = FindOrInitPeer(event.addr);

  CompleteRequest(peer->identifier(), event.addr, event.status, event.handle);
}

// A request for a connection - from an upstream client _or_ a remote peer - completed, successfully
// or not. This may be due to a ConnectionComplete event being received, or due to a CommandStatus
// response being received in response to a CreateConnection command
void BrEdrConnectionManager::CompleteRequest(PeerId peer_id, DeviceAddress address,
                                             hci::Status status,
                                             hci_spec::ConnectionHandle handle) {
  bt_log_scope("peer: %s, addr: %s, handle: %#.4x", bt_str(peer_id), bt_str(address), handle);

  auto req_iter = connection_requests_.find(peer_id);
  if (req_iter == connection_requests_.end()) {
    // Prevent logspam for rejected during cooldown.
    if (deny_incoming_.contains(address)) {
      return;
    }
    // This could potentially happen if the peer expired from the peer cache during the connection
    // procedure
    bt_log(INFO, "gap-bredr", "ConnectionComplete received with no known request (status: %s)",
           bt_str(status));
    return;
  }
  auto& request = req_iter->second;

  bool completed_request_was_outgoing =
      pending_request_ && pending_request_->peer_address() == address;
  bool failed = !status.is_success();

  const char* direction = completed_request_was_outgoing ? "outgoing" : "incoming";
  const char* result = status ? "complete" : "error";
  hci_spec::ConnectionRole role = completed_request_was_outgoing
                                      ? hci_spec::ConnectionRole::kMaster
                                      : hci_spec::ConnectionRole::kPeripheral;
  if (request.role_change()) {
    role = request.role_change().value();
  }

  bt_log(INFO, "gap-bredr", "%s connection %s (status: %s, role: %s)", direction, result,
         bt_str(status), role == hci_spec::ConnectionRole::kMaster ? "leader" : "follower");

  if (completed_request_was_outgoing) {
    // Determine the modified status in case of cancellation or timeout
    status = pending_request_->CompleteRequest(status);
    pending_request_.reset();
  } else {
    // If this was an incoming attempt, clear it
    request.CompleteIncoming();
  }

  if (failed) {
    if (request.HasIncoming() || (!completed_request_was_outgoing && request.AwaitingOutgoing())) {
      // This request failed, but we're still waiting on either:
      // * an in-progress incoming request or
      // * to attempt our own outgoing request
      // Therefore we don't notify yet - instead take no action, and wait until we finish those
      // steps before completing the request and notifying callbacks
      TryCreateNextConnection();
      return;
    }
    request.NotifyCallbacks(status, [] { return nullptr; });
    connection_requests_.erase(req_iter);
  } else {
    // Callbacks will be notified when interrogation completes
    InitializeConnection(address, handle, role);
  }

  TryCreateNextConnection();
}

void BrEdrConnectionManager::OnPeerDisconnect(const hci::Connection* connection) {
  auto handle = connection->handle();

  auto it = connections_.find(handle);
  if (it == connections_.end()) {
    bt_log(WARN, "gap-bredr", "disconnect from unknown connection handle %#.4x", handle);
    return;
  }

  auto conn = std::move(it->second);
  connections_.erase(it);

  bt_log(INFO, "gap-bredr", "peer disconnected (peer: %s, handle: %#.4x)", bt_str(conn.peer_id()),
         handle);
  CleanUpConnection(handle, std::move(conn));
}

void BrEdrConnectionManager::CleanUpConnection(hci_spec::ConnectionHandle handle,
                                               BrEdrConnection conn) {
  l2cap_->RemoveConnection(handle);
  RecordDisconnectInspect(conn);
  // |conn| is destroyed when it goes out of scope.
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnIoCapabilityRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kIOCapabilityRequestEventCode);
  const auto& params = event.params<hci_spec::IOCapabilityRequestEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(ERROR, "gap-bredr", "got %s for unconnected addr %s", __func__, bt_str(params.bd_addr));
    SendIoCapabilityRequestNegativeReply(params.bd_addr, hci_spec::StatusCode::kPairingNotAllowed);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  auto [handle, conn_ptr] = *conn_pair;
  auto reply = conn_ptr->pairing_state().OnIoCapabilityRequest();

  if (!reply) {
    SendIoCapabilityRequestNegativeReply(params.bd_addr, hci_spec::StatusCode::kPairingNotAllowed);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  const hci_spec::IOCapability io_capability = *reply;

  // TODO(fxbug.dev/601): Add OOB status from PeerCache.
  const uint8_t oob_data_present = 0x00;  // None present.

  // TODO(fxbug.dev/1249): Determine this based on the service requirements.
  const hci_spec::AuthRequirements auth_requirements =
      io_capability == hci_spec::IOCapability::kNoInputNoOutput
          ? hci_spec::AuthRequirements::kGeneralBonding
          : hci_spec::AuthRequirements::kMITMGeneralBonding;

  SendIoCapabilityRequestReply(params.bd_addr, io_capability, oob_data_present, auth_requirements);
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnIoCapabilityResponse(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kIOCapabilityResponseEventCode);
  const auto& params = event.params<hci_spec::IOCapabilityResponseEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(INFO, "gap-bredr", "got %s for unconnected addr %s", __func__, bt_str(params.bd_addr));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  conn_pair->second->pairing_state().OnIoCapabilityResponse(params.io_capability);
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnLinkKeyRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kLinkKeyRequestEventCode);
  const auto& params = event.params<hci_spec::LinkKeyRequestParams>();

  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);
  auto* peer = cache_->FindByAddress(addr);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "no peer with address %s found", bt_str(addr));
    SendLinkKeyRequestNegativeReply(params.bd_addr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto peer_id = peer->identifier();
  auto conn_pair = FindConnectionById(peer_id);

  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "can't find connection for ltk (id: %s)", bt_str(peer_id));
    SendLinkKeyRequestNegativeReply(params.bd_addr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  auto& [handle, conn] = *conn_pair;

  auto link_key = conn->pairing_state().OnLinkKeyRequest();
  if (!link_key.has_value()) {
    SendLinkKeyRequestNegativeReply(params.bd_addr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  SendLinkKeyRequestReply(params.bd_addr, link_key.value());
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnLinkKeyNotification(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kLinkKeyNotificationEventCode);
  const auto& params = event.params<hci_spec::LinkKeyNotificationEventParams>();

  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  auto* peer = cache_->FindByAddress(addr);
  if (!peer) {
    bt_log(WARN, "gap-bredr",
           "no known peer with address %s found; link key not stored (key type: %u)", bt_str(addr),
           params.key_type);
    cache_->LogBrEdrBondingEvent(false);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(INFO, "gap-bredr", "got link key notification (key type: %u, peer: %s)", params.key_type,
         bt_str(peer->identifier()));

  auto key_type = hci_spec::LinkKeyType{params.key_type};
  sm::SecurityProperties sec_props;
  if (key_type == hci_spec::LinkKeyType::kChangedCombination) {
    if (!peer->bredr() || !peer->bredr()->bonded()) {
      bt_log(WARN, "gap-bredr", "can't update link key of unbonded peer %s",
             bt_str(peer->identifier()));
      cache_->LogBrEdrBondingEvent(false);
      return hci::CommandChannel::EventCallbackResult::kContinue;
    }

    // Reuse current properties
    ZX_DEBUG_ASSERT(peer->bredr()->link_key());
    sec_props = peer->bredr()->link_key()->security();
    key_type = *sec_props.GetLinkKeyType();
  } else {
    sec_props = sm::SecurityProperties(key_type);
  }

  auto peer_id = peer->identifier();

  if (sec_props.level() == sm::SecurityLevel::kNoSecurity) {
    bt_log(WARN, "gap-bredr", "link key for peer %s has insufficient security; not stored",
           bt_str(peer_id));
    cache_->LogBrEdrBondingEvent(false);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  UInt128 key_value;
  std::copy(params.link_key, &params.link_key[key_value.size()], key_value.begin());
  hci_spec::LinkKey hci_key(key_value, 0, 0);
  sm::LTK key(sec_props, hci_key);

  auto handle = FindConnectionById(peer_id);
  if (!handle) {
    bt_log(WARN, "gap-bredr", "can't find current connection for ltk (peer: %s)", bt_str(peer_id));
  } else {
    handle->second->link().set_bredr_link_key(hci_key, key_type);
    handle->second->pairing_state().OnLinkKeyNotification(key_value, key_type);
  }

  if (cache_->StoreBrEdrBond(addr, key)) {
    cache_->LogBrEdrBondingEvent(true);
  } else {
    cache_->LogBrEdrBondingEvent(false);
    bt_log(ERROR, "gap-bredr", "failed to cache bonding data (peer: %s)", bt_str(peer_id));
  }
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnSimplePairingComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kSimplePairingCompleteEventCode);
  const auto& params = event.params<hci_spec::SimplePairingCompleteEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "got Simple Pairing Complete (status: %s) for unconnected addr %s",
           bt_str(hci::Status(params.status)), bt_str(params.bd_addr));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  conn_pair->second->pairing_state().OnSimplePairingComplete(params.status);
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnUserConfirmationRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kUserConfirmationRequestEventCode);
  const auto& params = event.params<hci_spec::UserConfirmationRequestEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "got %s for unconnected addr %s", __func__, bt_str(params.bd_addr));
    SendUserConfirmationRequestNegativeReply(params.bd_addr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto confirm_cb = [this, self = weak_ptr_factory_.GetWeakPtr(),
                     bd_addr = params.bd_addr](bool confirm) {
    if (!self) {
      return;
    }

    if (confirm) {
      SendUserConfirmationRequestReply(bd_addr);
    } else {
      SendUserConfirmationRequestNegativeReply(bd_addr);
    }
  };
  conn_pair->second->pairing_state().OnUserConfirmationRequest(letoh32(params.numeric_value),
                                                               std::move(confirm_cb));
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnUserPasskeyRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kUserPasskeyRequestEventCode);
  const auto& params = event.params<hci_spec::UserPasskeyRequestEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "got %s for unconnected addr %s", __func__, bt_str(params.bd_addr));
    SendUserPasskeyRequestNegativeReply(params.bd_addr);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto passkey_cb = [this, self = weak_ptr_factory_.GetWeakPtr(),
                     bd_addr = params.bd_addr](std::optional<uint32_t> passkey) {
    if (!self) {
      return;
    }

    if (passkey) {
      SendUserPasskeyRequestReply(bd_addr, *passkey);
    } else {
      SendUserPasskeyRequestNegativeReply(bd_addr);
    }
  };
  conn_pair->second->pairing_state().OnUserPasskeyRequest(std::move(passkey_cb));
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnUserPasskeyNotification(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kUserPasskeyNotificationEventCode);
  const auto& params = event.params<hci_spec::UserPasskeyNotificationEventParams>();

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "got %s for unconnected addr %s", __func__, bt_str(params.bd_addr));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  conn_pair->second->pairing_state().OnUserPasskeyNotification(letoh32(params.numeric_value));
  return hci::CommandChannel::EventCallbackResult::kContinue;
}

hci::CommandChannel::EventCallbackResult BrEdrConnectionManager::OnRoleChange(
    const hci::EventPacket& event) {
  ZX_ASSERT(event.event_code() == hci_spec::kRoleChangeEventCode);
  const auto& params = event.params<hci_spec::RoleChangeEventParams>();

  DeviceAddress address(DeviceAddress::Type::kBREDR, params.bd_addr);
  Peer* peer = cache_->FindByAddress(address);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "got %s for unknown peer (address: %s)", __func__, bt_str(address));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }
  PeerId peer_id = peer->identifier();

  // When a role change is requested in the HCI_Accept_Connection_Request command, a HCI_Role_Change
  // event may be received prior to the HCI_Connection_Complete event (so no connection object will
  // exist yet) (Core Spec v5.2, Vol 2, Part F, Sec 3.1).
  auto request_iter = connection_requests_.find(peer_id);
  if (request_iter != connection_requests_.end()) {
    request_iter->second.set_role_change(params.new_role);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto conn_pair = FindConnectionByAddress(params.bd_addr);
  if (!conn_pair) {
    bt_log(WARN, "gap-bredr", "got %s for unconnected peer %s", __func__, bt_str(peer_id));
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  hci::Connection::Role new_role = params.new_role == hci_spec::ConnectionRole::kMaster
                                       ? hci::Connection::Role::kMaster
                                       : hci::Connection::Role::kPeripheral;
  const char* new_role_str = new_role == hci::Connection::Role::kMaster ? "leader" : "follower";

  if (hci_is_error(event, WARN, "gap-bredr", "role change failed and remains %s (peer: %s)",
                   new_role_str, bt_str(peer_id))) {
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(DEBUG, "gap-bredr", "role changed to %s (peer: %s)", new_role_str, bt_str(peer_id));
  conn_pair->second->link().set_role(new_role);

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

bool BrEdrConnectionManager::Connect(PeerId peer_id, ConnectResultCallback on_connection_result) {
  Peer* peer = cache_->FindById(peer_id);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "%s: peer not found (peer: %s)", __func__, bt_str(peer_id));
    return false;
  }

  if (peer->technology() == TechnologyType::kLowEnergy) {
    bt_log(ERROR, "gap-bredr", "peer does not support BrEdr: %s", bt_str(*peer));
    return false;
  }

  // Br/Edr peers should always be connectable by definition
  ZX_ASSERT(peer->connectable());

  // Succeed immediately or after interrogation if there is already an active connection.
  auto conn = FindConnectionById(peer_id);
  if (conn) {
    conn->second->AddRequestCallback(std::move(on_connection_result));
    return true;
  }

  // If we are already waiting to connect to |peer_id| then we store
  // |on_connection_result| to be processed after the connection attempt
  // completes (in either success of failure).
  auto pending_iter = connection_requests_.find(peer_id);
  if (pending_iter != connection_requests_.end()) {
    pending_iter->second.AddCallback(std::move(on_connection_result));
    return true;
  }
  // If we are not already connected or pending, initiate a new connection
  auto [request_iter, _] = connection_requests_.try_emplace(
      peer_id, peer->address(), peer_id, peer->MutBrEdr().RegisterInitializingConnection(),
      std::move(on_connection_result));
  request_iter->second.AttachInspect(
      inspect_properties_.requests_node_,
      inspect_properties_.requests_node_.UniqueName(kInspectRequestNodeNamePrefix));

  TryCreateNextConnection();

  return true;
}

std::optional<BrEdrConnectionManager::CreateConnectionParams>
BrEdrConnectionManager::NextCreateConnectionParams() {
  if (connection_requests_.empty()) {
    bt_log(TRACE, "gap-bredr", "no pending requests remaining");
    return std::nullopt;
  }

  Peer* peer = nullptr;
  // We use a rough heuristic of ordering likely connection requests by presence in the peer cache.
  // If a peer is still in the cache, that implies it was seen more recently which is likely to
  // correlate with being physically close and therefore still in range when we attempt to connect.
  //
  // So first try a request for which we have a peer struct:
  for (auto& [identifier, request] : connection_requests_) {
    const auto& addr = request.address();
    peer = cache_->FindByAddress(addr);
    if (peer && peer->bredr() && !request.HasIncoming())
      return std::optional(CreateConnectionParams{peer->identifier(), addr,
                                                  peer->bredr()->clock_offset(),
                                                  peer->bredr()->page_scan_repetition_mode()});
  }

  // Otherwise, fall back to any other requests - it is entirely possible that while a connection is
  // pending, discovery has ended and the peer which was intended to be connected to has timed out
  // of the peer cache, but may still be in range and connectable.
  for (auto& [identifier, request] : connection_requests_) {
    if (!request.HasIncoming()) {
      return std::optional(
          CreateConnectionParams{identifier, request.address(), std::nullopt, std::nullopt});
    }
  }
  // Finally, if we didn't find a connection request we could process at this time:
  return std::nullopt;
}

void BrEdrConnectionManager::TryCreateNextConnection() {
  // There can only be one outstanding BrEdr CreateConnection request at a time
  if (pending_request_) {
    return;
  }

  auto next = NextCreateConnectionParams();
  if (next) {
    InitiatePendingConnection(*next);
  }
}

void BrEdrConnectionManager::InitiatePendingConnection(CreateConnectionParams params) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_failure = [self, addr = params.addr](hci::Status status, auto peer_id) {
    if (self && !status)
      self->CompleteRequest(peer_id, addr, status, /*handle=*/0);
  };
  auto on_timeout = [self] {
    if (self)
      self->OnRequestTimeout();
  };
  pending_request_.emplace(params.peer_id, params.addr, on_timeout);
  pending_request_->CreateConnection(hci_->command_channel(), dispatcher_, params.clock_offset,
                                     params.page_scan_repetition_mode, request_timeout_,
                                     on_failure);
}

void BrEdrConnectionManager::OnRequestTimeout() {
  if (pending_request_) {
    pending_request_->Timeout();
    SendCreateConnectionCancelCommand(pending_request_->peer_address());
  }
}

void BrEdrConnectionManager::SendCreateConnectionCancelCommand(DeviceAddress addr) {
  auto cancel = hci::CommandPacket::New(hci_spec::kCreateConnectionCancel,
                                        sizeof(hci_spec::CreateConnectionCancelCommandParams));
  auto params = cancel->mutable_payload<hci_spec::CreateConnectionCancelCommandParams>();
  params->bd_addr = addr.value();
  hci_->command_channel()->SendCommand(std::move(cancel), [](auto, const hci::EventPacket& event) {
    hci_is_error(event, WARN, "hci-bredr", "failed to cancel connection request");
  });
}

void BrEdrConnectionManager::SendAuthenticationRequested(hci_spec::ConnectionHandle handle,
                                                         hci::StatusCallback cb) {
  auto auth_request = hci::CommandPacket::New(
      hci_spec::kAuthenticationRequested, sizeof(hci_spec::AuthenticationRequestedCommandParams));
  auth_request->mutable_payload<hci_spec::AuthenticationRequestedCommandParams>()
      ->connection_handle = htole16(handle);

  // Complete on command status because Authentication Complete Event is already registered.
  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }
  hci_->command_channel()->SendCommand(std::move(auth_request), std::move(command_cb),
                                       hci_spec::kCommandStatusEventCode);
}

void BrEdrConnectionManager::SendIoCapabilityRequestReply(
    DeviceAddressBytes bd_addr, hci_spec::IOCapability io_capability, uint8_t oob_data_present,
    hci_spec::AuthRequirements auth_requirements, hci::StatusCallback cb) {
  auto packet = hci::CommandPacket::New(hci_spec::kIOCapabilityRequestReply,
                                        sizeof(hci_spec::IOCapabilityRequestReplyCommandParams));
  auto params = packet->mutable_payload<hci_spec::IOCapabilityRequestReplyCommandParams>();
  params->bd_addr = bd_addr;
  params->io_capability = io_capability;
  params->oob_data_present = oob_data_present;
  params->auth_requirements = auth_requirements;
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendIoCapabilityRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                                  hci_spec::StatusCode reason,
                                                                  hci::StatusCallback cb) {
  auto packet =
      hci::CommandPacket::New(hci_spec::kIOCapabilityRequestNegativeReply,
                              sizeof(hci_spec::IOCapabilityRequestNegativeReplyCommandParams));
  auto params = packet->mutable_payload<hci_spec::IOCapabilityRequestNegativeReplyCommandParams>();
  params->bd_addr = bd_addr;
  params->reason = reason;
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendUserConfirmationRequestReply(DeviceAddressBytes bd_addr,
                                                              hci::StatusCallback cb) {
  auto packet =
      hci::CommandPacket::New(hci_spec::kUserConfirmationRequestReply,
                              sizeof(hci_spec::UserConfirmationRequestReplyCommandParams));
  packet->mutable_payload<hci_spec::UserConfirmationRequestReplyCommandParams>()->bd_addr = bd_addr;
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendUserConfirmationRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                                      hci::StatusCallback cb) {
  auto packet =
      hci::CommandPacket::New(hci_spec::kUserConfirmationRequestNegativeReply,
                              sizeof(hci_spec::UserConfirmationRequestNegativeReplyCommandParams));
  packet->mutable_payload<hci_spec::UserConfirmationRequestNegativeReplyCommandParams>()->bd_addr =
      bd_addr;
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendUserPasskeyRequestReply(DeviceAddressBytes bd_addr,
                                                         uint32_t numeric_value,
                                                         hci::StatusCallback cb) {
  auto packet = hci::CommandPacket::New(hci_spec::kUserPasskeyRequestReply,
                                        sizeof(hci_spec::UserPasskeyRequestReplyCommandParams));
  auto params = packet->mutable_payload<hci_spec::UserPasskeyRequestReplyCommandParams>();
  params->bd_addr = bd_addr;
  params->numeric_value = htole32(numeric_value);
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendUserPasskeyRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                                 hci::StatusCallback cb) {
  auto packet =
      hci::CommandPacket::New(hci_spec::kUserPasskeyRequestNegativeReply,
                              sizeof(hci_spec::UserPasskeyRequestNegativeReplyCommandParams));
  packet->mutable_payload<hci_spec::UserPasskeyRequestNegativeReplyCommandParams>()->bd_addr =
      bd_addr;
  SendCommandWithStatusCallback(std::move(packet), std::move(cb));
}

void BrEdrConnectionManager::SendLinkKeyRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                             hci::StatusCallback cb) {
  auto negative_reply =
      hci::CommandPacket::New(hci_spec::kLinkKeyRequestNegativeReply,
                              sizeof(hci_spec::LinkKeyRequestNegativeReplyCommandParams));
  auto negative_reply_params =
      negative_reply->mutable_payload<hci_spec::LinkKeyRequestNegativeReplyCommandParams>();
  negative_reply_params->bd_addr = bd_addr;
  SendCommandWithStatusCallback(std::move(negative_reply), std::move(cb));
}

void BrEdrConnectionManager::SendLinkKeyRequestReply(DeviceAddressBytes bd_addr,
                                                     hci_spec::LinkKey link_key,
                                                     hci::StatusCallback cb) {
  auto reply = hci::CommandPacket::New(hci_spec::kLinkKeyRequestReply,
                                       sizeof(hci_spec::LinkKeyRequestReplyCommandParams));
  auto reply_params = reply->mutable_payload<hci_spec::LinkKeyRequestReplyCommandParams>();
  reply_params->bd_addr = bd_addr;
  const auto& key_value = link_key.value();
  std::copy(key_value.begin(), key_value.end(), reply_params->link_key);
  SendCommandWithStatusCallback(std::move(reply), std::move(cb));
}

void BrEdrConnectionManager::SendCommandWithStatusCallback(
    std::unique_ptr<hci::CommandPacket> command_packet, hci::StatusCallback cb) {
  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }
  hci_->command_channel()->SendCommand(std::move(command_packet), std::move(command_cb));
}

void BrEdrConnectionManager::SendAcceptConnectionRequest(DeviceAddressBytes addr,
                                                         hci::StatusCallback cb) {
  auto accept = hci::CommandPacket::New(hci_spec::kAcceptConnectionRequest,
                                        sizeof(hci_spec::AcceptConnectionRequestCommandParams));
  auto accept_params = accept->mutable_payload<hci_spec::AcceptConnectionRequestCommandParams>();
  accept_params->bd_addr = addr;
  // This role switch preference can fail. A HCI_Role_Change event will be generated if the role
  // switch is successful (Core Spec v5.2, Vol 2, Part F, Sec 3.1).
  accept_params->role = hci_spec::ConnectionRole::kMaster;

  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }

  hci_->command_channel()->SendCommand(std::move(accept), std::move(command_cb),
                                       hci_spec::kCommandStatusEventCode);
}

void BrEdrConnectionManager::SendRejectConnectionRequest(DeviceAddress addr,
                                                         hci_spec::StatusCode reason,
                                                         hci::StatusCallback cb) {
  auto reject = hci::CommandPacket::New(hci_spec::kRejectConnectionRequest,
                                        sizeof(hci_spec::RejectConnectionRequestCommandParams));
  auto reject_params = reject->mutable_payload<hci_spec::RejectConnectionRequestCommandParams>();
  reject_params->bd_addr = addr.value();
  reject_params->reason = reason;

  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }

  hci_->command_channel()->SendCommand(std::move(reject), std::move(command_cb),
                                       hci_spec::kCommandStatusEventCode);
}

void BrEdrConnectionManager::SendRejectSynchronousRequest(DeviceAddress addr,
                                                          hci_spec::StatusCode reason,
                                                          hci::StatusCallback cb) {
  auto reject =
      hci::CommandPacket::New(hci_spec::kRejectSynchronousConnectionRequest,
                              sizeof(hci_spec::RejectSynchronousConnectionRequestCommandParams));
  auto reject_params =
      reject->mutable_payload<hci_spec::RejectSynchronousConnectionRequestCommandParams>();
  reject_params->bd_addr = addr.value();
  reject_params->reason = reason;

  hci::CommandChannel::CommandCallback command_cb;
  if (cb) {
    command_cb = [cb = std::move(cb)](auto, const hci::EventPacket& event) {
      cb(event.ToStatus());
    };
  }

  hci_->command_channel()->SendCommand(std::move(reject), std::move(command_cb),
                                       hci_spec::kCommandStatusEventCode);
}

void BrEdrConnectionManager::RecordDisconnectInspect(const BrEdrConnection& conn) {
  // Add item to recent disconnections list.
  auto& inspect_item = inspect_properties_.last_disconnected_list.CreateItem();
  inspect_item.node.CreateString(kInspectLastDisconnectedItemPeerPropertyName,
                                 conn.peer_id().ToString(), &inspect_item.values);
  inspect_item.node.CreateUint(kInspectLastDisconnectedItemDurationPropertyName,
                               conn.duration().to_secs(), &inspect_item.values);
  inspect_item.node.CreateInt(kInspectTimestampPropertyName, async::Now(dispatcher_).get(),
                              &inspect_item.values);
}

}  // namespace bt::gap
