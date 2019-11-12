// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_controller.h"

#include <endian.h>
#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {

namespace testing {
namespace {

template <typename NUM_TYPE, typename ENUM_TYPE>
void SetBit(NUM_TYPE* num_type, ENUM_TYPE bit) {
  *num_type |= static_cast<NUM_TYPE>(bit);
}

template <typename NUM_TYPE, typename ENUM_TYPE>
void UnsetBit(NUM_TYPE* num_type, ENUM_TYPE bit) {
  *num_type &= ~static_cast<NUM_TYPE>(bit);
}

template <typename NUM_TYPE, typename ENUM_TYPE>
bool CheckBit(NUM_TYPE num_type, ENUM_TYPE bit) {
  return (num_type & static_cast<NUM_TYPE>(bit));
}

hci::LEPeerAddressType ToPeerAddrType(DeviceAddress::Type type) {
  hci::LEPeerAddressType result = hci::LEPeerAddressType::kAnonymous;

  switch (type) {
    case DeviceAddress::Type::kLEPublic:
      result = hci::LEPeerAddressType::kPublic;
      break;
    case DeviceAddress::Type::kLERandom:
      result = hci::LEPeerAddressType::kRandom;
      break;
    default:
      break;
  }

  return result;
}

}  // namespace

FakeController::Settings::Settings() {
  std::memset(this, 0, sizeof(*this));
  hci_version = hci::HCIVersion::k5_0;
  num_hci_command_packets = 250;
}

void FakeController::Settings::ApplyDualModeDefaults() {
  std::memset(this, 0, sizeof(*this));
  hci_version = hci::HCIVersion::k5_0;
  num_hci_command_packets = 250;

  acl_data_packet_length = 512;
  total_num_acl_data_packets = 1;
  le_acl_data_packet_length = 512;
  le_total_num_acl_data_packets = 1;

  SetBit(&lmp_features_page0, hci::LMPFeature::kLESupported);
  SetBit(&lmp_features_page0, hci::LMPFeature::kSimultaneousLEAndBREDR);
  SetBit(&lmp_features_page0, hci::LMPFeature::kExtendedFeatures);
  SetBit(&lmp_features_page0, hci::LMPFeature::kRSSIwithInquiryResults);
  SetBit(&lmp_features_page0, hci::LMPFeature::kExtendedInquiryResponse);

  AddBREDRSupportedCommands();
  AddLESupportedCommands();
}

void FakeController::Settings::ApplyLEOnlyDefaults() {
  ApplyDualModeDefaults();

  UnsetBit(&lmp_features_page0, hci::LMPFeature::kSimultaneousLEAndBREDR);
  SetBit(&lmp_features_page0, hci::LMPFeature::kBREDRNotSupported);

  std::memset(supported_commands, 0, sizeof(supported_commands));
  AddLESupportedCommands();
}

void FakeController::Settings::AddBREDRSupportedCommands() {
  SetBit(supported_commands + 0, hci::SupportedCommand::kCreateConnection);
  SetBit(supported_commands + 0, hci::SupportedCommand::kCreateConnectionCancel);
  SetBit(supported_commands + 0, hci::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 7, hci::SupportedCommand::kWriteLocalName);
  SetBit(supported_commands + 7, hci::SupportedCommand::kReadLocalName);
  SetBit(supported_commands + 7, hci::SupportedCommand::kReadScanEnable);
  SetBit(supported_commands + 7, hci::SupportedCommand::kWriteScanEnable);
  SetBit(supported_commands + 8, hci::SupportedCommand::kReadPageScanActivity);
  SetBit(supported_commands + 8, hci::SupportedCommand::kWritePageScanActivity);
  SetBit(supported_commands + 12, hci::SupportedCommand::kReadInquiryMode);
  SetBit(supported_commands + 12, hci::SupportedCommand::kWriteInquiryMode);
  SetBit(supported_commands + 13, hci::SupportedCommand::kReadPageScanType);
  SetBit(supported_commands + 13, hci::SupportedCommand::kWritePageScanType);
  SetBit(supported_commands + 14, hci::SupportedCommand::kReadBufferSize);
  SetBit(supported_commands + 17, hci::SupportedCommand::kReadSimplePairingMode);
  SetBit(supported_commands + 17, hci::SupportedCommand::kWriteSimplePairingMode);
  SetBit(supported_commands + 17, hci::SupportedCommand::kWriteExtendedInquiryResponse);
}

void FakeController::Settings::AddLESupportedCommands() {
  SetBit(supported_commands, hci::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 5, hci::SupportedCommand::kSetEventMask);
  SetBit(supported_commands + 5, hci::SupportedCommand::kReset);
  SetBit(supported_commands + 14, hci::SupportedCommand::kReadLocalVersionInformation);
  SetBit(supported_commands + 14, hci::SupportedCommand::kReadLocalSupportedFeatures);
  SetBit(supported_commands + 14, hci::SupportedCommand::kReadLocalExtendedFeatures);
  SetBit(supported_commands + 24, hci::SupportedCommand::kWriteLEHostSupport);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLESetEventMask);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLEReadBufferSize);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLEReadLocalSupportedFeatures);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLESetRandomAddress);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLESetAdvertisingParameters);
  SetBit(supported_commands + 25, hci::SupportedCommand::kLESetAdvertisingData);
  SetBit(supported_commands + 26, hci::SupportedCommand::kLESetScanResponseData);
  SetBit(supported_commands + 26, hci::SupportedCommand::kLESetAdvertisingEnable);
  SetBit(supported_commands + 26, hci::SupportedCommand::kLECreateConnection);
  SetBit(supported_commands + 26, hci::SupportedCommand::kLECreateConnectionCancel);
  SetBit(supported_commands + 27, hci::SupportedCommand::kLEConnectionUpdate);
}

void FakeController::Settings::ApplyLegacyLEConfig() {
  ApplyLEOnlyDefaults();

  hci_version = hci::HCIVersion::k4_2;

  SetBit(supported_commands + 26, hci::SupportedCommand::kLESetScanParameters);
  SetBit(supported_commands + 26, hci::SupportedCommand::kLESetScanEnable);
}

void FakeController::Settings::ApplyLEConfig() {
  ApplyLEOnlyDefaults();

  SetBit(&le_features, hci::LESupportedFeature::kLEExtendedAdvertising);
}

FakeController::LEScanState::LEScanState()
    : enabled(false),
      scan_type(hci::LEScanType::kPassive),
      scan_interval(0),
      scan_window(0),
      filter_duplicates(false),
      filter_policy(hci::LEScanFilterPolicy::kNoWhiteList) {}

FakeController::LEAdvertisingState::LEAdvertisingState()
    : enabled(false),
      adv_type(hci::LEAdvertisingType::kAdvInd),
      own_address_type(hci::LEOwnAddressType::kPublic),
      interval_min(0),
      interval_max(0),
      data_length(0),
      scan_rsp_length(0) {
  std::memset(data, 0, sizeof(data));
  std::memset(scan_rsp_data, 0, sizeof(scan_rsp_data));
}

FakeController::FakeController()
    : page_scan_type_(hci::PageScanType::kStandardScan),
      page_scan_interval_(0x0800),
      page_scan_window_(0x0012),
      next_conn_handle_(0u),
      le_connect_pending_(false),
      next_le_sig_id_(1u),
      data_callback_(nullptr),
      data_dispatcher_(nullptr),
      auto_completed_packets_event_enabled_(true),
      auto_disconnection_complete_event_enabled_(true) {}

FakeController::~FakeController() { Stop(); }

void FakeController::SetDefaultResponseStatus(hci::OpCode opcode, hci::StatusCode status) {
  ZX_DEBUG_ASSERT(status != hci::StatusCode::kSuccess);
  default_status_map_[opcode] = status;
}

void FakeController::ClearDefaultResponseStatus(hci::OpCode opcode) {
  default_status_map_.erase(opcode);
}

bool FakeController::AddPeer(std::unique_ptr<FakePeer> peer) {
  ZX_DEBUG_ASSERT(peer);
  if (peers_.count(peer->address()) != 0u) {
    return false;
  }
  peer->set_ctrl(this);

  // If a scan is enabled then send an advertising report for the peer that just got registered if
  // it supports advertising.
  SendSingleAdvertisingReport(*peer);

  peers_[peer->address()] = std::move(peer);
  return true;
}

void FakeController::RemovePeer(const DeviceAddress& address) { peers_.erase(address); }

FakePeer* FakeController::FindPeer(const DeviceAddress& address) {
  auto iter = peers_.find(address);
  return (iter == peers_.end()) ? nullptr : iter->second.get();
}

FakePeer* FakeController::FindByConnHandle(hci::ConnectionHandle handle) {
  for (auto& [addr, peer] : peers_) {
    if (peer->HasLink(handle)) {
      return peer.get();
    }
  }
  return nullptr;
}

uint8_t FakeController::NextL2CAPCommandId() {
  // TODO(armansito): Guard against overflow?
  return next_le_sig_id_++;
}

void FakeController::RespondWithCommandComplete(hci::OpCode opcode, const ByteBuffer& params) {
  DynamicByteBuffer buffer(sizeof(hci::CommandCompleteEventParams) + params.size());
  MutablePacketView<hci::CommandCompleteEventParams> event(&buffer, params.size());

  event.mutable_header()->num_hci_command_packets = settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);
  event.mutable_payload_data().Write(params);

  SendEvent(hci::kCommandCompleteEventCode, buffer);
}

void FakeController::RespondWithSuccess(hci::OpCode opcode) {
  hci::SimpleReturnParams out_params;
  out_params.status = hci::StatusCode::kSuccess;
  RespondWithCommandComplete(opcode, BufferView(&out_params, sizeof(out_params)));
}

void FakeController::RespondWithCommandStatus(hci::OpCode opcode, hci::StatusCode status) {
  StaticByteBuffer<sizeof(hci::CommandStatusEventParams)> buffer;
  MutablePacketView<hci::CommandStatusEventParams> event(&buffer);

  event.mutable_header()->status = status;
  event.mutable_header()->num_hci_command_packets = settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);

  SendEvent(hci::kCommandStatusEventCode, buffer);
}

void FakeController::SendEvent(hci::EventCode event_code, const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci::EventHeader) + payload.size());
  MutablePacketView<hci::EventHeader> event(&buffer, payload.size());

  event.mutable_header()->event_code = event_code;
  event.mutable_header()->parameter_total_size = payload.size();
  event.mutable_payload_data().Write(payload);

  SendCommandChannelPacket(buffer);
}

void FakeController::SendLEMetaEvent(hci::EventCode subevent_code, const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci::LEMetaEventParams) + payload.size());
  buffer[0] = subevent_code;
  buffer.Write(payload, 1);

  SendEvent(hci::kLEMetaEventCode, buffer);
}

void FakeController::SendACLPacket(hci::ConnectionHandle handle, const ByteBuffer& payload) {
  ZX_DEBUG_ASSERT(payload.size() <= hci::kMaxACLPayloadSize);

  DynamicByteBuffer buffer(sizeof(hci::ACLDataHeader) + payload.size());
  MutablePacketView<hci::ACLDataHeader> acl(&buffer, payload.size());

  acl.mutable_header()->handle_and_flags = htole16(handle);
  acl.mutable_header()->data_total_length = htole16(static_cast<uint16_t>(payload.size()));
  acl.mutable_payload_data().Write(payload);

  SendACLDataChannelPacket(buffer);
}

void FakeController::SendL2CAPBFrame(hci::ConnectionHandle handle, l2cap::ChannelId channel_id,
                                     const ByteBuffer& payload) {
  ZX_DEBUG_ASSERT(payload.size() <= hci::kMaxACLPayloadSize - sizeof(l2cap::BasicHeader));

  DynamicByteBuffer buffer(sizeof(l2cap::BasicHeader) + payload.size());
  MutablePacketView<l2cap::BasicHeader> bframe(&buffer, payload.size());

  bframe.mutable_header()->length = htole16(payload.size());
  bframe.mutable_header()->channel_id = htole16(channel_id);
  bframe.mutable_payload_data().Write(payload);

  SendACLPacket(handle, buffer);
}

void FakeController::SendL2CAPCFrame(hci::ConnectionHandle handle, bool is_le,
                                     l2cap::CommandCode code, uint8_t id,
                                     const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(l2cap::CommandHeader) + payload.size());
  MutablePacketView<l2cap::CommandHeader> cframe(&buffer, payload.size());

  cframe.mutable_header()->code = code;
  cframe.mutable_header()->id = id;
  cframe.mutable_header()->length = payload.size();
  cframe.mutable_payload_data().Write(payload);

  SendL2CAPBFrame(handle, is_le ? l2cap::kLESignalingChannelId : l2cap::kSignalingChannelId,
                  buffer);
}

void FakeController::SendNumberOfCompletedPacketsEvent(hci::ConnectionHandle handle, uint16_t num) {
  StaticByteBuffer<sizeof(hci::NumberOfCompletedPacketsEventParams) +
                   sizeof(hci::NumberOfCompletedPacketsEventData)>
      buffer;

  auto* params = reinterpret_cast<hci::NumberOfCompletedPacketsEventParams*>(buffer.mutable_data());
  params->number_of_handles = 1;
  params->data->connection_handle = htole16(handle);
  params->data->hc_num_of_completed_packets = htole16(num);

  SendEvent(hci::kNumberOfCompletedPacketsEventCode, buffer);
}

void FakeController::ConnectLowEnergy(const DeviceAddress& addr, hci::ConnectionRole role) {
  async::PostTask(dispatcher(), [addr, role, this] {
    FakePeer* peer = FindPeer(addr);
    if (!peer) {
      bt_log(WARN, "fake-hci", "no peer found with address: %s", addr.ToString().c_str());
      return;
    }

    // TODO(armansito): Don't worry about managing multiple links per peer
    // until this supports Bluetooth classic.
    if (peer->connected()) {
      bt_log(WARN, "fake-hci", "peer already connected");
      return;
    }

    hci::ConnectionHandle handle = ++next_conn_handle_;
    peer->AddLink(handle);

    NotifyConnectionState(addr, handle, /*connected=*/true);

    auto interval_min = hci::defaults::kLEConnectionIntervalMin;
    auto interval_max = hci::defaults::kLEConnectionIntervalMax;

    hci::LEConnectionParameters conn_params(interval_min + ((interval_max - interval_min) / 2), 0,
                                            hci::defaults::kLESupervisionTimeout);
    peer->set_le_params(conn_params);

    hci::LEConnectionCompleteSubeventParams params;
    std::memset(&params, 0, sizeof(params));

    params.status = hci::StatusCode::kSuccess;
    params.peer_address = addr.value();
    params.peer_address_type = ToPeerAddrType(addr.type());
    params.conn_latency = htole16(conn_params.latency());
    params.conn_interval = htole16(conn_params.interval());
    params.supervision_timeout = htole16(conn_params.supervision_timeout());
    params.role = role;
    params.connection_handle = htole16(handle);

    SendLEMetaEvent(hci::kLEConnectionCompleteSubeventCode, BufferView(&params, sizeof(params)));
  });
}

void FakeController::L2CAPConnectionParameterUpdate(
    const DeviceAddress& addr, const hci::LEPreferredConnectionParameters& params) {
  async::PostTask(dispatcher(), [addr, params, this] {
    FakePeer* peer = FindPeer(addr);
    if (!peer) {
      bt_log(WARN, "fake-hci", "no peer found with address: %s", addr.ToString().c_str());
      return;
    }

    if (!peer->connected()) {
      bt_log(WARN, "fake-hci", "peer not connected");
      return;
    }

    ZX_DEBUG_ASSERT(!peer->logical_links().empty());

    l2cap::ConnectionParameterUpdateRequestPayload payload;
    payload.interval_min = htole16(params.min_interval());
    payload.interval_max = htole16(params.max_interval());
    payload.slave_latency = htole16(params.max_latency());
    payload.timeout_multiplier = htole16(params.supervision_timeout());

    // TODO(armansito): Instead of picking the first handle we should pick
    // the handle that matches the current LE-U link.
    SendL2CAPCFrame(*peer->logical_links().begin(), true, l2cap::kConnectionParameterUpdateRequest,
                    NextL2CAPCommandId(), BufferView(&payload, sizeof(payload)));
  });
}

void FakeController::Disconnect(const DeviceAddress& addr) {
  async::PostTask(dispatcher(), [addr, this] {
    FakePeer* peer = FindPeer(addr);
    if (!peer || !peer->connected()) {
      bt_log(WARN, "fake-hci", "no connected peer found with address: %s", addr.ToString().c_str());
      return;
    }

    auto links = peer->Disconnect();
    ZX_DEBUG_ASSERT(!peer->connected());
    ZX_DEBUG_ASSERT(!links.empty());

    for (auto link : links) {
      NotifyConnectionState(addr, link, /*connected=*/false);
      SendDisconnectionCompleteEvent(link);
    }
  });
}

void FakeController::SendDisconnectionCompleteEvent(hci::ConnectionHandle handle) {
  hci::DisconnectionCompleteEventParams params;
  params.status = hci::StatusCode::kSuccess;
  params.connection_handle = htole16(handle);
  params.reason = hci::StatusCode::kRemoteUserTerminatedConnection;
  SendEvent(hci::kDisconnectionCompleteEventCode, BufferView(&params, sizeof(params)));
}

bool FakeController::MaybeRespondWithDefaultStatus(hci::OpCode opcode) {
  auto iter = default_status_map_.find(opcode);
  if (iter == default_status_map_.end())
    return false;

  bt_log(INFO, "fake-hci", "responding with error (command: %#.4x, status: %#.2x)", opcode,
         iter->second);

  hci::SimpleReturnParams params;
  params.status = iter->second;
  RespondWithCommandComplete(opcode, BufferView(&params, sizeof(params)));
  return true;
}

void FakeController::SendInquiryResponses() {
  // TODO(jamuraa): combine some of these into a single response event
  for (const auto& [addr, peer] : peers_) {
    if (!peer->has_inquiry_response()) {
      continue;
    }

    SendCommandChannelPacket(peer->CreateInquiryResponseEvent(inquiry_mode_));
    inquiry_num_responses_left_--;
    if (inquiry_num_responses_left_ == 0) {
      break;
    }
  }
}

void FakeController::SendAdvertisingReports() {
  if (!le_scan_state_.enabled || peers_.empty())
    return;

  for (const auto& iter : peers_) {
    SendSingleAdvertisingReport(*iter.second);
  }

  // We'll send new reports for the same peers if duplicate filtering is
  // disabled.
  if (!le_scan_state_.filter_duplicates) {
    async::PostTask(dispatcher(), [this] { SendAdvertisingReports(); });
  }
}

void FakeController::SendSingleAdvertisingReport(const FakePeer& peer) {
  if (!le_scan_state_.enabled || !peer.has_advertising_reports()) {
    return;
  }
  // We want to send scan response packets only during an active scan and if
  // the peer is scannable.
  bool need_scan_rsp = (le_scan_state().scan_type == hci::LEScanType::kActive) && peer.scannable();
  SendCommandChannelPacket(
      peer.CreateAdvertisingReportEvent(need_scan_rsp && peer.should_batch_reports()));

  // If the original report did not include a scan response then we send it as
  // a separate event.
  if (need_scan_rsp && !peer.should_batch_reports()) {
    SendCommandChannelPacket(peer.CreateScanResponseReportEvent());
  }
}

void FakeController::NotifyAdvertisingState() {
  if (advertising_state_cb_) {
    advertising_state_cb_();
  }
}

void FakeController::NotifyConnectionState(const DeviceAddress& addr, hci::ConnectionHandle handle,
                                           bool connected, bool canceled) {
  if (conn_state_cb_) {
    conn_state_cb_(addr, handle, connected, canceled);
  }
}

void FakeController::NotifyLEConnectionParameters(const DeviceAddress& addr,
                                                  const hci::LEConnectionParameters& params) {
  if (le_conn_params_cb_) {
    le_conn_params_cb_(addr, params);
  }
}

void FakeController::OnCreateConnectionCommandReceived(
    const hci::CreateConnectionCommandParams& params) {
  // Cannot issue this command while a request is already pending.
  if (bredr_connect_pending_) {
    RespondWithCommandStatus(hci::kCreateConnection, hci::StatusCode::kCommandDisallowed);
    return;
  }

  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR, params.bd_addr);
  hci::StatusCode status = hci::StatusCode::kSuccess;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = hci::StatusCode::kConnectionAlreadyExists;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci::kCreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != hci::StatusCode::kSuccess)
    return;

  bredr_connect_pending_ = true;
  pending_bredr_connect_addr_ = peer_address;

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO, "fake-hci", "requested peer %s cannot be connected; request will time out",
           peer_address.ToString().c_str());

    pending_bredr_connect_rsp_.Reset([this, peer_address] {
      hci::ConnectionCompleteEventParams response = {};

      response.status = hci::StatusCode::kPageTimeout;
      response.bd_addr = peer_address.value();

      bredr_connect_pending_ = false;
      SendEvent(hci::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
    });

    // Default page timeout of 5.12s
    // See Core Spec v5.0 Vol 2, Part E, Section 6.6
    constexpr zx::duration default_page_timeout = zx::usec(625 * 0x2000);

    async::PostDelayedTask(
        dispatcher(), [cb = pending_bredr_connect_rsp_.callback()] { cb(); }, default_page_timeout);
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = hci::StatusCode::kConnectionLimitExceeded;
  } else {
    status = peer->connect_response();
  }

  hci::ConnectionCompleteEventParams response = {};

  response.status = status;
  response.bd_addr = params.bd_addr;
  response.link_type = hci::LinkType::kACL;
  response.encryption_enabled = 0x0;

  if (status == hci::StatusCode::kSuccess) {
    hci::ConnectionHandle handle = ++next_conn_handle_;
    response.connection_handle = htole16(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  pending_bredr_connect_rsp_.Reset([response, peer, this] {
    bredr_connect_pending_ = false;

    if (response.status == hci::StatusCode::kSuccess) {
      bool notify = !peer->connected();
      hci::ConnectionHandle handle = le16toh(response.connection_handle);
      peer->AddLink(handle);
      if (notify && peer->connected()) {
        NotifyConnectionState(peer->address(), handle, /*connected=*/true);
      }
    }

    SendEvent(hci::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
  });
  async::PostTask(dispatcher(), [cb = pending_bredr_connect_rsp_.callback()] { cb(); });
}

void FakeController::OnLECreateConnectionCommandReceived(
    const hci::LECreateConnectionCommandParams& params) {
  // Cannot issue this command while a request is already pending.
  if (le_connect_pending_) {
    RespondWithCommandStatus(hci::kLECreateConnection, hci::StatusCode::kCommandDisallowed);
    return;
  }

  DeviceAddress::Type addr_type = hci::AddressTypeFromHCI(params.peer_address_type);
  ZX_DEBUG_ASSERT(addr_type != DeviceAddress::Type::kBREDR);

  const DeviceAddress peer_address(addr_type, params.peer_address);
  hci::StatusCode status = hci::StatusCode::kSuccess;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = hci::StatusCode::kConnectionAlreadyExists;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci::kLECreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != hci::StatusCode::kSuccess)
    return;

  le_connect_pending_ = true;
  if (!le_connect_params_) {
    le_connect_params_ = LEConnectParams();
  }

  le_connect_params_->own_address_type = params.own_address_type;
  le_connect_params_->peer_address = peer_address;

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO, "fake-hci", "requested fake peer cannot be connected; request will time out");
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = hci::StatusCode::kConnectionLimitExceeded;
  } else {
    status = peer->connect_response();
  }

  hci::LEConnectionCompleteSubeventParams response;
  std::memset(&response, 0, sizeof(response));

  response.status = status;
  response.peer_address = params.peer_address;
  response.peer_address_type = ToPeerAddrType(addr_type);

  if (status == hci::StatusCode::kSuccess) {
    uint16_t interval_min = le16toh(params.conn_interval_min);
    uint16_t interval_max = le16toh(params.conn_interval_max);
    uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

    hci::LEConnectionParameters conn_params(interval, le16toh(params.conn_latency),
                                            le16toh(params.supervision_timeout));
    peer->set_le_params(conn_params);

    response.conn_latency = params.conn_latency;
    response.conn_interval = le16toh(interval);
    response.supervision_timeout = params.supervision_timeout;

    response.role = hci::ConnectionRole::kMaster;

    hci::ConnectionHandle handle = ++next_conn_handle_;
    response.connection_handle = htole16(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  pending_le_connect_rsp_.Reset([response, peer, this] {
    le_connect_pending_ = false;

    if (response.status == hci::StatusCode::kSuccess) {
      bool notify = !peer->connected();
      hci::ConnectionHandle handle = le16toh(response.connection_handle);
      peer->AddLink(handle);
      if (notify && peer->connected()) {
        NotifyConnectionState(peer->address(), handle, /*connected=*/true);
      }
    }

    SendLEMetaEvent(hci::kLEConnectionCompleteSubeventCode,
                    BufferView(&response, sizeof(response)));
  });
  async::PostDelayedTask(
      dispatcher(), [cb = pending_le_connect_rsp_.callback()] { cb(); },
      settings_.le_connection_delay);
}

void FakeController::OnLEConnectionUpdateCommandReceived(
    const hci::LEConnectionUpdateCommandParams& params) {
  hci::ConnectionHandle handle = le16toh(params.connection_handle);
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci::kLEConnectionUpdate, hci::StatusCode::kUnknownConnectionId);
    return;
  }

  ZX_DEBUG_ASSERT(peer->connected());

  uint16_t min_interval = le16toh(params.conn_interval_min);
  uint16_t max_interval = le16toh(params.conn_interval_max);
  uint16_t max_latency = le16toh(params.conn_latency);
  uint16_t supv_timeout = le16toh(params.supervision_timeout);

  if (min_interval > max_interval) {
    RespondWithCommandStatus(hci::kLEConnectionUpdate,
                             hci::StatusCode::kInvalidHCICommandParameters);
    return;
  }

  RespondWithCommandStatus(hci::kLEConnectionUpdate, hci::StatusCode::kSuccess);

  hci::LEConnectionParameters conn_params(min_interval + ((max_interval - min_interval) / 2),
                                          max_latency, supv_timeout);
  peer->set_le_params(conn_params);

  hci::LEConnectionUpdateCompleteSubeventParams reply;
  reply.status = hci::StatusCode::kSuccess;
  reply.connection_handle = params.connection_handle;
  reply.conn_interval = htole16(conn_params.interval());
  reply.conn_latency = params.conn_latency;
  reply.supervision_timeout = params.supervision_timeout;

  SendLEMetaEvent(hci::kLEConnectionUpdateCompleteSubeventCode, BufferView(&reply, sizeof(reply)));

  NotifyLEConnectionParameters(peer->address(), conn_params);
}

void FakeController::OnDisconnectCommandReceived(const hci::DisconnectCommandParams& params) {
  hci::ConnectionHandle handle = le16toh(params.connection_handle);

  // Find the peer that matches the disconnected handle.
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci::kDisconnect, hci::StatusCode::kUnknownConnectionId);
    return;
  }

  ZX_DEBUG_ASSERT(peer->connected());

  RespondWithCommandStatus(hci::kDisconnect, hci::StatusCode::kSuccess);

  bool notify = peer->connected();
  peer->RemoveLink(handle);
  if (notify && !peer->connected()) {
    NotifyConnectionState(peer->address(), handle, /*connected=*/false);
  }

  if (auto_disconnection_complete_event_enabled_) {
    SendDisconnectionCompleteEvent(handle);
  }
}

void FakeController::OnCommandPacketReceived(const PacketView<hci::CommandHeader>& command_packet) {
  hci::OpCode opcode = le16toh(command_packet.header().opcode);
  if (MaybeRespondWithDefaultStatus(opcode))
    return;

  // TODO(NET-825): Validate size of payload to be the correct length below.
  switch (opcode) {
    case hci::kReadLocalVersionInfo: {
      hci::ReadLocalVersionInfoReturnParams params;
      std::memset(&params, 0, sizeof(params));
      params.hci_version = settings_.hci_version;
      RespondWithCommandComplete(hci::kReadLocalVersionInfo, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kReadLocalSupportedCommands: {
      hci::ReadLocalSupportedCommandsReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      std::memcpy(params.supported_commands, settings_.supported_commands,
                  sizeof(params.supported_commands));
      RespondWithCommandComplete(hci::kReadLocalSupportedCommands,
                                 BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kReadLocalSupportedFeatures: {
      hci::ReadLocalSupportedFeaturesReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.lmp_features = htole64(settings_.lmp_features_page0);
      RespondWithCommandComplete(hci::kReadLocalSupportedFeatures,
                                 BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kLESetRandomAddress: {
      if (le_advertising_state().enabled || le_scan_state().enabled) {
        bt_log(INFO, "fake-hci", "cannot set LE random address while scanning or advertising");
        hci::SimpleReturnParams out_params;
        out_params.status = hci::StatusCode::kCommandDisallowed;
        RespondWithCommandComplete(opcode, BufferView(&out_params, sizeof(out_params)));
        return;
      }
      const auto& in_params = command_packet.payload<hci::LESetRandomAddressCommandParams>();
      le_random_address_ = DeviceAddress(DeviceAddress::Type::kLERandom, in_params.random_address);

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kLESetAdvertisingParameters: {
      const auto& in_params =
          command_packet.payload<hci::LESetAdvertisingParametersCommandParams>();
      // TODO(jamuraa): when we parse advertising params, return Invalid HCI
      // Command Parameters when apporopriate (Vol 2, Part E, 7.8.9 p1259)
      if (le_adv_state_.enabled) {
        hci::SimpleReturnParams out_params;
        out_params.status = hci::StatusCode::kCommandDisallowed;
        RespondWithCommandComplete(opcode, BufferView(&out_params, sizeof(out_params)));
        return;
      }

      le_adv_state_.interval_min = le16toh(in_params.adv_interval_min);
      le_adv_state_.interval_max = le16toh(in_params.adv_interval_max);
      le_adv_state_.adv_type = in_params.adv_type;
      le_adv_state_.own_address_type = in_params.own_address_type;

      bt_log(INFO, "fake-hci", "start advertising using address type: %hhd",
             le_adv_state_.own_address_type);

      RespondWithSuccess(opcode);
      NotifyAdvertisingState();
      break;
    }
    case hci::kLESetAdvertisingData: {
      const auto& in_params = command_packet.payload<hci::LESetAdvertisingDataCommandParams>();
      le_adv_state_.data_length = in_params.adv_data_length;
      std::memcpy(le_adv_state_.data, in_params.adv_data, le_adv_state_.data_length);

      RespondWithSuccess(opcode);
      NotifyAdvertisingState();
      break;
    }
    case hci::kLESetScanResponseData: {
      const auto& in_params = command_packet.payload<hci::LESetScanResponseDataCommandParams>();
      le_adv_state_.scan_rsp_length = in_params.scan_rsp_data_length;
      std::memcpy(le_adv_state_.scan_rsp_data, in_params.scan_rsp_data,
                  le_adv_state_.scan_rsp_length);

      RespondWithSuccess(opcode);
      NotifyAdvertisingState();
      break;
    }
    case hci::kLESetAdvertisingEnable: {
      const auto& in_params = command_packet.payload<hci::LESetAdvertisingEnableCommandParams>();
      le_adv_state_.enabled = (in_params.advertising_enable == hci::GenericEnableParam::kEnable);

      RespondWithSuccess(opcode);
      NotifyAdvertisingState();
      break;
    }
    case hci::kReadBDADDR: {
      hci::ReadBDADDRReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.bd_addr = settings_.bd_addr.value();
      RespondWithCommandComplete(hci::kReadBDADDR, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kReadBufferSize: {
      hci::ReadBufferSizeReturnParams params;
      std::memset(&params, 0, sizeof(params));
      params.hc_acl_data_packet_length = htole16(settings_.acl_data_packet_length);
      params.hc_total_num_acl_data_packets = settings_.total_num_acl_data_packets;
      RespondWithCommandComplete(hci::kReadBufferSize, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kDisconnect: {
      OnDisconnectCommandReceived(command_packet.payload<hci::DisconnectCommandParams>());
      break;
    }
    case hci::kCreateConnection: {
      OnCreateConnectionCommandReceived(
          command_packet.payload<hci::CreateConnectionCommandParams>());
      break;
    }
    case hci::kCreateConnectionCancel: {
      hci::CreateConnectionCancelReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.bd_addr = pending_bredr_connect_addr_.value();

      if (!bredr_connect_pending_) {
        // No request is currently pending.
        params.status = hci::StatusCode::kUnknownConnectionId;
        RespondWithCommandComplete(hci::kCreateConnectionCancel,
                                   BufferView(&params, sizeof(params)));
        return;
      }

      bredr_connect_pending_ = false;
      pending_bredr_connect_rsp_.Cancel();

      NotifyConnectionState(pending_bredr_connect_addr_, 0, /*connected=*/false, /*canceled=*/true);

      hci::ConnectionCompleteEventParams response = {};

      response.status = hci::StatusCode::kUnknownConnectionId;
      response.bd_addr = pending_bredr_connect_addr_.value();

      RespondWithCommandComplete(hci::kCreateConnectionCancel, BufferView(&params, sizeof(params)));
      SendEvent(hci::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
      break;
    }
    case hci::kWriteLocalName: {
      const auto& in_params = command_packet.payload<hci::WriteLocalNameCommandParams>();
      size_t name_len = 0;
      for (; name_len < hci::kMaxNameLength; ++name_len) {
        if (in_params.local_name[name_len] == '\0') {
          break;
        }
      }
      local_name_ = std::string(in_params.local_name, in_params.local_name + name_len);
      RespondWithSuccess(opcode);
      break;
    }
    case hci::kReadLocalName: {
      hci::ReadLocalNameReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      auto mut_view = MutableBufferView(params.local_name, hci::kMaxNameLength);
      mut_view.Write((uint8_t*)(local_name_.c_str()),
                     std::min(local_name_.length() + 1, hci::kMaxNameLength));
      RespondWithCommandComplete(hci::kReadLocalName, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kReadScanEnable: {
      hci::ReadScanEnableReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.scan_enable = bredr_scan_state_;

      RespondWithCommandComplete(hci::kReadScanEnable, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kWriteScanEnable: {
      const auto& in_params = command_packet.payload<hci::WriteScanEnableCommandParams>();
      bredr_scan_state_ = in_params.scan_enable;

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kReadPageScanActivity: {
      hci::ReadPageScanActivityReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.page_scan_interval = htole16(page_scan_interval_);
      params.page_scan_window = htole16(page_scan_window_);

      RespondWithCommandComplete(hci::kReadPageScanActivity, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kWritePageScanActivity: {
      const auto& in_params = command_packet.payload<hci::WritePageScanActivityCommandParams>();
      page_scan_interval_ = letoh16(in_params.page_scan_interval);
      page_scan_window_ = letoh16(in_params.page_scan_window);

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kReadInquiryMode: {
      hci::ReadInquiryModeReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.inquiry_mode = inquiry_mode_;
      RespondWithCommandComplete(hci::kReadInquiryMode, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kWriteInquiryMode: {
      const auto& in_params = command_packet.payload<hci::WriteInquiryModeCommandParams>();
      inquiry_mode_ = in_params.inquiry_mode;
      RespondWithSuccess(opcode);
      break;
    };
    case hci::kReadPageScanType: {
      hci::ReadPageScanTypeReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.page_scan_type = page_scan_type_;

      RespondWithCommandComplete(hci::kReadPageScanType, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kWritePageScanType: {
      const auto& in_params = command_packet.payload<hci::WritePageScanTypeCommandParams>();
      page_scan_type_ = in_params.page_scan_type;

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kReadSimplePairingMode: {
      hci::ReadSimplePairingModeReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      if (CheckBit(settings_.lmp_features_page1,
                   hci::LMPFeature::kSecureSimplePairingHostSupport)) {
        params.simple_pairing_mode = hci::GenericEnableParam::kEnable;
      } else {
        params.simple_pairing_mode = hci::GenericEnableParam::kDisable;
      }

      RespondWithCommandComplete(hci::kReadSimplePairingMode, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kWriteSimplePairingMode: {
      const auto& in_params = command_packet.payload<hci::WriteSimplePairingModeCommandParams>();
      // "A host shall not set the Simple Pairing Mode to 'disabled'"
      // Spec 5.0 Vol 2 Part E Sec 7.3.59
      if (in_params.simple_pairing_mode != hci::GenericEnableParam::kEnable) {
        hci::SimpleReturnParams params;
        params.status = hci::StatusCode::kInvalidHCICommandParameters;
        RespondWithCommandComplete(hci::kWriteSimplePairingMode,
                                   BufferView(&params, sizeof(params)));
        break;
      }

      SetBit(&settings_.lmp_features_page1, hci::LMPFeature::kSecureSimplePairingHostSupport);

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kWriteExtendedInquiryResponse: {
      const auto& in_params = command_packet.payload<hci::WriteExtendedInquiryResponseParams>();

      // As of now, we don't support FEC encoding enabled.
      if (in_params.fec_required != 0x00) {
        RespondWithCommandStatus(opcode, hci::kInvalidHCICommandParameters);
      }
      RespondWithSuccess(opcode);
      break;
    }
    case hci::kLEConnectionUpdate: {
      OnLEConnectionUpdateCommandReceived(
          command_packet.payload<hci::LEConnectionUpdateCommandParams>());
      break;
    }
    case hci::kLECreateConnection: {
      OnLECreateConnectionCommandReceived(
          command_packet.payload<hci::LECreateConnectionCommandParams>());
      break;
    }
    case hci::kLECreateConnectionCancel: {
      hci::SimpleReturnParams params;
      params.status = hci::StatusCode::kSuccess;

      if (!le_connect_pending_) {
        // No request is currently pending.
        params.status = hci::StatusCode::kCommandDisallowed;
        RespondWithCommandComplete(hci::kLECreateConnectionCancel,
                                   BufferView(&params, sizeof(params)));
        return;
      }

      le_connect_pending_ = false;
      pending_le_connect_rsp_.Cancel();
      ZX_DEBUG_ASSERT(le_connect_params_);

      NotifyConnectionState(le_connect_params_->peer_address, 0, /*connected=*/false,
                            /*canceled=*/true);

      hci::LEConnectionCompleteSubeventParams response;
      std::memset(&response, 0, sizeof(response));

      response.status = hci::StatusCode::kUnknownConnectionId;
      response.peer_address = le_connect_params_->peer_address.value();
      response.peer_address_type = ToPeerAddrType(le_connect_params_->peer_address.type());

      RespondWithCommandComplete(hci::kLECreateConnectionCancel,
                                 BufferView(&params, sizeof(params)));
      SendLEMetaEvent(hci::kLEConnectionCompleteSubeventCode,
                      BufferView(&response, sizeof(response)));
      break;
    }
    case hci::kLEReadLocalSupportedFeatures: {
      hci::LEReadLocalSupportedFeaturesReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.le_features = htole64(settings_.le_features);
      RespondWithCommandComplete(hci::kLEReadLocalSupportedFeatures,
                                 BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kLEReadSupportedStates: {
      hci::LEReadSupportedStatesReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.le_states = htole64(settings_.le_supported_states);
      RespondWithCommandComplete(hci::kLEReadSupportedStates, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kLEReadBufferSize: {
      hci::LEReadBufferSizeReturnParams params;
      params.status = hci::StatusCode::kSuccess;
      params.hc_le_acl_data_packet_length = htole16(settings_.le_acl_data_packet_length);
      params.hc_total_num_le_acl_data_packets = settings_.le_total_num_acl_data_packets;
      RespondWithCommandComplete(hci::kLEReadBufferSize, BufferView(&params, sizeof(params)));
      break;
    }
    case hci::kSetEventMask: {
      const auto& in_params = command_packet.payload<hci::SetEventMaskCommandParams>();
      settings_.event_mask = le64toh(in_params.event_mask);

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kLESetEventMask: {
      const auto& in_params = command_packet.payload<hci::LESetEventMaskCommandParams>();
      settings_.le_event_mask = le64toh(in_params.le_event_mask);

      RespondWithSuccess(opcode);
      break;
    }
    case hci::kReadLocalExtendedFeatures: {
      const auto& in_params = command_packet.payload<hci::ReadLocalExtendedFeaturesCommandParams>();

      hci::ReadLocalExtendedFeaturesReturnParams out_params;
      out_params.page_number = in_params.page_number;
      out_params.maximum_page_number = 2;

      if (in_params.page_number > 2) {
        out_params.status = hci::StatusCode::kInvalidHCICommandParameters;
      } else {
        out_params.status = hci::StatusCode::kSuccess;

        switch (in_params.page_number) {
          case 0:
            out_params.extended_lmp_features = htole64(settings_.lmp_features_page0);
            break;
          case 1:
            out_params.extended_lmp_features = htole64(settings_.lmp_features_page1);
            break;
          case 2:
            out_params.extended_lmp_features = htole64(settings_.lmp_features_page2);
            break;
        }
      }
      RespondWithCommandComplete(hci::kReadLocalExtendedFeatures,
                                 BufferView(&out_params, sizeof(out_params)));
      break;
    }
    case hci::kLESetScanParameters: {
      const auto& in_params = command_packet.payload<hci::LESetScanParametersCommandParams>();

      hci::SimpleReturnParams out_params;
      if (le_scan_state_.enabled) {
        out_params.status = hci::StatusCode::kCommandDisallowed;
      } else {
        out_params.status = hci::StatusCode::kSuccess;
        le_scan_state_.scan_type = in_params.scan_type;
        le_scan_state_.scan_interval = le16toh(in_params.scan_interval);
        le_scan_state_.scan_window = le16toh(in_params.scan_window);
        le_scan_state_.own_address_type = in_params.own_address_type;
        le_scan_state_.filter_policy = in_params.filter_policy;
      }

      RespondWithCommandComplete(opcode, BufferView(&out_params, sizeof(out_params)));
      break;
    }
    case hci::kLESetScanEnable: {
      const auto& in_params = command_packet.payload<hci::LESetScanEnableCommandParams>();

      le_scan_state_.enabled = (in_params.scanning_enabled == hci::GenericEnableParam::kEnable);
      le_scan_state_.filter_duplicates =
          (in_params.filter_duplicates == hci::GenericEnableParam::kEnable);

      // Post the scan state update before scheduling the HCI Command Complete
      // event. This guarantees that single-threaded unit tests receive the scan
      // state update BEFORE the HCI command sequence terminates.
      if (scan_state_cb_) {
        scan_state_cb_(le_scan_state_.enabled);
      }

      RespondWithSuccess(opcode);

      if (le_scan_state_.enabled)
        SendAdvertisingReports();
      break;
    }

    case hci::kInquiry: {
      const auto& in_params = command_packet.payload<hci::InquiryCommandParams>();

      if (in_params.lap != hci::kGIAC && in_params.lap != hci::kLIAC) {
        RespondWithCommandStatus(opcode, hci::kInvalidHCICommandParameters);
        break;
      }

      if (in_params.inquiry_length == 0x00 || in_params.inquiry_length > hci::kInquiryLengthMax) {
        RespondWithCommandStatus(opcode, hci::kInvalidHCICommandParameters);
        break;
      }

      inquiry_num_responses_left_ = in_params.num_responses;
      if (in_params.num_responses == 0) {
        inquiry_num_responses_left_ = -1;
      }

      RespondWithCommandStatus(opcode, hci::kSuccess);

      bt_log(INFO, "fake-hci", "sending inquiry responses..");
      SendInquiryResponses();

      async::PostDelayedTask(
          dispatcher(),
          [this] {
            hci::InquiryCompleteEventParams params;
            params.status = hci::kSuccess;
            SendEvent(hci::kInquiryCompleteEventCode, BufferView(&params, sizeof(params)));
          },
          zx::msec(in_params.inquiry_length * 1280));
      break;
    }
    case hci::kReset: {
      // TODO(jamuraa): actually do some resetting of stuff here
      RespondWithSuccess(opcode);
      break;
    }
    case hci::kWriteLEHostSupport: {
      const auto& in_params = command_packet.payload<hci::WriteLEHostSupportCommandParams>();

      if (in_params.le_supported_host == hci::GenericEnableParam::kEnable) {
        SetBit(&settings_.lmp_features_page1, hci::LMPFeature::kLESupportedHost);
      } else {
        UnsetBit(&settings_.lmp_features_page1, hci::LMPFeature::kLESupportedHost);
      }
      RespondWithSuccess(opcode);
      break;
    }
    default: {
      hci::SimpleReturnParams params;
      params.status = hci::StatusCode::kUnknownCommand;
      RespondWithCommandComplete(opcode, BufferView(&params, sizeof(params)));
      break;
    }
  }
}

void FakeController::OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) {
  if (data_callback_) {
    DynamicByteBuffer packet_copy(acl_data_packet);
    async::PostTask(data_dispatcher_, [packet_copy = std::move(packet_copy),
                                       cb = data_callback_.share()]() mutable { cb(packet_copy); });
  }

  if (acl_data_packet.size() < sizeof(hci::ACLDataHeader)) {
    bt_log(WARN, "fake-hci", "malformed ACL packet!");
    return;
  }

  const auto& header = acl_data_packet.As<hci::ACLDataHeader>();
  hci::ConnectionHandle handle = le16toh(header.handle_and_flags) & 0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "ACL data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
  peer->OnRxL2CAP(handle, acl_data_packet.view(sizeof(hci::ACLDataHeader)));
}

void FakeController::SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(dispatcher);
  ZX_DEBUG_ASSERT(!data_callback_);
  ZX_DEBUG_ASSERT(!data_dispatcher_);

  data_callback_ = std::move(callback);
  data_dispatcher_ = dispatcher;
}

void FakeController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness (this catches bugs with
  // setting multiple data callbacks in class hierarchies).
  data_callback_ = nullptr;
}

}  // namespace testing
}  // namespace bt
