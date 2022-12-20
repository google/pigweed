// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_controller.h"

#include <endian.h>
#include <lib/async/cpp/task.h>

#include <cstddef>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::testing {
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

hci_spec::LEPeerAddressType ToPeerAddrType(DeviceAddress::Type type) {
  hci_spec::LEPeerAddressType result = hci_spec::LEPeerAddressType::kAnonymous;

  switch (type) {
    case DeviceAddress::Type::kLEPublic:
      result = hci_spec::LEPeerAddressType::kPublic;
      break;
    case DeviceAddress::Type::kLERandom:
      result = hci_spec::LEPeerAddressType::kRandom;
      break;
    default:
      break;
  }

  return result;
}

}  // namespace

namespace hci_android = hci_spec::vendor::android;

FakeController::Settings::Settings() {
  std::memset(this, 0, sizeof(*this));
  hci_version = hci_spec::HCIVersion::k5_0;
  num_hci_command_packets = 250;
}

void FakeController::Settings::ApplyDualModeDefaults() {
  std::memset(this, 0, sizeof(*this));
  hci_version = hci_spec::HCIVersion::k5_0;
  num_hci_command_packets = 250;

  acl_data_packet_length = 512;
  total_num_acl_data_packets = 1;
  le_acl_data_packet_length = 512;
  le_total_num_acl_data_packets = 1;

  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kLESupported);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kSimultaneousLEAndBREDR);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kExtendedFeatures);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kRSSIwithInquiryResults);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kExtendedInquiryResponse);

  le_features = 0;

  AddBREDRSupportedCommands();
  AddLESupportedCommands();
}

void FakeController::Settings::ApplyLEOnlyDefaults() {
  ApplyDualModeDefaults();

  UnsetBit(&lmp_features_page0, hci_spec::LMPFeature::kSimultaneousLEAndBREDR);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kBREDRNotSupported);
  std::memset(supported_commands, 0, sizeof(supported_commands));

  AddLESupportedCommands();
}

void FakeController::Settings::AddBREDRSupportedCommands() {
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kCreateConnection);
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kCreateConnectionCancel);
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kWriteLocalName);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kReadLocalName);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kReadScanEnable);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kWriteScanEnable);
  SetBit(supported_commands + 8, hci_spec::SupportedCommand::kReadPageScanActivity);
  SetBit(supported_commands + 8, hci_spec::SupportedCommand::kWritePageScanActivity);
  SetBit(supported_commands + 9, hci_spec::SupportedCommand::kWriteClassOfDevice);
  SetBit(supported_commands + 10, hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  SetBit(supported_commands + 12, hci_spec::SupportedCommand::kReadInquiryMode);
  SetBit(supported_commands + 12, hci_spec::SupportedCommand::kWriteInquiryMode);
  SetBit(supported_commands + 13, hci_spec::SupportedCommand::kReadPageScanType);
  SetBit(supported_commands + 13, hci_spec::SupportedCommand::kWritePageScanType);
  SetBit(supported_commands + 14, hci_spec::SupportedCommand::kReadBufferSize);
  SetBit(supported_commands + 17, hci_spec::SupportedCommand::kReadSimplePairingMode);
  SetBit(supported_commands + 17, hci_spec::SupportedCommand::kWriteSimplePairingMode);
  SetBit(supported_commands + 17, hci_spec::SupportedCommand::kWriteExtendedInquiryResponse);
}

void FakeController::Settings::AddLESupportedCommands() {
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 5, hci_spec::SupportedCommand::kSetEventMask);
  SetBit(supported_commands + 5, hci_spec::SupportedCommand::kReset);
  SetBit(supported_commands + 14, hci_spec::SupportedCommand::kReadLocalVersionInformation);
  SetBit(supported_commands + 14, hci_spec::SupportedCommand::kReadLocalSupportedFeatures);
  SetBit(supported_commands + 14, hci_spec::SupportedCommand::kReadLocalExtendedFeatures);
  SetBit(supported_commands + 24, hci_spec::SupportedCommand::kWriteLEHostSupport);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLESetEventMask);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLEReadBufferSizeV1);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLEReadLocalSupportedFeatures);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLESetRandomAddress);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLESetAdvertisingParameters);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLESetAdvertisingData);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLESetScanResponseData);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLESetAdvertisingEnable);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLECreateConnection);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLECreateConnectionCancel);
  SetBit(supported_commands + 27, hci_spec::SupportedCommand::kLEConnectionUpdate);
  SetBit(supported_commands + 27, hci_spec::SupportedCommand::kLEReadRemoteFeatures);
  SetBit(supported_commands + 28, hci_spec::SupportedCommand::kLEStartEncryption);
}

void FakeController::Settings::ApplyLegacyLEConfig() {
  ApplyLEOnlyDefaults();

  hci_version = hci_spec::HCIVersion::k4_2;

  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLESetScanParameters);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLESetScanEnable);
}

void FakeController::Settings::ApplyExtendedLEConfig() {
  ApplyLEOnlyDefaults();

  SetBit(&le_features, hci_spec::LESupportedFeature::kLEExtendedAdvertising);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLESetAdvertisingSetRandomAddress);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLESetExtendedAdvertisingParameters);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLESetExtendedAdvertisingData);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLESetExtendedScanResponseData);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLESetExtendedAdvertisingEnable);
  SetBit(supported_commands + 36, hci_spec::SupportedCommand::kLEReadMaximumAdvertisingDataLength);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLEReadNumberOfSupportedAdvertisingSets);
  SetBit(supported_commands + 37, hci_spec::SupportedCommand::kLERemoveAdvertisingSet);
  SetBit(supported_commands + 37, hci_spec::SupportedCommand::kLEClearAdvertisingSets);
}

void FakeController::Settings::ApplyAndroidVendorExtensionDefaults() {
  // Settings for the android vendor extensions component within the Fake Controller. These settings
  // correspond to the vendor capabilities returned by the controller. See
  // src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h and LEGetVendorCapabilities
  // for more information.
  android_extension_settings.max_advt_instances = 3;
  android_extension_settings.total_scan_results_storage = htole16(1024);
}

void FakeController::SetDefaultCommandStatus(hci_spec::OpCode opcode, hci_spec::StatusCode status) {
  default_command_status_map_[opcode] = status;
}

void FakeController::ClearDefaultCommandStatus(hci_spec::OpCode opcode) {
  default_command_status_map_.erase(opcode);
}

void FakeController::SetDefaultResponseStatus(hci_spec::OpCode opcode,
                                              hci_spec::StatusCode status) {
  BT_DEBUG_ASSERT(status != hci_spec::StatusCode::SUCCESS);
  default_status_map_[opcode] = status;
}

void FakeController::ClearDefaultResponseStatus(hci_spec::OpCode opcode) {
  default_status_map_.erase(opcode);
}

bool FakeController::AddPeer(std::unique_ptr<FakePeer> peer) {
  BT_DEBUG_ASSERT(peer);
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

void FakeController::SendCommand(pw::span<const std::byte> command) {
  BT_ASSERT(command.size() >= sizeof(hci_spec::CommandHeader));

  // Post the packet to simulate async HCI behavior.
  async::PostTask(dispatcher(), [self = weak_ptr_factory_.GetWeakPtr(),
                                 command = DynamicByteBuffer(BufferView(command))]() {
    if (!self) {
      return;
    }
    const size_t payload_size = command.size() - sizeof(hci_spec::CommandHeader);
    PacketView<hci_spec::CommandHeader> packet_view(&command, payload_size);
    self->OnCommandPacketReceived(packet_view);
  });
}

FakePeer* FakeController::FindByConnHandle(hci_spec::ConnectionHandle handle) {
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

void FakeController::RespondWithCommandComplete(hci_spec::OpCode opcode,
                                                hci_spec::StatusCode status) {
  hci_spec::SimpleReturnParams params;
  params.status = status;
  RespondWithCommandComplete(opcode, BufferView(&params, sizeof(params)));
}

void FakeController::RespondWithCommandComplete(hci_spec::OpCode opcode, const ByteBuffer& params) {
  DynamicByteBuffer buffer(sizeof(hci_spec::CommandCompleteEventParams) + params.size());
  MutablePacketView<hci_spec::CommandCompleteEventParams> event(&buffer, params.size());

  event.mutable_header()->num_hci_command_packets = settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);
  event.mutable_payload_data().Write(params);

  SendEvent(hci_spec::kCommandCompleteEventCode, buffer);
}

void FakeController::RespondWithCommandStatus(hci_spec::OpCode opcode,
                                              hci_spec::StatusCode status) {
  StaticByteBuffer<sizeof(hci_spec::CommandStatusEventParams)> buffer;
  MutablePacketView<hci_spec::CommandStatusEventParams> event(&buffer);

  event.mutable_header()->status = status;
  event.mutable_header()->num_hci_command_packets = settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);

  SendEvent(hci_spec::kCommandStatusEventCode, buffer);
}

void FakeController::SendEvent(hci_spec::EventCode event_code, const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + payload.size());
  MutablePacketView<hci_spec::EventHeader> event(&buffer, payload.size());

  event.mutable_header()->event_code = event_code;
  event.mutable_header()->parameter_total_size = payload.size();
  event.mutable_payload_data().Write(payload);

  SendCommandChannelPacket(buffer);
}

void FakeController::SendLEMetaEvent(hci_spec::EventCode subevent_code, const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::LEMetaEventParams) + payload.size());
  buffer[0] = subevent_code;
  buffer.Write(payload, 1);
  SendEvent(hci_spec::kLEMetaEventCode, buffer);
}

void FakeController::SendVendorEvent(hci_spec::EventCode subevent_code, const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::VendorEventParams) + payload.size());
  buffer[0] = subevent_code;
  buffer.Write(payload, 1);
  SendEvent(hci_spec::kVendorDebugEventCode, buffer);
}

void FakeController::SendACLPacket(hci_spec::ConnectionHandle handle, const ByteBuffer& payload) {
  BT_DEBUG_ASSERT(payload.size() <= hci_spec::kMaxACLPayloadSize);

  DynamicByteBuffer buffer(sizeof(hci_spec::ACLDataHeader) + payload.size());
  MutablePacketView<hci_spec::ACLDataHeader> acl(&buffer, payload.size());

  acl.mutable_header()->handle_and_flags = htole16(handle);
  acl.mutable_header()->data_total_length = htole16(static_cast<uint16_t>(payload.size()));
  acl.mutable_payload_data().Write(payload);

  SendACLDataChannelPacket(buffer);
}

void FakeController::SendL2CAPBFrame(hci_spec::ConnectionHandle handle, l2cap::ChannelId channel_id,
                                     const ByteBuffer& payload) {
  BT_DEBUG_ASSERT(payload.size() <= hci_spec::kMaxACLPayloadSize - sizeof(l2cap::BasicHeader));

  DynamicByteBuffer buffer(sizeof(l2cap::BasicHeader) + payload.size());
  MutablePacketView<l2cap::BasicHeader> bframe(&buffer, payload.size());

  bframe.mutable_header()->length = htole16(payload.size());
  bframe.mutable_header()->channel_id = htole16(channel_id);
  bframe.mutable_payload_data().Write(payload);

  SendACLPacket(handle, buffer);
}

void FakeController::SendL2CAPCFrame(hci_spec::ConnectionHandle handle, bool is_le,
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

void FakeController::SendNumberOfCompletedPacketsEvent(hci_spec::ConnectionHandle handle,
                                                       uint16_t num) {
  StaticByteBuffer<sizeof(hci_spec::NumberOfCompletedPacketsEventParams) +
                   sizeof(hci_spec::NumberOfCompletedPacketsEventData)>
      buffer;

  auto* params =
      reinterpret_cast<hci_spec::NumberOfCompletedPacketsEventParams*>(buffer.mutable_data());
  params->number_of_handles = 1;
  params->data->connection_handle = htole16(handle);
  params->data->hc_num_of_completed_packets = htole16(num);

  SendEvent(hci_spec::kNumberOfCompletedPacketsEventCode, buffer);
}

void FakeController::ConnectLowEnergy(const DeviceAddress& addr, hci_spec::ConnectionRole role) {
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

    hci_spec::ConnectionHandle handle = ++next_conn_handle_;
    peer->AddLink(handle);

    NotifyConnectionState(addr, handle, /*connected=*/true);

    auto interval_min = hci_spec::defaults::kLEConnectionIntervalMin;
    auto interval_max = hci_spec::defaults::kLEConnectionIntervalMax;

    hci_spec::LEConnectionParameters conn_params(interval_min + ((interval_max - interval_min) / 2),
                                                 0, hci_spec::defaults::kLESupervisionTimeout);
    peer->set_le_params(conn_params);

    hci_spec::LEConnectionCompleteSubeventParams params;
    std::memset(&params, 0, sizeof(params));

    params.status = hci_spec::StatusCode::SUCCESS;
    params.peer_address = addr.value();
    params.peer_address_type = ToPeerAddrType(addr.type());
    params.conn_latency = htole16(conn_params.latency());
    params.conn_interval = htole16(conn_params.interval());
    params.supervision_timeout = htole16(conn_params.supervision_timeout());
    params.role = role;
    params.connection_handle = htole16(handle);

    SendLEMetaEvent(hci_spec::kLEConnectionCompleteSubeventCode,
                    BufferView(&params, sizeof(params)));
  });
}

void FakeController::SendConnectionRequest(const DeviceAddress& addr,
                                           hci_spec::LinkType link_type) {
  FakePeer* peer = FindPeer(addr);
  BT_ASSERT(peer);
  peer->set_last_connection_request_link_type(link_type);

  bt_log(DEBUG, "fake-hci", "sending connection request (addr: %s, link: %s)", bt_str(addr),
         hci_spec::LinkTypeToString(link_type).c_str());
  hci_spec::ConnectionRequestEventParams params;
  std::memset(&params, 0, sizeof(params));
  params.bd_addr = addr.value();
  params.link_type = link_type;
  SendEvent(hci_spec::kConnectionRequestEventCode, BufferView(&params, sizeof(params)));
}

void FakeController::L2CAPConnectionParameterUpdate(
    const DeviceAddress& addr, const hci_spec::LEPreferredConnectionParameters& params) {
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

    BT_DEBUG_ASSERT(!peer->logical_links().empty());

    l2cap::ConnectionParameterUpdateRequestPayload payload;
    payload.interval_min = htole16(params.min_interval());
    payload.interval_max = htole16(params.max_interval());
    payload.peripheral_latency = htole16(params.max_latency());
    payload.timeout_multiplier = htole16(params.supervision_timeout());

    // TODO(armansito): Instead of picking the first handle we should pick
    // the handle that matches the current LE-U link.
    SendL2CAPCFrame(*peer->logical_links().begin(), /*is_le=*/true,
                    l2cap::kConnectionParameterUpdateRequest, NextL2CAPCommandId(),
                    BufferView(&payload, sizeof(payload)));
  });
}

void FakeController::SendLEConnectionUpdateCompleteSubevent(
    hci_spec::ConnectionHandle handle, const hci_spec::LEConnectionParameters& params,
    hci_spec::StatusCode status) {
  hci_spec::LEConnectionUpdateCompleteSubeventParams subevent;
  subevent.status = status;
  subevent.connection_handle = htole16(handle);
  subevent.conn_interval = htole16(params.interval());
  subevent.conn_latency = htole16(params.latency());
  subevent.supervision_timeout = htole16(params.supervision_timeout());

  SendLEMetaEvent(hci_spec::kLEConnectionUpdateCompleteSubeventCode,
                  BufferView(&subevent, sizeof(subevent)));
}

void FakeController::Disconnect(const DeviceAddress& addr, hci_spec::StatusCode reason) {
  async::PostTask(dispatcher(), [this, addr, reason] {
    FakePeer* peer = FindPeer(addr);
    if (!peer || !peer->connected()) {
      bt_log(WARN, "fake-hci", "no connected peer found with address: %s", addr.ToString().c_str());
      return;
    }

    auto links = peer->Disconnect();
    BT_DEBUG_ASSERT(!peer->connected());
    BT_DEBUG_ASSERT(!links.empty());

    for (auto link : links) {
      NotifyConnectionState(addr, link, /*connected=*/false);
      SendDisconnectionCompleteEvent(link, reason);
    }
  });
}

void FakeController::SendDisconnectionCompleteEvent(hci_spec::ConnectionHandle handle,
                                                    hci_spec::StatusCode reason) {
  hci_spec::DisconnectionCompleteEventParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.connection_handle = htole16(handle);
  params.reason = reason;
  SendEvent(hci_spec::kDisconnectionCompleteEventCode, BufferView(&params, sizeof(params)));
}

void FakeController::SendEncryptionChangeEvent(hci_spec::ConnectionHandle handle,
                                               hci_spec::StatusCode status,
                                               hci_spec::EncryptionStatus encryption_enabled) {
  hci_spec::EncryptionChangeEventParams params;
  params.status = status;
  params.connection_handle = htole16(handle);
  params.encryption_enabled = encryption_enabled;
  SendEvent(hci_spec::kEncryptionChangeEventCode, BufferView(&params, sizeof(params)));
}

bool FakeController::MaybeRespondWithDefaultCommandStatus(hci_spec::OpCode opcode) {
  auto iter = default_command_status_map_.find(opcode);
  if (iter == default_command_status_map_.end()) {
    return false;
  }

  RespondWithCommandStatus(opcode, iter->second);
  return true;
}

bool FakeController::MaybeRespondWithDefaultStatus(hci_spec::OpCode opcode) {
  auto iter = default_status_map_.find(opcode);
  if (iter == default_status_map_.end())
    return false;

  bt_log(INFO, "fake-hci", "responding with error (command: %#.4x, status: %#.2hhx)", opcode,
         iter->second);
  RespondWithCommandComplete(opcode, iter->second);
  return true;
}

void FakeController::SendInquiryResponses() {
  // TODO(jamuraa): combine some of these into a single response event
  for (const auto& [addr, peer] : peers_) {
    if (!peer->supports_bredr()) {
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
  if (!le_scan_state_.enabled || !peer.supports_le() || !peer.advertising_enabled()) {
    return;
  }
  // We want to send scan response packets only during an active scan and if
  // the peer is scannable.
  bool need_scan_rsp =
      (le_scan_state().scan_type == hci_spec::LEScanType::kActive) && peer.scannable();
  SendCommandChannelPacket(
      peer.CreateAdvertisingReportEvent(need_scan_rsp && peer.should_batch_reports()));

  // If the original report did not include a scan response then we send it as
  // a separate event.
  if (need_scan_rsp && !peer.should_batch_reports()) {
    SendCommandChannelPacket(peer.CreateScanResponseReportEvent());
  }
}

void FakeController::NotifyControllerParametersChanged() {
  if (controller_parameters_cb_) {
    controller_parameters_cb_();
  }
}

void FakeController::NotifyAdvertisingState() {
  if (advertising_state_cb_) {
    advertising_state_cb_();
  }
}

void FakeController::NotifyConnectionState(const DeviceAddress& addr,
                                           hci_spec::ConnectionHandle handle, bool connected,
                                           bool canceled) {
  if (conn_state_cb_) {
    conn_state_cb_(addr, handle, connected, canceled);
  }
}

void FakeController::NotifyLEConnectionParameters(const DeviceAddress& addr,
                                                  const hci_spec::LEConnectionParameters& params) {
  if (le_conn_params_cb_) {
    le_conn_params_cb_(addr, params);
  }
}

void FakeController::OnCreateConnectionCommandReceived(
    const hci_spec::CreateConnectionCommandView& params) {
  acl_create_connection_command_count_++;

  // Cannot issue this command while a request is already pending.
  if (bredr_connect_pending_) {
    RespondWithCommandStatus(hci_spec::kCreateConnection, hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));
  hci_spec::StatusCode status = hci_spec::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = hci_spec::StatusCode::CONNECTION_ALREADY_EXISTS;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kCreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != hci_spec::StatusCode::SUCCESS)
    return;

  bredr_connect_pending_ = true;
  pending_bredr_connect_addr_ = peer_address;

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO, "fake-hci", "requested peer %s cannot be connected; request will time out",
           peer_address.ToString().c_str());

    bredr_connect_rsp_task_.Cancel();
    bredr_connect_rsp_task_.set_handler([this, peer_address] {
      hci_spec::ConnectionCompleteEventParams response = {};

      response.status = hci_spec::StatusCode::PAGE_TIMEOUT;
      response.bd_addr = peer_address.value();

      bredr_connect_pending_ = false;
      SendEvent(hci_spec::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
    });

    // Default page timeout of 5.12s
    // See Core Spec v5.0 Vol 2, Part E, Section 6.6
    constexpr zx::duration default_page_timeout = zx::usec(static_cast<int64_t>(625) * 0x2000);
    bredr_connect_rsp_task_.PostDelayed(dispatcher(), default_page_timeout);
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = hci_spec::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  hci_spec::ConnectionCompleteEventParams response = {};

  response.status = status;
  response.bd_addr = DeviceAddressBytes(params.bd_addr());
  response.link_type = hci_spec::LinkType::kACL;
  response.encryption_enabled = 0x0;

  if (status == hci_spec::StatusCode::SUCCESS) {
    hci_spec::ConnectionHandle handle = ++next_conn_handle_;
    response.connection_handle = htole16(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  bredr_connect_rsp_task_.Cancel();
  bredr_connect_rsp_task_.set_handler([response, peer, this] {
    bredr_connect_pending_ = false;

    if (response.status == hci_spec::StatusCode::SUCCESS) {
      bool notify = !peer->connected();
      hci_spec::ConnectionHandle handle = le16toh(response.connection_handle);
      peer->AddLink(handle);
      if (notify && peer->connected()) {
        NotifyConnectionState(peer->address(), handle, /*connected=*/true);
      }
    }

    SendEvent(hci_spec::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
  });
  bredr_connect_rsp_task_.Post(dispatcher());
}

void FakeController::OnLECreateConnectionCommandReceived(
    const hci_spec::LECreateConnectionCommandParams& params) {
  le_create_connection_command_count_++;
  if (le_create_connection_cb_) {
    le_create_connection_cb_(params);
  }

  // Cannot issue this command while a request is already pending.
  if (le_connect_pending_) {
    RespondWithCommandStatus(hci_spec::kLECreateConnection,
                             hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  DeviceAddress::Type addr_type = hci::AddressTypeFromHCI(params.peer_address_type);
  BT_DEBUG_ASSERT(addr_type != DeviceAddress::Type::kBREDR);

  const DeviceAddress peer_address(addr_type, params.peer_address);
  hci_spec::StatusCode status = hci_spec::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = hci_spec::StatusCode::CONNECTION_ALREADY_EXISTS;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kLECreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != hci_spec::StatusCode::SUCCESS)
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
    status = hci_spec::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  hci_spec::LEConnectionCompleteSubeventParams response;
  std::memset(&response, 0, sizeof(response));

  response.status = status;
  response.peer_address = params.peer_address;
  response.peer_address_type = ToPeerAddrType(addr_type);

  if (status == hci_spec::StatusCode::SUCCESS) {
    uint16_t interval_min = le16toh(params.conn_interval_min);
    uint16_t interval_max = le16toh(params.conn_interval_max);
    uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

    hci_spec::LEConnectionParameters conn_params(interval, le16toh(params.conn_latency),
                                                 le16toh(params.supervision_timeout));
    peer->set_le_params(conn_params);

    response.conn_latency = params.conn_latency;
    response.conn_interval = le16toh(interval);
    response.supervision_timeout = params.supervision_timeout;

    response.role = hci_spec::ConnectionRole::CENTRAL;

    hci_spec::ConnectionHandle handle = ++next_conn_handle_;
    response.connection_handle = htole16(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  le_connect_rsp_task_.Cancel();
  le_connect_rsp_task_.set_handler([response, address = peer_address, this]() {
    auto peer = FindPeer(address);
    if (!peer) {
      // The peer has been removed; Ignore this response
      return;
    }

    le_connect_pending_ = false;

    if (response.status == hci_spec::StatusCode::SUCCESS) {
      bool not_previously_connected = !peer->connected();
      hci_spec::ConnectionHandle handle = le16toh(response.connection_handle);
      peer->AddLink(handle);
      if (not_previously_connected && peer->connected()) {
        NotifyConnectionState(peer->address(), handle, /*connected=*/true);
      }
    }

    SendLEMetaEvent(hci_spec::kLEConnectionCompleteSubeventCode,
                    BufferView(&response, sizeof(response)));
  });
  le_connect_rsp_task_.PostDelayed(dispatcher(), settings_.le_connection_delay);
}

void FakeController::OnLEConnectionUpdateCommandReceived(
    const hci_spec::LEConnectionUpdateCommandParams& params) {
  hci_spec::ConnectionHandle handle = le16toh(params.connection_handle);
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  uint16_t min_interval = le16toh(params.conn_interval_min);
  uint16_t max_interval = le16toh(params.conn_interval_max);
  uint16_t max_latency = le16toh(params.conn_latency);
  uint16_t supv_timeout = le16toh(params.supervision_timeout);

  if (min_interval > max_interval) {
    RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                             hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEConnectionUpdate, hci_spec::StatusCode::SUCCESS);

  hci_spec::LEConnectionParameters conn_params(min_interval + ((max_interval - min_interval) / 2),
                                               max_latency, supv_timeout);
  peer->set_le_params(conn_params);

  hci_spec::LEConnectionUpdateCompleteSubeventParams reply;
  if (peer->supports_ll_conn_update_procedure()) {
    reply.status = hci_spec::StatusCode::SUCCESS;
    reply.connection_handle = params.connection_handle;
    reply.conn_interval = htole16(conn_params.interval());
    reply.conn_latency = params.conn_latency;
    reply.supervision_timeout = params.supervision_timeout;
  } else {
    reply.status = hci_spec::StatusCode::UNSUPPORTED_REMOTE_FEATURE;
    reply.connection_handle = params.connection_handle;
    reply.conn_interval = 0;
    reply.conn_latency = 0;
    reply.supervision_timeout = 0;
  }

  SendLEMetaEvent(hci_spec::kLEConnectionUpdateCompleteSubeventCode,
                  BufferView(&reply, sizeof(reply)));

  NotifyLEConnectionParameters(peer->address(), conn_params);
}

void FakeController::OnDisconnectCommandReceived(const hci_spec::DisconnectCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();

  // Find the peer that matches the disconnected handle.
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kDisconnect, hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  RespondWithCommandStatus(hci_spec::kDisconnect, hci_spec::StatusCode::SUCCESS);

  bool notify = peer->connected();
  peer->RemoveLink(handle);
  if (notify && !peer->connected()) {
    NotifyConnectionState(peer->address(), handle, /*connected=*/false);
  }

  if (auto_disconnection_complete_event_enabled_) {
    SendDisconnectionCompleteEvent(handle);
  }
}

void FakeController::OnWriteLEHostSupportCommandReceived(
    const hci_spec::WriteLEHostSupportCommandParams& params) {
  if (params.le_supported_host == hci_spec::GenericEnableParam::ENABLE) {
    SetBit(&settings_.lmp_features_page1, hci_spec::LMPFeature::kLESupportedHost);
  } else {
    UnsetBit(&settings_.lmp_features_page1, hci_spec::LMPFeature::kLESupportedHost);
  }

  RespondWithCommandComplete(hci_spec::kWriteLEHostSupport, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReset() {
  // TODO(fxbug.dev/78955): actually do some resetting of stuff here
  RespondWithCommandComplete(hci_spec::kReset, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnInquiry(hci_spec::InquiryCommandView params) {
  // Confirm that LAP array is equal to either kGIAC or kLIAC.
  if (params.lap().Read() != hci_spec::InquiryAccessCode::GIAC &&
      params.lap().Read() != hci_spec::InquiryAccessCode::LIAC) {
    RespondWithCommandStatus(hci_spec::kInquiry,
                             hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.inquiry_length().Read() == 0x00 ||
      params.inquiry_length().Read() > hci_spec::kInquiryLengthMax) {
    RespondWithCommandStatus(hci_spec::kInquiry,
                             hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  inquiry_num_responses_left_ = params.num_responses().Read();
  if (params.num_responses().Read() == 0) {
    inquiry_num_responses_left_ = -1;
  }

  RespondWithCommandStatus(hci_spec::kInquiry, hci_spec::StatusCode::SUCCESS);

  bt_log(INFO, "fake-hci", "sending inquiry responses..");
  SendInquiryResponses();

  async::PostDelayedTask(
      dispatcher(),
      [this] {
        hci_spec::InquiryCompleteEventParams output;
        output.status = hci_spec::StatusCode::SUCCESS;
        SendEvent(hci_spec::kInquiryCompleteEventCode, BufferView(&output, sizeof(output)));
      },
      zx::msec(static_cast<int64_t>(params.inquiry_length().Read()) * 1280));
}

void FakeController::OnLESetScanEnable(const hci_spec::LESetScanEnableCommandParams& params) {
  le_scan_state_.enabled = (params.scanning_enabled == hci_spec::GenericEnableParam::ENABLE);
  le_scan_state_.filter_duplicates =
      (params.filter_duplicates == hci_spec::GenericEnableParam::ENABLE);

  // Post the scan state update before scheduling the HCI Command Complete
  // event. This guarantees that single-threaded unit tests receive the scan
  // state update BEFORE the HCI command sequence terminates.
  if (scan_state_cb_) {
    scan_state_cb_(le_scan_state_.enabled);
  }

  RespondWithCommandComplete(hci_spec::kLESetScanEnable, hci_spec::StatusCode::SUCCESS);

  if (le_scan_state_.enabled) {
    SendAdvertisingReports();
  }
}

void FakeController::OnLESetScanParamaters(
    const hci_spec::LESetScanParametersCommandParams& params) {
  hci_spec::StatusCode status = hci_spec::StatusCode::SUCCESS;

  if (le_scan_state_.enabled) {
    status = hci_spec::StatusCode::COMMAND_DISALLOWED;
  } else {
    status = hci_spec::StatusCode::SUCCESS;
    le_scan_state_.scan_type = params.scan_type;
    le_scan_state_.scan_interval = le16toh(params.scan_interval);
    le_scan_state_.scan_window = le16toh(params.scan_window);
    le_scan_state_.own_address_type = params.own_address_type;
    le_scan_state_.filter_policy = params.filter_policy;
  }

  RespondWithCommandComplete(hci_spec::kLESetScanParameters, status);
}

void FakeController::OnReadLocalExtendedFeatures(
    const hci_spec::ReadLocalExtendedFeaturesCommandParams& params) {
  hci_spec::ReadLocalExtendedFeaturesReturnParams out_params;
  out_params.page_number = params.page_number;
  out_params.maximum_page_number = 2;

  if (params.page_number > 2) {
    out_params.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
  } else {
    out_params.status = hci_spec::StatusCode::SUCCESS;

    switch (params.page_number) {
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

  RespondWithCommandComplete(hci_spec::kReadLocalExtendedFeatures,
                             BufferView(&out_params, sizeof(out_params)));
}

void FakeController::OnSetEventMask(hci_spec::SetEventMaskCommandView params) {
  settings_.event_mask = params.event_mask().Read();
  RespondWithCommandComplete(hci_spec::kSetEventMask, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnLESetEventMask(const hci_spec::LESetEventMaskCommandParams& params) {
  settings_.le_event_mask = le64toh(params.le_event_mask);
  RespondWithCommandComplete(hci_spec::kLESetEventMask, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnLEReadBufferSize() {
  hci_spec::LEReadBufferSizeReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.hc_le_acl_data_packet_length = htole16(settings_.le_acl_data_packet_length);
  params.hc_total_num_le_acl_data_packets = settings_.le_total_num_acl_data_packets;
  RespondWithCommandComplete(hci_spec::kLEReadBufferSize, BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadSupportedStates() {
  hci_spec::LEReadSupportedStatesReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.le_states = htole64(settings_.le_supported_states);
  RespondWithCommandComplete(hci_spec::kLEReadSupportedStates, BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadLocalSupportedFeatures() {
  hci_spec::LEReadLocalSupportedFeaturesReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.le_features = htole64(settings_.le_features);
  RespondWithCommandComplete(hci_spec::kLEReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLECreateConnectionCancel() {
  if (!le_connect_pending_) {
    // No request is currently pending.
    RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_connect_pending_ = false;
  le_connect_rsp_task_.Cancel();
  BT_DEBUG_ASSERT(le_connect_params_);

  NotifyConnectionState(le_connect_params_->peer_address, 0, /*connected=*/false,
                        /*canceled=*/true);

  hci_spec::LEConnectionCompleteSubeventParams response;
  std::memset(&response, 0, sizeof(response));

  response.status = hci_spec::StatusCode::UNKNOWN_CONNECTION_ID;
  response.peer_address = le_connect_params_->peer_address.value();
  response.peer_address_type = ToPeerAddrType(le_connect_params_->peer_address.type());

  RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel, hci_spec::StatusCode::SUCCESS);
  SendLEMetaEvent(hci_spec::kLEConnectionCompleteSubeventCode,
                  BufferView(&response, sizeof(response)));
}

void FakeController::OnWriteExtendedInquiryResponse(
    const hci_spec::WriteExtendedInquiryResponseParams& params) {
  // As of now, we don't support FEC encoding enabled.
  if (params.fec_required != 0x00) {
    RespondWithCommandStatus(hci_spec::kWriteExtendedInquiryResponse,
                             hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  }

  RespondWithCommandComplete(hci_spec::kWriteExtendedInquiryResponse,
                             hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnWriteSimplePairingMode(
    const hci_spec::WriteSimplePairingModeCommandParams& params) {
  // "A host shall not set the Simple Pairing Mode to 'disabled'"
  // Spec 5.0 Vol 2 Part E Sec 7.3.59
  if (params.simple_pairing_mode != hci_spec::GenericEnableParam::ENABLE) {
    RespondWithCommandComplete(hci_spec::kWriteSimplePairingMode,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  SetBit(&settings_.lmp_features_page1, hci_spec::LMPFeature::kSecureSimplePairingHostSupport);
  RespondWithCommandComplete(hci_spec::kWriteSimplePairingMode, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadSimplePairingMode() {
  hci_spec::ReadSimplePairingModeReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  if (CheckBit(settings_.lmp_features_page1,
               hci_spec::LMPFeature::kSecureSimplePairingHostSupport)) {
    params.simple_pairing_mode = hci_spec::GenericEnableParam::ENABLE;
  } else {
    params.simple_pairing_mode = hci_spec::GenericEnableParam::DISABLE;
  }

  RespondWithCommandComplete(hci_spec::kReadSimplePairingMode, BufferView(&params, sizeof(params)));
}

void FakeController::OnWritePageScanType(const hci_spec::WritePageScanTypeCommandParams& params) {
  page_scan_type_ = params.page_scan_type;
  RespondWithCommandComplete(hci_spec::kWritePageScanType, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanType() {
  hci_spec::ReadPageScanTypeReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.page_scan_type = page_scan_type_;
  RespondWithCommandComplete(hci_spec::kReadPageScanType, BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteInquiryMode(const hci_spec::WriteInquiryModeCommandParams& params) {
  inquiry_mode_ = params.inquiry_mode;
  RespondWithCommandComplete(hci_spec::kWriteInquiryMode, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadInquiryMode() {
  hci_spec::ReadInquiryModeReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.inquiry_mode = inquiry_mode_;
  RespondWithCommandComplete(hci_spec::kReadInquiryMode, BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteClassOfDevice(const hci_spec::WriteClassOfDeviceCommandParams& params) {
  device_class_ = params.class_of_device;
  NotifyControllerParametersChanged();
  RespondWithCommandComplete(hci_spec::kWriteClassOfDevice, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnWritePageScanActivity(hci_spec::WritePageScanActivityCommandView params) {
  page_scan_interval_ = params.page_scan_interval().Read();
  page_scan_window_ = params.page_scan_window().Read();
  RespondWithCommandComplete(hci_spec::kWritePageScanActivity, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanActivity() {
  hci_spec::ReadPageScanActivityReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.page_scan_interval = htole16(page_scan_interval_);
  params.page_scan_window = htole16(page_scan_window_);
  RespondWithCommandComplete(hci_spec::kReadPageScanActivity, BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteScanEnable(const hci_spec::WriteScanEnableCommandParams& params) {
  bredr_scan_state_ = params.scan_enable;
  RespondWithCommandComplete(hci_spec::kWriteScanEnable, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadScanEnable() {
  hci_spec::ReadScanEnableReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.scan_enable = bredr_scan_state_;
  RespondWithCommandComplete(hci_spec::kReadScanEnable, BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalName() {
  hci_spec::ReadLocalNameReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  auto mut_view = MutableBufferView(params.local_name, hci_spec::kMaxNameLength);
  mut_view.Write((uint8_t*)(local_name_.c_str()),
                 std::min(local_name_.length() + 1, hci_spec::kMaxNameLength));
  RespondWithCommandComplete(hci_spec::kReadLocalName, BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteLocalName(hci_spec::WriteLocalNameCommandView params) {
  std::size_t name_len = 0;

  auto local_name = params.local_name().BackingStorage().data();

  for (; name_len < hci_spec::kMaxNameLength; ++name_len) {
    if (local_name[name_len] == '\0') {
      break;
    }
  }
  local_name_ = std::string(local_name, local_name + name_len);
  NotifyControllerParametersChanged();
  RespondWithCommandComplete(hci_spec::kWriteLocalName, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnCreateConnectionCancel() {
  hci_spec::CreateConnectionCancelReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.bd_addr = pending_bredr_connect_addr_.value();

  if (!bredr_connect_pending_) {
    // No request is currently pending.
    params.status = hci_spec::StatusCode::UNKNOWN_CONNECTION_ID;
    RespondWithCommandComplete(hci_spec::kCreateConnectionCancel,
                               BufferView(&params, sizeof(params)));
    return;
  }

  bredr_connect_pending_ = false;
  bredr_connect_rsp_task_.Cancel();

  NotifyConnectionState(pending_bredr_connect_addr_, 0, /*connected=*/false, /*canceled=*/true);

  hci_spec::ConnectionCompleteEventParams response = {};

  response.status = hci_spec::StatusCode::UNKNOWN_CONNECTION_ID;
  response.bd_addr = pending_bredr_connect_addr_.value();

  RespondWithCommandComplete(hci_spec::kCreateConnectionCancel,
                             BufferView(&params, sizeof(params)));
  SendEvent(hci_spec::kConnectionCompleteEventCode, BufferView(&response, sizeof(response)));
}

void FakeController::OnReadBufferSize() {
  hci_spec::ReadBufferSizeReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.hc_acl_data_packet_length = htole16(settings_.acl_data_packet_length);
  params.hc_total_num_acl_data_packets = settings_.total_num_acl_data_packets;
  params.hc_synchronous_data_packet_length = htole16(settings_.synchronous_data_packet_length);
  params.hc_total_num_synchronous_data_packets = settings_.total_num_synchronous_data_packets;
  RespondWithCommandComplete(hci_spec::kReadBufferSize, BufferView(&params, sizeof(params)));
}

void FakeController::OnReadBRADDR() {
  hci_spec::ReadBDADDRReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.bd_addr = settings_.bd_addr.value();
  RespondWithCommandComplete(hci_spec::kReadBDADDR, BufferView(&params, sizeof(params)));
}

void FakeController::OnLESetAdvertisingEnable(
    const hci_spec::LESetAdvertisingEnableCommandParams& params) {
  // TODO(fxbug.dev/81444): if own address type is random, check that a random address is set

  legacy_advertising_state_.enabled =
      params.advertising_enable == hci_spec::GenericEnableParam::ENABLE;
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingEnable, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetScanResponseData(
    const hci_spec::LESetScanResponseDataCommandParams& params) {
  legacy_advertising_state_.scan_rsp_length = params.scan_rsp_data_length;

  if (params.scan_rsp_data_length == 0) {
    std::memset(legacy_advertising_state_.scan_rsp_data, 0,
                sizeof(legacy_advertising_state_.scan_rsp_data));
  } else {
    std::memcpy(legacy_advertising_state_.scan_rsp_data, params.scan_rsp_data,
                params.scan_rsp_data_length);
  }

  RespondWithCommandComplete(hci_spec::kLESetScanResponseData, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingData(
    const hci_spec::LESetAdvertisingDataCommandParams& params) {
  legacy_advertising_state_.data_length = params.adv_data_length;

  if (params.adv_data_length == 0) {
    std::memset(legacy_advertising_state_.data, 0, sizeof(legacy_advertising_state_.data));
  } else {
    std::memcpy(legacy_advertising_state_.data, params.adv_data, params.adv_data_length);
  }

  RespondWithCommandComplete(hci_spec::kLESetAdvertisingData, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingParameters(
    const hci_spec::LESetAdvertisingParametersCommandParams& params) {
  if (legacy_advertising_state_.enabled) {
    bt_log(INFO, "fake-hci", "cannot set advertising parameters while advertising enabled");
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  uint16_t interval_min = le16toh(params.adv_interval_min);
  uint16_t interval_max = le16toh(params.adv_interval_max);

  // Core Spec Volume 4, Part E, Section 7.8.5: For high duty cycle directed advertising, the
  // Advertising_Interval_Min and Advertising_Interval_Max parameters are not used and shall be
  // ignored.
  if (params.adv_type != hci_spec::LEAdvertisingType::kAdvDirectIndHighDutyCycle) {
    if (interval_min >= interval_max) {
      bt_log(INFO, "fake-hci", "advertising interval min (%d) not strictly less than max (%d)",
             interval_min, interval_max);
      RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                                 hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }

    if (interval_min < hci_spec::kLEAdvertisingIntervalMin) {
      bt_log(INFO, "fake-hci", "advertising interval min (%d) less than spec min (%d)",
             interval_min, hci_spec::kLEAdvertisingIntervalMin);
      RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                                 hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }

    if (interval_max > hci_spec::kLEAdvertisingIntervalMax) {
      bt_log(INFO, "fake-hci", "advertising interval max (%d) greater than spec max (%d)",
             interval_max, hci_spec::kLEAdvertisingIntervalMax);
      RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                                 hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }
  }

  legacy_advertising_state_.interval_min = interval_min;
  legacy_advertising_state_.interval_max = interval_max;
  legacy_advertising_state_.adv_type = params.adv_type;
  legacy_advertising_state_.own_address_type = params.own_address_type;

  bt_log(INFO, "fake-hci", "start advertising using address type: %hhd",
         legacy_advertising_state_.own_address_type);

  RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetRandomAddress(const hci_spec::LESetRandomAddressCommandParams& params) {
  if (legacy_advertising_state().enabled || le_scan_state().enabled) {
    bt_log(INFO, "fake-hci", "cannot set LE random address while scanning or advertising");
    RespondWithCommandComplete(hci_spec::kLESetRandomAddress,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  legacy_advertising_state_.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom, params.random_address);
  RespondWithCommandComplete(hci_spec::kLESetRandomAddress, hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnReadLocalSupportedFeatures() {
  hci_spec::ReadLocalSupportedFeaturesReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.lmp_features = htole64(settings_.lmp_features_page0);
  RespondWithCommandComplete(hci_spec::kReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalSupportedCommands() {
  hci_spec::ReadLocalSupportedCommandsReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  std::memcpy(params.supported_commands, settings_.supported_commands,
              sizeof(params.supported_commands));
  RespondWithCommandComplete(hci_spec::kReadLocalSupportedCommands,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalVersionInfo() {
  hci_spec::ReadLocalVersionInfoReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.hci_version = settings_.hci_version;
  RespondWithCommandComplete(hci_spec::kReadLocalVersionInfo, BufferView(&params, sizeof(params)));
}

void FakeController::OnReadRemoteNameRequestCommandReceived(
    hci_spec::RemoteNameRequestCommandView params) {
  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kRemoteNameRequest,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kRemoteNameRequest, hci_spec::StatusCode::SUCCESS);

  struct RemoteNameRequestCompleteEventParams {
    hci_spec::StatusCode status;
    DeviceAddressBytes bd_addr;
    uint8_t remote_name[hci_spec::kMaxNameLength];
  } __PACKED;
  RemoteNameRequestCompleteEventParams response = {};
  response.bd_addr = DeviceAddressBytes(params.bd_addr());
  std::strncpy((char*)response.remote_name, peer->name().c_str(), hci_spec::kMaxNameLength);
  response.status = hci_spec::StatusCode::SUCCESS;
  SendEvent(hci_spec::kRemoteNameRequestCompleteEventCode, BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteSupportedFeaturesCommandReceived(
    hci_spec::ReadRemoteSupportedFeaturesCommandView params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteSupportedFeatures, hci_spec::StatusCode::SUCCESS);

  hci_spec::ReadRemoteSupportedFeaturesCompleteEventParams response = {};
  response.status = hci_spec::StatusCode::SUCCESS;
  response.connection_handle = htole16(params.connection_handle().Read());
  response.lmp_features = settings_.lmp_features_page0;
  SendEvent(hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteVersionInfoCommandReceived(
    hci_spec::ReadRemoteVersionInfoCommandView params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteVersionInfo, hci_spec::StatusCode::SUCCESS);

  hci_spec::ReadRemoteVersionInfoCompleteEventParams response = {};
  response.status = hci_spec::StatusCode::SUCCESS;
  response.connection_handle = htole16(params.connection_handle().Read());
  response.lmp_version = hci_spec::HCIVersion::k4_2;
  response.manufacturer_name = 0xFFFF;  // anything
  response.lmp_subversion = 0xADDE;     // anything
  SendEvent(hci_spec::kReadRemoteVersionInfoCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteExtendedFeaturesCommandReceived(
    hci_spec::ReadRemoteExtendedFeaturesCommandView params) {
  hci_spec::ReadRemoteExtendedFeaturesCompleteEventParams response = {};

  switch (params.page_number().Read()) {
    case 1:
      response.lmp_features = settings_.lmp_features_page1;
      break;
    case 2:
      response.lmp_features = settings_.lmp_features_page2;
      break;
    default:
      RespondWithCommandStatus(hci_spec::kReadRemoteExtendedFeatures,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
  }

  RespondWithCommandStatus(hci_spec::kReadRemoteExtendedFeatures, hci_spec::StatusCode::SUCCESS);
  response.page_number = params.page_number().Read();
  response.max_page_number = 3;
  response.connection_handle = htole16(params.connection_handle().Read());
  response.status = hci_spec::StatusCode::SUCCESS;
  SendEvent(hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnAuthenticationRequestedCommandReceived(
    hci_spec::AuthenticationRequestedCommandView params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kAuthenticationRequested,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kAuthenticationRequested, hci_spec::StatusCode::SUCCESS);

  hci_spec::LinkKeyRequestParams request = {};
  request.bd_addr = peer->address_.value();
  SendEvent(hci_spec::kLinkKeyRequestEventCode, BufferView(&request, sizeof(request)));
}

void FakeController::OnLinkKeyRequestReplyCommandReceived(
    const hci_spec::LinkKeyRequestReplyCommandView params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLinkKeyRequestReply,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLinkKeyRequestReply, hci_spec::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kLinkKeyRequestReply, hci_spec::StatusCode::SUCCESS);

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    hci_spec::AuthenticationCompleteEventParams auth_complete;
    auth_complete.status = hci_spec::StatusCode::SUCCESS;
    auth_complete.connection_handle = htole16(conn_handle);
    SendEvent(hci_spec::kAuthenticationCompleteEventCode,
              BufferView(&auth_complete, sizeof(auth_complete)));
  }
}

void FakeController::OnLinkKeyRequestNegativeReplyCommandReceived(
    hci_spec::LinkKeyRequestNegativeReplyCommandView params) {
  FakePeer* peer =
      FindPeer(DeviceAddress(DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLinkKeyRequestNegativeReply,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }
  RespondWithCommandStatus(hci_spec::kLinkKeyRequestNegativeReply, hci_spec::StatusCode::SUCCESS);

  hci_spec::IOCapabilityRequestEventParams request = {};
  request.bd_addr = DeviceAddressBytes(params.bd_addr());
  SendEvent(hci_spec::kIOCapabilityRequestEventCode, BufferView(&request, sizeof(request)));
}

void FakeController::OnIOCapabilityRequestReplyCommand(
    hci_spec::IoCapabilityRequestReplyCommandView params) {
  RespondWithCommandStatus(hci_spec::kIOCapabilityRequestReply, hci_spec::StatusCode::SUCCESS);

  hci_spec::IOCapabilityResponseEventParams io_response = {};
  io_response.bd_addr = DeviceAddressBytes(params.bd_addr());
  // By specifying kNoInputNoOutput, we constrain the possible subsequent event types
  // to just UserConfirmationRequestEventCode.
  io_response.io_capability = hci_spec::IoCapability::NO_INPUT_NO_OUTPUT;
  io_response.oob_data_present = 0x00;  // OOB auth data not present
  io_response.auth_requirements = hci_spec::AuthenticationRequirements::GENERAL_BONDING;
  SendEvent(hci_spec::kIOCapabilityResponseEventCode,
            BufferView(&io_response, sizeof(io_response)));

  // Event type based on |params.io_capability| and |io_response.io_capability|.
  hci_spec::UserConfirmationRequestEventParams request = {};
  request.bd_addr = DeviceAddressBytes(params.bd_addr());
  request.numeric_value = 0;
  SendEvent(hci_spec::kUserConfirmationRequestEventCode, BufferView(&request, sizeof(request)));
}

void FakeController::OnUserConfirmationRequestReplyCommand(
    const hci_spec::UserConfirmationRequestReplyCommandParams& params) {
  FakePeer* peer = FindPeer(DeviceAddress(DeviceAddress::Type::kBREDR, params.bd_addr));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kUserConfirmationRequestReply,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestReply, hci_spec::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = params.bd_addr;
  pairing_event.status = hci_spec::StatusCode::SUCCESS;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));

  hci_spec::LinkKeyNotificationEventParams link_key_event;
  link_key_event.bd_addr = params.bd_addr;
  uint8_t key[] = {0xc0, 0xde, 0xfa, 0x57, 0x4b, 0xad, 0xf0, 0x0d,
                   0xa7, 0x60, 0x06, 0x1e, 0xca, 0x1e, 0xca, 0xfe};
  std::copy(key, key + sizeof(key), link_key_event.link_key);
  link_key_event.key_type = 4;  // Unauthenticated Combination Key generated from P-192
  SendEvent(hci_spec::kLinkKeyNotificationEventCode,
            BufferView(&link_key_event, sizeof(link_key_event)));

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    hci_spec::AuthenticationCompleteEventParams auth_complete;
    auth_complete.status = hci_spec::StatusCode::SUCCESS;
    auth_complete.connection_handle = htole16(conn_handle);
    SendEvent(hci_spec::kAuthenticationCompleteEventCode,
              BufferView(&auth_complete, sizeof(auth_complete)));
  }
}

void FakeController::OnUserConfirmationRequestNegativeReplyCommand(
    const hci_spec::UserConfirmationRequestNegativeReplyCommandParams& params) {
  FakePeer* peer = FindPeer(DeviceAddress(DeviceAddress::Type::kBREDR, params.bd_addr));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kUserConfirmationRequestNegativeReply,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestNegativeReply,
                           hci_spec::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kUserConfirmationRequestNegativeReply,
                             hci_spec::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = params.bd_addr;
  pairing_event.status = hci_spec::StatusCode::AUTHENTICATION_FAILURE;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));
}

void FakeController::OnSetConnectionEncryptionCommand(
    hci_spec::SetConnectionEncryptionCommandView params) {
  RespondWithCommandStatus(hci_spec::kSetConnectionEncryption, hci_spec::StatusCode::SUCCESS);

  hci_spec::EncryptionChangeEventParams response;
  response.connection_handle = htole16(params.connection_handle().Read());
  response.status = hci_spec::StatusCode::SUCCESS;
  response.encryption_enabled = hci_spec::EncryptionStatus::kOn;
  SendEvent(hci_spec::kEncryptionChangeEventCode, BufferView(&response, sizeof(response)));
}

void FakeController::OnReadEncryptionKeySizeCommand(
    const hci_spec::ReadEncryptionKeySizeParams& params) {
  hci_spec::ReadEncryptionKeySizeReturnParams response;
  response.status = hci_spec::StatusCode::SUCCESS;
  response.connection_handle = params.connection_handle;
  response.key_size = 16;
  RespondWithCommandComplete(hci_spec::kReadEncryptionKeySize,
                             BufferView(&response, sizeof(response)));
}

void FakeController::OnEnhancedAcceptSynchronousConnectionRequestCommand(
    hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandView params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer || !peer->last_connection_request_link_type().has_value()) {
    RespondWithCommandStatus(hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
                           hci_spec::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  hci_spec::SynchronousConnectionCompleteEventParams response;
  response.status = hci_spec::StatusCode::SUCCESS;
  response.connection_handle = htole16(sco_handle);
  response.bd_addr = peer->address().value();
  response.link_type = peer->last_connection_request_link_type().value();
  response.transmission_interval = 1;
  response.retransmission_window = 2;
  response.rx_packet_length = 3;
  response.tx_packet_length = 4;
  response.air_coding_format = static_cast<hci_spec::CodingFormat>(
      params.connection_parameters().transmit_coding_format().coding_format().Read());
  SendEvent(hci_spec::kSynchronousConnectionCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnEnhancedSetupSynchronousConnectionCommand(
    hci_spec::EnhancedSetupSynchronousConnectionCommandView params) {
  const hci_spec::ConnectionHandle acl_handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(acl_handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kEnhancedSetupSynchronousConnection,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kEnhancedSetupSynchronousConnection,
                           hci_spec::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  hci_spec::SynchronousConnectionCompleteEventParams response;
  response.status = hci_spec::StatusCode::SUCCESS;
  response.connection_handle = htole16(sco_handle);
  response.bd_addr = peer->address().value();
  response.link_type = hci_spec::LinkType::kExtendedSCO;
  response.transmission_interval = 1;
  response.retransmission_window = 2;
  response.rx_packet_length = 3;
  response.tx_packet_length = 4;
  response.air_coding_format = static_cast<hci_spec::CodingFormat>(
      params.connection_parameters().transmit_coding_format().coding_format().Read());
  SendEvent(hci_spec::kSynchronousConnectionCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnLEReadRemoteFeaturesCommand(
    const hci_spec::LEReadRemoteFeaturesCommandParams& params) {
  if (le_read_remote_features_cb_) {
    le_read_remote_features_cb_();
  }

  const hci_spec::ConnectionHandle handle = letoh16(params.connection_handle);
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLEReadRemoteFeatures,
                             hci_spec::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEReadRemoteFeatures, hci_spec::StatusCode::SUCCESS);

  hci_spec::LEReadRemoteFeaturesCompleteSubeventParams response;
  response.connection_handle = params.connection_handle;
  response.status = hci_spec::StatusCode::SUCCESS;
  response.le_features = peer->le_features().le_features;
  SendLEMetaEvent(hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
                  BufferView(&response, sizeof(response)));
}

void FakeController::OnLEStartEncryptionCommand(
    const hci_spec::LEStartEncryptionCommandParams& params) {
  RespondWithCommandStatus(hci_spec::kLEStartEncryption, hci_spec::StatusCode::SUCCESS);
  SendEncryptionChangeEvent(params.connection_handle, hci_spec::StatusCode::SUCCESS,
                            hci_spec::EncryptionStatus::kOn);
}

void FakeController::OnWriteSynchronousFlowControlEnableCommand(
    const hci_spec::WriteSynchronousFlowControlEnableParams& params) {
  constexpr size_t flow_control_enable_octet = 10;
  bool supported =
      settings_.supported_commands[flow_control_enable_octet] &
      static_cast<uint8_t>(hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  if (!supported) {
    RespondWithCommandComplete(hci_spec::kWriteSynchronousFlowControlEnable,
                               hci_spec::StatusCode::UNKNOWN_COMMAND);
    return;
  }
  RespondWithCommandComplete(hci_spec::kWriteSynchronousFlowControlEnable,
                             hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnLESetAdvertisingSetRandomAddress(
    const hci_spec::LESetAdvertisingSetRandomAddressCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci",
           "unknown advertising handle (%d), "
           "use HCI_LE_Set_Extended_Advertising_Parameters to create one first",
           handle);
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.IsConnectableAdvertising() && state.enabled) {
    bt_log(INFO, "fake-hci", "cannot set LE random address while connectable advertising enabled");
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  state.random_address = DeviceAddress(DeviceAddress::Type::kLERandom, params.adv_random_address);
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                             hci_spec::StatusCode::SUCCESS);
}

void FakeController::OnLESetExtendedAdvertisingParameters(
    const hci_spec::LESetExtendedAdvertisingParametersCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // ensure we can allocate memory for this advertising set if not already present
  if (extended_advertising_states_.count(handle) == 0 &&
      extended_advertising_states_.size() >= num_supported_advertising_sets()) {
    bt_log(INFO, "fake-hci", "no available memory for new advertising set, handle: %d", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::MEMORY_CAPACITY_EXCEEDED);
    return;
  }

  // for backwards compatibility, we only support legacy pdus
  constexpr uint16_t legacy_pdu = hci_spec::kLEAdvEventPropBitUseLegacyPDUs;
  if ((params.adv_event_properties & legacy_pdu) == 0) {
    bt_log(INFO, "fake-hci", "only legacy PDUs are supported, extended PDUs are not supported yet");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // ensure we have a valid bit combination in the advertising event properties
  constexpr uint16_t prop_bits_adv_ind =
      legacy_pdu | hci_spec::kLEAdvEventPropBitConnectable | hci_spec::kLEAdvEventPropBitScannable;
  constexpr uint16_t prop_bits_adv_direct_ind_low_duty_cycle =
      legacy_pdu | hci_spec::kLEAdvEventPropBitConnectable | hci_spec::kLEAdvEventPropBitDirected;
  constexpr uint16_t prop_bits_adv_direct_ind_high_duty_cycle =
      prop_bits_adv_direct_ind_low_duty_cycle |
      hci_spec::kLEAdvEventPropBitHighDutyCycleDirectedConnectable;
  constexpr uint16_t prop_bits_adv_scan_ind = legacy_pdu | hci_spec::kLEAdvEventPropBitScannable;
  constexpr uint16_t prop_bits_adv_nonconn_ind = legacy_pdu;

  hci_spec::LEAdvertisingType adv_type;
  switch (params.adv_event_properties) {
    case prop_bits_adv_ind:
      adv_type = hci_spec::LEAdvertisingType::kAdvInd;
      break;
    case prop_bits_adv_direct_ind_high_duty_cycle:
      adv_type = hci_spec::LEAdvertisingType::kAdvDirectIndHighDutyCycle;
      break;
    case prop_bits_adv_direct_ind_low_duty_cycle:
      adv_type = hci_spec::LEAdvertisingType::kAdvDirectIndLowDutyCycle;
      break;
    case prop_bits_adv_scan_ind:
      adv_type = hci_spec::LEAdvertisingType::kAdvScanInd;
      break;
    case prop_bits_adv_nonconn_ind:
      adv_type = hci_spec::LEAdvertisingType::kAdvNonConnInd;
      break;
    default:
      bt_log(INFO, "fake-hci", "invalid bit combination: %d", params.adv_event_properties);
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                                 hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
  }

  // In case there is an error below, we want to reject all parameters instead of storing a dead
  // state and taking up an advertising handle. Avoid creating the LEAdvertisingState directly in
  // the map and add it in only once we have made sure all is good.
  LEAdvertisingState state;
  if (extended_advertising_states_.count(handle) != 0) {
    state = extended_advertising_states_[handle];
  }

  uint32_t interval_min =
      hci_spec::DecodeExtendedAdvertisingInterval(params.primary_adv_interval_min);
  uint32_t interval_max =
      hci_spec::DecodeExtendedAdvertisingInterval(params.primary_adv_interval_max);

  if (interval_min >= interval_max) {
    bt_log(INFO, "fake-hci", "advertising interval min (%d) not strictly less than max (%d)",
           interval_min, interval_max);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (interval_min < hci_spec::kLEExtendedAdvertisingIntervalMin) {
    bt_log(INFO, "fake-hci", "advertising interval min (%d) less than spec min (%d)", interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (interval_max > hci_spec::kLEExtendedAdvertisingIntervalMax) {
    bt_log(INFO, "fake-hci", "advertising interval max (%d) greater than spec max (%d)",
           interval_max, hci_spec::kLEAdvertisingIntervalMax);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (params.primary_adv_channel_map == 0) {
    bt_log(INFO, "fake-hci", "at least one bit must be set in primary advertising channel map");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.adv_tx_power != hci_spec::kLEExtendedAdvertisingTxPowerNoPreference &&
      (params.adv_tx_power < hci_spec::kLEAdvertisingTxPowerMin ||
       params.adv_tx_power > hci_spec::kLEAdvertisingTxPowerMax)) {
    bt_log(INFO, "fake-hci", "advertising tx power out of range: %d", params.adv_tx_power);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // TODO(fxbug.dev/80049): Core spec Volume 4, Part E, Section 7.8.53: if legacy advertising PDUs
  // are being used, the Primary_Advertising_PHY shall indicate the LE 1M PHY.
  if (params.primary_adv_phy != hci_spec::LEPHY::kLE1M) {
    bt_log(INFO, "fake-hci", "only legacy pdus are supported, requires advertising on 1M PHY");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (params.secondary_adv_phy != hci_spec::LEPHY::kLE1M) {
    bt_log(INFO, "fake-hci", "secondary advertising PHY must be selected");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (state.enabled) {
    bt_log(INFO, "fake-hci", "cannot set parameters while advertising set is enabled");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  // all errors checked, set parameters that we care about
  state.adv_type = adv_type;
  state.own_address_type = params.own_address_type;
  state.interval_min = interval_min;
  state.interval_max = interval_max;

  // write full state back only at the end (we don't have a reference because we only want to write
  // if there are no errors)
  extended_advertising_states_[handle] = state;

  hci_spec::LESetExtendedAdvertisingParametersReturnParams return_params;
  return_params.status = hci_spec::StatusCode::SUCCESS;
  return_params.selected_tx_power = hci_spec::kLEAdvertisingTxPowerMax;
  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                             BufferView(&return_params, sizeof(return_params)));
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedAdvertisingData(
    const hci_spec::LESetExtendedAdvertisingDataCommandParams& params) {
  // controller currently doesn't support fragmented advertising, assert so we fail if we ever use
  // it in host code without updating the controller for tests
  BT_ASSERT(params.operation == hci_spec::LESetExtendedAdvDataOp::kComplete);
  BT_ASSERT(params.fragment_preference ==
            hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment);

  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing advertising data entirely doesn't require us to check for error conditions
  if (params.adv_data_length == 0) {
    state.data_length = 0;
    std::memset(state.data, 0, sizeof(state.data));
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               hci_spec::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // directed advertising doesn't support advertising data
  if (state.IsDirectedAdvertising()) {
    bt_log(INFO, "fake-hci", "cannot provide advertising data when using directed advertising");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // For backwards compatibility with older devices, the host currently uses legacy advertising
  // PDUs. The advertising data cannot exceed the legacy advertising PDU limit.
  if (params.adv_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO, "fake-hci", "data length (%d bytes) larger than legacy PDU size limit",
           params.adv_data_length);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  state.data_length = params.adv_data_length;
  std::memcpy(state.data, params.adv_data, params.adv_data_length);
  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                             hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedScanResponseData(
    const hci_spec::LESetExtendedScanResponseDataCommandParams& params) {
  // controller currently doesn't support fragmented advertising, assert so we fail if we ever use
  // it in host code without updating the controller for tests
  BT_ASSERT(params.operation == hci_spec::LESetExtendedAdvDataOp::kComplete);
  BT_ASSERT(params.fragment_preference ==
            hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment);

  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error conditions
  if (params.scan_rsp_data_length == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               hci_spec::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.IsScannableAdvertising()) {
    bt_log(INFO, "fake-hci", "cannot provide scan response data for unscannable advertising types");
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // For backwards compatibility with older devices, the host currently uses legacy advertising
  // PDUs. The scan response data cannot exceed the legacy advertising PDU limit.
  if (params.scan_rsp_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO, "fake-hci", "data length (%d bytes) larger than legacy PDU size limit",
           params.scan_rsp_data_length);
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  state.scan_rsp_length = params.scan_rsp_data_length;
  std::memcpy(state.scan_rsp_data, params.scan_rsp_data, params.scan_rsp_data_length);

  RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                             hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}
void FakeController::OnLESetExtendedAdvertisingEnable(
    const hci_spec::LESetExtendedAdvertisingEnableCommandParams& params) {
  // do some preliminary checks before making any state changes
  if (params.number_of_sets != 0) {
    std::unordered_set<hci_spec::AdvertisingHandle> handles;

    for (uint8_t i = 0; i < params.number_of_sets; i++) {
      hci_spec::AdvertisingHandle handle = params.data[i].adv_handle;

      if (!IsValidAdvertisingHandle(handle)) {
        bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
        RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                   hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
        return;
      }

      // cannot have two array entries for the same advertising handle
      if (handles.count(handle) != 0) {
        bt_log(INFO, "fake-hci", "cannot refer to handle more than once (handle: %d)", handle);
        RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                   hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
        return;
      }
      handles.insert(handle);

      // cannot have instructions for an advertising handle we don't know about
      if (extended_advertising_states_.count(handle) == 0) {
        bt_log(INFO, "fake-hci", "cannot enable/disable an unknown handle (handle: %d)", handle);
        RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                   hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
        return;
      }
    }
  }

  if (params.enable == hci_spec::GenericEnableParam::DISABLE) {
    if (params.number_of_sets == 0) {
      // if params.enable == kDisable and params.number_of_sets == 0, spec asks we disable all
      for (auto& element : extended_advertising_states_) {
        element.second.enabled = false;
      }
    } else {
      for (int i = 0; i < params.number_of_sets; i++) {
        hci_spec::AdvertisingHandle handle = params.data[i].adv_handle;
        extended_advertising_states_[handle].enabled = false;
      }
    }

    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                               hci_spec::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // rest of the function deals with enabling advertising for a given set of advertising sets
  BT_ASSERT(params.enable == hci_spec::GenericEnableParam::ENABLE);

  if (params.number_of_sets == 0) {
    bt_log(INFO, "fake-hci", "cannot enable with an empty advertising set list");
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  for (uint8_t i = 0; i < params.number_of_sets; i++) {
    // FakeController currently doesn't support testing with duration and max events. When those are
    // used in the host, these checks will fail and remind us to add the necessary code to
    // FakeController.
    BT_ASSERT(params.data[i].duration == 0);
    BT_ASSERT(params.data[i].max_extended_adv_events == 0);

    hci_spec::AdvertisingHandle handle = params.data[i].adv_handle;
    LEAdvertisingState& state = extended_advertising_states_[handle];

    if (state.IsDirectedAdvertising() && state.data_length == 0) {
      bt_log(INFO, "fake-hci", "cannot enable type requiring advertising data without setting it");
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                 hci_spec::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    if (state.IsScannableAdvertising() && state.scan_rsp_length == 0) {
      bt_log(INFO, "fake-hci", "cannot enable, requires scan response data but hasn't been set");
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                 hci_spec::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    // TODO(fxbug.dev/81444): if own address type is random, check that a random address is set
    state.enabled = true;
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                             hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadMaximumAdvertisingDataLength() {
  hci_spec::LEReadMaxAdvertisingDataLengthReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;

  // TODO(fxbug.dev/77476): Extended advertising supports sending larger amounts of data, but they
  // have to be fragmented across multiple commands to the controller. This is not yet supported in
  // this implementation. We should support larger than kMaxLEExtendedAdvertisingDataLength
  // advertising data with fragmentation.
  params.max_adv_data_length = htole16(hci_spec::kMaxLEAdvertisingDataLength);
  RespondWithCommandComplete(hci_spec::kLEReadMaxAdvertisingDataLength,
                             BufferView(&params, sizeof(params)));
}
void FakeController::OnLEReadNumberOfSupportedAdvertisingSets() {
  hci_spec::LEReadNumSupportedAdvertisingSetsReturnParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.num_supported_adv_sets = htole16(num_supported_advertising_sets_);
  RespondWithCommandComplete(hci_spec::kLEReadNumSupportedAdvertisingSets,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLERemoveAdvertisingSet(
    const hci_spec::LERemoveAdvertisingSetCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                               hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);
    RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                               hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  if (extended_advertising_states_[handle].enabled) {
    bt_log(INFO, "fake-hci", "cannot remove enabled advertising set (handle: %d)", handle);
    RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                               hci_spec::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  extended_advertising_states_.erase(handle);
  RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEClearAdvertisingSets() {
  for (const auto& element : extended_advertising_states_) {
    if (element.second.enabled) {
      bt_log(INFO, "fake-hci", "cannot remove currently enabled advertising set (handle: %d)",
             element.second.enabled);
      RespondWithCommandComplete(hci_spec::kLEClearAdvertisingSets,
                                 hci_spec::StatusCode::COMMAND_DISALLOWED);
      return;
    }
  }

  extended_advertising_states_.clear();
  RespondWithCommandComplete(hci_spec::kLEClearAdvertisingSets, hci_spec::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadAdvertisingChannelTxPower() {
  if (!respond_to_tx_power_read_) {
    return;
  }

  hci_spec::LEReadAdvertisingChannelTxPowerReturnParams params;
  // Send back arbitrary tx power.
  params.status = hci_spec::StatusCode::SUCCESS;
  params.tx_power = 9;
  RespondWithCommandComplete(hci_spec::kLEReadAdvertisingChannelTxPower,
                             BufferView(&params, sizeof(params)));
}

void FakeController::SendLEAdvertisingSetTerminatedEvent(hci_spec::ConnectionHandle conn_handle,
                                                         hci_spec::AdvertisingHandle adv_handle) {
  hci_spec::LEAdvertisingSetTerminatedSubeventParams params;
  params.status = hci_spec::StatusCode::SUCCESS;
  params.connection_handle = conn_handle;
  params.adv_handle = adv_handle;
  SendLEMetaEvent(hci_spec::kLEAdvertisingSetTerminatedSubeventCode,
                  BufferView(&params, sizeof(params)));
}

void FakeController::SendAndroidLEMultipleAdvertisingStateChangeSubevent(
    hci_spec::ConnectionHandle conn_handle, hci_spec::AdvertisingHandle adv_handle) {
  hci_android::LEMultiAdvtStateChangeSubeventParams params;
  params.adv_handle = adv_handle;
  params.status = hci_spec::StatusCode::SUCCESS;  // Connection received
  params.connection_handle = conn_handle;
  SendVendorEvent(hci_android::kLEMultiAdvtStateChangeSubeventCode,
                  BufferView(&params, sizeof(params)));
}

void FakeController::OnCommandPacketReceived(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  hci_spec::OpCode opcode = le16toh(command_packet.header().opcode);

  bt_log(TRACE, "fake-hci", "received command packet with opcode: %#.4x", opcode);
  // We handle commands immediately unless a client has explicitly set a listener for `opcode`.
  if (paused_opcode_listeners_.find(opcode) == paused_opcode_listeners_.end()) {
    HandleReceivedCommandPacket(command_packet);
    return;
  }

  bt_log(DEBUG, "fake-hci", "pausing response for opcode: %#.4x", opcode);
  paused_opcode_listeners_[opcode](
      [this, packet_data = DynamicByteBuffer(command_packet.data())]() {
        PacketView<hci_spec::CommandHeader> command_packet(
            &packet_data, packet_data.size() - sizeof(hci_spec::CommandHeader));
        HandleReceivedCommandPacket(command_packet);
      });
}

void FakeController::OnAndroidLEGetVendorCapabilities() {
  hci_android::LEGetVendorCapabilitiesReturnParams params;
  std::memcpy(&params, &settings_.android_extension_settings, sizeof(params));
  params.status = hci_spec::StatusCode::SUCCESS;
  RespondWithCommandComplete(hci_android::kLEGetVendorCapabilities,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnAndroidStartA2dpOffload(
    const hci_android::StartA2dpOffloadCommandParams& params) {
  hci_android::StartA2dpOffloadCommandReturnParams ret;
  ret.opcode = hci_android::kStartA2dpOffloadCommandSubopcode;

  // return in case A2DP offload already started
  if (offloaded_a2dp_channel_state_) {
    ret.status = hci_spec::StatusCode::CONNECTION_ALREADY_EXISTS;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
    return;
  }

  // SCMS-T is not currently supported
  hci_android::A2dpScmsTEnable const scms_t_enable = params.scms_t_enable;
  if (scms_t_enable.enabled == hci_spec::GenericEnableParam::ENABLE) {
    ret.status = hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
    return;
  }

  // return in case any parameter has an invalid value
  ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;

  hci_android::A2dpCodecType const codec_type =
      static_cast<hci_android::A2dpCodecType>(le32toh(params.codec_type));
  switch (codec_type) {
    case hci_android::A2dpCodecType::kSbc:
    case hci_android::A2dpCodecType::kAac:
    case hci_android::A2dpCodecType::kAptx:
    case hci_android::A2dpCodecType::kAptxhd:
    case hci_android::A2dpCodecType::kLdac:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpSamplingFrequency const sampling_frequency =
      static_cast<hci_android::A2dpSamplingFrequency>(le32toh(params.sampling_frequency));
  switch (sampling_frequency) {
    case hci_android::A2dpSamplingFrequency::k44100Hz:
    case hci_android::A2dpSamplingFrequency::k48000Hz:
    case hci_android::A2dpSamplingFrequency::k88200Hz:
    case hci_android::A2dpSamplingFrequency::k96000Hz:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpBitsPerSample const bits_per_sample =
      static_cast<hci_android::A2dpBitsPerSample>(params.bits_per_sample);
  switch (bits_per_sample) {
    case hci_android::A2dpBitsPerSample::k16BitsPerSample:
    case hci_android::A2dpBitsPerSample::k24BitsPerSample:
    case hci_android::A2dpBitsPerSample::k32BitsPerSample:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpChannelMode const channel_mode =
      static_cast<hci_android::A2dpChannelMode>(params.channel_mode);
  switch (channel_mode) {
    case hci_android::A2dpChannelMode::kMono:
    case hci_android::A2dpChannelMode::kStereo:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
      return;
  }

  uint32_t const encoded_audio_bitrate = le32toh(params.encoded_audio_bitrate);
  // Bits 0x01000000 to 0xFFFFFFFF are reserved
  if (encoded_audio_bitrate >= 0x01000000) {
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
    return;
  }

  OffloadedA2dpChannel state;
  state.codec_type = codec_type;
  state.max_latency = le16toh(params.max_latency);
  state.scms_t_enable = scms_t_enable;
  state.sampling_frequency = sampling_frequency;
  state.bits_per_sample = bits_per_sample;
  state.channel_mode = channel_mode;
  state.encoded_audio_bitrate = encoded_audio_bitrate;
  state.connection_handle = le16toh(params.connection_handle);
  state.l2cap_channel_id = le16toh(params.l2cap_channel_id);
  state.l2cap_mtu_size = le16toh(params.l2cap_mtu_size);
  offloaded_a2dp_channel_state_ = state;

  ret.status = hci_spec::StatusCode::SUCCESS;
  RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidStopA2dpOffload() {
  hci_android::StartA2dpOffloadCommandReturnParams ret;
  ret.opcode = hci_android::kStopA2dpOffloadCommandSubopcode;

  if (!offloaded_a2dp_channel_state_) {
    ret.status = hci_spec::StatusCode::REPEATED_ATTEMPTS;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
    return;
  }

  offloaded_a2dp_channel_state_ = std::nullopt;

  ret.status = hci_spec::StatusCode::SUCCESS;
  RespondWithCommandComplete(hci_android::kA2dpOffloadCommand, BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidA2dpOffloadCommand(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case hci_android::kStartA2dpOffloadCommandSubopcode: {
      auto params = payload.To<hci_android::StartA2dpOffloadCommandParams>();
      OnAndroidStartA2dpOffload(params);
      break;
    }
    case hci_android::kStopA2dpOffloadCommandSubopcode:
      OnAndroidStopA2dpOffload();
      break;
    default:
      bt_log(WARN, "fake-hci", "unhandled android A2DP offload command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(subopcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
      break;
  }
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtParam(
    const hci_android::LEMultiAdvtSetAdvtParamCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  // ensure we can allocate memory for this advertising set if not already present
  if (extended_advertising_states_.count(handle) == 0 &&
      extended_advertising_states_.size() >= num_supported_advertising_sets()) {
    bt_log(INFO, "fake-hci", "no available memory for new advertising set, handle: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::MEMORY_CAPACITY_EXCEEDED;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  // In case there is an error below, we want to reject all parameters instead of storing a dead
  // state and taking up an advertising handle. Avoid creating the LEAdvertisingState directly in
  // the map and add it in only once we have made sure all is good.
  LEAdvertisingState state;
  if (extended_advertising_states_.count(handle) != 0) {
    state = extended_advertising_states_[handle];
  }

  uint16_t interval_min = le16toh(params.adv_interval_min);
  uint16_t interval_max = le16toh(params.adv_interval_max);

  if (interval_min >= interval_max) {
    bt_log(INFO, "fake-hci", "advertising interval min (%d) not strictly less than max (%d)",
           interval_min, interval_max);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (interval_min < hci_spec::kLEAdvertisingIntervalMin) {
    bt_log(INFO, "fake-hci", "advertising interval min (%d) less than spec min (%d)", interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (interval_max > hci_spec::kLEAdvertisingIntervalMax) {
    bt_log(INFO, "fake-hci", "advertising interval max (%d) greater than spec max (%d)",
           interval_max, hci_spec::kLEAdvertisingIntervalMax);
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  state.interval_min = interval_min;
  state.interval_max = interval_max;
  state.adv_type = params.adv_type;
  state.own_address_type = params.own_address_type;

  // write full state back only at the end (we don't have a reference because we only want to write
  // if there are no errors)
  extended_advertising_states_[handle] = state;

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = hci_spec::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtData(
    const hci_android::LEMultiAdvtSetAdvtDataCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing advertising data entirely doesn't require us to check for error conditions
  if (params.adv_data_length == 0) {
    state.data_length = 0;
    std::memset(state.data, 0, sizeof(state.data));
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::SUCCESS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    NotifyAdvertisingState();
    return;
  }

  // directed advertising doesn't support advertising data
  if (state.IsDirectedAdvertising()) {
    bt_log(INFO, "fake-hci", "cannot provide advertising data when using directed advertising");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (params.adv_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO, "fake-hci", "data length (%d bytes) larger than legacy PDU size limit",
           params.adv_data_length);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  state.data_length = params.adv_data_length;
  std::memcpy(state.data, params.adv_data, params.adv_data_length);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = hci_spec::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetScanResp(
    const hci_android::LEMultiAdvtSetScanRespCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error conditions
  if (params.scan_rsp_data_length == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::SUCCESS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.IsScannableAdvertising()) {
    bt_log(INFO, "fake-hci", "cannot provide scan response data for unscannable advertising types");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (params.scan_rsp_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO, "fake-hci", "data length (%d bytes) larger than legacy PDU size limit",
           params.scan_rsp_data_length);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  state.scan_rsp_length = params.scan_rsp_data_length;
  std::memcpy(state.scan_rsp_data, params.scan_rsp_data, params.scan_rsp_data_length);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = hci_spec::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetRandomAddr(
    const hci_android::LEMultiAdvtSetRandomAddrCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO, "fake-hci", "advertising handle (%d) maps to an unknown advertising set", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.IsConnectableAdvertising() && state.enabled) {
    bt_log(INFO, "fake-hci", "cannot set LE random address while connectable advertising enabled");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::COMMAND_DISALLOWED;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  state.random_address = DeviceAddress(DeviceAddress::Type::kLERandom, params.random_address);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = hci_spec::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidLEMultiAdvtEnable(
    const hci_android::LEMultiAdvtEnableCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtEnableSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
    return;
  }

  bool enabled = false;
  if (params.enable == hci_spec::GenericEnableParam::ENABLE) {
    enabled = true;
  }

  extended_advertising_states_[handle].enabled = enabled;

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = hci_spec::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtEnableSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt, BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvt(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case hci_android::kLEMultiAdvtSetAdvtParamSubopcode: {
      auto params = payload.To<hci_android::LEMultiAdvtSetAdvtParamCommandParams>();
      OnAndroidLEMultiAdvtSetAdvtParam(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetAdvtDataSubopcode: {
      auto params = payload.To<hci_android::LEMultiAdvtSetAdvtDataCommandParams>();
      OnAndroidLEMultiAdvtSetAdvtData(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetScanRespSubopcode: {
      auto params = payload.To<hci_android::LEMultiAdvtSetScanRespCommandParams>();
      OnAndroidLEMultiAdvtSetScanResp(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetRandomAddrSubopcode: {
      auto params = payload.To<hci_android::LEMultiAdvtSetRandomAddrCommandParams>();
      OnAndroidLEMultiAdvtSetRandomAddr(params);
      break;
    }
    case hci_android::kLEMultiAdvtEnableSubopcode: {
      auto params = payload.To<hci_android::LEMultiAdvtEnableCommandParams>();
      OnAndroidLEMultiAdvtEnable(params);
      break;
    }
    default: {
      bt_log(WARN, "fake-hci", "unhandled android multiple advertising command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(subopcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::OnVendorCommand(const PacketView<hci_spec::CommandHeader>& command_packet) {
  auto opcode = le16toh(command_packet.header().opcode);

  switch (opcode) {
    case hci_android::kLEGetVendorCapabilities:
      OnAndroidLEGetVendorCapabilities();
      break;
    case hci_android::kA2dpOffloadCommand:
      OnAndroidA2dpOffloadCommand(command_packet);
      break;
    case hci_android::kLEMultiAdvt:
      OnAndroidLEMultiAdvt(command_packet);
      break;
    default:
      bt_log(WARN, "fake-hci", "received unhandled vendor command with opcode: %#.4x", opcode);
      RespondWithCommandComplete(opcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
      break;
  }
}

void FakeController::HandleReceivedCommandPacket(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  hci_spec::OpCode opcode = le16toh(command_packet.header().opcode);

  if (MaybeRespondWithDefaultCommandStatus(opcode)) {
    return;
  }

  if (MaybeRespondWithDefaultStatus(opcode)) {
    return;
  }

  auto ogf = hci_spec::GetOGF(opcode);
  if (ogf == hci_spec::kVendorOGF) {
    OnVendorCommand(command_packet);
    return;
  }

  // TODO(fxbug.dev/937): Validate size of payload to be the correct length below.
  switch (opcode) {
    case hci_spec::kReadLocalVersionInfo: {
      OnReadLocalVersionInfo();
      break;
    }
    case hci_spec::kReadLocalSupportedCommands: {
      OnReadLocalSupportedCommands();
      break;
    }
    case hci_spec::kReadLocalSupportedFeatures: {
      OnReadLocalSupportedFeatures();
      break;
    }
    case hci_spec::kLESetRandomAddress: {
      const auto& params = command_packet.payload<hci_spec::LESetRandomAddressCommandParams>();
      OnLESetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetAdvertisingParameters: {
      const auto& params =
          command_packet.payload<hci_spec::LESetAdvertisingParametersCommandParams>();
      OnLESetAdvertisingParameters(params);
      break;
    }
    case hci_spec::kLESetAdvertisingData: {
      const auto& params = command_packet.payload<hci_spec::LESetAdvertisingDataCommandParams>();
      OnLESetAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetScanResponseData: {
      const auto& params = command_packet.payload<hci_spec::LESetScanResponseDataCommandParams>();
      OnLESetScanResponseData(params);
      break;
    }
    case hci_spec::kLESetAdvertisingEnable: {
      const auto& params = command_packet.payload<hci_spec::LESetAdvertisingEnableCommandParams>();
      OnLESetAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLESetAdvertisingSetRandomAddress: {
      const auto& params =
          command_packet.payload<hci_spec::LESetAdvertisingSetRandomAddressCommandParams>();
      OnLESetAdvertisingSetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingParameters: {
      const auto& params =
          command_packet.payload<hci_spec::LESetExtendedAdvertisingParametersCommandParams>();
      OnLESetExtendedAdvertisingParameters(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingData: {
      const auto& params =
          command_packet.payload<hci_spec::LESetExtendedAdvertisingDataCommandParams>();
      OnLESetExtendedAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetExtendedScanResponseData: {
      const auto& params =
          command_packet.payload<hci_spec::LESetExtendedScanResponseDataCommandParams>();
      OnLESetExtendedScanResponseData(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingEnable: {
      const auto& params =
          command_packet.payload<hci_spec::LESetExtendedAdvertisingEnableCommandParams>();
      OnLESetExtendedAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLERemoveAdvertisingSet: {
      const auto& params = command_packet.payload<hci_spec::LERemoveAdvertisingSetCommandParams>();
      OnLERemoveAdvertisingSet(params);
      break;
    }
    case hci_spec::kLEReadMaxAdvertisingDataLength: {
      OnLEReadMaximumAdvertisingDataLength();
      break;
    }
    case hci_spec::kLEReadNumSupportedAdvertisingSets: {
      OnLEReadNumberOfSupportedAdvertisingSets();
      break;
    }
    case hci_spec::kLEClearAdvertisingSets: {
      OnLEClearAdvertisingSets();
      break;
    }
    case hci_spec::kReadBDADDR: {
      OnReadBRADDR();
      break;
    }
    case hci_spec::kReadBufferSize: {
      OnReadBufferSize();
      break;
    }
    case hci_spec::kCreateConnectionCancel: {
      OnCreateConnectionCancel();
      break;
    }
    case hci_spec::kReadLocalName: {
      OnReadLocalName();
      break;
    }
    case hci_spec::kReadScanEnable: {
      OnReadScanEnable();
      break;
    }
    case hci_spec::kWriteScanEnable: {
      const auto& params = command_packet.payload<hci_spec::WriteScanEnableCommandParams>();
      OnWriteScanEnable(params);
      break;
    }
    case hci_spec::kReadPageScanActivity: {
      OnReadPageScanActivity();
      break;
    }
    case hci_spec::kWriteClassOfDevice: {
      const auto& params = command_packet.payload<hci_spec::WriteClassOfDeviceCommandParams>();
      OnWriteClassOfDevice(params);
      break;
    }
    case hci_spec::kReadInquiryMode: {
      OnReadInquiryMode();
      break;
    }
    case hci_spec::kWriteInquiryMode: {
      const auto& params = command_packet.payload<hci_spec::WriteInquiryModeCommandParams>();
      OnWriteInquiryMode(params);
      break;
    };
    case hci_spec::kReadPageScanType: {
      OnReadPageScanType();
      break;
    }
    case hci_spec::kWritePageScanType: {
      const auto& params = command_packet.payload<hci_spec::WritePageScanTypeCommandParams>();
      OnWritePageScanType(params);
      break;
    }
    case hci_spec::kReadSimplePairingMode: {
      OnReadSimplePairingMode();
      break;
    }
    case hci_spec::kWriteSimplePairingMode: {
      const auto& params = command_packet.payload<hci_spec::WriteSimplePairingModeCommandParams>();
      OnWriteSimplePairingMode(params);
      break;
    }
    case hci_spec::kWriteExtendedInquiryResponse: {
      const auto& params = command_packet.payload<hci_spec::WriteExtendedInquiryResponseParams>();
      OnWriteExtendedInquiryResponse(params);
      break;
    }
    case hci_spec::kLEConnectionUpdate: {
      const auto& params = command_packet.payload<hci_spec::LEConnectionUpdateCommandParams>();
      OnLEConnectionUpdateCommandReceived(params);
      break;
    }
    case hci_spec::kLECreateConnection: {
      const auto& params = command_packet.payload<hci_spec::LECreateConnectionCommandParams>();
      OnLECreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kLECreateConnectionCancel: {
      OnLECreateConnectionCancel();
      break;
    }
    case hci_spec::kLEReadLocalSupportedFeatures: {
      OnLEReadLocalSupportedFeatures();
      break;
    }
    case hci_spec::kLEReadSupportedStates: {
      OnLEReadSupportedStates();
      break;
    }
    case hci_spec::kLEReadBufferSize: {
      OnLEReadBufferSize();
      break;
    }
    case hci_spec::kLESetEventMask: {
      const auto& params = command_packet.payload<hci_spec::LESetEventMaskCommandParams>();
      OnLESetEventMask(params);
      break;
    }
    case hci_spec::kReadLocalExtendedFeatures: {
      const auto& params =
          command_packet.payload<hci_spec::ReadLocalExtendedFeaturesCommandParams>();
      OnReadLocalExtendedFeatures(params);
      break;
    }
    case hci_spec::kLESetScanParameters: {
      const auto& params = command_packet.payload<hci_spec::LESetScanParametersCommandParams>();
      OnLESetScanParamaters(params);
      break;
    }
    case hci_spec::kLESetScanEnable: {
      const auto& params = command_packet.payload<hci_spec::LESetScanEnableCommandParams>();
      OnLESetScanEnable(params);
      break;
    }
    case hci_spec::kReset: {
      OnReset();
      break;
    }
    case hci_spec::kWriteLEHostSupport: {
      const auto& params = command_packet.payload<hci_spec::WriteLEHostSupportCommandParams>();
      OnWriteLEHostSupportCommandReceived(params);
      break;
    }
    case hci_spec::kLinkKeyRequestReply: {
      const auto& params = command_packet.payload<hci_spec::LinkKeyRequestReplyCommandView>();
      OnLinkKeyRequestReplyCommandReceived(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestReply: {
      const auto& params =
          command_packet.payload<hci_spec::UserConfirmationRequestReplyCommandParams>();
      OnUserConfirmationRequestReplyCommand(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestNegativeReply: {
      const auto& params =
          command_packet.payload<hci_spec::UserConfirmationRequestNegativeReplyCommandParams>();
      OnUserConfirmationRequestNegativeReplyCommand(params);
      break;
    }
    case hci_spec::kReadEncryptionKeySize: {
      const auto& params = command_packet.payload<hci_spec::ReadEncryptionKeySizeParams>();
      OnReadEncryptionKeySizeCommand(params);
      break;
    }
    case hci_spec::kLEReadRemoteFeatures: {
      const auto& params = command_packet.payload<hci_spec::LEReadRemoteFeaturesCommandParams>();
      OnLEReadRemoteFeaturesCommand(params);
      break;
    }
    case hci_spec::kLEReadAdvertisingChannelTxPower: {
      OnLEReadAdvertisingChannelTxPower();
      break;
    }
    case hci_spec::kLEStartEncryption: {
      const auto& params = command_packet.payload<hci_spec::LEStartEncryptionCommandParams>();
      OnLEStartEncryptionCommand(params);
      break;
    }
    case hci_spec::kWriteSynchronousFlowControlEnable: {
      const auto& params =
          command_packet.payload<hci_spec::WriteSynchronousFlowControlEnableParams>();
      OnWriteSynchronousFlowControlEnableCommand(params);
      break;
    }
    case hci_spec::kInquiry:
    case hci_spec::kEnhancedAcceptSynchronousConnectionRequest:
    case hci_spec::kEnhancedSetupSynchronousConnection:
    case hci_spec::kCreateConnection:
    case hci_spec::kDisconnect:
    case hci_spec::kLinkKeyRequestNegativeReply:
    case hci_spec::kAuthenticationRequested:
    case hci_spec::kSetConnectionEncryption:
    case hci_spec::kRemoteNameRequest:
    case hci_spec::kReadRemoteSupportedFeatures:
    case hci_spec::kReadRemoteExtendedFeatures:
    case hci_spec::kReadRemoteVersionInfo:
    case hci_spec::kIOCapabilityRequestReply:
    case hci_spec::kSetEventMask:
    case hci_spec::kWriteLocalName:
    case hci_spec::kWritePageScanActivity: {
      // This case is for packet types that have been migrated to the new Emboss architecture. Their
      // old version can be still be assembled from the HciEmulator channel, so here we repackage
      // and forward them as Emboss packets.
      auto emboss_packet = bt::hci::EmbossCommandPacket::New<hci_spec::EmbossCommandHeaderView>(
          opcode, command_packet.size());
      bt::MutableBufferView dest = emboss_packet.mutable_data();
      command_packet.data().view().Copy(&dest);
      HandleReceivedCommandPacket(emboss_packet);
      break;
    }
    default: {
      bt_log(WARN, "fake-hci", "received unhandled command with opcode: %#.4x", opcode);
      RespondWithCommandComplete(opcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::HandleReceivedCommandPacket(hci::EmbossCommandPacket& command_packet) {
  hci_spec::OpCode opcode = command_packet.opcode();

  if (MaybeRespondWithDefaultCommandStatus(opcode)) {
    return;
  }

  if (MaybeRespondWithDefaultStatus(opcode)) {
    return;
  }

  auto ogf = command_packet.ogf();
  if (ogf == hci_spec::kVendorOGF) {
    bt_log(WARN, "fake-hci",
           "vendor commands not yet migrated to Emboss, yet received Emboss vendor command with "
           "opcode: %#.4x",
           opcode);
    RespondWithCommandComplete(opcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
    return;
  }

  switch (opcode) {
    case hci_spec::kInquiry: {
      auto params = command_packet.view<hci_spec::InquiryCommandView>();
      OnInquiry(params);
      break;
    }
    case hci_spec::kEnhancedAcceptSynchronousConnectionRequest: {
      auto params =
          command_packet.view<hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandView>();
      OnEnhancedAcceptSynchronousConnectionRequestCommand(params);
      break;
    }
    case hci_spec::kEnhancedSetupSynchronousConnection: {
      auto params = command_packet.view<hci_spec::EnhancedSetupSynchronousConnectionCommandView>();
      OnEnhancedSetupSynchronousConnectionCommand(params);
      break;
    }
    case hci_spec::kCreateConnection: {
      const auto params = command_packet.view<hci_spec::CreateConnectionCommandView>();
      OnCreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kDisconnect: {
      const auto params = command_packet.view<hci_spec::DisconnectCommandView>();
      OnDisconnectCommandReceived(params);
      break;
    }
    case hci_spec::kLinkKeyRequestNegativeReply: {
      const auto params = command_packet.view<hci_spec::LinkKeyRequestNegativeReplyCommandView>();
      OnLinkKeyRequestNegativeReplyCommandReceived(params);
      break;
    }
    case hci_spec::kAuthenticationRequested: {
      const auto params = command_packet.view<hci_spec::AuthenticationRequestedCommandView>();
      OnAuthenticationRequestedCommandReceived(params);
      break;
    }
    case hci_spec::kSetConnectionEncryption: {
      const auto params = command_packet.view<hci_spec::SetConnectionEncryptionCommandView>();
      OnSetConnectionEncryptionCommand(params);
      break;
    }
    case hci_spec::kRemoteNameRequest: {
      const auto params = command_packet.view<hci_spec::RemoteNameRequestCommandView>();
      OnReadRemoteNameRequestCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteSupportedFeatures: {
      const auto params = command_packet.view<hci_spec::ReadRemoteSupportedFeaturesCommandView>();
      OnReadRemoteSupportedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteExtendedFeatures: {
      const auto params = command_packet.view<hci_spec::ReadRemoteExtendedFeaturesCommandView>();
      OnReadRemoteExtendedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteVersionInfo: {
      const auto params = command_packet.view<hci_spec::ReadRemoteVersionInfoCommandView>();
      OnReadRemoteVersionInfoCommandReceived(params);
      break;
    }
    case hci_spec::kIOCapabilityRequestReply: {
      const auto params = command_packet.view<hci_spec::IoCapabilityRequestReplyCommandView>();
      OnIOCapabilityRequestReplyCommand(params);
      break;
    }
    case hci_spec::kSetEventMask: {
      const auto params = command_packet.view<hci_spec::SetEventMaskCommandView>();
      OnSetEventMask(params);
      break;
    }
    case hci_spec::kWriteLocalName: {
      const auto params = command_packet.view<hci_spec::WriteLocalNameCommandView>();
      OnWriteLocalName(params);
      break;
    }
    case hci_spec::kWritePageScanActivity: {
      const auto& params = command_packet.view<hci_spec::WritePageScanActivityCommandView>();
      OnWritePageScanActivity(params);
      break;
    }
    default: {
      bt_log(WARN, "fake-hci",
             "received Emboss command either unimplemented or not yet migrated to Emboss, with "
             "opcode: %#.4x",
             opcode);
      RespondWithCommandComplete(opcode, hci_spec::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) {
  if (acl_data_callback_) {
    DynamicByteBuffer packet_copy(acl_data_packet);
    async::PostTask(data_dispatcher_,
                    [packet_copy = std::move(packet_copy),
                     cb = acl_data_callback_.share()]() mutable { cb(packet_copy); });
  }

  if (acl_data_packet.size() < sizeof(hci_spec::ACLDataHeader)) {
    bt_log(WARN, "fake-hci", "malformed ACL packet!");
    return;
  }

  const auto& header = acl_data_packet.To<hci_spec::ACLDataHeader>();
  hci_spec::ConnectionHandle handle = le16toh(header.handle_and_flags) & 0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "ACL data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
  peer->OnRxL2CAP(handle, acl_data_packet.view(sizeof(hci_spec::ACLDataHeader)));
}

void FakeController::OnScoDataPacketReceived(const ByteBuffer& sco_data_packet) {
  if (sco_data_callback_) {
    sco_data_callback_(sco_data_packet);
  }

  if (sco_data_packet.size() < sizeof(hci_spec::SynchronousDataHeader)) {
    bt_log(WARN, "fake-hci", "malformed SCO packet!");
    return;
  }

  const auto& header = sco_data_packet.To<hci_spec::SynchronousDataHeader>();
  hci_spec::ConnectionHandle handle = le16toh(header.handle_and_flags) & 0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "SCO data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
}

void FakeController::SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher) {
  BT_DEBUG_ASSERT(callback);
  BT_DEBUG_ASSERT(dispatcher);
  BT_DEBUG_ASSERT(!acl_data_callback_);
  BT_DEBUG_ASSERT(!data_dispatcher_);

  acl_data_callback_ = std::move(callback);
  data_dispatcher_ = dispatcher;
}

void FakeController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness (this catches bugs with
  // setting multiple data callbacks in class hierarchies).
  acl_data_callback_ = nullptr;
}

bool FakeController::LEAdvertisingState::IsDirectedAdvertising() const {
  return adv_type == hci_spec::LEAdvertisingType::kAdvDirectIndHighDutyCycle ||
         adv_type == hci_spec::LEAdvertisingType::kAdvDirectIndLowDutyCycle;
}

bool FakeController::LEAdvertisingState::IsScannableAdvertising() const {
  return adv_type == hci_spec::LEAdvertisingType::kAdvInd ||
         adv_type == hci_spec::LEAdvertisingType::kAdvScanInd;
}

bool FakeController::LEAdvertisingState::IsConnectableAdvertising() const {
  return adv_type == hci_spec::LEAdvertisingType::kAdvInd ||
         adv_type == hci_spec::LEAdvertisingType::kAdvDirectIndHighDutyCycle ||
         adv_type == hci_spec::LEAdvertisingType::kAdvDirectIndLowDutyCycle;
}
}  // namespace bt::testing
