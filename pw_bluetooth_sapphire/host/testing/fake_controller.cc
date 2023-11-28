// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"

#include <cpp-string/string_printf.h>
#include <endian.h>
#include <pw_bluetooth/hci_vendor.emb.h>

#include <cstddef>

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/defaults.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/util.h"

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

}  // namespace

namespace hci_android = hci_spec::vendor::android;

void FakeController::Settings::ApplyDualModeDefaults() {
  le_connection_delay = std::chrono::seconds(0);
  hci_version = pw::bluetooth::emboss::CoreSpecificationVersion::V5_0;
  num_hci_command_packets = 250;
  event_mask = 0;
  le_event_mask = 0;
  bd_addr = DeviceAddress();
  lmp_features_page0 = 0;
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kLESupportedHost);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kSimultaneousLEAndBREDR);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kExtendedFeatures);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kRSSIwithInquiryResults);
  SetBit(&lmp_features_page0, hci_spec::LMPFeature::kExtendedInquiryResponse);
  lmp_features_page1 = 0;
  lmp_features_page2 = 0;
  le_features = 0;
  le_supported_states = 0;
  std::memset(supported_commands, 0, sizeof(supported_commands));
  AddBREDRSupportedCommands();
  AddLESupportedCommands();
  acl_data_packet_length = 512;
  total_num_acl_data_packets = 1;
  le_acl_data_packet_length = 512;
  le_total_num_acl_data_packets = 1;
  synchronous_data_packet_length = 0;
  total_num_synchronous_data_packets = 0;
  android_extension_settings.SetToZeros();
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
  SetBit(supported_commands + 0,
         hci_spec::SupportedCommand::kCreateConnectionCancel);
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kWriteLocalName);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kReadLocalName);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kReadScanEnable);
  SetBit(supported_commands + 7, hci_spec::SupportedCommand::kWriteScanEnable);
  SetBit(supported_commands + 8,
         hci_spec::SupportedCommand::kReadPageScanActivity);
  SetBit(supported_commands + 8,
         hci_spec::SupportedCommand::kWritePageScanActivity);
  SetBit(supported_commands + 9,
         hci_spec::SupportedCommand::kWriteClassOfDevice);
  SetBit(supported_commands + 10,
         hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  SetBit(supported_commands + 12, hci_spec::SupportedCommand::kReadInquiryMode);
  SetBit(supported_commands + 12,
         hci_spec::SupportedCommand::kWriteInquiryMode);
  SetBit(supported_commands + 13,
         hci_spec::SupportedCommand::kReadPageScanType);
  SetBit(supported_commands + 13,
         hci_spec::SupportedCommand::kWritePageScanType);
  SetBit(supported_commands + 14, hci_spec::SupportedCommand::kReadBufferSize);
  SetBit(supported_commands + 17,
         hci_spec::SupportedCommand::kReadSimplePairingMode);
  SetBit(supported_commands + 17,
         hci_spec::SupportedCommand::kWriteSimplePairingMode);
  SetBit(supported_commands + 17,
         hci_spec::SupportedCommand::kWriteExtendedInquiryResponse);
  SetBit(supported_commands + 32,
         hci_spec::SupportedCommand::kWriteSecureConnectionsHostSupport);
}

void FakeController::Settings::AddLESupportedCommands() {
  SetBit(supported_commands + 0, hci_spec::SupportedCommand::kDisconnect);
  SetBit(supported_commands + 5, hci_spec::SupportedCommand::kSetEventMask);
  SetBit(supported_commands + 5, hci_spec::SupportedCommand::kReset);
  SetBit(supported_commands + 14,
         hci_spec::SupportedCommand::kReadLocalVersionInformation);
  SetBit(supported_commands + 14,
         hci_spec::SupportedCommand::kReadLocalSupportedFeatures);
  SetBit(supported_commands + 14,
         hci_spec::SupportedCommand::kReadLocalExtendedFeatures);
  SetBit(supported_commands + 24,
         hci_spec::SupportedCommand::kWriteLEHostSupport);
  SetBit(supported_commands + 25, hci_spec::SupportedCommand::kLESetEventMask);
  SetBit(supported_commands + 25,
         hci_spec::SupportedCommand::kLEReadBufferSizeV1);
  SetBit(supported_commands + 25,
         hci_spec::SupportedCommand::kLEReadLocalSupportedFeatures);
  SetBit(supported_commands + 25,
         hci_spec::SupportedCommand::kLESetRandomAddress);
  SetBit(supported_commands + 25,
         hci_spec::SupportedCommand::kLESetAdvertisingParameters);
  SetBit(supported_commands + 25,
         hci_spec::SupportedCommand::kLESetAdvertisingData);
  SetBit(supported_commands + 26,
         hci_spec::SupportedCommand::kLESetScanResponseData);
  SetBit(supported_commands + 26,
         hci_spec::SupportedCommand::kLESetAdvertisingEnable);
  SetBit(supported_commands + 26,
         hci_spec::SupportedCommand::kLECreateConnection);
  SetBit(supported_commands + 26,
         hci_spec::SupportedCommand::kLECreateConnectionCancel);
  SetBit(supported_commands + 27,
         hci_spec::SupportedCommand::kLEConnectionUpdate);
  SetBit(supported_commands + 27,
         hci_spec::SupportedCommand::kLEReadRemoteFeatures);
  SetBit(supported_commands + 28,
         hci_spec::SupportedCommand::kLEStartEncryption);
}

void FakeController::Settings::ApplyLegacyLEConfig() {
  ApplyLEOnlyDefaults();

  hci_version = pw::bluetooth::emboss::CoreSpecificationVersion::V4_2;

  SetBit(supported_commands + 26,
         hci_spec::SupportedCommand::kLESetScanParameters);
  SetBit(supported_commands + 26, hci_spec::SupportedCommand::kLESetScanEnable);
}

void FakeController::Settings::ApplyExtendedLEConfig() {
  ApplyLEOnlyDefaults();

  SetBit(&le_features, hci_spec::LESupportedFeature::kLEExtendedAdvertising);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLESetAdvertisingSetRandomAddress);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLESetExtendedAdvertisingParameters);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLESetExtendedAdvertisingData);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLESetExtendedScanResponseData);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLESetExtendedAdvertisingEnable);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLEReadMaximumAdvertisingDataLength);
  SetBit(supported_commands + 36,
         hci_spec::SupportedCommand::kLEReadNumberOfSupportedAdvertisingSets);
  SetBit(supported_commands + 37,
         hci_spec::SupportedCommand::kLERemoveAdvertisingSet);
  SetBit(supported_commands + 37,
         hci_spec::SupportedCommand::kLEClearAdvertisingSets);
}

void FakeController::Settings::ApplyAndroidVendorExtensionDefaults() {
  // Settings for the android vendor extensions component within the Fake
  // Controller. These settings correspond to the vendor capabilities returned
  // by the controller. See
  // src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h and
  // LEGetVendorCapabilities for more information.
  android_extension_settings.view().status().Write(
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  android_extension_settings.view().max_advt_instances().Write(3);
  android_extension_settings.view().total_scan_results_storage().Write(1024);
}

void FakeController::SetDefaultCommandStatus(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  default_command_status_map_[opcode] = status;
}

void FakeController::ClearDefaultCommandStatus(hci_spec::OpCode opcode) {
  default_command_status_map_.erase(opcode);
}

void FakeController::SetDefaultResponseStatus(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  BT_DEBUG_ASSERT(status != pw::bluetooth::emboss::StatusCode::SUCCESS);
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

  peer->set_controller(this);

  // If a scan is enabled then send an advertising report for the peer that just
  // got registered if it supports advertising.
  SendAdvertisingReport(*peer);

  peers_[peer->address()] = std::move(peer);
  return true;
}

void FakeController::RemovePeer(const DeviceAddress& address) {
  peers_.erase(address);
}

FakePeer* FakeController::FindPeer(const DeviceAddress& address) {
  auto iter = peers_.find(address);
  return (iter == peers_.end()) ? nullptr : iter->second.get();
}

void FakeController::SendCommand(pw::span<const std::byte> command) {
  BT_ASSERT(command.size() >= sizeof(hci_spec::CommandHeader));

  // Post the packet to simulate async HCI behavior.
  (void)heap_dispatcher().Post(
      [self = GetWeakPtr(), command = DynamicByteBuffer(BufferView(command))](
          pw::async::Context /*ctx*/, pw::Status status) {
        if (!self.is_alive() || !status.ok()) {
          return;
        }
        const size_t payload_size =
            command.size() - sizeof(hci_spec::CommandHeader);
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

void FakeController::RespondWithCommandComplete(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  hci_spec::SimpleReturnParams params;
  params.status = status;
  RespondWithCommandComplete(opcode, BufferView(&params, sizeof(params)));
}

void FakeController::RespondWithCommandComplete(hci_spec::OpCode opcode,
                                                const ByteBuffer& params) {
  DynamicByteBuffer buffer(sizeof(hci_spec::CommandCompleteEventParams) +
                           params.size());
  MutablePacketView<hci_spec::CommandCompleteEventParams> event(&buffer,
                                                                params.size());

  event.mutable_header()->num_hci_command_packets =
      settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);
  event.mutable_payload_data().Write(params);

  SendEvent(hci_spec::kCommandCompleteEventCode, buffer);
}

void FakeController::RespondWithCommandStatus(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  StaticByteBuffer<sizeof(hci_spec::CommandStatusEventParams)> buffer;
  MutablePacketView<hci_spec::CommandStatusEventParams> event(&buffer);

  event.mutable_header()->status = status;
  event.mutable_header()->num_hci_command_packets =
      settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode = htole16(opcode);

  SendEvent(hci_spec::kCommandStatusEventCode, buffer);
}

void FakeController::SendEvent(hci_spec::EventCode event_code,
                               const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + payload.size());
  MutablePacketView<hci_spec::EventHeader> event(&buffer, payload.size());

  event.mutable_header()->event_code = event_code;
  event.mutable_header()->parameter_total_size = payload.size();
  event.mutable_payload_data().Write(payload);

  SendCommandChannelPacket(buffer);
}

void FakeController::SendLEMetaEvent(hci_spec::EventCode subevent_code,
                                     const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::LEMetaEventParams) +
                           payload.size());
  buffer[0] = subevent_code;
  buffer.Write(payload, 1);
  SendEvent(hci_spec::kLEMetaEventCode, buffer);
}

void FakeController::SendACLPacket(hci_spec::ConnectionHandle handle,
                                   const ByteBuffer& payload) {
  BT_DEBUG_ASSERT(payload.size() <= hci_spec::kMaxACLPayloadSize);

  DynamicByteBuffer buffer(sizeof(hci_spec::ACLDataHeader) + payload.size());
  MutablePacketView<hci_spec::ACLDataHeader> acl(&buffer, payload.size());

  acl.mutable_header()->handle_and_flags = htole16(handle);
  acl.mutable_header()->data_total_length =
      htole16(static_cast<uint16_t>(payload.size()));
  acl.mutable_payload_data().Write(payload);

  SendACLDataChannelPacket(buffer);
}

void FakeController::SendL2CAPBFrame(hci_spec::ConnectionHandle handle,
                                     l2cap::ChannelId channel_id,
                                     const ByteBuffer& payload) {
  BT_DEBUG_ASSERT(payload.size() <=
                  hci_spec::kMaxACLPayloadSize - sizeof(l2cap::BasicHeader));

  DynamicByteBuffer buffer(sizeof(l2cap::BasicHeader) + payload.size());
  MutablePacketView<l2cap::BasicHeader> bframe(&buffer, payload.size());

  bframe.mutable_header()->length = htole16(payload.size());
  bframe.mutable_header()->channel_id = htole16(channel_id);
  bframe.mutable_payload_data().Write(payload);

  SendACLPacket(handle, buffer);
}

void FakeController::SendL2CAPCFrame(hci_spec::ConnectionHandle handle,
                                     bool is_le,
                                     l2cap::CommandCode code,
                                     uint8_t id,
                                     const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(l2cap::CommandHeader) + payload.size());
  MutablePacketView<l2cap::CommandHeader> cframe(&buffer, payload.size());

  cframe.mutable_header()->code = code;
  cframe.mutable_header()->id = id;
  cframe.mutable_header()->length = payload.size();
  cframe.mutable_payload_data().Write(payload);

  SendL2CAPBFrame(
      handle,
      is_le ? l2cap::kLESignalingChannelId : l2cap::kSignalingChannelId,
      buffer);
}

void FakeController::SendNumberOfCompletedPacketsEvent(
    hci_spec::ConnectionHandle handle, uint16_t num) {
  StaticByteBuffer<sizeof(hci_spec::NumberOfCompletedPacketsEventParams) +
                   sizeof(hci_spec::NumberOfCompletedPacketsEventData)>
      buffer;

  auto* params =
      reinterpret_cast<hci_spec::NumberOfCompletedPacketsEventParams*>(
          buffer.mutable_data());
  params->number_of_handles = 1;
  params->data->connection_handle = htole16(handle);
  params->data->hc_num_of_completed_packets = htole16(num);

  SendEvent(hci_spec::kNumberOfCompletedPacketsEventCode, buffer);
}

void FakeController::ConnectLowEnergy(
    const DeviceAddress& addr, pw::bluetooth::emboss::ConnectionRole role) {
  (void)heap_dispatcher().Post(
      [addr, role, this](pw::async::Context /*ctx*/, pw::Status status) {
        if (!status.ok()) {
          return;
        }
        FakePeer* peer = FindPeer(addr);
        if (!peer) {
          bt_log(WARN,
                 "fake-hci",
                 "no peer found with address: %s",
                 addr.ToString().c_str());
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

        hci_spec::LEConnectionParameters conn_params(
            interval_min + ((interval_max - interval_min) / 2),
            0,
            hci_spec::defaults::kLESupervisionTimeout);
        peer->set_le_params(conn_params);

        auto packet = hci::EmbossEventPacket::New<
            pw::bluetooth::emboss::LEConnectionCompleteSubeventWriter>(
            hci_spec::kLEMetaEventCode);
        auto view = packet.view_t();
        view.le_meta_event().subevent_code().Write(
            hci_spec::kLEConnectionCompleteSubeventCode);
        view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
        view.peer_address().CopyFrom(addr.value().view());
        view.peer_address_type().Write(
            DeviceAddress::DeviceAddrToLePeerAddr(addr.type()));
        view.peripheral_latency().Write(conn_params.latency());
        view.connection_interval().Write(conn_params.interval());
        view.supervision_timeout().Write(conn_params.supervision_timeout());
        view.role().Write(role);
        view.connection_handle().Write(handle);
        SendCommandChannelPacket(packet.data());
      });
}

void FakeController::SendConnectionRequest(
    const DeviceAddress& addr, pw::bluetooth::emboss::LinkType link_type) {
  FakePeer* peer = FindPeer(addr);
  BT_ASSERT(peer);
  peer->set_last_connection_request_link_type(link_type);

  bt_log(DEBUG,
         "fake-hci",
         "sending connection request (addr: %s, link: %s)",
         bt_str(addr),
         hci_spec::LinkTypeToString(link_type));
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::ConnectionRequestEventWriter>(
      hci_spec::kConnectionRequestEventCode);
  packet.view_t().bd_addr().CopyFrom(addr.value().view());
  packet.view_t().link_type().Write(link_type);
  SendCommandChannelPacket(packet.data());
}

void FakeController::L2CAPConnectionParameterUpdate(
    const DeviceAddress& addr,
    const hci_spec::LEPreferredConnectionParameters& params) {
  (void)heap_dispatcher().Post(
      [addr, params, this](pw::async::Context /*ctx*/, pw::Status status) {
        if (!status.ok()) {
          return;
        }
        FakePeer* peer = FindPeer(addr);
        if (!peer) {
          bt_log(WARN,
                 "fake-hci",
                 "no peer found with address: %s",
                 addr.ToString().c_str());
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
        SendL2CAPCFrame(*peer->logical_links().begin(),
                        /*is_le=*/true,
                        l2cap::kConnectionParameterUpdateRequest,
                        NextL2CAPCommandId(),
                        BufferView(&payload, sizeof(payload)));
      });
}

void FakeController::SendLEConnectionUpdateCompleteSubevent(
    hci_spec::ConnectionHandle handle,
    const hci_spec::LEConnectionParameters& params,
    pw::bluetooth::emboss::StatusCode status) {
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LEConnectionUpdateCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionUpdateCompleteSubeventCode);
  view.status().Write(status);
  view.connection_handle().Write(handle);
  view.connection_interval().UncheckedWrite(params.interval());
  view.peripheral_latency().Write(params.latency());
  view.supervision_timeout().UncheckedWrite(params.supervision_timeout());
  SendCommandChannelPacket(packet.data());
}

void FakeController::Disconnect(const DeviceAddress& addr,
                                pw::bluetooth::emboss::StatusCode reason) {
  (void)heap_dispatcher().Post(
      [this, addr, reason](pw::async::Context /*ctx*/, pw::Status status) {
        if (!status.ok()) {
          return;
        }
        FakePeer* peer = FindPeer(addr);
        if (!peer || !peer->connected()) {
          bt_log(WARN,
                 "fake-hci",
                 "no connected peer found with address: %s",
                 addr.ToString().c_str());
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

void FakeController::SendDisconnectionCompleteEvent(
    hci_spec::ConnectionHandle handle,
    pw::bluetooth::emboss::StatusCode reason) {
  auto event = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::DisconnectionCompleteEventWriter>(
      hci_spec::kDisconnectionCompleteEventCode);
  event.view_t().status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  event.view_t().connection_handle().Write(handle);
  event.view_t().reason().Write(reason);
  SendCommandChannelPacket(event.data());
}

void FakeController::SendEncryptionChangeEvent(
    hci_spec::ConnectionHandle handle,
    pw::bluetooth::emboss::StatusCode status,
    pw::bluetooth::emboss::EncryptionStatus encryption_enabled) {
  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::EncryptionChangeEventV1Writer>(
      hci_spec::kEncryptionChangeEventCode);
  response.view_t().status().Write(status);
  response.view_t().connection_handle().Write(handle);
  response.view_t().encryption_enabled().Write(encryption_enabled);
  SendCommandChannelPacket(response.data());
}

bool FakeController::MaybeRespondWithDefaultCommandStatus(
    hci_spec::OpCode opcode) {
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

  bt_log(INFO,
         "fake-hci",
         "responding with error (command: %#.4x, status: %#.2hhx)",
         opcode,
         static_cast<unsigned char>(iter->second));
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

static void WriteScanResponseReport(const FakePeer& peer,
                                    hci_spec::LEAdvertisingReportData* report) {
  BT_DEBUG_ASSERT(peer.scannable());
  report->event_type = hci_spec::LEAdvertisingEventType::kScanRsp;

  report->address = peer.address().value();
  report->address_type = hci_spec::LEAddressType::kPublic;
  if (peer.address().type() == DeviceAddress::Type::kLERandom) {
    report->address_type = hci_spec::LEAddressType::kRandom;
  }

  report->length_data = peer.scan_response().size();
  std::memcpy(
      report->data, peer.scan_response().data(), peer.scan_response().size());

  report->data[report->length_data] = peer.rssi();
}

DynamicByteBuffer FakeController::BuildLegacyAdvertisingReportEvent(
    const FakePeer& peer, bool include_scan_rsp) {
  BT_DEBUG_ASSERT(peer.advertising_data().size() <=
                  hci_spec::kMaxLEAdvertisingDataLength);
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) +
                      peer.advertising_data().size() + sizeof(int8_t);

  if (include_scan_rsp) {
    BT_DEBUG_ASSERT(peer.scannable());
    BT_DEBUG_ASSERT(peer.scan_response().size() <=
                    hci_spec::kMaxLEAdvertisingDataLength);
    param_size += sizeof(hci_spec::LEAdvertisingReportData) +
                  peer.scan_response().size() + sizeof(int8_t);
  }

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(
          payload->subevent_parameters);
  subevent_payload->num_reports = 1;
  if (include_scan_rsp) {
    subevent_payload->num_reports++;
  }
  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
      subevent_payload->reports);
  if (peer.directed_advertising_enabled()) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvDirectInd;
  } else if (peer.connectable()) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvInd;
  } else if (peer.scannable()) {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvScanInd;
  } else {
    report->event_type = hci_spec::LEAdvertisingEventType::kAdvNonConnInd;
  }

  if (peer.address().type() == DeviceAddress::Type::kLERandom) {
    report->address_type = hci_spec::LEAddressType::kRandom;
    if (peer.address_resolved()) {
      report->address_type = hci_spec::LEAddressType::kRandomIdentity;
    }
  } else {
    report->address_type = hci_spec::LEAddressType::kPublic;
    if (peer.address_resolved()) {
      report->address_type = hci_spec::LEAddressType::kPublicIdentity;
    }
  }

  report->address = peer.address().value();
  report->length_data = peer.advertising_data().size();
  std::memcpy(report->data,
              peer.advertising_data().data(),
              peer.advertising_data().size());
  report->data[report->length_data] = peer.rssi();

  if (include_scan_rsp) {
    auto* scan_response_report =
        reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
            report->data + report->length_data + sizeof(int8_t));
    WriteScanResponseReport(peer, scan_response_report);
  }

  return buffer;
}

DynamicByteBuffer FakeController::BuildLegacyScanResponseReportEvent(
    const FakePeer& peer) const {
  BT_DEBUG_ASSERT(peer.scannable());
  BT_DEBUG_ASSERT(peer.scan_response().size() <=
                  hci_spec::kMaxLEAdvertisingDataLength);
  size_t param_size = sizeof(hci_spec::LEMetaEventParams) +
                      sizeof(hci_spec::LEAdvertisingReportSubeventParams) +
                      sizeof(hci_spec::LEAdvertisingReportData) +
                      peer.scan_response().size() + sizeof(int8_t);

  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + param_size);
  MutablePacketView<hci_spec::EventHeader> event(&buffer, param_size);
  event.mutable_header()->event_code = hci_spec::kLEMetaEventCode;
  event.mutable_header()->parameter_total_size = param_size;

  auto payload = event.mutable_payload<hci_spec::LEMetaEventParams>();
  payload->subevent_code = hci_spec::kLEAdvertisingReportSubeventCode;

  auto subevent_payload =
      reinterpret_cast<hci_spec::LEAdvertisingReportSubeventParams*>(
          payload->subevent_parameters);
  subevent_payload->num_reports = 1;

  auto report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
      subevent_payload->reports);
  WriteScanResponseReport(peer, report);

  return buffer;
}

void FakeController::FillExtendedAdvertisingReport(
    const FakePeer& peer,
    pw::bluetooth::emboss::LEExtendedAdvertisingReportDataWriter report,
    const ByteBuffer& data,
    bool is_fragmented,
    bool is_scan_response) const {
  if (peer.use_extended_advertising_pdus()) {
    report.event_type().directed().Write(peer.directed_advertising_enabled());
    report.event_type().connectable().Write(peer.connectable());
    report.event_type().scannable().Write(peer.scannable());
    report.event_type().scan_response().Write(is_scan_response);

    if (is_fragmented) {
      report.event_type().data_status().Write(
          pw::bluetooth::emboss::LEAdvertisingDataStatus::INCOMPLETE);
    } else {
      report.event_type().data_status().Write(
          pw::bluetooth::emboss::LEAdvertisingDataStatus::COMPLETE);
    }
  } else {
    report.event_type().legacy().Write(true);
    if (is_scan_response) {
      report.event_type().scan_response().Write(true);
    }

    if (peer.directed_advertising_enabled()) {  // ADV_DIRECT_IND
      report.event_type().directed().Write(true);
      report.event_type().connectable().Write(true);
    } else if (peer.connectable()) {  // ADV_IND
      report.event_type().connectable().Write(true);
      report.event_type().scannable().Write(true);
    } else if (peer.scannable()) {  // ADV_SCAN_IND
      report.event_type().scannable().Write(true);
    }
    // else ADV_NONCONN_IND
  }

  if (peer.address().type() == DeviceAddress::Type::kLERandom) {
    if (peer.address_resolved()) {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::RANDOM_IDENTITY);
    } else {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::RANDOM);
    }
  } else {
    if (peer.address_resolved()) {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::PUBLIC_IDENTITY);
    } else {
      report.address_type().Write(
          pw::bluetooth::emboss::LEExtendedAddressType::PUBLIC);
    }
  }

  report.address().bd_addr().CopyFrom(peer.address().value().view().bd_addr());
  report.primary_phy().Write(
      pw::bluetooth::emboss::LEPrimaryAdvertisingPHY::LE_1M);
  report.secondary_phy().Write(
      pw::bluetooth::emboss::LESecondaryAdvertisingPHY::NONE);
  report.advertising_sid().Write(0);
  report.tx_power().Write(peer.tx_power());
  report.rssi().Write(peer.rssi());
  report.periodic_advertising_interval().Write(0);

  // skip direct_address_type and direct_address for now since we don't use it

  report.data_length().Write(data.size());
  std::memcpy(report.data().BackingStorage().begin(), data.data(), data.size());
}

DynamicByteBuffer FakeController::BuildExtendedAdvertisingReports(
    const FakePeer& peer, const ByteBuffer& data, bool is_scan_response) const {
  using pw::bluetooth::emboss::LEExtendedAdvertisingReportDataWriter;
  using pw::bluetooth::emboss::LEExtendedAdvertisingReportSubeventWriter;

  size_t num_full_reports =
      data.size() / hci_spec::kMaxPduLEExtendedAdvertisingDataLength;
  size_t full_report_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
      hci_spec::kMaxPduLEExtendedAdvertisingDataLength;
  size_t last_report_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportData::MinSizeInBytes() +
      (data.size() % hci_spec::kMaxPduLEExtendedAdvertisingDataLength);

  size_t reports_size = num_full_reports * full_report_size + last_report_size;
  size_t packet_size =
      pw::bluetooth::emboss::LEExtendedAdvertisingReportSubevent::
          MinSizeInBytes() +
      reports_size;

  auto event =
      hci::EmbossEventPacket::New<LEExtendedAdvertisingReportSubeventWriter>(
          hci_spec::kLEMetaEventCode, packet_size);
  auto packet =
      event.view<LEExtendedAdvertisingReportSubeventWriter>(reports_size);
  packet.le_meta_event().subevent_code().Write(
      hci_spec::kLEExtendedAdvertisingReportSubeventCode);

  uint8_t num_reports = num_full_reports + 1;
  packet.num_reports().Write(num_reports);

  for (size_t i = 0; i < num_full_reports; i++) {
    bool is_fragmented = false;
    if (num_reports > 1) {
      is_fragmented = true;
    }

    LEExtendedAdvertisingReportDataWriter report(
        packet.reports().BackingStorage().begin() + full_report_size * i,
        full_report_size);
    FillExtendedAdvertisingReport(
        peer, report, data, is_fragmented, is_scan_response);
  }

  LEExtendedAdvertisingReportDataWriter report(
      packet.reports().BackingStorage().begin() +
          full_report_size * num_full_reports,
      last_report_size);
  FillExtendedAdvertisingReport(peer, report, data, false, is_scan_response);

  return event.release();
}

DynamicByteBuffer FakeController::BuildExtendedAdvertisingReportEvent(
    const FakePeer& peer) const {
  BT_DEBUG_ASSERT(peer.advertising_data().size() <=
                  hci_spec::kMaxLEExtendedAdvertisingDataLength);
  return BuildExtendedAdvertisingReports(peer, peer.advertising_data(), false);
}

DynamicByteBuffer FakeController::BuildExtendedScanResponseEvent(
    const FakePeer& peer) const {
  BT_DEBUG_ASSERT(peer.scannable());
  BT_DEBUG_ASSERT(peer.scan_response().size() <=
                  hci_spec::kMaxLEExtendedAdvertisingDataLength);
  return BuildExtendedAdvertisingReports(peer, peer.scan_response(), true);
}

void FakeController::SendAdvertisingReports() {
  if (!le_scan_state_.enabled || peers_.empty()) {
    return;
  }

  for (const auto& iter : peers_) {
    SendAdvertisingReport(*iter.second);
  }

  // We'll send new reports for the same peers if duplicate filtering is
  // disabled.
  if (!le_scan_state_.filter_duplicates) {
    (void)heap_dispatcher().Post(
        [this](pw::async::Context /*ctx*/, pw::Status status) {
          if (status.ok()) {
            SendAdvertisingReports();
          }
        });
  }
}

void FakeController::SendAdvertisingReport(const FakePeer& peer) {
  if (!le_scan_state_.enabled || !peer.supports_le() ||
      !peer.advertising_enabled()) {
    return;
  }

  // We want to send scan response packets only during an active scan and if the
  // peer is scannable.
  bool is_active_scan =
      (le_scan_state_.scan_type == pw::bluetooth::emboss::LEScanType::ACTIVE);
  bool need_scan_rsp = (is_active_scan && peer.scannable());

  if (received_extended_operations_) {
    SendCommandChannelPacket(BuildExtendedAdvertisingReportEvent(peer));

    if (need_scan_rsp) {
      SendCommandChannelPacket(BuildExtendedScanResponseEvent(peer));
    }
  } else {
    bool include_scan_rsp = (need_scan_rsp && peer.should_batch_reports());
    SendCommandChannelPacket(
        BuildLegacyAdvertisingReportEvent(peer, include_scan_rsp));

    // If the original report did not include a scan response then we send it as
    // a separate event.
    if (need_scan_rsp && !peer.should_batch_reports()) {
      SendCommandChannelPacket(BuildLegacyScanResponseReportEvent(peer));
    }
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
                                           hci_spec::ConnectionHandle handle,
                                           bool connected,
                                           bool canceled) {
  if (conn_state_cb_) {
    conn_state_cb_(addr, handle, connected, canceled);
  }
}

void FakeController::NotifyLEConnectionParameters(
    const DeviceAddress& addr, const hci_spec::LEConnectionParameters& params) {
  if (le_conn_params_cb_) {
    le_conn_params_cb_(addr, params);
  }
}

void FakeController::OnCreateConnectionCommandReceived(
    const pw::bluetooth::emboss::CreateConnectionCommandView& params) {
  acl_create_connection_command_count_++;

  // Cannot issue this command while a request is already pending.
  if (bredr_connect_pending_) {
    RespondWithCommandStatus(
        hci_spec::kCreateConnection,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));
  pw::bluetooth::emboss::StatusCode status =
      pw::bluetooth::emboss::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kCreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != pw::bluetooth::emboss::StatusCode::SUCCESS)
    return;

  bredr_connect_pending_ = true;
  pending_bredr_connect_addr_ = peer_address;

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO,
           "fake-hci",
           "requested peer %s cannot be connected; request will time out",
           peer_address.ToString().c_str());

    bredr_connect_rsp_task_.Cancel();
    bredr_connect_rsp_task_.set_function(
        [this, peer_address](pw::async::Context /*ctx*/, pw::Status status) {
          if (!status.ok()) {
            return;
          }
          bredr_connect_pending_ = false;

          auto response = hci::EmbossEventPacket::New<
              pw::bluetooth::emboss::ConnectionCompleteEventWriter>(
              hci_spec::kConnectionCompleteEventCode);
          response.view_t().status().Write(
              pw::bluetooth::emboss::StatusCode::PAGE_TIMEOUT);
          response.view_t().bd_addr().CopyFrom(peer_address.value().view());
          SendCommandChannelPacket(response.data());
        });

    // Default page timeout of 5.12s
    // See Core Spec v5.0 Vol 2, Part E, Section 6.6
    constexpr pw::chrono::SystemClock::duration default_page_timeout =
        std::chrono::microseconds(static_cast<int64_t>(625) * 0x2000);
    bredr_connect_rsp_task_.PostAfter(default_page_timeout);
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = pw::bluetooth::emboss::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::ConnectionCompleteEventWriter>(
      hci_spec::kConnectionCompleteEventCode);
  response.view_t().status().Write(status);
  response.view_t().bd_addr().CopyFrom(params.bd_addr());
  response.view_t().link_type().Write(pw::bluetooth::emboss::LinkType::ACL);
  response.view_t().encryption_enabled().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);

  if (status == pw::bluetooth::emboss::StatusCode::SUCCESS) {
    hci_spec::ConnectionHandle handle = ++next_conn_handle_;
    response.view_t().connection_handle().Write(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  bredr_connect_rsp_task_.Cancel();
  bredr_connect_rsp_task_.set_function(
      [response, peer, this](pw::async::Context /*ctx*/,
                             pw::Status status) mutable {
        if (!status.ok()) {
          return;
        }
        bredr_connect_pending_ = false;

        if (response.view_t().status().Read() ==
            pw::bluetooth::emboss::StatusCode::SUCCESS) {
          bool notify = !peer->connected();
          hci_spec::ConnectionHandle handle =
              response.view_t().connection_handle().Read();
          peer->AddLink(handle);
          if (notify && peer->connected()) {
            NotifyConnectionState(peer->address(), handle, /*connected=*/true);
          }
        }

        SendCommandChannelPacket(response.data());
      });
  bredr_connect_rsp_task_.Post();
}

void FakeController::OnLECreateConnectionCommandReceived(
    const pw::bluetooth::emboss::LECreateConnectionCommandView& params) {
  le_create_connection_command_count_++;
  if (le_create_connection_cb_) {
    le_create_connection_cb_(params);
  }

  // Cannot issue this command while a request is already pending.
  if (le_connect_pending_) {
    RespondWithCommandStatus(
        hci_spec::kLECreateConnection,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  DeviceAddress::Type addr_type =
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read());
  BT_DEBUG_ASSERT(addr_type != DeviceAddress::Type::kBREDR);

  const DeviceAddress peer_address(addr_type,
                                   DeviceAddressBytes(params.peer_address()));
  pw::bluetooth::emboss::StatusCode status =
      pw::bluetooth::emboss::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected())
      status = pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS;
    else
      status = peer->connect_status();
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kLECreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != pw::bluetooth::emboss::StatusCode::SUCCESS)
    return;

  le_connect_pending_ = true;
  if (!le_connect_params_) {
    le_connect_params_ = LEConnectParams();
  }

  le_connect_params_->own_address_type = params.own_address_type().Read();
  le_connect_params_->peer_address = peer_address;

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO,
           "fake-hci",
           "requested fake peer cannot be connected; request will time out");
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = pw::bluetooth::emboss::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LEConnectionCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionCompleteSubeventCode);
  view.status().Write(status);
  view.peer_address().CopyFrom(params.peer_address());
  view.peer_address_type().Write(
      DeviceAddress::DeviceAddrToLePeerAddr(addr_type));

  if (status == pw::bluetooth::emboss::StatusCode::SUCCESS) {
    uint16_t interval_min = params.connection_interval_min().UncheckedRead();
    uint16_t interval_max = params.connection_interval_max().UncheckedRead();
    uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

    hci_spec::LEConnectionParameters conn_params(
        interval,
        params.max_latency().UncheckedRead(),
        params.supervision_timeout().UncheckedRead());
    peer->set_le_params(conn_params);

    view.peripheral_latency().UncheckedCopyFrom(params.max_latency());
    view.connection_interval().UncheckedWrite(interval);
    view.supervision_timeout().UncheckedCopyFrom(params.supervision_timeout());
    view.role().Write(pw::bluetooth::emboss::ConnectionRole::CENTRAL);
    view.connection_handle().Write(++next_conn_handle_);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect())
    return;

  le_connect_rsp_task_.Cancel();
  le_connect_rsp_task_.set_function([packet, address = peer_address, this](
                                        pw::async::Context /*ctx*/,
                                        pw::Status status) {
    auto peer = FindPeer(address);
    if (!peer || !status.ok()) {
      // The peer has been removed or dispatcher shut down; Ignore this response
      return;
    }

    le_connect_pending_ = false;

    auto view =
        packet.view<pw::bluetooth::emboss::LEConnectionCompleteSubeventView>();
    if (view.status().Read() == pw::bluetooth::emboss::StatusCode::SUCCESS) {
      bool not_previously_connected = !peer->connected();
      hci_spec::ConnectionHandle handle = view.connection_handle().Read();
      peer->AddLink(handle);
      if (not_previously_connected && peer->connected()) {
        NotifyConnectionState(peer->address(), handle, /*connected=*/true);
      }
    }

    SendCommandChannelPacket(packet.data());
  });
  le_connect_rsp_task_.PostAfter(settings_.le_connection_delay);
}

void FakeController::OnLEConnectionUpdateCommandReceived(
    const pw::bluetooth::emboss::LEConnectionUpdateCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kLEConnectionUpdate,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  uint16_t min_interval = params.connection_interval_min().UncheckedRead();
  uint16_t max_interval = params.connection_interval_max().UncheckedRead();
  uint16_t max_latency = params.max_latency().UncheckedRead();
  uint16_t supv_timeout = params.supervision_timeout().UncheckedRead();

  if (min_interval > max_interval) {
    RespondWithCommandStatus(
        hci_spec::kLEConnectionUpdate,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::LEConnectionParameters conn_params(
      min_interval + ((max_interval - min_interval) / 2),
      max_latency,
      supv_timeout);
  peer->set_le_params(conn_params);

  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LEConnectionUpdateCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionUpdateCompleteSubeventCode);
  view.connection_handle().CopyFrom(params.connection_handle());
  if (peer->supports_ll_conn_update_procedure()) {
    view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
    view.connection_interval().UncheckedWrite(conn_params.interval());
    view.peripheral_latency().CopyFrom(params.max_latency());
    view.supervision_timeout().UncheckedCopyFrom(params.supervision_timeout());
  } else {
    view.status().Write(
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_REMOTE_FEATURE);
  }
  SendCommandChannelPacket(packet.data());

  NotifyLEConnectionParameters(peer->address(), conn_params);
}

void FakeController::OnDisconnectCommandReceived(
    const pw::bluetooth::emboss::DisconnectCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();

  // Find the peer that matches the disconnected handle.
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kDisconnect,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  RespondWithCommandStatus(hci_spec::kDisconnect,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

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
    const pw::bluetooth::emboss::WriteLEHostSupportCommandView& params) {
  if (params.le_supported_host().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    SetBit(&settings_.lmp_features_page1,
           hci_spec::LMPFeature::kLESupportedHost);
  } else {
    UnsetBit(&settings_.lmp_features_page1,
             hci_spec::LMPFeature::kLESupportedHost);
  }

  RespondWithCommandComplete(hci_spec::kWriteLEHostSupport,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnWriteSecureConnectionsHostSupport(
    const pw::bluetooth::emboss::WriteSecureConnectionsHostSupportCommandView&
        params) {
  // Core Spec Volume 4, Part E, Section 7.3.92: If the Host issues this command
  // while the Controller is paging, has page scanning enabled, or has an ACL
  // connection, the Controller shall return the error code Command Disallowed
  // (0x0C).
  bool has_acl_connection = false;
  for (auto& [_, peer] : peers_) {
    if (peer->connected()) {
      has_acl_connection = true;
      break;
    }
  }
  if (bredr_connect_pending_ || isBREDRPageScanEnabled() ||
      has_acl_connection) {
    RespondWithCommandComplete(
        hci_spec::kWriteSecureConnectionsHostSupport,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (params.secure_connections_host_support().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    SetBit(&settings_.lmp_features_page1,
           hci_spec::LMPFeature::kSecureConnectionsHostSupport);
  } else {
    UnsetBit(&settings_.lmp_features_page1,
             hci_spec::LMPFeature::kSecureConnectionsHostSupport);
  }

  RespondWithCommandComplete(hci_spec::kWriteSecureConnectionsHostSupport,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReset() {
  // TODO(fxbug.dev/78955): actually do some resetting of stuff here
  RespondWithCommandComplete(hci_spec::kReset,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnInquiry(
    const pw::bluetooth::emboss::InquiryCommandView& params) {
  // Confirm that LAP array is equal to either kGIAC or kLIAC.
  if (params.lap().Read() != pw::bluetooth::emboss::InquiryAccessCode::GIAC &&
      params.lap().Read() != pw::bluetooth::emboss::InquiryAccessCode::LIAC) {
    RespondWithCommandStatus(
        hci_spec::kInquiry,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.inquiry_length().Read() == 0x00 ||
      params.inquiry_length().Read() > hci_spec::kInquiryLengthMax) {
    RespondWithCommandStatus(
        hci_spec::kInquiry,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  inquiry_num_responses_left_ = params.num_responses().Read();
  if (params.num_responses().Read() == 0) {
    inquiry_num_responses_left_ = -1;
  }

  RespondWithCommandStatus(hci_spec::kInquiry,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  bt_log(INFO, "fake-hci", "sending inquiry responses..");
  SendInquiryResponses();

  (void)heap_dispatcher().PostAfter(
      [this](pw::async::Context /*ctx*/, pw::Status status) {
        if (!status.ok()) {
          return;
        }
        auto output = hci::EmbossEventPacket::New<
            pw::bluetooth::emboss::InquiryCompleteEventWriter>(
            hci_spec::kInquiryCompleteEventCode);
        output.view_t().status().Write(
            pw::bluetooth::emboss::StatusCode::SUCCESS);
        SendCommandChannelPacket(output.data());
      },
      std::chrono::milliseconds(params.inquiry_length().Read()) * 1280);
}

void FakeController::OnLESetScanEnable(
    const pw::bluetooth::emboss::LESetScanEnableCommandView& params) {
  le_scan_state_.enabled = false;
  if (params.le_scan_enable().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    le_scan_state_.enabled = true;
  }

  le_scan_state_.filter_duplicates = false;
  if (params.filter_duplicates().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    le_scan_state_.filter_duplicates = true;
  }

  // Post the scan state update before scheduling the HCI Command Complete
  // event. This guarantees that single-threaded unit tests receive the scan
  // state update BEFORE the HCI command sequence terminates.
  if (scan_state_cb_) {
    scan_state_cb_(le_scan_state_.enabled);
  }

  RespondWithCommandComplete(hci_spec::kLESetScanEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);

  if (le_scan_state_.enabled) {
    SendAdvertisingReports();
  }
}

void FakeController::OnLESetExtendedScanEnable(
    const pw::bluetooth::emboss::LESetExtendedScanEnableCommandView& params) {
  received_extended_operations_ = true;

  le_scan_state_.enabled = false;
  if (params.scanning_enabled().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    le_scan_state_.enabled = true;
  }

  le_scan_state_.filter_duplicates = true;
  if (params.filter_duplicates().Read() ==
      pw::bluetooth::emboss::LEExtendedDuplicateFilteringOption::DISABLED) {
    le_scan_state_.filter_duplicates = false;
  }

  le_scan_state_.duration = params.duration().Read();
  le_scan_state_.period = params.period().Read();

  // Post the scan state update before scheduling the HCI Command Complete
  // event. This guarantees that single-threaded unit tests receive the scan
  // state update BEFORE the HCI command sequence terminates.
  if (scan_state_cb_) {
    scan_state_cb_(le_scan_state_.enabled);
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedScanEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);

  if (le_scan_state_.enabled) {
    SendAdvertisingReports();
  }
}

void FakeController::OnLESetScanParameters(
    const pw::bluetooth::emboss::LESetScanParametersCommandView& params) {
  if (le_scan_state_.enabled) {
    RespondWithCommandComplete(
        hci_spec::kLESetScanParameters,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_scan_state_.own_address_type = params.own_address_type().Read();
  le_scan_state_.filter_policy = params.scanning_filter_policy().Read();
  le_scan_state_.scan_type = params.le_scan_type().Read();
  le_scan_state_.scan_interval = params.le_scan_interval().Read();
  le_scan_state_.scan_window = params.le_scan_window().Read();

  RespondWithCommandComplete(hci_spec::kLESetScanParameters,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnLESetExtendedScanParameters(
    const pw::bluetooth::emboss::LESetExtendedScanParametersCommandView&
        params) {
  received_extended_operations_ = true;

  if (le_scan_state_.enabled) {
    RespondWithCommandComplete(
        hci_spec::kLESetScanParameters,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (params.num_entries().Read() == 0) {
    RespondWithCommandComplete(
        hci_spec::kLESetScanParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  le_scan_state_.own_address_type = params.own_address_type().Read();
  le_scan_state_.filter_policy = params.scanning_filter_policy().Read();

  // ExtendedLowEnergyScanner sets the same parameters for both the LE 1M and LE
  // Coded PHYs. We just take the parameters from the LE 1M PHY for now since we
  // don't support using different parameters for different PHYs.
  if (!params.scanning_phys().le_1m().Read()) {
    RespondWithCommandComplete(
        hci_spec::kLESetScanParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  le_scan_state_.scan_type = params.data()[0].scan_type().Read();
  le_scan_state_.scan_interval = params.data()[0].scan_interval().Read();
  le_scan_state_.scan_window = params.data()[0].scan_window().Read();
  RespondWithCommandComplete(hci_spec::kLESetExtendedScanParameters,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadLocalExtendedFeatures(
    const pw::bluetooth::emboss::ReadLocalExtendedFeaturesCommandView& params) {
  hci_spec::ReadLocalExtendedFeaturesReturnParams out_params;
  out_params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  out_params.page_number = params.page_number().Read();
  out_params.maximum_page_number = 2;
  out_params.extended_lmp_features = 0;

  switch (params.page_number().Read()) {
    case 0:
      out_params.extended_lmp_features = htole64(settings_.lmp_features_page0);
      break;
    case 1:
      out_params.extended_lmp_features = htole64(settings_.lmp_features_page1);
      break;
    case 2:
      out_params.extended_lmp_features = htole64(settings_.lmp_features_page2);
      break;
    default:
      out_params.status =
          pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
  }

  RespondWithCommandComplete(hci_spec::kReadLocalExtendedFeatures,
                             BufferView(&out_params, sizeof(out_params)));
}

void FakeController::OnSetEventMask(
    const pw::bluetooth::emboss::SetEventMaskCommandView& params) {
  settings_.event_mask = params.event_mask().Read();
  RespondWithCommandComplete(hci_spec::kSetEventMask,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnLESetEventMask(
    const pw::bluetooth::emboss::LESetEventMaskCommandView& params) {
  settings_.le_event_mask = params.le_event_mask().BackingStorage().ReadUInt();
  RespondWithCommandComplete(hci_spec::kLESetEventMask,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnLEReadBufferSizeV1() {
  hci_spec::LEReadBufferSizeReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.hc_le_acl_data_packet_length =
      htole16(settings_.le_acl_data_packet_length);
  params.hc_total_num_le_acl_data_packets =
      settings_.le_total_num_acl_data_packets;
  RespondWithCommandComplete(hci_spec::kLEReadBufferSizeV1,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadSupportedStates() {
  hci_spec::LEReadSupportedStatesReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.le_states = htole64(settings_.le_supported_states);
  RespondWithCommandComplete(hci_spec::kLEReadSupportedStates,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadLocalSupportedFeatures() {
  hci_spec::LEReadLocalSupportedFeaturesReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.le_features = htole64(settings_.le_features);
  RespondWithCommandComplete(hci_spec::kLEReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLECreateConnectionCancel() {
  if (!le_connect_pending_) {
    // No request is currently pending.
    RespondWithCommandComplete(
        hci_spec::kLECreateConnectionCancel,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_connect_pending_ = false;
  le_connect_rsp_task_.Cancel();
  BT_DEBUG_ASSERT(le_connect_params_);

  NotifyConnectionState(le_connect_params_->peer_address,
                        0,
                        /*connected=*/false,
                        /*canceled=*/true);

  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LEConnectionCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionCompleteSubeventCode);
  view.status().Write(pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
  view.peer_address().CopyFrom(le_connect_params_->peer_address.value().view());
  view.peer_address_type().Write(DeviceAddress::DeviceAddrToLePeerAddr(
      le_connect_params_->peer_address.type()));

  RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  SendCommandChannelPacket(packet.data());
}

void FakeController::OnWriteExtendedInquiryResponse(
    const pw::bluetooth::emboss::WriteExtendedInquiryResponseCommandView&
        params) {
  // As of now, we don't support FEC encoding enabled.
  if (params.fec_required().Read() != 0x00) {
    RespondWithCommandStatus(
        hci_spec::kWriteExtendedInquiryResponse,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  }

  RespondWithCommandComplete(hci_spec::kWriteExtendedInquiryResponse,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnWriteSimplePairingMode(
    const pw::bluetooth::emboss::WriteSimplePairingModeCommandView& params) {
  // "A host shall not set the Simple Pairing Mode to 'disabled'"
  // Spec 5.0 Vol 2 Part E Sec 7.3.59
  if (params.simple_pairing_mode().Read() !=
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    RespondWithCommandComplete(
        hci_spec::kWriteSimplePairingMode,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  SetBit(&settings_.lmp_features_page1,
         hci_spec::LMPFeature::kSecureSimplePairingHostSupport);
  RespondWithCommandComplete(hci_spec::kWriteSimplePairingMode,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadSimplePairingMode() {
  hci_spec::ReadSimplePairingModeReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  if (CheckBit(settings_.lmp_features_page1,
               hci_spec::LMPFeature::kSecureSimplePairingHostSupport)) {
    params.simple_pairing_mode =
        pw::bluetooth::emboss::GenericEnableParam::ENABLE;
  } else {
    params.simple_pairing_mode =
        pw::bluetooth::emboss::GenericEnableParam::DISABLE;
  }

  RespondWithCommandComplete(hci_spec::kReadSimplePairingMode,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWritePageScanType(
    const pw::bluetooth::emboss::WritePageScanTypeCommandView& params) {
  page_scan_type_ = params.page_scan_type().Read();
  RespondWithCommandComplete(hci_spec::kWritePageScanType,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanType() {
  hci_spec::ReadPageScanTypeReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.page_scan_type = page_scan_type_;
  RespondWithCommandComplete(hci_spec::kReadPageScanType,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteInquiryMode(
    const pw::bluetooth::emboss::WriteInquiryModeCommandView& params) {
  inquiry_mode_ = params.inquiry_mode().Read();
  RespondWithCommandComplete(hci_spec::kWriteInquiryMode,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadInquiryMode() {
  hci_spec::ReadInquiryModeReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.inquiry_mode = inquiry_mode_;
  RespondWithCommandComplete(hci_spec::kReadInquiryMode,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteClassOfDevice(
    const pw::bluetooth::emboss::WriteClassOfDeviceCommandView& params) {
  device_class_ =
      DeviceClass(params.class_of_device().BackingStorage().ReadUInt());
  NotifyControllerParametersChanged();
  RespondWithCommandComplete(hci_spec::kWriteClassOfDevice,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnWritePageScanActivity(
    const pw::bluetooth::emboss::WritePageScanActivityCommandView& params) {
  page_scan_interval_ = params.page_scan_interval().Read();
  page_scan_window_ = params.page_scan_window().Read();
  RespondWithCommandComplete(hci_spec::kWritePageScanActivity,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanActivity() {
  hci_spec::ReadPageScanActivityReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.page_scan_interval = htole16(page_scan_interval_);
  params.page_scan_window = htole16(page_scan_window_);
  RespondWithCommandComplete(hci_spec::kReadPageScanActivity,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteScanEnable(
    const pw::bluetooth::emboss::WriteScanEnableCommandView& params) {
  bredr_scan_state_ = params.scan_enable().BackingStorage().ReadUInt();
  RespondWithCommandComplete(hci_spec::kWriteScanEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadScanEnable() {
  hci_spec::ReadScanEnableReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.scan_enable = bredr_scan_state_;
  RespondWithCommandComplete(hci_spec::kReadScanEnable,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalName() {
  hci_spec::ReadLocalNameReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  auto mut_view =
      MutableBufferView(params.local_name, hci_spec::kMaxNameLength);
  mut_view.Write((const uint8_t*)(local_name_.c_str()),
                 std::min(local_name_.length() + 1, hci_spec::kMaxNameLength));
  RespondWithCommandComplete(hci_spec::kReadLocalName,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteLocalName(
    const pw::bluetooth::emboss::WriteLocalNameCommandView& params) {
  std::size_t name_len = 0;

  auto local_name = params.local_name().BackingStorage().data();

  for (; name_len < hci_spec::kMaxNameLength; ++name_len) {
    if (local_name[name_len] == '\0') {
      break;
    }
  }
  local_name_ = std::string(local_name, local_name + name_len);
  NotifyControllerParametersChanged();
  RespondWithCommandComplete(hci_spec::kWriteLocalName,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnCreateConnectionCancel() {
  hci_spec::CreateConnectionCancelReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.bd_addr = pending_bredr_connect_addr_.value();

  if (!bredr_connect_pending_) {
    // No request is currently pending.
    params.status = pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID;
    RespondWithCommandComplete(hci_spec::kCreateConnectionCancel,
                               BufferView(&params, sizeof(params)));
    return;
  }

  bredr_connect_pending_ = false;
  bredr_connect_rsp_task_.Cancel();

  NotifyConnectionState(
      pending_bredr_connect_addr_, 0, /*connected=*/false, /*canceled=*/true);

  RespondWithCommandComplete(hci_spec::kCreateConnectionCancel,
                             BufferView(&params, sizeof(params)));

  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::ConnectionCompleteEventWriter>(
      hci_spec::kConnectionCompleteEventCode);
  response.view_t().status().Write(
      pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
  response.view_t().bd_addr().CopyFrom(
      pending_bredr_connect_addr_.value().view());
  SendCommandChannelPacket(response.data());
}

void FakeController::OnReadBufferSize() {
  hci_spec::ReadBufferSizeReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.hc_acl_data_packet_length = htole16(settings_.acl_data_packet_length);
  params.hc_total_num_acl_data_packets = settings_.total_num_acl_data_packets;
  params.hc_synchronous_data_packet_length =
      htole16(settings_.synchronous_data_packet_length);
  params.hc_total_num_synchronous_data_packets =
      settings_.total_num_synchronous_data_packets;
  RespondWithCommandComplete(hci_spec::kReadBufferSize,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadBRADDR() {
  hci_spec::ReadBDADDRReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.bd_addr = settings_.bd_addr.value();
  RespondWithCommandComplete(hci_spec::kReadBDADDR,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLESetAdvertisingEnable(
    const pw::bluetooth::emboss::LESetAdvertisingEnableCommandView& params) {
  // TODO(fxbug.dev/81444): if own address type is random, check that a random
  // address is set

  legacy_advertising_state_.enabled =
      params.advertising_enable().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE;
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetScanResponseData(
    const pw::bluetooth::emboss::LESetScanResponseDataCommandView& params) {
  legacy_advertising_state_.scan_rsp_length =
      params.scan_response_data_length().Read();

  if (params.scan_response_data_length().Read() == 0) {
    std::memset(legacy_advertising_state_.scan_rsp_data,
                0,
                sizeof(legacy_advertising_state_.scan_rsp_data));
  } else {
    std::memcpy(legacy_advertising_state_.scan_rsp_data,
                params.scan_response_data().BackingStorage().data(),
                params.scan_response_data_length().Read());
  }

  RespondWithCommandComplete(hci_spec::kLESetScanResponseData,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingData(
    const pw::bluetooth::emboss::LESetAdvertisingDataCommandView& params) {
  legacy_advertising_state_.data_length =
      params.advertising_data_length().Read();

  if (params.advertising_data_length().Read() == 0) {
    std::memset(legacy_advertising_state_.data,
                0,
                sizeof(legacy_advertising_state_.data));
  } else {
    std::memcpy(legacy_advertising_state_.data,
                params.advertising_data().BackingStorage().data(),
                params.advertising_data_length().Read());
  }

  RespondWithCommandComplete(hci_spec::kLESetAdvertisingData,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingParameters(
    const pw::bluetooth::emboss::LESetAdvertisingParametersCommandView&
        params) {
  if (legacy_advertising_state_.enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot set advertising parameters while advertising enabled");
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  uint16_t interval_min = params.advertising_interval_min().UncheckedRead();
  uint16_t interval_max = params.advertising_interval_max().UncheckedRead();

  // Core Spec Volume 4, Part E, Section 7.8.5: For high duty cycle directed
  // advertising, the Advertising_Interval_Min and Advertising_Interval_Max
  // parameters are not used and shall be ignored.
  if (params.adv_type().Read() != pw::bluetooth::emboss::LEAdvertisingType::
                                      CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED) {
    if (interval_min >= interval_max) {
      bt_log(INFO,
             "fake-hci",
             "advertising interval min (%d) not strictly less than max (%d)",
             interval_min,
             interval_max);
      RespondWithCommandComplete(
          hci_spec::kLESetAdvertisingParameters,
          pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }

    if (interval_min < hci_spec::kLEAdvertisingIntervalMin) {
      bt_log(INFO,
             "fake-hci",
             "advertising interval min (%d) less than spec min (%d)",
             interval_min,
             hci_spec::kLEAdvertisingIntervalMin);
      RespondWithCommandComplete(
          hci_spec::kLESetAdvertisingParameters,
          pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }

    if (interval_max > hci_spec::kLEAdvertisingIntervalMax) {
      bt_log(INFO,
             "fake-hci",
             "advertising interval max (%d) greater than spec max (%d)",
             interval_max,
             hci_spec::kLEAdvertisingIntervalMax);
      RespondWithCommandComplete(
          hci_spec::kLESetAdvertisingParameters,
          pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }
  }

  legacy_advertising_state_.interval_min = interval_min;
  legacy_advertising_state_.interval_max = interval_max;
  legacy_advertising_state_.adv_type =
      static_cast<pw::bluetooth::emboss::LEAdvertisingType>(
          params.adv_type().Read());
  legacy_advertising_state_.own_address_type = params.own_address_type().Read();

  bt_log(INFO,
         "fake-hci",
         "start advertising using address type: %hhd",
         static_cast<char>(legacy_advertising_state_.own_address_type));

  RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetRandomAddress(
    const pw::bluetooth::emboss::LESetRandomAddressCommandView& params) {
  if (legacy_advertising_state().enabled || le_scan_state().enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot set LE random address while scanning or advertising");
    RespondWithCommandComplete(
        hci_spec::kLESetRandomAddress,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  legacy_advertising_state_.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom,
                    DeviceAddressBytes(params.random_address()));
  RespondWithCommandComplete(hci_spec::kLESetRandomAddress,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnReadLocalSupportedFeatures() {
  hci_spec::ReadLocalSupportedFeaturesReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.lmp_features = htole64(settings_.lmp_features_page0);
  RespondWithCommandComplete(hci_spec::kReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalSupportedCommands() {
  hci_spec::ReadLocalSupportedCommandsReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  std::memcpy(params.supported_commands,
              settings_.supported_commands,
              sizeof(params.supported_commands));
  RespondWithCommandComplete(hci_spec::kReadLocalSupportedCommands,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalVersionInfo() {
  hci_spec::ReadLocalVersionInfoReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.hci_version = settings_.hci_version;
  RespondWithCommandComplete(hci_spec::kReadLocalVersionInfo,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadRemoteNameRequestCommandReceived(
    const pw::bluetooth::emboss::RemoteNameRequestCommandView& params) {
  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kRemoteNameRequest,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kRemoteNameRequest,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  struct RemoteNameRequestCompleteEventParams {
    pw::bluetooth::emboss::StatusCode status;
    DeviceAddressBytes bd_addr;
    uint8_t remote_name[hci_spec::kMaxNameLength];
  } __attribute__((packed));
  RemoteNameRequestCompleteEventParams response = {};
  response.bd_addr = DeviceAddressBytes(params.bd_addr());
  std::strncpy((char*)response.remote_name,
               peer->name().c_str(),
               hci_spec::kMaxNameLength);
  response.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  SendEvent(hci_spec::kRemoteNameRequestCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteSupportedFeaturesCommandReceived(
    const pw::bluetooth::emboss::ReadRemoteSupportedFeaturesCommandView&
        params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteSupportedFeatures,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::ReadRemoteSupportedFeaturesCompleteEventParams response = {};
  response.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  response.connection_handle = htole16(params.connection_handle().Read());
  response.lmp_features = settings_.lmp_features_page0;
  SendEvent(hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteVersionInfoCommandReceived(
    const pw::bluetooth::emboss::ReadRemoteVersionInfoCommandView& params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteVersionInfo,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::ReadRemoteVersionInfoCompleteEventWriter>(
      hci_spec::kReadRemoteVersionInfoCompleteEventCode);
  auto view = response.view_t();
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.connection_handle().CopyFrom(params.connection_handle());
  view.version().Write(pw::bluetooth::emboss::CoreSpecificationVersion::V4_2);
  view.company_identifier().Write(0xFFFF);  // anything
  view.subversion().Write(0xADDE);          // anything
  SendCommandChannelPacket(response.data());
}

void FakeController::OnReadRemoteExtendedFeaturesCommandReceived(
    const pw::bluetooth::emboss::ReadRemoteExtendedFeaturesCommandView&
        params) {
  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::ReadRemoteExtendedFeaturesCompleteEventWriter>(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode);
  auto view = response.view_t();

  switch (params.page_number().Read()) {
    case 1: {
      view.lmp_features().BackingStorage().WriteUInt(
          settings_.lmp_features_page1);
      break;
    }
    case 2: {
      view.lmp_features().BackingStorage().WriteUInt(
          settings_.lmp_features_page2);
      break;
    }
    default: {
      RespondWithCommandStatus(
          hci_spec::kReadRemoteExtendedFeatures,
          pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
    }
  }

  RespondWithCommandStatus(hci_spec::kReadRemoteExtendedFeatures,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.page_number().CopyFrom(params.page_number());
  view.max_page_number().Write(3);
  view.connection_handle().CopyFrom(params.connection_handle());
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SendCommandChannelPacket(response.data());
}

void FakeController::OnAuthenticationRequestedCommandReceived(
    const pw::bluetooth::emboss::AuthenticationRequestedCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kAuthenticationRequested,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kAuthenticationRequested,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  auto event = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LinkKeyRequestEventWriter>(
      hci_spec::kLinkKeyRequestEventCode);
  event.view_t().bd_addr().CopyFrom(peer->address_.value().view());
  SendCommandChannelPacket(event.data());
}

void FakeController::OnLinkKeyRequestReplyCommandReceived(
    const pw::bluetooth::emboss::LinkKeyRequestReplyCommandView& params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                             DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kLinkKeyRequestReply,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLinkKeyRequestReply,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kLinkKeyRequestReply,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    auto event = hci::EmbossEventPacket::New<
        pw::bluetooth::emboss::AuthenticationCompleteEventWriter>(
        hci_spec::kAuthenticationCompleteEventCode);
    event.view_t().status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
    event.view_t().connection_handle().Write(conn_handle);
    SendCommandChannelPacket(event.data());
  }
}

void FakeController::OnLinkKeyRequestNegativeReplyCommandReceived(
    const pw::bluetooth::emboss::LinkKeyRequestNegativeReplyCommandView&
        params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kLinkKeyRequestNegativeReply,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }
  RespondWithCommandStatus(hci_spec::kLinkKeyRequestNegativeReply,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  auto event = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::IoCapabilityRequestEventWriter>(
      hci_spec::kIOCapabilityRequestEventCode);
  event.view_t().bd_addr().CopyFrom(params.bd_addr());
  SendCommandChannelPacket(event.data());
}

void FakeController::OnIOCapabilityRequestReplyCommand(
    const pw::bluetooth::emboss::IoCapabilityRequestReplyCommandView& params) {
  RespondWithCommandStatus(hci_spec::kIOCapabilityRequestReply,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  auto io_response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::IoCapabilityResponseEventWriter>(
      hci_spec::kIOCapabilityResponseEventCode);
  io_response.view_t().bd_addr().CopyFrom(params.bd_addr());
  io_response.view_t().io_capability().Write(
      pw::bluetooth::emboss::IoCapability::NO_INPUT_NO_OUTPUT);
  io_response.view_t().oob_data_present().Write(
      pw::bluetooth::emboss::GenericPresenceParam::NOT_PRESENT);
  io_response.view_t().authentication_requirements().Write(
      pw::bluetooth::emboss::AuthenticationRequirements::GENERAL_BONDING);
  SendCommandChannelPacket(io_response.data());

  // Event type based on |params.io_capability| and |io_response.io_capability|.
  hci_spec::UserConfirmationRequestEventParams request = {};
  request.bd_addr = DeviceAddressBytes(params.bd_addr());
  request.numeric_value = 0;
  SendEvent(hci_spec::kUserConfirmationRequestEventCode,
            BufferView(&request, sizeof(request)));
}

void FakeController::OnUserConfirmationRequestReplyCommand(
    const pw::bluetooth::emboss::UserConfirmationRequestReplyCommandView&
        params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kUserConfirmationRequestReply,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestReply,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = DeviceAddressBytes(params.bd_addr());
  pairing_event.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));

  auto link_key_event = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LinkKeyNotificationEventWriter>(
      hci_spec::kLinkKeyNotificationEventCode);
  auto link_key_view = link_key_event.view_t();
  link_key_view.bd_addr().CopyFrom(params.bd_addr());
  uint8_t key[] = {0xc0,
                   0xde,
                   0xfa,
                   0x57,
                   0x4b,
                   0xad,
                   0xf0,
                   0x0d,
                   0xa7,
                   0x60,
                   0x06,
                   0x1e,
                   0xca,
                   0x1e,
                   0xca,
                   0xfe};
  link_key_view.link_key().value().BackingStorage().CopyFrom(
      ::emboss::support::ReadOnlyContiguousBuffer(key, sizeof(key)),
      sizeof(key));
  link_key_view.key_type().Write(
      pw::bluetooth::emboss::KeyType::UNAUTHENTICATED_COMBINATION_FROM_P192);
  SendCommandChannelPacket(link_key_event.data());

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    auto event = hci::EmbossEventPacket::New<
        pw::bluetooth::emboss::AuthenticationCompleteEventWriter>(
        hci_spec::kAuthenticationCompleteEventCode);
    event.view_t().status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
    event.view_t().connection_handle().Write(conn_handle);
    SendCommandChannelPacket(event.data());
  }
}

void FakeController::OnUserConfirmationRequestNegativeReplyCommand(
    const pw::bluetooth::emboss::
        UserConfirmationRequestNegativeReplyCommandView& params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kUserConfirmationRequestNegativeReply,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestNegativeReply,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kUserConfirmationRequestNegativeReply,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = DeviceAddressBytes(params.bd_addr());
  pairing_event.status =
      pw::bluetooth::emboss::StatusCode::AUTHENTICATION_FAILURE;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));
}

void FakeController::OnSetConnectionEncryptionCommand(
    const pw::bluetooth::emboss::SetConnectionEncryptionCommandView& params) {
  RespondWithCommandStatus(hci_spec::kSetConnectionEncryption,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  SendEncryptionChangeEvent(params.connection_handle().Read(),
                            pw::bluetooth::emboss::StatusCode::SUCCESS,
                            pw::bluetooth::emboss::EncryptionStatus::
                                ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE);
}

void FakeController::OnReadEncryptionKeySizeCommand(
    const pw::bluetooth::emboss::ReadEncryptionKeySizeCommandView& params) {
  hci_spec::ReadEncryptionKeySizeReturnParams response;
  response.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  response.connection_handle = params.connection_handle().Read();
  response.key_size = 16;
  RespondWithCommandComplete(hci_spec::kReadEncryptionKeySize,
                             BufferView(&response, sizeof(response)));
}

void FakeController::OnEnhancedAcceptSynchronousConnectionRequestCommand(
    const pw::bluetooth::emboss::
        EnhancedAcceptSynchronousConnectionRequestCommandView& params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                             DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer || !peer->last_connection_request_link_type().has_value()) {
    RespondWithCommandStatus(
        hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(
      hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
      pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::SynchronousConnectionCompleteEventWriter>(
      hci_spec::kSynchronousConnectionCompleteEventCode);
  auto view = packet.view_t();
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.connection_handle().Write(sco_handle);
  view.bd_addr().CopyFrom(peer->address().value().view());
  view.link_type().Write(peer->last_connection_request_link_type().value());
  view.transmission_interval().Write(1);
  view.retransmission_window().Write(2);
  view.rx_packet_length().Write(3);
  view.tx_packet_length().Write(4);
  view.air_mode().Write(params.connection_parameters()
                            .transmit_coding_format()
                            .coding_format()
                            .Read());
  SendCommandChannelPacket(packet.data());
}

void FakeController::OnEnhancedSetupSynchronousConnectionCommand(
    const pw::bluetooth::emboss::EnhancedSetupSynchronousConnectionCommandView&
        params) {
  const hci_spec::ConnectionHandle acl_handle =
      params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(acl_handle);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kEnhancedSetupSynchronousConnection,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kEnhancedSetupSynchronousConnection,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::SynchronousConnectionCompleteEventWriter>(
      hci_spec::kSynchronousConnectionCompleteEventCode);
  auto view = packet.view_t();
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.connection_handle().Write(sco_handle);
  view.bd_addr().CopyFrom(peer->address().value().view());
  view.link_type().Write(pw::bluetooth::emboss::LinkType::ESCO);
  view.transmission_interval().Write(1);
  view.retransmission_window().Write(2);
  view.rx_packet_length().Write(3);
  view.tx_packet_length().Write(4);
  view.air_mode().Write(params.connection_parameters()
                            .transmit_coding_format()
                            .coding_format()
                            .Read());
  SendCommandChannelPacket(packet.data());
}

void FakeController::OnLEReadRemoteFeaturesCommand(
    const hci_spec::LEReadRemoteFeaturesCommandParams& params) {
  if (le_read_remote_features_cb_) {
    le_read_remote_features_cb_();
  }

  const hci_spec::ConnectionHandle handle = le16toh(params.connection_handle);
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(
        hci_spec::kLEReadRemoteFeatures,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEReadRemoteFeatures,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);

  auto response = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LEReadRemoteFeaturesCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = response.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode);
  view.connection_handle().Write(params.connection_handle);
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.le_features().BackingStorage().WriteUInt(
      peer->le_features().le_features);
  SendCommandChannelPacket(response.data());
}

void FakeController::OnLEStartEncryptionCommand(
    const pw::bluetooth::emboss::LEEnableEncryptionCommandView& params) {
  RespondWithCommandStatus(hci_spec::kLEStartEncryption,
                           pw::bluetooth::emboss::StatusCode::SUCCESS);
  SendEncryptionChangeEvent(params.connection_handle().Read(),
                            pw::bluetooth::emboss::StatusCode::SUCCESS,
                            pw::bluetooth::emboss::EncryptionStatus::
                                ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE);
}

void FakeController::OnWriteSynchronousFlowControlEnableCommand(
    const pw::bluetooth::emboss::WriteSynchronousFlowControlEnableCommandView&
        params) {
  constexpr size_t flow_control_enable_octet = 10;
  bool supported =
      settings_.supported_commands[flow_control_enable_octet] &
      static_cast<uint8_t>(
          hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  if (!supported) {
    RespondWithCommandComplete(
        hci_spec::kWriteSynchronousFlowControlEnable,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
    return;
  }
  RespondWithCommandComplete(hci_spec::kWriteSynchronousFlowControlEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnLESetAdvertisingSetRandomAddress(
    const pw::bluetooth::emboss::LESetAdvertisingSetRandomAddressCommandView&
        params) {
  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingSetRandomAddress,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "unknown advertising handle (%d), "
           "use HCI_LE_Set_Extended_Advertising_Parameters to create one first",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingSetRandomAddress,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.IsConnectableAdvertising() && state.enabled) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot set LE random address while connectable advertising enabled");
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingSetRandomAddress,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  state.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom,
                    DeviceAddressBytes(params.random_address()));
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
}

void FakeController::OnLESetExtendedAdvertisingParameters(
    const pw::bluetooth::emboss::
        LESetExtendedAdvertisingParametersV1CommandView& params) {
  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // ensure we can allocate memory for this advertising set if not already
  // present
  if (extended_advertising_states_.count(handle) == 0 &&
      extended_advertising_states_.size() >= num_supported_advertising_sets()) {
    bt_log(INFO,
           "fake-hci",
           "no available memory for new advertising set, handle: %d",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::MEMORY_CAPACITY_EXCEEDED);
    return;
  }

  // for backwards compatibility, we only support legacy pdus
  if (!params.advertising_event_properties().use_legacy_pdus().Read()) {
    bt_log(
        INFO,
        "fake-hci",
        "only legacy PDUs are supported, extended PDUs are not supported yet");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // ensure we have a valid bit combination in the advertising event properties
  constexpr uint16_t legacy_pdu = hci_spec::kLEAdvEventPropBitUseLegacyPDUs;
  constexpr uint16_t prop_bits_adv_ind =
      legacy_pdu | hci_spec::kLEAdvEventPropBitConnectable |
      hci_spec::kLEAdvEventPropBitScannable;
  constexpr uint16_t prop_bits_adv_direct_ind_low_duty_cycle =
      legacy_pdu | hci_spec::kLEAdvEventPropBitConnectable |
      hci_spec::kLEAdvEventPropBitDirected;
  constexpr uint16_t prop_bits_adv_direct_ind_high_duty_cycle =
      prop_bits_adv_direct_ind_low_duty_cycle |
      hci_spec::kLEAdvEventPropBitHighDutyCycleDirectedConnectable;
  constexpr uint16_t prop_bits_adv_scan_ind =
      legacy_pdu | hci_spec::kLEAdvEventPropBitScannable;
  constexpr uint16_t prop_bits_adv_nonconn_ind = legacy_pdu;

  pw::bluetooth::emboss::LEAdvertisingType adv_type;
  uint16_t advertising_event_properties =
      params.advertising_event_properties().BackingStorage().ReadUInt();
  switch (advertising_event_properties) {
    case prop_bits_adv_ind:
      adv_type = pw::bluetooth::emboss::LEAdvertisingType::
          CONNECTABLE_AND_SCANNABLE_UNDIRECTED;
      break;
    case prop_bits_adv_direct_ind_high_duty_cycle:
      adv_type = pw::bluetooth::emboss::LEAdvertisingType::
          CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED;
      break;
    case prop_bits_adv_direct_ind_low_duty_cycle:
      adv_type = pw::bluetooth::emboss::LEAdvertisingType::
          CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED;
      break;
    case prop_bits_adv_scan_ind:
      adv_type = pw::bluetooth::emboss::LEAdvertisingType::SCANNABLE_UNDIRECTED;
      break;
    case prop_bits_adv_nonconn_ind:
      adv_type =
          pw::bluetooth::emboss::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED;
      break;
    default:
      bt_log(INFO,
             "fake-hci",
             "invalid bit combination: %d",
             advertising_event_properties);
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingParameters,
          pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
  }

  // In case there is an error below, we want to reject all parameters instead
  // of storing a dead state and taking up an advertising handle. Avoid creating
  // the LEAdvertisingState directly in the map and add it in only once we have
  // made sure all is good.
  LEAdvertisingState state;
  if (extended_advertising_states_.count(handle) != 0) {
    state = extended_advertising_states_[handle];
  }

  uint32_t interval_min = params.primary_advertising_interval_min().Read();
  uint32_t interval_max = params.primary_advertising_interval_max().Read();

  if (interval_min >= interval_max) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) not strictly less than max (%d)",
           interval_min,
           interval_max);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (interval_min < hci_spec::kLEExtendedAdvertisingIntervalMin) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) less than spec min (%d)",
           interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (interval_max > hci_spec::kLEExtendedAdvertisingIntervalMax) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval max (%d) greater than spec max (%d)",
           interval_max,
           hci_spec::kLEAdvertisingIntervalMax);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  uint8_t advertising_channels =
      params.primary_advertising_channel_map().BackingStorage().ReadUInt();
  if (!advertising_channels) {
    bt_log(INFO,
           "fake-hci",
           "at least one bit must be set in primary advertising channel map");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  int8_t advertising_tx_power = params.advertising_tx_power().Read();
  if (advertising_tx_power !=
          hci_spec::kLEExtendedAdvertisingTxPowerNoPreference &&
      (advertising_tx_power < hci_spec::kLEAdvertisingTxPowerMin ||
       advertising_tx_power > hci_spec::kLEAdvertisingTxPowerMax)) {
    bt_log(INFO,
           "fake-hci",
           "advertising tx power out of range: %d",
           advertising_tx_power);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // TODO(fxbug.dev/80049): Core spec Volume 4, Part E, Section 7.8.53: if
  // legacy advertising PDUs are being used, the Primary_Advertising_PHY shall
  // indicate the LE 1M PHY.
  if (params.primary_advertising_phy().Read() !=
      pw::bluetooth::emboss::LEPrimaryAdvertisingPHY::LE_1M) {
    bt_log(INFO,
           "fake-hci",
           "only legacy pdus are supported, requires advertising on 1M PHY");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (params.secondary_advertising_phy().Read() !=
      pw::bluetooth::emboss::LESecondaryAdvertisingPHY::LE_1M) {
    bt_log(INFO, "fake-hci", "secondary advertising PHY must be selected");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (state.enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot set parameters while advertising set is enabled");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  // all errors checked, set parameters that we care about
  state.adv_type = adv_type;
  state.own_address_type = params.own_address_type().Read();
  state.interval_min = interval_min;
  state.interval_max = interval_max;

  // write full state back only at the end (we don't have a reference because we
  // only want to write if there are no errors)
  extended_advertising_states_[handle] = state;

  hci_spec::LESetExtendedAdvertisingParametersReturnParams return_params;
  return_params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  return_params.selected_tx_power = hci_spec::kLEAdvertisingTxPowerMax;
  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                             BufferView(&return_params, sizeof(return_params)));
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedAdvertisingData(
    const pw::bluetooth::emboss::LESetExtendedAdvertisingDataCommandView&
        params) {
  // controller currently doesn't support fragmented advertising, assert so we
  // fail if we ever use it in host code without updating the controller for
  // tests
  BT_ASSERT(params.operation().Read() ==
            pw::bluetooth::emboss::LESetExtendedAdvDataOp::COMPLETE);
  BT_ASSERT(params.fragment_preference().Read() ==
            pw::bluetooth::emboss::LEExtendedAdvFragmentPreference::
                SHOULD_NOT_FRAGMENT);

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing advertising data entirely doesn't require us to check for error
  // conditions
  size_t advertising_data_length = params.advertising_data_length().Read();
  if (advertising_data_length == 0) {
    state.data_length = 0;
    std::memset(state.data, 0, sizeof(state.data));
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // directed advertising doesn't support advertising data
  if (state.IsDirectedAdvertising()) {
    bt_log(INFO,
           "fake-hci",
           "cannot provide advertising data when using directed advertising");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // For backwards compatibility with older devices, the host currently uses
  // legacy advertising PDUs. The advertising data cannot exceed the legacy
  // advertising PDU limit.
  if (advertising_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than legacy PDU size limit",
           advertising_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  state.data_length = advertising_data_length;
  std::memcpy(state.data,
              params.advertising_data().BackingStorage().data(),
              advertising_data_length);
  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedScanResponseData(
    const pw::bluetooth::emboss::LESetExtendedScanResponseDataCommandView&
        params) {
  // controller currently doesn't support fragmented advertising, assert so we
  // fail if we ever use it in host code without updating the controller for
  // tests
  BT_ASSERT(params.operation().Read() ==
            pw::bluetooth::emboss::LESetExtendedAdvDataOp::COMPLETE);
  BT_ASSERT(params.fragment_preference().Read() ==
            pw::bluetooth::emboss::LEExtendedAdvFragmentPreference::
                SHOULD_NOT_FRAGMENT);

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error
  // conditions
  if (params.scan_response_data_length().Read() == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.IsScannableAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot provide scan response data for unscannable advertising types");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // For backwards compatibility with older devices, the host currently uses
  // legacy advertising PDUs. The scan response data cannot exceed the legacy
  // advertising PDU limit.
  if (params.scan_response_data_length().Read() >
      hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%d bytes) larger than legacy PDU size limit",
           params.scan_response_data_length().Read());
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  state.scan_rsp_length = params.scan_response_data_length().Read();
  std::memcpy(state.scan_rsp_data,
              params.scan_response_data().BackingStorage().data(),
              params.scan_response_data_length().Read());

  RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedAdvertisingEnable(
    const pw::bluetooth::emboss::LESetExtendedAdvertisingEnableCommandView&
        params) {
  uint8_t num_sets = params.num_sets().Read();

  // do some preliminary checks before making any state changes
  if (num_sets != 0) {
    std::unordered_set<hci_spec::AdvertisingHandle> handles;

    for (uint8_t i = 0; i < num_sets; i++) {
      hci_spec::AdvertisingHandle handle =
          params.data()[i].advertising_handle().Read();

      if (!IsValidAdvertisingHandle(handle)) {
        bt_log(
            ERROR, "fake-hci", "advertising handle outside range: %d", handle);
        RespondWithCommandComplete(
            hci_spec::kLESetExtendedAdvertisingEnable,
            pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
        return;
      }

      // cannot have two array entries for the same advertising handle
      if (handles.count(handle) != 0) {
        bt_log(INFO,
               "fake-hci",
               "cannot refer to handle more than once (handle: %d)",
               handle);
        RespondWithCommandComplete(
            hci_spec::kLESetExtendedAdvertisingEnable,
            pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
        return;
      }
      handles.insert(handle);

      // cannot have instructions for an advertising handle we don't know about
      if (extended_advertising_states_.count(handle) == 0) {
        bt_log(INFO,
               "fake-hci",
               "cannot enable/disable an unknown handle (handle: %d)",
               handle);
        RespondWithCommandComplete(
            hci_spec::kLESetExtendedAdvertisingEnable,
            pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
        return;
      }
    }
  }

  if (params.enable().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::DISABLE) {
    if (num_sets == 0) {
      // if params.enable == kDisable and params.num_sets == 0, spec asks we
      // disable all
      for (auto& element : extended_advertising_states_) {
        element.second.enabled = false;
      }
    } else {
      for (int i = 0; i < num_sets; i++) {
        hci_spec::AdvertisingHandle handle =
            params.data()[i].advertising_handle().Read();
        extended_advertising_states_[handle].enabled = false;
      }
    }

    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // rest of the function deals with enabling advertising for a given set of
  // advertising sets
  BT_ASSERT(params.enable().Read() ==
            pw::bluetooth::emboss::GenericEnableParam::ENABLE);

  if (num_sets == 0) {
    bt_log(
        INFO, "fake-hci", "cannot enable with an empty advertising set list");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingEnable,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  for (uint8_t i = 0; i < num_sets; i++) {
    // FakeController currently doesn't support testing with duration and max
    // events. When those are used in the host, these checks will fail and
    // remind us to add the necessary code to FakeController.
    BT_ASSERT(params.data()[i].duration().Read() == 0);
    BT_ASSERT(params.data()[i].max_extended_advertising_events().Read() == 0);

    hci_spec::AdvertisingHandle handle =
        params.data()[i].advertising_handle().Read();
    LEAdvertisingState& state = extended_advertising_states_[handle];

    if (state.IsDirectedAdvertising() && state.data_length == 0) {
      bt_log(
          INFO,
          "fake-hci",
          "cannot enable type requiring advertising data without setting it");
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingEnable,
          pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    if (state.IsScannableAdvertising() && state.scan_rsp_length == 0) {
      bt_log(INFO,
             "fake-hci",
             "cannot enable, requires scan response data but hasn't been set");
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingEnable,
          pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    // TODO(fxbug.dev/81444): if own address type is random, check that a random
    // address is set
    state.enabled = true;
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadMaximumAdvertisingDataLength() {
  hci_spec::LEReadMaxAdvertisingDataLengthReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;

  // TODO(fxbug.dev/77476): Extended advertising supports sending larger amounts
  // of data, but they have to be fragmented across multiple commands to the
  // controller. This is not yet supported in this implementation. We should
  // support larger than kMaxPduLEExtendedAdvertisingDataLength advertising data
  // with fragmentation.
  params.max_adv_data_length = htole16(hci_spec::kMaxLEAdvertisingDataLength);
  RespondWithCommandComplete(hci_spec::kLEReadMaxAdvertisingDataLength,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadNumberOfSupportedAdvertisingSets() {
  hci_spec::LEReadNumSupportedAdvertisingSetsReturnParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.num_supported_adv_sets = htole16(num_supported_advertising_sets_);
  RespondWithCommandComplete(hci_spec::kLEReadNumSupportedAdvertisingSets,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLERemoveAdvertisingSet(
    const hci_spec::LERemoveAdvertisingSetCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLERemoveAdvertisingSet,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLERemoveAdvertisingSet,
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  if (extended_advertising_states_[handle].enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot remove enabled advertising set (handle: %d)",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLERemoveAdvertisingSet,
        pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  extended_advertising_states_.erase(handle);
  RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEClearAdvertisingSets() {
  for (const auto& element : extended_advertising_states_) {
    if (element.second.enabled) {
      bt_log(INFO,
             "fake-hci",
             "cannot remove currently enabled advertising set (handle: %d)",
             element.second.enabled);
      RespondWithCommandComplete(
          hci_spec::kLEClearAdvertisingSets,
          pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
      return;
    }
  }

  extended_advertising_states_.clear();
  RespondWithCommandComplete(hci_spec::kLEClearAdvertisingSets,
                             pw::bluetooth::emboss::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadAdvertisingChannelTxPower() {
  if (!respond_to_tx_power_read_) {
    return;
  }

  hci_spec::LEReadAdvertisingChannelTxPowerReturnParams params;
  // Send back arbitrary tx power.
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.tx_power = 9;
  RespondWithCommandComplete(hci_spec::kLEReadAdvertisingChannelTxPower,
                             BufferView(&params, sizeof(params)));
}

void FakeController::SendLEAdvertisingSetTerminatedEvent(
    hci_spec::ConnectionHandle conn_handle,
    hci_spec::AdvertisingHandle adv_handle) {
  hci_spec::LEAdvertisingSetTerminatedSubeventParams params;
  params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  params.connection_handle = conn_handle;
  params.adv_handle = adv_handle;
  SendLEMetaEvent(hci_spec::kLEAdvertisingSetTerminatedSubeventCode,
                  BufferView(&params, sizeof(params)));
}

void FakeController::SendAndroidLEMultipleAdvertisingStateChangeSubevent(
    hci_spec::ConnectionHandle conn_handle,
    hci_spec::AdvertisingHandle adv_handle) {
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::vendor::android_hci::LEMultiAdvtStateChangeSubeventWriter>(
      hci_spec::kVendorDebugEventCode);
  auto view = packet.view_t();
  view.vendor_event().subevent_code().Write(
      hci_android::kLEMultiAdvtStateChangeSubeventCode);
  view.advertising_handle().Write(adv_handle);
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.connection_handle().Write(conn_handle);
  SendCommandChannelPacket(packet.data());
}

void FakeController::OnCommandPacketReceived(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  hci_spec::OpCode opcode = le16toh(command_packet.header().opcode);

  bt_log(
      TRACE, "fake-hci", "received command packet with opcode: %#.4x", opcode);
  // We handle commands immediately unless a client has explicitly set a
  // listener for `opcode`.
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
  RespondWithCommandComplete(hci_android::kLEGetVendorCapabilities,
                             settings_.android_extension_settings.data());
}

void FakeController::OnAndroidStartA2dpOffload(
    const pw::bluetooth::vendor::android_hci::StartA2dpOffloadCommandView&
        params) {
  hci_android::StartA2dpOffloadCommandReturnParams ret;
  ret.opcode = hci_android::kStartA2dpOffloadCommandSubopcode;

  // return in case A2DP offload already started
  if (offloaded_a2dp_channel_state_) {
    ret.status = pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  // SCMS-T is not currently supported
  hci_android::A2dpScmsTEnable scms_t_enable;
  scms_t_enable.enabled = params.scms_t_enable().enabled().Read();
  scms_t_enable.header = params.scms_t_enable().header().Read();
  if (scms_t_enable.enabled ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  // return in case any parameter has an invalid value
  ret.status =
      pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;

  hci_android::A2dpCodecType const codec_type =
      static_cast<hci_android::A2dpCodecType>(
          le32toh(static_cast<uint32_t>(params.codec_type().Read())));
  switch (codec_type) {
    case hci_android::A2dpCodecType::kSbc:
    case hci_android::A2dpCodecType::kAac:
    case hci_android::A2dpCodecType::kAptx:
    case hci_android::A2dpCodecType::kAptxhd:
    case hci_android::A2dpCodecType::kLdac:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                                 BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpSamplingFrequency const sampling_frequency =
      static_cast<hci_android::A2dpSamplingFrequency>(
          le32toh(static_cast<uint32_t>(params.sampling_frequency().Read())));
  switch (sampling_frequency) {
    case hci_android::A2dpSamplingFrequency::k44100Hz:
    case hci_android::A2dpSamplingFrequency::k48000Hz:
    case hci_android::A2dpSamplingFrequency::k88200Hz:
    case hci_android::A2dpSamplingFrequency::k96000Hz:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                                 BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpBitsPerSample const bits_per_sample =
      static_cast<hci_android::A2dpBitsPerSample>(
          params.bits_per_sample().Read());
  switch (bits_per_sample) {
    case hci_android::A2dpBitsPerSample::k16BitsPerSample:
    case hci_android::A2dpBitsPerSample::k24BitsPerSample:
    case hci_android::A2dpBitsPerSample::k32BitsPerSample:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                                 BufferView(&ret, sizeof(ret)));
      return;
  }

  hci_android::A2dpChannelMode const channel_mode =
      static_cast<hci_android::A2dpChannelMode>(params.channel_mode().Read());
  switch (channel_mode) {
    case hci_android::A2dpChannelMode::kMono:
    case hci_android::A2dpChannelMode::kStereo:
      break;
    default:
      RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                                 BufferView(&ret, sizeof(ret)));
      return;
  }

  uint32_t const encoded_audio_bitrate =
      le32toh(params.encoded_audio_bitrate().Read());
  // Bits 0x01000000 to 0xFFFFFFFF are reserved
  if (encoded_audio_bitrate >= 0x01000000) {
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  OffloadedA2dpChannel state;
  state.codec_type = codec_type;
  state.max_latency = le16toh(params.max_latency().Read());
  state.scms_t_enable = scms_t_enable;
  state.sampling_frequency = sampling_frequency;
  state.bits_per_sample = bits_per_sample;
  state.channel_mode = channel_mode;
  state.encoded_audio_bitrate = encoded_audio_bitrate;
  state.connection_handle = le16toh(params.connection_handle().Read());
  state.l2cap_channel_id = le16toh(params.l2cap_channel_id().Read());
  state.l2cap_mtu_size = le16toh(params.l2cap_mtu_size().Read());
  offloaded_a2dp_channel_state_ = state;

  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                             BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidStopA2dpOffload() {
  hci_android::StartA2dpOffloadCommandReturnParams ret;
  ret.opcode = hci_android::kStopA2dpOffloadCommandSubopcode;

  if (!offloaded_a2dp_channel_state_) {
    ret.status = pw::bluetooth::emboss::StatusCode::REPEATED_ATTEMPTS;
    RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  offloaded_a2dp_channel_state_ = std::nullopt;

  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  RespondWithCommandComplete(hci_android::kA2dpOffloadCommand,
                             BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidA2dpOffloadCommand(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case hci_android::kStartA2dpOffloadCommandSubopcode: {
      auto view =
          pw::bluetooth::vendor::android_hci::MakeStartA2dpOffloadCommandView(
              command_packet.data().data(),
              pw::bluetooth::vendor::android_hci::StartA2dpOffloadCommand::
                  MaxSizeInBytes());
      OnAndroidStartA2dpOffload(view);
      break;
    }
    case hci_android::kStopA2dpOffloadCommandSubopcode:
      OnAndroidStopA2dpOffload();
      break;
    default:
      bt_log(WARN,
             "fake-hci",
             "unhandled android A2DP offload command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(
          subopcode, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
      break;
  }
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtParam(
    const hci_android::LEMultiAdvtSetAdvtParamCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  // ensure we can allocate memory for this advertising set if not already
  // present
  if (extended_advertising_states_.count(handle) == 0 &&
      extended_advertising_states_.size() >= num_supported_advertising_sets()) {
    bt_log(INFO,
           "fake-hci",
           "no available memory for new advertising set, handle: %d",
           handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = pw::bluetooth::emboss::StatusCode::MEMORY_CAPACITY_EXCEEDED;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  // In case there is an error below, we want to reject all parameters instead
  // of storing a dead state and taking up an advertising handle. Avoid creating
  // the LEAdvertisingState directly in the map and add it in only once we have
  // made sure all is good.
  LEAdvertisingState state;
  if (extended_advertising_states_.count(handle) != 0) {
    state = extended_advertising_states_[handle];
  }

  uint16_t interval_min = le16toh(params.adv_interval_min);
  uint16_t interval_max = le16toh(params.adv_interval_max);

  if (interval_min >= interval_max) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) not strictly less than max (%d)",
           interval_min,
           interval_max);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (interval_min < hci_spec::kLEAdvertisingIntervalMin) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) less than spec min (%d)",
           interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (interval_max > hci_spec::kLEAdvertisingIntervalMax) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval max (%d) greater than spec max (%d)",
           interval_max,
           hci_spec::kLEAdvertisingIntervalMax);
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  state.interval_min = interval_min;
  state.interval_max = interval_max;
  state.adv_type = params.adv_type;
  state.own_address_type = params.own_address_type;

  // write full state back only at the end (we don't have a reference because we
  // only want to write if there are no errors)
  extended_advertising_states_[handle] = state;

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                             BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtData(
    const hci_android::LEMultiAdvtSetAdvtDataCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing advertising data entirely doesn't require us to check for error
  // conditions
  if (params.adv_data_length == 0) {
    state.data_length = 0;
    std::memset(state.data, 0, sizeof(state.data));
    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    NotifyAdvertisingState();
    return;
  }

  // directed advertising doesn't support advertising data
  if (state.IsDirectedAdvertising()) {
    bt_log(INFO,
           "fake-hci",
           "cannot provide advertising data when using directed advertising");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (params.adv_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%d bytes) larger than legacy PDU size limit",
           params.adv_data_length);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  state.data_length = params.adv_data_length;
  std::memcpy(state.data, params.adv_data, params.adv_data_length);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                             BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetScanResp(
    const hci_android::LEMultiAdvtSetScanRespCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error
  // conditions
  if (params.scan_rsp_data_length == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.IsScannableAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot provide scan response data for unscannable advertising types");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (params.scan_rsp_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%d bytes) larger than legacy PDU size limit",
           params.scan_rsp_data_length);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  state.scan_rsp_length = params.scan_rsp_data_length;
  std::memcpy(
      state.scan_rsp_data, params.scan_rsp_data, params.scan_rsp_data_length);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                             BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetRandomAddr(
    const hci_android::LEMultiAdvtSetRandomAddrCommandParams& params) {
  hci_spec::AdvertisingHandle handle = params.adv_handle;

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.IsConnectableAdvertising() && state.enabled) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot set LE random address while connectable advertising enabled");

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status = pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED;
    ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  state.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom, params.random_address);

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtSetRandomAddrSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                             BufferView(&ret, sizeof(ret)));
}

void FakeController::OnAndroidLEMultiAdvtEnable(
    const pw::bluetooth::vendor::android_hci::LEMultiAdvtEnableCommandView&
        params) {
  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
    ret.status =
        pw::bluetooth::emboss::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER;
    ret.opcode = hci_android::kLEMultiAdvtEnableSubopcode;
    RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                               BufferView(&ret, sizeof(ret)));
    return;
  }

  bool enabled = false;
  if (params.enable().Read() ==
      pw::bluetooth::emboss::GenericEnableParam::ENABLE) {
    enabled = true;
  }

  extended_advertising_states_[handle].enabled = enabled;

  hci_android::LEMultiAdvtSetAdvtParamReturnParams ret;
  ret.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
  ret.opcode = hci_android::kLEMultiAdvtEnableSubopcode;
  RespondWithCommandComplete(hci_android::kLEMultiAdvt,
                             BufferView(&ret, sizeof(ret)));
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvt(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case hci_android::kLEMultiAdvtSetAdvtParamSubopcode: {
      auto params =
          payload.To<hci_android::LEMultiAdvtSetAdvtParamCommandParams>();
      OnAndroidLEMultiAdvtSetAdvtParam(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetAdvtDataSubopcode: {
      auto params =
          payload.To<hci_android::LEMultiAdvtSetAdvtDataCommandParams>();
      OnAndroidLEMultiAdvtSetAdvtData(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetScanRespSubopcode: {
      auto params =
          payload.To<hci_android::LEMultiAdvtSetScanRespCommandParams>();
      OnAndroidLEMultiAdvtSetScanResp(params);
      break;
    }
    case hci_android::kLEMultiAdvtSetRandomAddrSubopcode: {
      auto params =
          payload.To<hci_android::LEMultiAdvtSetRandomAddrCommandParams>();
      OnAndroidLEMultiAdvtSetRandomAddr(params);
      break;
    }
    case hci_android::kLEMultiAdvtEnableSubopcode: {
      auto view =
          pw::bluetooth::vendor::android_hci::MakeLEMultiAdvtEnableCommandView(
              command_packet.data().data(),
              pw::bluetooth::vendor::android_hci::LEMultiAdvtEnableCommand::
                  MaxSizeInBytes());
      OnAndroidLEMultiAdvtEnable(view);
      break;
    }
    default: {
      bt_log(WARN,
             "fake-hci",
             "unhandled android multiple advertising command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(
          subopcode, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::OnVendorCommand(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
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
      bt_log(WARN,
             "fake-hci",
             "received unhandled vendor command with opcode: %#.4x",
             opcode);
      RespondWithCommandComplete(
          opcode, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
      break;
  }
}

void FakeController::OnACLDataPacketReceived(
    const ByteBuffer& acl_data_packet) {
  if (acl_data_callback_) {
    BT_DEBUG_ASSERT(data_dispatcher_);
    DynamicByteBuffer packet_copy(acl_data_packet);
    (void)data_dispatcher_->Post(
        [packet_copy = std::move(packet_copy), cb = acl_data_callback_.share()](
            pw::async::Context /*ctx*/, pw::Status status) mutable {
          if (status.ok()) {
            cb(packet_copy);
          }
        });
  }

  if (acl_data_packet.size() < sizeof(hci_spec::ACLDataHeader)) {
    bt_log(WARN, "fake-hci", "malformed ACL packet!");
    return;
  }

  const auto& header = acl_data_packet.To<hci_spec::ACLDataHeader>();
  hci_spec::ConnectionHandle handle =
      le16toh(header.handle_and_flags) & 0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "ACL data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
  peer->OnRxL2CAP(handle,
                  acl_data_packet.view(sizeof(hci_spec::ACLDataHeader)));
}

void FakeController::OnScoDataPacketReceived(
    const ByteBuffer& sco_data_packet) {
  if (sco_data_callback_) {
    sco_data_callback_(sco_data_packet);
  }

  if (sco_data_packet.size() < sizeof(hci_spec::SynchronousDataHeader)) {
    bt_log(WARN, "fake-hci", "malformed SCO packet!");
    return;
  }

  const auto& header = sco_data_packet.To<hci_spec::SynchronousDataHeader>();
  hci_spec::ConnectionHandle handle =
      le16toh(header.handle_and_flags) & 0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "SCO data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
}

void FakeController::SetDataCallback(DataCallback callback,
                                     pw::async::Dispatcher& pw_dispatcher) {
  BT_DEBUG_ASSERT(callback);
  BT_DEBUG_ASSERT(!acl_data_callback_);
  BT_DEBUG_ASSERT(!data_dispatcher_);

  acl_data_callback_ = std::move(callback);
  data_dispatcher_.emplace(pw_dispatcher);
}

void FakeController::ClearDataCallback() {
  // Leave dispatcher set (if already set) to preserve its write-once-ness (this
  // catches bugs with setting multiple data callbacks in class hierarchies).
  acl_data_callback_ = nullptr;
}

bool FakeController::LEAdvertisingState::IsDirectedAdvertising() const {
  return adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED ||
         adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED;
}

bool FakeController::LEAdvertisingState::IsScannableAdvertising() const {
  return adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_AND_SCANNABLE_UNDIRECTED ||
         adv_type ==
             pw::bluetooth::emboss::LEAdvertisingType::SCANNABLE_UNDIRECTED;
}

bool FakeController::LEAdvertisingState::IsConnectableAdvertising() const {
  return adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_AND_SCANNABLE_UNDIRECTED ||
         adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED ||
         adv_type == pw::bluetooth::emboss::LEAdvertisingType::
                         CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED;
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

  // TODO(fxbug.dev/937): Validate size of payload to be the correct length
  // below.
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
    case hci_spec::kLERemoveAdvertisingSet: {
      const auto& params =
          command_packet
              .payload<hci_spec::LERemoveAdvertisingSetCommandParams>();
      OnLERemoveAdvertisingSet(params);
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
    case hci_spec::kReadPageScanActivity: {
      OnReadPageScanActivity();
      break;
    }
    case hci_spec::kReadInquiryMode: {
      OnReadInquiryMode();
      break;
    }
    case hci_spec::kReadPageScanType: {
      OnReadPageScanType();
      break;
    }
    case hci_spec::kReadSimplePairingMode: {
      OnReadSimplePairingMode();
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
    case hci_spec::kLEReadBufferSizeV1: {
      OnLEReadBufferSizeV1();
      break;
    }
    case hci_spec::kReset: {
      OnReset();
      break;
    }
    case hci_spec::kLinkKeyRequestReply: {
      const auto& params =
          command_packet
              .payload<pw::bluetooth::emboss::LinkKeyRequestReplyCommandView>();
      OnLinkKeyRequestReplyCommandReceived(params);
      break;
    }
    case hci_spec::kLEReadRemoteFeatures: {
      const auto& params =
          command_packet.payload<hci_spec::LEReadRemoteFeaturesCommandParams>();
      OnLEReadRemoteFeaturesCommand(params);
      break;
    }
    case hci_spec::kLEReadAdvertisingChannelTxPower: {
      OnLEReadAdvertisingChannelTxPower();
      break;
    }
    case hci_spec::kAuthenticationRequested:
    case hci_spec::kCreateConnection:
    case hci_spec::kDisconnect:
    case hci_spec::kEnhancedAcceptSynchronousConnectionRequest:
    case hci_spec::kEnhancedSetupSynchronousConnection:
    case hci_spec::kIOCapabilityRequestReply:
    case hci_spec::kInquiry:
    case hci_spec::kLEClearAdvertisingSets:
    case hci_spec::kLEConnectionUpdate:
    case hci_spec::kLECreateConnection:
    case hci_spec::kLEReadMaxAdvertisingDataLength:
    case hci_spec::kLEReadNumSupportedAdvertisingSets:
    case hci_spec::kLESetAdvertisingData:
    case hci_spec::kLESetAdvertisingEnable:
    case hci_spec::kLESetAdvertisingParameters:
    case hci_spec::kLESetAdvertisingSetRandomAddress:
    case hci_spec::kLESetEventMask:
    case hci_spec::kLESetExtendedAdvertisingData:
    case hci_spec::kLESetExtendedAdvertisingEnable:
    case hci_spec::kLESetExtendedAdvertisingParameters:
    case hci_spec::kLESetExtendedScanEnable:
    case hci_spec::kLESetExtendedScanParameters:
    case hci_spec::kLESetExtendedScanResponseData:
    case hci_spec::kLESetRandomAddress:
    case hci_spec::kLESetScanEnable:
    case hci_spec::kLESetScanParameters:
    case hci_spec::kLESetScanResponseData:
    case hci_spec::kLEStartEncryption:
    case hci_spec::kLinkKeyRequestNegativeReply:
    case hci_spec::kReadEncryptionKeySize:
    case hci_spec::kReadLocalExtendedFeatures:
    case hci_spec::kReadRemoteExtendedFeatures:
    case hci_spec::kReadRemoteSupportedFeatures:
    case hci_spec::kReadRemoteVersionInfo:
    case hci_spec::kRemoteNameRequest:
    case hci_spec::kSetConnectionEncryption:
    case hci_spec::kSetEventMask:
    case hci_spec::kUserConfirmationRequestNegativeReply:
    case hci_spec::kUserConfirmationRequestReply:
    case hci_spec::kWriteClassOfDevice:
    case hci_spec::kWriteExtendedInquiryResponse:
    case hci_spec::kWriteInquiryMode:
    case hci_spec::kWriteLEHostSupport:
    case hci_spec::kWriteLocalName:
    case hci_spec::kWritePageScanActivity:
    case hci_spec::kWritePageScanType:
    case hci_spec::kWriteScanEnable:
    case hci_spec::kWriteSecureConnectionsHostSupport:
    case hci_spec::kWriteSimplePairingMode:
    case hci_spec::kWriteSynchronousFlowControlEnable: {
      // This case is for packet types that have been migrated to the new Emboss
      // architecture. Their old version can be still be assembled from the
      // HciEmulator channel, so here we repackage and forward them as Emboss
      // packets.
      auto emboss_packet = bt::hci::EmbossCommandPacket::New<
          pw::bluetooth::emboss::CommandHeaderView>(opcode,
                                                    command_packet.size());
      bt::MutableBufferView dest = emboss_packet.mutable_data();
      command_packet.data().view().Copy(&dest);
      HandleReceivedCommandPacket(emboss_packet);
      break;
    }
    default: {
      bt_log(WARN,
             "fake-hci",
             "received unhandled command with opcode: %#.4x",
             opcode);
      RespondWithCommandComplete(
          opcode, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::HandleReceivedCommandPacket(
    const hci::EmbossCommandPacket& command_packet) {
  hci_spec::OpCode opcode = command_packet.opcode();

  if (MaybeRespondWithDefaultCommandStatus(opcode)) {
    return;
  }

  if (MaybeRespondWithDefaultStatus(opcode)) {
    return;
  }

  auto ogf = command_packet.ogf();
  if (ogf == hci_spec::kVendorOGF) {
    bt_log(WARN,
           "fake-hci",
           "vendor commands not yet migrated to Emboss; received Emboss vendor "
           "command with "
           "opcode: %#.4x",
           opcode);
    RespondWithCommandComplete(
        opcode, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
    return;
  }

  switch (opcode) {
    case hci_spec::kInquiry: {
      auto params =
          command_packet.view<pw::bluetooth::emboss::InquiryCommandView>();
      OnInquiry(params);
      break;
    }
    case hci_spec::kEnhancedAcceptSynchronousConnectionRequest: {
      auto params = command_packet.view<
          pw::bluetooth::emboss::
              EnhancedAcceptSynchronousConnectionRequestCommandView>();
      OnEnhancedAcceptSynchronousConnectionRequestCommand(params);
      break;
    }
    case hci_spec::kEnhancedSetupSynchronousConnection: {
      auto params =
          command_packet
              .view<pw::bluetooth::emboss::
                        EnhancedSetupSynchronousConnectionCommandView>();
      OnEnhancedSetupSynchronousConnectionCommand(params);
      break;
    }
    case hci_spec::kCreateConnection: {
      const auto params =
          command_packet
              .view<pw::bluetooth::emboss::CreateConnectionCommandView>();
      OnCreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kDisconnect: {
      const auto params =
          command_packet.view<pw::bluetooth::emboss::DisconnectCommandView>();
      OnDisconnectCommandReceived(params);
      break;
    }
    case hci_spec::kLESetAdvertisingEnable: {
      const auto params =
          command_packet
              .view<pw::bluetooth::emboss::LESetAdvertisingEnableCommandView>();
      OnLESetAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingEnable: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::LESetExtendedAdvertisingEnableCommandView>();
      OnLESetExtendedAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLinkKeyRequestNegativeReply: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::LinkKeyRequestNegativeReplyCommandView>();
      OnLinkKeyRequestNegativeReplyCommandReceived(params);
      break;
    }
    case hci_spec::kAuthenticationRequested: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::AuthenticationRequestedCommandView>();
      OnAuthenticationRequestedCommandReceived(params);
      break;
    }
    case hci_spec::kSetConnectionEncryption: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::SetConnectionEncryptionCommandView>();
      OnSetConnectionEncryptionCommand(params);
      break;
    }
    case hci_spec::kRemoteNameRequest: {
      const auto params =
          command_packet
              .view<pw::bluetooth::emboss::RemoteNameRequestCommandView>();
      OnReadRemoteNameRequestCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteSupportedFeatures: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::ReadRemoteSupportedFeaturesCommandView>();
      OnReadRemoteSupportedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteExtendedFeatures: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::ReadRemoteExtendedFeaturesCommandView>();
      OnReadRemoteExtendedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteVersionInfo: {
      const auto params =
          command_packet
              .view<pw::bluetooth::emboss::ReadRemoteVersionInfoCommandView>();
      OnReadRemoteVersionInfoCommandReceived(params);
      break;
    }
    case hci_spec::kIOCapabilityRequestReply: {
      const auto params = command_packet.view<
          pw::bluetooth::emboss::IoCapabilityRequestReplyCommandView>();
      OnIOCapabilityRequestReplyCommand(params);
      break;
    }
    case hci_spec::kSetEventMask: {
      const auto params =
          command_packet.view<pw::bluetooth::emboss::SetEventMaskCommandView>();
      OnSetEventMask(params);
      break;
    }
    case hci_spec::kWriteLocalName: {
      const auto params =
          command_packet
              .view<pw::bluetooth::emboss::WriteLocalNameCommandView>();
      OnWriteLocalName(params);
      break;
    }
    case hci_spec::kWriteScanEnable: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WriteScanEnableCommandView>();
      OnWriteScanEnable(params);
      break;
    }
    case hci_spec::kWritePageScanActivity: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WritePageScanActivityCommandView>();
      OnWritePageScanActivity(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestReply: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::UserConfirmationRequestReplyCommandView>();
      OnUserConfirmationRequestReplyCommand(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestNegativeReply: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::
                        UserConfirmationRequestNegativeReplyCommandView>();
      OnUserConfirmationRequestNegativeReplyCommand(params);
      break;
    }
    case hci_spec::kWriteSynchronousFlowControlEnable: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::
                        WriteSynchronousFlowControlEnableCommandView>();
      OnWriteSynchronousFlowControlEnableCommand(params);
      break;
    }
    case hci_spec::kWriteExtendedInquiryResponse: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::WriteExtendedInquiryResponseCommandView>();
      OnWriteExtendedInquiryResponse(params);
      break;
    }
    case hci_spec::kWriteSimplePairingMode: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WriteSimplePairingModeCommandView>();
      OnWriteSimplePairingMode(params);
      break;
    }
    case hci_spec::kWriteClassOfDevice: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WriteClassOfDeviceCommandView>();
      OnWriteClassOfDevice(params);
      break;
    }
    case hci_spec::kWriteInquiryMode: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WriteInquiryModeCommandView>();
      OnWriteInquiryMode(params);
      break;
    };
    case hci_spec::kWritePageScanType: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WritePageScanTypeCommandView>();
      OnWritePageScanType(params);
      break;
    }
    case hci_spec::kWriteLEHostSupport: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::WriteLEHostSupportCommandView>();
      OnWriteLEHostSupportCommandReceived(params);
      break;
    }
    case hci_spec::kWriteSecureConnectionsHostSupport: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::
                        WriteSecureConnectionsHostSupportCommandView>();
      OnWriteSecureConnectionsHostSupport(params);
      break;
    }
    case hci_spec::kReadEncryptionKeySize: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::ReadEncryptionKeySizeCommandView>();
      OnReadEncryptionKeySizeCommand(params);
      break;
    }
    case hci_spec::kLESetEventMask: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetEventMaskCommandView>();
      OnLESetEventMask(params);
      break;
    }
    case hci_spec::kLESetRandomAddress: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetRandomAddressCommandView>();
      OnLESetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetAdvertisingData: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetAdvertisingDataCommandView>();
      OnLESetAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetScanResponseData: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetScanResponseDataCommandView>();
      OnLESetScanResponseData(params);
      break;
    }
    case hci_spec::kLESetScanParameters: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetScanParametersCommandView>();
      OnLESetScanParameters(params);
      break;
    }
    case hci_spec::kLESetExtendedScanParameters: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetExtendedScanParametersCommandView>();
      OnLESetExtendedScanParameters(params);
      break;
    }
    case hci_spec::kLESetScanEnable: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LESetScanEnableCommandView>();
      OnLESetScanEnable(params);
      break;
    }
    case hci_spec::kLESetExtendedScanEnable: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetExtendedScanEnableCommandView>();
      OnLESetExtendedScanEnable(params);
      break;
    }
    case hci_spec::kLECreateConnection: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LECreateConnectionCommandView>();
      OnLECreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kLEConnectionUpdate: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LEConnectionUpdateCommandView>();
      OnLEConnectionUpdateCommandReceived(params);
      break;
    }
    case hci_spec::kLEStartEncryption: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::LEEnableEncryptionCommandView>();
      OnLEStartEncryptionCommand(params);
      break;
    }
    case hci_spec::kReadLocalExtendedFeatures: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::ReadLocalExtendedFeaturesCommandView>();
      OnReadLocalExtendedFeatures(params);
      break;
    }
    case hci_spec::kLESetAdvertisingParameters: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetAdvertisingParametersCommandView>();
      OnLESetAdvertisingParameters(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingData: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetExtendedAdvertisingDataCommandView>();
      OnLESetExtendedAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetExtendedScanResponseData: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetExtendedScanResponseDataCommandView>();
      OnLESetExtendedScanResponseData(params);
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
    case hci_spec::kLESetAdvertisingSetRandomAddress: {
      const auto& params = command_packet.view<
          pw::bluetooth::emboss::LESetAdvertisingSetRandomAddressCommandView>();
      OnLESetAdvertisingSetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingParameters: {
      const auto& params =
          command_packet
              .view<pw::bluetooth::emboss::
                        LESetExtendedAdvertisingParametersV1CommandView>();
      OnLESetExtendedAdvertisingParameters(params);
      break;
    }
    default: {
      bt_log(WARN, "fake-hci", "opcode: %#.4x", opcode);
      break;
    }
  }
}
}  // namespace bt::testing
