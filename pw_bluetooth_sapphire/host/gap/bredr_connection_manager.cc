// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_manager.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt {
namespace gap {

using common::HostError;

namespace {

void SetPageScanEnabled(bool enabled, fxl::RefPtr<hci::Transport> hci,
                        async_dispatcher_t* dispatcher,
                        hci::StatusCallback cb) {
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
    auto write_enable = hci::CommandPacket::New(
        hci::kWriteScanEnable, sizeof(hci::WriteScanEnableCommandParams));
    write_enable->mutable_view()
        ->mutable_payload<hci::WriteScanEnableCommandParams>()
        ->scan_enable = scan_type;
    hci->command_channel()->SendCommand(
        std::move(write_enable), dispatcher,
        [cb = std::move(finish_cb), enabled](
            auto, const hci::EventPacket& event) { cb(event.ToStatus()); });
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
  return event_id;
}

BrEdrConnectionManager::BrEdrConnectionManager(
    fxl::RefPtr<hci::Transport> hci, RemoteDeviceCache* device_cache,
    fbl::RefPtr<data::Domain> data_domain, bool use_interlaced_scan)
    : hci_(hci),
      cache_(device_cache),
      data_domain_(data_domain),
      interrogator_(cache_, hci_, async_get_default_dispatcher()),
      page_scan_interval_(0),
      page_scan_window_(0),
      use_interlaced_scan_(use_interlaced_scan),
      dispatcher_(async_get_default_dispatcher()),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(cache_);
  ZX_DEBUG_ASSERT(data_domain_);
  ZX_DEBUG_ASSERT(dispatcher_);

  hci_cmd_runner_ =
      std::make_unique<hci::SequentialCommandRunner>(dispatcher_, hci_);

  // Register event handlers
  conn_complete_handler_id_ = AddEventHandler(
      hci::kConnectionCompleteEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnConnectionComplete));
  conn_request_handler_id_ = AddEventHandler(
      hci::kConnectionRequestEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnConnectionRequest));
  disconn_cmpl_handler_id_ = AddEventHandler(
      hci::kDisconnectionCompleteEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnDisconnectionComplete));
  link_key_request_handler_id_ = AddEventHandler(
      hci::kLinkKeyRequestEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnLinkKeyRequest));
  link_key_notification_handler_id_ = AddEventHandler(
      hci::kLinkKeyNotificationEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnLinkKeyNotification));
  io_cap_req_handler_id_ = AddEventHandler(
      hci::kIOCapabilityRequestEventCode,
      fbl::BindMember(this, &BrEdrConnectionManager::OnIOCapabilitiesRequest));
  user_conf_handler_id_ = AddEventHandler(
      hci::kUserConfirmationRequestEventCode,
      fbl::BindMember(this,
                      &BrEdrConnectionManager::OnUserConfirmationRequest));
}

BrEdrConnectionManager::~BrEdrConnectionManager() {
  // Disconnect any connections that we're holding.
  connections_.clear();
  SetPageScanEnabled(false, hci_, dispatcher_, [](const auto) {});
  hci_->command_channel()->RemoveEventHandler(conn_request_handler_id_);
  hci_->command_channel()->RemoveEventHandler(conn_complete_handler_id_);
  hci_->command_channel()->RemoveEventHandler(disconn_cmpl_handler_id_);
  hci_->command_channel()->RemoveEventHandler(link_key_request_handler_id_);
  hci_->command_channel()->RemoveEventHandler(
      link_key_notification_handler_id_);
  hci_->command_channel()->RemoveEventHandler(io_cap_req_handler_id_);
  hci_->command_channel()->RemoveEventHandler(user_conf_handler_id_);
}

void BrEdrConnectionManager::SetConnectable(bool connectable,
                                            hci::StatusCallback status_cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!connectable) {
    SetPageScanEnabled(false, hci_, dispatcher_,
                       [self, cb = std::move(status_cb)](const auto& status) {
                         if (self) {
                           self->page_scan_interval_ = 0;
                           self->page_scan_window_ = 0;
                         } else if (status) {
                           cb(hci::Status(common::HostError::kFailed));
                           return;
                         }
                         cb(status);
                       });
    return;
  }

  WritePageScanSettings(
      hci::kPageScanR1Interval, hci::kPageScanR1Window, use_interlaced_scan_,
      [self, cb = std::move(status_cb)](const auto& status) mutable {
        if (bt_is_error(status, WARN, "gap-bredr",
                        "Write Page Scan Settings failed")) {
          cb(status);
          return;
        }
        if (!self) {
          cb(hci::Status(common::HostError::kFailed));
          return;
        }
        SetPageScanEnabled(true, self->hci_, self->dispatcher_, std::move(cb));
      });
}

void BrEdrConnectionManager::SetPairingDelegate(
    fxl::WeakPtr<PairingDelegate> delegate) {
  // TODO(armansito): implement
}

DeviceId BrEdrConnectionManager::GetPeerId(hci::ConnectionHandle handle) const {
  auto it = connections_.find(handle);
  if (it == connections_.end()) {
    return common::kInvalidDeviceId;
  }

  auto* device = cache_->FindDeviceByAddress(it->second->peer_address());
  ZX_DEBUG_ASSERT_MSG(device, "Couldn't find device for handle %#.4x", handle);
  return device->identifier();
}

bool BrEdrConnectionManager::OpenL2capChannel(DeviceId device_id,
                                              l2cap::PSM psm, SocketCallback cb,
                                              async_dispatcher_t* dispatcher) {
  auto* device = cache_->FindDeviceById(device_id);
  if (!device || !device->bredr() || !device->bredr()->connected()) {
    return false;
  }

  auto it = std::find_if(connections_.begin(), connections_.end(),
                         [&](const auto& x) {
                           return x.second->peer_address() == device->address();
                         });

  // If we're connected we must have an ID
  ZX_DEBUG_ASSERT_MSG(it != connections_.end(),
                      "couldn't find handle for device %s", bt_str(device_id));

  data_domain_->OpenL2capChannel(
      it->first, psm,
      [cb = std::move(cb)](zx::socket s, auto) { cb(std::move(s)); },
      dispatcher);
  return true;
}

BrEdrConnectionManager::SearchId BrEdrConnectionManager::AddServiceSearch(
    const common::UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
    BrEdrConnectionManager::SearchCallback callback) {
  return discoverer_.AddSearch(uuid, std::move(attributes),
                               std::move(callback));
}

bool BrEdrConnectionManager::RemoveServiceSearch(SearchId id) {
  return discoverer_.RemoveSearch(id);
}

void BrEdrConnectionManager::WritePageScanSettings(uint16_t interval,
                                                   uint16_t window,
                                                   bool interlaced,
                                                   hci::StatusCallback cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!hci_cmd_runner_->IsReady()) {
    // TODO(jamuraa): could run the three "settings" commands in parallel and
    // remove the sequence runner.
    cb(hci::Status(common::HostError::kInProgress));
    return;
  }

  auto write_activity =
      hci::CommandPacket::New(hci::kWritePageScanActivity,
                              sizeof(hci::WritePageScanActivityCommandParams));
  auto* activity_params =
      write_activity->mutable_view()
          ->mutable_payload<hci::WritePageScanActivityCommandParams>();
  activity_params->page_scan_interval = htole16(interval);
  activity_params->page_scan_window = htole16(window);

  hci_cmd_runner_->QueueCommand(
      std::move(write_activity),
      [self, interval, window](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr",
                                  "write page scan activity failed")) {
          return;
        }

        self->page_scan_interval_ = interval;
        self->page_scan_window_ = window;

        bt_log(SPEW, "gap-bredr", "page scan activity updated");
      });

  auto write_type = hci::CommandPacket::New(
      hci::kWritePageScanType, sizeof(hci::WritePageScanTypeCommandParams));
  auto* type_params =
      write_type->mutable_view()
          ->mutable_payload<hci::WritePageScanTypeCommandParams>();
  type_params->page_scan_type = (interlaced ? hci::PageScanType::kInterlacedScan
                                            : hci::PageScanType::kStandardScan);

  hci_cmd_runner_->QueueCommand(
      std::move(write_type), [self, interlaced](const hci::EventPacket& event) {
        if (!self || hci_is_error(event, WARN, "gap-bredr",
                                  "write page scan type failed")) {
          return;
        }

        self->page_scan_type_ = (interlaced ? hci::PageScanType::kInterlacedScan
                                            : hci::PageScanType::kStandardScan);

        bt_log(SPEW, "gap-bredr", "page scan type updated");
      });

  hci_cmd_runner_->RunCommands(std::move(cb));
}

void BrEdrConnectionManager::OnConnectionRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kConnectionRequestEventCode);
  const auto& params =
      event.view().payload<hci::ConnectionRequestEventParams>();
  std::string link_type_str =
      params.link_type == hci::LinkType::kACL ? "ACL" : "(e)SCO";

  bt_log(TRACE, "gap-bredr", "%s conn request from %s (%s)",
         link_type_str.c_str(), params.bd_addr.ToString().c_str(),
         params.class_of_device.ToString().c_str());

  if (params.link_type == hci::LinkType::kACL) {
    // Accept the connection, performing a role switch. We receive a
    // Connection Complete event when the connection is complete, and finish
    // the link then.
    bt_log(INFO, "gap-bredr", "accept incoming connection");

    auto accept = hci::CommandPacket::New(
        hci::kAcceptConnectionRequest,
        sizeof(hci::AcceptConnectionRequestCommandParams));
    auto accept_params =
        accept->mutable_view()
            ->mutable_payload<hci::AcceptConnectionRequestCommandParams>();
    accept_params->bd_addr = params.bd_addr;
    accept_params->role = hci::ConnectionRole::kMaster;

    hci_->command_channel()->SendCommand(std::move(accept), dispatcher_,
                                         nullptr, hci::kCommandStatusEventCode);
    return;
  }

  // Reject this connection.
  bt_log(INFO, "gap-bredr", "reject unsupported connection");

  auto reject = hci::CommandPacket::New(
      hci::kRejectConnectionRequest,
      sizeof(hci::RejectConnectionRequestCommandParams));
  auto reject_params =
      reject->mutable_view()
          ->mutable_payload<hci::RejectConnectionRequestCommandParams>();
  reject_params->bd_addr = params.bd_addr;
  reject_params->reason = hci::StatusCode::kConnectionRejectedBadBdAddr;

  hci_->command_channel()->SendCommand(std::move(reject), dispatcher_, nullptr,
                                       hci::kCommandStatusEventCode);
}

void BrEdrConnectionManager::OnConnectionComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kConnectionCompleteEventCode);
  const auto& params =
      event.view().payload<hci::ConnectionCompleteEventParams>();
  bt_log(TRACE, "gap-bredr",
         "%s connection complete (status %#.2x, handle: %#.4x)",
         params.bd_addr.ToString().c_str(), params.status,
         params.connection_handle);
  if (hci_is_error(event, WARN, "gap-bredr", "connection error")) {
    return;
  }
  common::DeviceAddress addr(common::DeviceAddress::Type::kBREDR,
                             params.bd_addr);

  // TODO(jamuraa): support non-master connections.
  auto conn_ptr = hci::Connection::CreateACL(
      params.connection_handle, hci::Connection::Role::kMaster,
      common::DeviceAddress(),  // TODO(armansito): Pass local BD_ADDR here.
      addr, hci_);

  if (params.link_type != hci::LinkType::kACL) {
    // Drop the connection if we don't support it.
    return;
  }

  RemoteDevice* device = cache_->FindDeviceByAddress(addr);
  if (!device) {
    device = cache_->NewDevice(addr, true);
  }

  device->MutBrEdr().SetConnectionState(
      RemoteDevice::ConnectionState::kInitializing);

  // Interrogate this device to find out its version/capabilities.
  interrogator_.Start(
      device->identifier(), std::move(conn_ptr),
      [device, self = weak_ptr_factory_.GetWeakPtr()](auto status,
                                                      auto conn_ptr) {
        if (bt_is_error(status, WARN, "gap-bredr",
                        "interrogate failed, dropping connection")) {
          return;
        }

        bt_log(SPEW, "gap-bredr", "interrogation complete for %#.4x",
               conn_ptr->handle());

        if (!self) {
          return;
        }

        // TODO(armansito): Implement this callback.
        auto security_callback = [](hci::ConnectionHandle handle,
                                    sm::SecurityLevel level, auto cb) {
          bt_log(INFO, "gap-bredr",
                 "Ignoring security upgrade request; not implemented");
          cb(sm::Status(HostError::kNotSupported));
        };

        // Register with L2CAP to handle services on the ACL signaling channel.
        self->data_domain_->AddACLConnection(
            conn_ptr->handle(), conn_ptr->role(),
            [self, conn_ptr = conn_ptr->WeakPtr()] {
              if (!self || !conn_ptr) {
                return;
              }

              bt_log(ERROR, "gap-bredr",
                     "Link error received, closing connection %#.4x",
                     conn_ptr->handle());

              // Clean up after receiving the DisconnectComplete event.
              // TODO(NET-1442): Test link error behavior using FakeDevice.
              conn_ptr->Close();
            },
            std::move(security_callback), self->dispatcher_);

        auto handle = conn_ptr->handle();

        self->connections_.emplace(handle, std::move(conn_ptr));
        device->MutBrEdr().SetConnectionState(
            RemoteDevice::ConnectionState::kConnected);

        self->data_domain_->OpenL2capChannel(
            handle, l2cap::kSDP,
            [self, peer_id = device->identifier()](auto channel) {
              if (!self)
                return;
              auto client = sdp::Client::Create(std::move(channel));

              self->discoverer_.StartServiceDiscovery(peer_id,
                                                      std::move(client));
            },
            self->dispatcher_);
      });
}

void BrEdrConnectionManager::OnDisconnectionComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kDisconnectionCompleteEventCode);
  const auto& params =
      event.view().payload<hci::DisconnectionCompleteEventParams>();

  hci::ConnectionHandle handle = le16toh(params.connection_handle);
  if (hci_is_error(event, WARN, "gap-bredr",
                   "HCI disconnection error handle %#.4x", handle)) {
    return;
  }

  auto it = connections_.find(handle);

  if (it == connections_.end()) {
    bt_log(TRACE, "gap-bredr", "disconnect from unknown handle %#.4x", handle);
    return;
  }

  auto* device = cache_->FindDeviceByAddress(it->second->peer_address());
  ZX_DEBUG_ASSERT_MSG(device, "Couldn't find RemoteDevice for handle: %#.4x",
                      handle);
  device->MutBrEdr().SetConnectionState(
      RemoteDevice::ConnectionState::kNotConnected);
  auto conn = std::move(it->second);
  connections_.erase(it);

  bt_log(INFO, "gap-bredr",
         "%s disconnected - %s, handle: %#.4x, reason: %#.2x",
         bt_str(device->identifier()), bt_str(event.ToStatus()), handle,
         params.reason);

  data_domain_->RemoveConnection(handle);

  // Connection is already closed, so we don't need to send a disconnect.
  conn->set_closed();
}

void BrEdrConnectionManager::OnLinkKeyRequest(const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLinkKeyRequestEventCode);
  const auto& params = event.view().payload<hci::LinkKeyRequestParams>();

  common::DeviceAddress addr(common::DeviceAddress::Type::kBREDR,
                             params.bd_addr);

  auto* device = cache_->FindDeviceByAddress(addr);
  if (!device || !device->bredr()->bonded()) {
    bt_log(INFO, "gap-bredr", "no known peer with address %s found",
           addr.ToString().c_str());

    auto reply = hci::CommandPacket::New(
        hci::kLinkKeyRequestNegativeReply,
        sizeof(hci::LinkKeyRequestNegativeReplyCommandParams));
    auto reply_params =
        reply->mutable_view()
            ->mutable_payload<hci::LinkKeyRequestNegativeReplyCommandParams>();

    reply_params->bd_addr = params.bd_addr;

    hci_->command_channel()->SendCommand(std::move(reply), dispatcher_,
                                         nullptr);
    return;
  }

  bt_log(INFO, "gap-bredr", "recalling link key for bonded peer %s",
         bt_str(device->identifier()));

  auto reply = hci::CommandPacket::New(
      hci::kLinkKeyRequestReply, sizeof(hci::LinkKeyRequestReplyCommandParams));
  auto reply_params =
      reply->mutable_view()
          ->mutable_payload<hci::LinkKeyRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;
  const sm::LTK& link_key = *device->bredr()->link_key();
  ZX_DEBUG_ASSERT(link_key.security().enc_key_size() == 16);
  const auto& key_value = link_key.key().value();
  std::copy(key_value.begin(), key_value.end(), reply_params->link_key);

  hci_->command_channel()->SendCommand(
      std::move(reply), dispatcher_, [](auto, const hci::EventPacket& event) {
        bt_log(SPEW, "gap-bredr", "completed Link Key Request Reply");
      });
}

void BrEdrConnectionManager::OnLinkKeyNotification(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLinkKeyNotificationEventCode);
  const auto& params =
      event.view().payload<hci::LinkKeyNotificationEventParams>();

  common::DeviceAddress addr(common::DeviceAddress::Type::kBREDR,
                             params.bd_addr);

  bt_log(TRACE, "gap-bredr", "got link key (type %u) for address %s",
         params.key_type, addr.ToString().c_str());

  auto* device = cache_->FindDeviceByAddress(addr);
  if (!device) {
    bt_log(WARN, "gap-bredr",
           "no known peer with address %s found; link key not stored",
           addr.ToString().c_str());
    return;
  }

  const auto key_type = static_cast<hci::LinkKeyType>(params.key_type);
  sm::SecurityProperties sec_props;
  if (key_type == hci::LinkKeyType::kChangedCombination) {
    if (!device->bredr() || !device->bredr()->bonded()) {
      bt_log(WARN, "gap-bredr", "can't update link key of unbonded peer %s",
             bt_str(device->identifier()));
      return;
    }

    // Reuse current properties
    ZX_DEBUG_ASSERT(device->bredr()->link_key());
    sec_props = device->bredr()->link_key()->security();
  } else {
    sec_props = sm::SecurityProperties(key_type);
  }

  if (sec_props.level() == sm::SecurityLevel::kNoSecurity) {
    bt_log(WARN, "gap-bredr",
           "link key for peer %s has insufficient security; not stored",
           bt_str(device->identifier()));
    return;
  }

  common::UInt128 key_value;
  std::copy(params.link_key, &params.link_key[key_value.size()],
            key_value.begin());
  sm::LTK key(sec_props, hci::LinkKey(key_value, 0, 0));
  if (!cache_->StoreBrEdrBond(addr, key)) {
    bt_log(ERROR, "gap-bredr", "failed to cache bonding data (id: %s)",
           bt_str(device->identifier()));
  }
}

void BrEdrConnectionManager::OnIOCapabilitiesRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kIOCapabilityRequestEventCode);
  const auto& params =
      event.view().payload<hci::IOCapabilityRequestEventParams>();

  auto reply = hci::CommandPacket::New(
      hci::kIOCapabilityRequestReply,
      sizeof(hci::IOCapabilityRequestReplyCommandParams));
  auto reply_params =
      reply->mutable_view()
          ->mutable_payload<hci::IOCapabilityRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;
  // TODO(jamuraa, BT-169): ask the PairingDelegate if it's set what the IO
  // capabilities it has.
  reply_params->io_capability = hci::IOCapability::kNoInputNoOutput;
  // TODO(BT-8): Add OOB status from RemoteDeviceCache.
  reply_params->oob_data_present = 0x00;  // None present.
  // TODO(BT-656): Determine this based on the service requirements.
  reply_params->auth_requirements = hci::AuthRequirements::kGeneralBonding;

  hci_->command_channel()->SendCommand(std::move(reply), dispatcher_, nullptr);
}

void BrEdrConnectionManager::OnUserConfirmationRequest(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kUserConfirmationRequestEventCode);
  const auto& params =
      event.view().payload<hci::UserConfirmationRequestEventParams>();

  bt_log(INFO, "gap-bredr", "auto-confirming pairing from %s (%u)",
         bt_str(params.bd_addr), params.numeric_value);

  // TODO(jamuraa, BT-169): if we are not NoInput/NoOutput then we need to ask
  // the pairing delegate.  This currently will auto accept any pairing
  // (JustWorks)
  auto reply = hci::CommandPacket::New(
      hci::kUserConfirmationRequestReply,
      sizeof(hci::UserConfirmationRequestReplyCommandParams));
  auto reply_params =
      reply->mutable_view()
          ->mutable_payload<hci::UserConfirmationRequestReplyCommandParams>();

  reply_params->bd_addr = params.bd_addr;

  hci_->command_channel()->SendCommand(std::move(reply), dispatcher_, nullptr);
}

}  // namespace gap
}  // namespace bt
