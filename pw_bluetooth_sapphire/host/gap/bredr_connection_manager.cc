// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_manager.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt {
namespace gap {

using std::unique_ptr;
using ConnectionState = Peer::ConnectionState;

namespace {

void SetPageScanEnabled(bool enabled, fxl::RefPtr<hci::Transport> hci,
                        async_dispatcher_t* dispatcher, hci::StatusCallback cb) {
  ZX_DEBUG_ASSERT(cb);
  auto read_enable = hci::CommandPacket::New(hci::kReadScanEnable);
  auto finish_enable_cb = [enabled, dispatcher, hci, finish_cb = std::move(cb)](
                              auto, const hci::EventPacket& event) mutable {
    if (hci_is_error(event, WARN, "gap-bredr", "read scan enable failed")) {
      finish_cb(event.ToStatus());
      return;
    }

    auto params = event.return_params<hci::ReadScanEnableReturnParams>();
    uint8_t scan_type = params->scan_enable;
    if (enabled) {
      scan_type |= static_cast<uint8_t>(hci::ScanEnableBit::kPage);
    } else {
      scan_type &= ~static_cast<uint8_t>(hci::ScanEnableBit::kPage);
    }
    auto write_enable =
        hci::CommandPacket::New(hci::kWriteScanEnable, sizeof(hci::WriteScanEnableCommandParams));
    write_enable->mutable_payload<hci::WriteScanEnableCommandParams>()->scan_enable = scan_type;
    hci->command_channel()->SendCommand(
        std::move(write_enable), dispatcher,
        [cb = std::move(finish_cb)](auto, const hci::EventPacket& event) { cb(event.ToStatus()); });
  };
  hci->command_channel()->SendCommand(std::move(read_enable), dispatcher,
                                      std::move(finish_enable_cb));
}

}  // namespace

hci::CommandChannel::EventHandlerId BrEdrConnectionManager::AddEventHandler(
    const hci::EventCode& code, hci::CommandChannel::EventCallback cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto event_id = hci_->command_channel()->AddEventHandler(
      code,
      [self, callback = std::move(cb)](const auto& event) {
        if (self) {
          callback(event);
        }
      },
      dispatcher_);
  ZX_DEBUG_ASSERT(event_id);
  event_handler_ids_.push_back(event_id);
  return event_id;
}

BrEdrConnectionManager::BrEdrConnectionManager(fxl::RefPtr<hci::Transport> hci,
                                               PeerCache* peer_cache, DeviceAddress local_address,
                                               fbl::RefPtr<data::Domain> data_domain,
                                               bool use_interlaced_scan)
    : hci_(hci),
      cache_(peer_cache),
      local_address_(local_address),
      data_domain_(data_domain),
      interrogator_(cache_, hci_, async_get_default_dispatcher()),
      page_scan_interval_(0),
      page_scan_window_(0),
      use_interlaced_scan_(use_interlaced_scan),
      request_timeout_(kBrEdrCreateConnectionTimeout),
      dispatcher_(async_get_default_dispatcher()),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(cache_);
  ZX_DEBUG_ASSERT(data_domain_);
  ZX_DEBUG_ASSERT(dispatcher_);

  hci_cmd_runner_ = std::make_unique<hci::SequentialCommandRunner>(dispatcher_, hci_);

  // Register event handlers
  AddEventHandler(hci::kConnectionCompleteEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnConnectionComplete));
  AddEventHandler(hci::kConnectionRequestEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnConnectionRequest));
  AddEventHandler(hci::kDisconnectionCompleteEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnDisconnectionComplete));
  AddEventHandler(hci::kLinkKeyRequestEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnLinkKeyRequest));
  AddEventHandler(hci::kLinkKeyNotificationEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnLinkKeyNotification));
  AddEventHandler(hci::kIOCapabilityRequestEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnIOCapabilitiesRequest));
  AddEventHandler(hci::kUserConfirmationRequestEventCode,
                  fbl::BindMember(this, &BrEdrConnectionManager::OnUserConfirmationRequest));
}

BrEdrConnectionManager::~BrEdrConnectionManager() {
  if (pending_request_ && pending_request_->Cancel())
    SendCreateConnectionCancelCommand(pending_request_->peer_address());

  // Disconnect any connections that we're holding.
  connections_.clear();
  // Become unconnectable
  SetPageScanEnabled(false, hci_, dispatcher_, [](const auto) {});
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
    SetPageScanEnabled(false, hci_, dispatcher_, std::move(not_connectable_cb));
    return;
  }

  WritePageScanSettings(
      hci::kPageScanR1Interval, hci::kPageScanR1Window, use_interlaced_scan_,
      [self, cb = std::move(status_cb)](const auto& status) mutable {
        if (bt_is_error(status, WARN, "gap-bredr", "Write Page Scan Settings failed")) {
          cb(status);
          return;
        }
        if (!self) {
          cb(hci::Status(HostError::kFailed));
          return;
        }
        SetPageScanEnabled(true, self->hci_, self->dispatcher_, std::move(cb));
      });
}

void BrEdrConnectionManager::SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) {
  // TODO(armansito): implement
}

PeerId BrEdrConnectionManager::GetPeerId(hci::ConnectionHandle handle) const {
  auto it = connections_.find(handle);
  if (it == connections_.end()) {
    return kInvalidPeerId;
  }

  auto* peer = cache_->FindByAddress(it->second.link().peer_address());
  ZX_DEBUG_ASSERT_MSG(peer, "Couldn't find peer for handle %#.4x", handle);
  return peer->identifier();
}

bool BrEdrConnectionManager::OpenL2capChannel(PeerId peer_id, l2cap::PSM psm, SocketCallback cb,
                                              async_dispatcher_t* dispatcher) {
  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(SPEW, "gap-bredr", "can't open l2cap %s: connection not found", bt_str(peer_id));
    return false;
  }
  auto& [handle, connection] = *conn_pair;

  if (!connection->link().ltk()) {
    // Connection doesn't have a key, initiate an authentication request.
    auto auth_request = hci::CommandPacket::New(hci::kAuthenticationRequested,
                                                sizeof(hci::AuthenticationRequestedCommandParams));
    auth_request->mutable_view()
        ->mutable_payload<hci::AuthenticationRequestedCommandParams>()
        ->connection_handle = htole16(handle);

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto retry_cb = [peer_id, psm, cb = std::move(cb), dispatcher, self](
                        auto, const auto& event) mutable {
      if (hci_is_error(event, SPEW, "gap-bredr", "auth request failed")) {
        async::PostTask(dispatcher, [cb = std::move(cb)]() { cb(zx::socket()); });
        if (self) {
          self->Disconnect(peer_id);
        }
        return;
      }

      if (event.event_code() == hci::kCommandStatusEventCode) {
        return;
      }

      if (self) {
        // We should now have an LTK so try again.
        self->OpenL2capChannel(peer_id, psm, std::move(cb), dispatcher);
      }
    };

    bt_log(SPEW, "gap-bredr", "sending auth request to peer %s", bt_str(peer_id));
    hci_->command_channel()->SendCommand(std::move(auth_request), dispatcher_, std::move(retry_cb),
                                         hci::kAuthenticationCompleteEventCode);
    return true;
  }

  bt_log(SPEW, "gap-bredr", "opening l2cap channel on %#.4x for %s", psm, bt_str(peer_id));
  data_domain_->OpenL2capChannel(
      handle, psm, [cb = std::move(cb)](zx::socket s, auto) { cb(std::move(s)); }, dispatcher);
  return true;
}

BrEdrConnectionManager::SearchId BrEdrConnectionManager::AddServiceSearch(
    const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
    BrEdrConnectionManager::SearchCallback callback) {
  return discoverer_.AddSearch(uuid, std::move(attributes), std::move(callback));
}

bool BrEdrConnectionManager::RemoveServiceSearch(SearchId id) {
  return discoverer_.RemoveSearch(id);
}

bool BrEdrConnectionManager::Disconnect(PeerId peer_id) {
  if (connection_requests_.find(peer_id) != connection_requests_.end()) {
    bt_log(WARN, "gap-bredr", "Can't disconnect peer %s because it's being connected to",
           bt_str(peer_id));
    return false;
  }

  auto conn_pair = FindConnectionById(peer_id);
  if (!conn_pair) {
    bt_log(INFO, "gap-bredr", "No need to disconnect peer (id: %s): It is not connected",
           bt_str(peer_id));
    return true;
  }

  auto [handle, connection] = *conn_pair;
  CleanUpConnection(handle, std::move(connections_.extract(handle).mapped()),
                    true /* close_link */);
  return true;
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

  auto write_activity = hci::CommandPacket::New(hci::kWritePageScanActivity,
                                                sizeof(hci::WritePageScanActivityCommandParams));
  auto* activity_params =
      write_activity->mutable_payload<hci::WritePageScanActivityCommandParams>();
  activity_params->page_scan_interval = htole16(interval);
  activity_params->page_scan_window = htole16(window);

  hci_cmd_runner_->QueueCommand(
      std::move(write_activity), [self, interval, window](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr", "write page scan activity failed")) {
          return;
        }

        self->page_scan_interval_ = interval;
        self->page_scan_window_ = window;

        bt_log(SPEW, "gap-bredr", "page scan activity updated");
      });

  auto write_type =
      hci::CommandPacket::New(hci::kWritePageScanType, sizeof(hci::WritePageScanTypeCommandParams));
  auto* type_params = write_type->mutable_payload<hci::WritePageScanTypeCommandParams>();
  type_params->page_scan_type =
      (interlaced ? hci::PageScanType::kInterlacedScan : hci::PageScanType::kStandardScan);

  hci_cmd_runner_->QueueCommand(
      std::move(write_type), [self, interlaced](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr", "write page scan type failed")) {
          return;
        }

        self->page_scan_type_ =
            (interlaced ? hci::PageScanType::kInterlacedScan : hci::PageScanType::kStandardScan);

        bt_log(SPEW, "gap-bredr", "page scan type updated");
      });

  hci_cmd_runner_->RunCommands(std::move(cb));
}

std::optional<std::pair<hci::ConnectionHandle, BrEdrConnection*>>
BrEdrConnectionManager::FindConnectionById(PeerId peer_id) {
  auto it = std::find_if(connections_.begin(), connections_.end(),
                         [peer_id](const auto& c) { return c.second.peer_id() == peer_id; });

  if (it == connections_.end()) {
    return std::nullopt;
  }

  auto& [handle, conn] = *it;
  ZX_ASSERT(conn.link().ll_type() != hci::Connection::LinkType::kLE);

  return std::pair(handle, &conn);
}

void BrEdrConnectionManager::OnConnectionRequest(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kConnectionRequestEventCode);
  const auto& params = event.params<hci::ConnectionRequestEventParams>();
  std::string link_type_str = params.link_type == hci::LinkType::kACL ? "ACL" : "(e)SCO";

  bt_log(TRACE, "gap-bredr", "%s conn request from %s (%s)", link_type_str.c_str(),
         params.bd_addr.ToString().c_str(), params.class_of_device.ToString().c_str());

  if (params.link_type == hci::LinkType::kACL) {
    // Accept the connection, performing a role switch. We receive a
    // Connection Complete event when the connection is complete, and finish
    // the link then.
    bt_log(INFO, "gap-bredr", "accept incoming connection");

    auto accept = hci::CommandPacket::New(hci::kAcceptConnectionRequest,
                                          sizeof(hci::AcceptConnectionRequestCommandParams));
    auto accept_params = accept->mutable_payload<hci::AcceptConnectionRequestCommandParams>();
    accept_params->bd_addr = params.bd_addr;
    accept_params->role = hci::ConnectionRole::kMaster;

    hci_->command_channel()->SendCommand(std::move(accept), dispatcher_, nullptr,
                                         hci::kCommandStatusEventCode);
    return;
  }

  // Reject this connection.
  bt_log(INFO, "gap-bredr", "reject unsupported connection");

  auto reject = hci::CommandPacket::New(hci::kRejectConnectionRequest,
                                        sizeof(hci::RejectConnectionRequestCommandParams));
  auto reject_params = reject->mutable_payload<hci::RejectConnectionRequestCommandParams>();
  reject_params->bd_addr = params.bd_addr;
  reject_params->reason = hci::StatusCode::kConnectionRejectedBadBdAddr;

  hci_->command_channel()->SendCommand(std::move(reject), dispatcher_, nullptr,
                                       hci::kCommandStatusEventCode);
}

Peer* BrEdrConnectionManager::FindOrInitPeer(DeviceAddress addr) {
  Peer* peer = cache_->FindByAddress(addr);
  if (!peer) {
    bool connectable = true;
    peer = cache_->NewPeer(addr, connectable);
  }
  return peer;
}

void BrEdrConnectionManager::OnConnectionComplete(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kConnectionCompleteEventCode);

  const auto& params = event.params<hci::ConnectionCompleteEventParams>();
  auto connection_handle = letoh16(params.connection_handle);
  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  bt_log(TRACE, "gap-bredr", "%s connection complete (status %#.2x, handle: %#.4x)",
         bt_str(params.bd_addr), params.status, connection_handle);

  if (pending_request_ && pending_request_->peer_address() == addr) {
    auto status = hci::Status(params.status);
    status = pending_request_->CompleteRequest(status);

    if (!status)
      OnConnectFailure(status, pending_request_->peer_id());
  }

  if (params.link_type != hci::LinkType::kACL) {
    // Drop the connection if we don't support it.
    return;
  }

  if (!hci_is_error(event, WARN, "gap-bredr", "connection error")) {
    InitializeConnection(addr, std::move(connection_handle));
  }
}

// Initialize a full Br/Edr connection from the hci::Connection |link|
// Initialization begins the interrogation process, once completed we establish
// a fully usable Br/Edr connection
void BrEdrConnectionManager::InitializeConnection(DeviceAddress addr, uint16_t connection_handle) {
  // TODO(BT-288): support non-master connections.
  auto link = hci::Connection::CreateACL(connection_handle, hci::Connection::Role::kMaster,
                                         local_address_, addr, hci_);

  Peer* peer = FindOrInitPeer(addr);
  // We should never establish more than one link to a given peer
  ZX_DEBUG_ASSERT(!FindConnectionById(peer->identifier()));
  peer->MutBrEdr().SetConnectionState(ConnectionState::kInitializing);

  // Interrogate this peer to find out its version/capabilities.
  auto self = weak_ptr_factory_.GetWeakPtr();
  interrogator_.Start(
      peer->identifier(), std::move(link), [peer, self](auto status, auto conn_ptr) {
        if (bt_is_error(status, WARN, "gap-bredr", "interrogate failed, dropping connection"))
          return;
        bt_log(SPEW, "gap-bredr", "interrogation complete for %#.4x", conn_ptr->handle());
        if (!self)
          return;
        self->EstablishConnection(peer, status, std::move(conn_ptr));
      });
}

// Establish a full BrEdrConnection for a link that has been interrogated
void BrEdrConnectionManager::EstablishConnection(Peer* peer, hci::Status status,
                                                 unique_ptr<hci::Connection> connection) {
  auto self = weak_ptr_factory_.GetWeakPtr();

  auto error_handler = [self, connection = connection->WeakPtr()] {
    if (!self || !connection)
      return;
    bt_log(ERROR, "gap-bredr", "Link error received, closing connection %#.4x",
           connection->handle());

    // Clean up after receiving the DisconnectComplete event.
    // TODO(BT-70): Test link error behavior using FakePeer.
    connection->Close();
  };

  // TODO(armansito): Implement this callback.
  auto security_callback = [](hci::ConnectionHandle handle, sm::SecurityLevel level, auto cb) {
    bt_log(INFO, "gap-bredr", "Ignoring security upgrade request; not implemented");
    cb(sm::Status(HostError::kNotSupported));
  };

  // Register with L2CAP to handle services on the ACL signaling channel.
  data_domain_->AddACLConnection(connection->handle(), connection->role(), error_handler,
                                 std::move(security_callback), dispatcher_);

  auto handle = connection->handle();

  auto conn = connections_.try_emplace(handle, peer->identifier(), std::move(connection)).first;
  peer->MutBrEdr().SetConnectionState(ConnectionState::kConnected);

  if (discoverer_.search_count()) {
    data_domain_->OpenL2capChannel(
        handle, l2cap::kSDP,
        [self, peer_id = peer->identifier()](auto channel) {
          if (!self)
            return;
          auto client = sdp::Client::Create(std::move(channel));

          self->discoverer_.StartServiceDiscovery(peer_id, std::move(client));
        },
        dispatcher_);
  }

  auto request = connection_requests_.extract(peer->identifier());
  if (request) {
    auto conn_ptr = &(conn->second);
    request.mapped().NotifyCallbacks(hci::Status(), [conn_ptr] { return conn_ptr; });
    // If this was our in-flight request, close it
    if (peer->address() == pending_request_->peer_address()) {
      pending_request_.reset();
    }
    TryCreateNextConnection();
  }
}

void BrEdrConnectionManager::OnDisconnectionComplete(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kDisconnectionCompleteEventCode);
  const auto& params = event.params<hci::DisconnectionCompleteEventParams>();

  hci::ConnectionHandle handle = le16toh(params.connection_handle);
  if (hci_is_error(event, WARN, "gap-bredr", "HCI disconnection error handle %#.4x", handle)) {
    return;
  }

  auto it = connections_.find(handle);
  if (it == connections_.end()) {
    bt_log(TRACE, "gap-bredr", "disconnect from unknown handle %#.4x", handle);
    return;
  }

  auto* peer = cache_->FindByAddress(it->second.link().peer_address());
  bt_log(INFO, "gap-bredr", "%s disconnected - %s, handle: %#.4x, reason: %#.2x",
         bt_str(peer->identifier()), bt_str(event.ToStatus()), handle, params.reason);

  CleanUpConnection(handle, std::move(connections_.extract(it).mapped()), false /* close_link */);
}

void BrEdrConnectionManager::CleanUpConnection(hci::ConnectionHandle handle, BrEdrConnection conn,
                                               bool close_link) {
  auto* peer = cache_->FindByAddress(conn.link().peer_address());
  ZX_DEBUG_ASSERT_MSG(peer, "Couldn't find peer for handle: %#.4x", handle);
  peer->MutBrEdr().SetConnectionState(ConnectionState::kNotConnected);

  data_domain_->RemoveConnection(handle);

  if (!close_link) {
    // Connection is already closed, so we don't need to send a disconnect.
    conn.link().set_closed();
  }

  // |conn| is destroyed when it goes out of scope.
}

void BrEdrConnectionManager::OnLinkKeyRequest(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLinkKeyRequestEventCode);
  const auto& params = event.params<hci::LinkKeyRequestParams>();

  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  auto* peer = cache_->FindByAddress(addr);
  if (!peer || !peer->bredr() || !peer->bredr()->bonded()) {
    bt_log(INFO, "gap-bredr", "no bonded peer with address %s found", addr.ToString().c_str());

    auto reply = hci::CommandPacket::New(hci::kLinkKeyRequestNegativeReply,
                                         sizeof(hci::LinkKeyRequestNegativeReplyCommandParams));
    auto reply_params = reply->mutable_payload<hci::LinkKeyRequestNegativeReplyCommandParams>();

    reply_params->bd_addr = params.bd_addr;

    hci_->command_channel()->SendCommand(std::move(reply), dispatcher_, nullptr);
    return;
  }

  auto peer_id = peer->identifier();
  bt_log(INFO, "gap-bredr", "recalling link key for bonded peer %s", bt_str(peer_id));

  auto reply = hci::CommandPacket::New(hci::kLinkKeyRequestReply,
                                       sizeof(hci::LinkKeyRequestReplyCommandParams));
  auto reply_params = reply->mutable_payload<hci::LinkKeyRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;
  const sm::LTK& link_key = *peer->bredr()->link_key();
  ZX_DEBUG_ASSERT(link_key.security().enc_key_size() == 16);
  const auto& hci_key = link_key.key();
  const auto& key_value = hci_key.value();
  std::copy(key_value.begin(), key_value.end(), reply_params->link_key);

  auto handle = FindConnectionById(peer_id);
  if (!handle) {
    bt_log(WARN, "gap-bredr", "can't find connection for ltk (id: %s)", bt_str(peer_id));
  } else {
    handle->second->link().set_link_key(hci_key);
  }

  hci_->command_channel()->SendCommand(
      std::move(reply), dispatcher_, [](auto, const hci::EventPacket& event) {
        bt_log(SPEW, "gap-bredr", "completed Link Key Request Reply");
      });
}

void BrEdrConnectionManager::OnLinkKeyNotification(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLinkKeyNotificationEventCode);
  const auto& params = event.params<hci::LinkKeyNotificationEventParams>();

  DeviceAddress addr(DeviceAddress::Type::kBREDR, params.bd_addr);

  bt_log(TRACE, "gap-bredr", "got link key (type %u) for address %s", params.key_type,
         addr.ToString().c_str());

  auto* peer = cache_->FindByAddress(addr);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "no known peer with address %s found; link key not stored",
           addr.ToString().c_str());
    return;
  }

  const auto key_type = static_cast<hci::LinkKeyType>(params.key_type);
  sm::SecurityProperties sec_props;
  if (key_type == hci::LinkKeyType::kChangedCombination) {
    if (!peer->bredr() || !peer->bredr()->bonded()) {
      bt_log(WARN, "gap-bredr", "can't update link key of unbonded peer %s",
             bt_str(peer->identifier()));
      return;
    }

    // Reuse current properties
    ZX_DEBUG_ASSERT(peer->bredr()->link_key());
    sec_props = peer->bredr()->link_key()->security();
  } else {
    sec_props = sm::SecurityProperties(key_type);
  }

  auto peer_id = peer->identifier();

  if (sec_props.level() == sm::SecurityLevel::kNoSecurity) {
    bt_log(WARN, "gap-bredr", "link key for peer %s has insufficient security; not stored",
           bt_str(peer_id));
    return;
  }

  UInt128 key_value;
  std::copy(params.link_key, &params.link_key[key_value.size()], key_value.begin());
  hci::LinkKey hci_key(key_value, 0, 0);
  sm::LTK key(sec_props, hci_key);

  auto handle = FindConnectionById(peer_id);
  if (!handle) {
    bt_log(WARN, "gap-bredr", "can't find current connection for ltk (id: %s)", bt_str(peer_id));
  } else {
    handle->second->link().set_link_key(hci_key);
  }

  if (!cache_->StoreBrEdrBond(addr, key)) {
    bt_log(ERROR, "gap-bredr", "failed to cache bonding data (id: %s)", bt_str(peer_id));
  }
}

void BrEdrConnectionManager::OnIOCapabilitiesRequest(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kIOCapabilityRequestEventCode);
  const auto& params = event.params<hci::IOCapabilityRequestEventParams>();

  auto reply = hci::CommandPacket::New(hci::kIOCapabilityRequestReply,
                                       sizeof(hci::IOCapabilityRequestReplyCommandParams));
  auto reply_params = reply->mutable_payload<hci::IOCapabilityRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;
  // TODO(jamuraa, BT-169): ask the PairingDelegate if it's set what the IO
  // capabilities it has.
  reply_params->io_capability = hci::IOCapability::kNoInputNoOutput;
  // TODO(BT-8): Add OOB status from PeerCache.
  reply_params->oob_data_present = 0x00;  // None present.
  // TODO(BT-656): Determine this based on the service requirements.
  reply_params->auth_requirements = hci::AuthRequirements::kGeneralBonding;

  hci_->command_channel()->SendCommand(std::move(reply), dispatcher_, nullptr);
}

void BrEdrConnectionManager::OnUserConfirmationRequest(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kUserConfirmationRequestEventCode);
  const auto& params = event.params<hci::UserConfirmationRequestEventParams>();

  bt_log(INFO, "gap-bredr", "auto-confirming pairing from %s (%u)", bt_str(params.bd_addr),
         params.numeric_value);

  // TODO(jamuraa, BT-169): if we are not NoInput/NoOutput then we need to ask
  // the pairing delegate.  This currently will auto accept any pairing
  // (JustWorks)
  auto reply = hci::CommandPacket::New(hci::kUserConfirmationRequestReply,
                                       sizeof(hci::UserConfirmationRequestReplyCommandParams));
  auto reply_params = reply->mutable_payload<hci::UserConfirmationRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;

  hci_->command_channel()->SendCommand(std::move(reply), dispatcher_, nullptr);
}

bool BrEdrConnectionManager::Connect(PeerId peer_id, ConnectResultCallback on_connection_result) {
  Peer* peer = cache_->FindById(peer_id);
  if (!peer) {
    bt_log(WARN, "gap-bredr", "peer not found (id: %s)", bt_str(peer_id));
    return false;
  }

  if (peer->technology() == TechnologyType::kLowEnergy) {
    bt_log(ERROR, "gap-bredr", "peer does not support BrEdr: %s", peer->ToString().c_str());
    return false;
  }

  // Br/Edr peers should always be connectable by definition
  ZX_ASSERT(peer->connectable());

  // Succeed immediately if there is already an active connection.
  auto conn = FindConnectionById(peer_id);
  if (conn) {
    async::PostTask(dispatcher_,
                    [conn = conn->second, on_result = std::move(on_connection_result)]() mutable {
                      on_result(hci::Status(), conn);
                    });
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
  peer->MutBrEdr().SetConnectionState(ConnectionState::kInitializing);
  connection_requests_.try_emplace(peer_id, peer->address(), std::move(on_connection_result));

  TryCreateNextConnection();

  return true;
}

void BrEdrConnectionManager::TryCreateNextConnection() {
  // There can only be one outstanding BrEdr CreateConnection request at a time
  if (pending_request_)
    return;

  if (connection_requests_.empty()) {
    bt_log(SPEW, "gap-bredr", "no pending requests remaining");
    return;
  }

  Peer* peer = nullptr;
  for (auto& request : connection_requests_) {
    const auto& next_peer_addr = request.second.address();
    peer = cache_->FindByAddress(next_peer_addr);
    if (peer && peer->bredr())
      break;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_failure = [self](hci::Status status, auto peer_id) {
    if (self && !status)
      self->OnConnectFailure(status, peer_id);
  };
  auto on_timeout = [self] {
    if (self)
      self->OnRequestTimeout();
  };

  if (peer) {
    pending_request_.emplace(peer->identifier(), peer->address(), on_timeout);
    pending_request_->CreateConnection(
        hci_->command_channel(), dispatcher_, peer->bredr()->clock_offset(),
        peer->bredr()->page_scan_repetition_mode(), request_timeout_, on_failure);
  } else {
    // If there are no pending requests for peers which are in the cache, try
    // to connect to a peer which has left the cache, in case it is still
    // possible
    auto request = connection_requests_.begin();
    if (request != connection_requests_.end()) {
      auto identifier = request->first;
      auto address = request->second.address();

      pending_request_.emplace(identifier, address, on_timeout);
      pending_request_->CreateConnection(hci_->command_channel(), dispatcher_, std::nullopt,
                                         std::nullopt, request_timeout_, on_failure);
    }
  }
}

void BrEdrConnectionManager::OnConnectFailure(hci::Status status, PeerId peer_id) {
  // The request failed or timed out.
  bt_log(ERROR, "gap-bredr", "failed to connect to peer (id: %s)", bt_str(peer_id));
  Peer* peer = cache_->FindById(peer_id);
  // The peer may no longer be in the cache by the time this function is called
  if (peer) {
    peer->MutBrEdr().SetConnectionState(ConnectionState::kNotConnected);
  }

  pending_request_.reset();

  // Notify the matching pending callbacks about the failure.
  auto request = connection_requests_.extract(peer_id);
  ZX_DEBUG_ASSERT(request);
  request.mapped().NotifyCallbacks(status, [] { return nullptr; });

  // Process the next pending attempt.
  TryCreateNextConnection();
}

void BrEdrConnectionManager::OnRequestTimeout() {
  if (pending_request_) {
    pending_request_->Timeout();
    SendCreateConnectionCancelCommand(pending_request_->peer_address());
  }
}

void BrEdrConnectionManager::SendCreateConnectionCancelCommand(DeviceAddress addr) {
  auto cancel = hci::CommandPacket::New(hci::kCreateConnectionCancel,
                                        sizeof(hci::CreateConnectionCancelCommandParams));
  auto params = cancel->mutable_payload<hci::CreateConnectionCancelCommandParams>();
  params->bd_addr = addr.value();
  hci_->command_channel()->SendCommand(
      std::move(cancel), dispatcher_, [](auto, const hci::EventPacket& event) {
        hci_is_error(event, WARN, "hci-bredr", "failed to cancel connection request");
      });
}

}  // namespace gap
}  // namespace bt
