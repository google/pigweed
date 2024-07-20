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

#include <pw_bytes/endian.h>

#include <cstddef>
#include <cstdint>

#include "pw_bluetooth/hci_android.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/defaults.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"

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

namespace android_hci = hci_spec::vendor::android;
namespace android_emb = pw::bluetooth::vendor::android_hci;
namespace pwemb = pw::bluetooth::emboss;

void FakeController::Settings::ApplyDualModeDefaults() {
  le_connection_delay = std::chrono::seconds(0);
  hci_version = pwemb::CoreSpecificationVersion::V5_0;
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
  iso_data_packet_length = 512;
  total_num_iso_data_packets = 1;
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
  SetBit(supported_commands + 41,
         hci_spec::SupportedCommand::kLEReadBufferSizeV2);
  SetBit(supported_commands + 45,
         hci_spec::SupportedCommand::kReadLocalSupportedControllerDelay);
}

void FakeController::Settings::ApplyLegacyLEConfig() {
  ApplyLEOnlyDefaults();

  hci_version = pwemb::CoreSpecificationVersion::V4_2;

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
  // by the controller. See hci_vendor.emb LEGetVendorCapabilities for more
  // information.
  auto view = android_extension_settings.view();
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.max_advt_instances().Write(3);
  view.version_supported().major_number().Write(0);
  view.version_supported().minor_number().Write(55);
}

bool FakeController::Settings::is_event_unmasked(
    hci_spec::LEEventMask event) const {
  return le_event_mask | static_cast<uint64_t>(event);
}

void FakeController::SetDefaultCommandStatus(hci_spec::OpCode opcode,
                                             pwemb::StatusCode status) {
  default_command_status_map_[opcode] = status;
}

void FakeController::ClearDefaultCommandStatus(hci_spec::OpCode opcode) {
  default_command_status_map_.erase(opcode);
}

void FakeController::SetDefaultResponseStatus(hci_spec::OpCode opcode,
                                              pwemb::StatusCode status) {
  BT_DEBUG_ASSERT(status != pwemb::StatusCode::SUCCESS);
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
  if (peer->send_advertising_report()) {
    SendAdvertisingReport(*peer);
    SendScanResponseReport(*peer);
  }

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

void FakeController::RespondWithCommandComplete(hci_spec::OpCode opcode,
                                                pwemb::StatusCode status) {
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
  event.mutable_header()->command_opcode =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, opcode);
  event.mutable_payload_data().Write(params);

  SendEvent(hci_spec::kCommandCompleteEventCode, buffer);
}

void FakeController::RespondWithCommandComplete(
    hci_spec::OpCode opcode, hci::EmbossEventPacket* packet) {
  auto header = packet->template view<pwemb::CommandCompleteEventWriter>();

  header.num_hci_command_packets().Write(settings_.num_hci_command_packets);
  header.command_opcode_bits().BackingStorage().WriteUInt(opcode);

  SendEvent(hci_spec::kCommandCompleteEventCode, packet);
}

void FakeController::RespondWithCommandStatus(hci_spec::OpCode opcode,
                                              pwemb::StatusCode status) {
  StaticByteBuffer<sizeof(hci_spec::CommandStatusEventParams)> buffer;
  MutablePacketView<hci_spec::CommandStatusEventParams> event(&buffer);

  event.mutable_header()->status = status;
  event.mutable_header()->num_hci_command_packets =
      settings_.num_hci_command_packets;
  event.mutable_header()->command_opcode =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, opcode);

  SendEvent(hci_spec::kCommandStatusEventCode, buffer);
}

void FakeController::SendEvent(hci_spec::EventCode event_code,
                               const ByteBuffer& payload) {
  DynamicByteBuffer buffer(sizeof(hci_spec::EventHeader) + payload.size());
  MutablePacketView<hci_spec::EventHeader> event(&buffer, payload.size());

  event.mutable_header()->event_code = event_code;
  event.mutable_header()->parameter_total_size =
      static_cast<uint8_t>(payload.size());
  event.mutable_payload_data().Write(payload);

  SendCommandChannelPacket(buffer);
}

void FakeController::SendEvent(hci_spec::EventCode event_code,
                               hci::EmbossEventPacket* packet) {
  auto header = packet->template view<pwemb::EventHeaderWriter>();
  uint8_t parameter_total_size = static_cast<uint8_t>(
      packet->size() - pwemb::EventHeader::IntrinsicSizeInBytes());

  header.event_code_uint().Write(event_code);
  header.parameter_total_size().Write(parameter_total_size);

  SendCommandChannelPacket(packet->data());
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

  acl.mutable_header()->handle_and_flags =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, handle);
  acl.mutable_header()->data_total_length = pw::bytes::ConvertOrderTo(
      cpp20::endian::little, static_cast<uint16_t>(payload.size()));
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

  bframe.mutable_header()->length = pw::bytes::ConvertOrderTo(
      cpp20::endian::little, static_cast<uint16_t>(payload.size()));
  bframe.mutable_header()->channel_id =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, channel_id);
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
  cframe.mutable_header()->length = static_cast<uint16_t>(payload.size());
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
  params->data->connection_handle =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, handle);
  params->data->hc_num_of_completed_packets =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, num);

  SendEvent(hci_spec::kNumberOfCompletedPacketsEventCode, buffer);
}

void FakeController::ConnectLowEnergy(const DeviceAddress& addr,
                                      pwemb::ConnectionRole role) {
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

        uint16_t interval_min = hci_spec::defaults::kLEConnectionIntervalMin;
        uint16_t interval_max = hci_spec::defaults::kLEConnectionIntervalMax;
        uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

        hci_spec::LEConnectionParameters conn_params(
            interval, 0, hci_spec::defaults::kLESupervisionTimeout);
        peer->set_le_params(conn_params);

        auto packet = hci::EmbossEventPacket::New<
            pwemb::LEEnhancedConnectionCompleteSubeventV1Writer>(
            hci_spec::kLEMetaEventCode);
        auto view = packet.view_t();
        view.le_meta_event().subevent_code().Write(
            hci_spec::kLEEnhancedConnectionCompleteSubeventCode);
        view.status().Write(pwemb::StatusCode::SUCCESS);
        view.peer_address().CopyFrom(addr.value().view());
        view.peer_address_type().Write(
            DeviceAddress::DeviceAddrToLeAddr(addr.type()));
        view.peripheral_latency().Write(conn_params.latency());
        view.connection_interval().Write(conn_params.interval());
        view.supervision_timeout().Write(conn_params.supervision_timeout());
        view.role().Write(role);
        view.connection_handle().Write(handle);
        SendCommandChannelPacket(packet.data());
      });
}

void FakeController::SendConnectionRequest(const DeviceAddress& addr,
                                           pwemb::LinkType link_type) {
  FakePeer* peer = FindPeer(addr);
  BT_ASSERT(peer);
  peer->set_last_connection_request_link_type(link_type);

  bt_log(DEBUG,
         "fake-hci",
         "sending connection request (addr: %s, link: %s)",
         bt_str(addr),
         hci_spec::LinkTypeToString(link_type));
  auto packet =
      hci::EmbossEventPacket::New<pwemb::ConnectionRequestEventWriter>(
          hci_spec::kConnectionRequestEventCode);
  packet.view_t().bd_addr().CopyFrom(addr.value().view());
  packet.view_t().link_type().Write(link_type);
  SendCommandChannelPacket(packet.data());
}

void FakeController::L2CAPConnectionParameterUpdate(
    const DeviceAddress& addr,
    const hci_spec::LEPreferredConnectionParameters& params) {
  (void)heap_dispatcher().Post([addr, params, this](pw::async::Context /*ctx*/,
                                                    pw::Status status) {
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
    payload.interval_min =
        pw::bytes::ConvertOrderTo(cpp20::endian::little, params.min_interval());
    payload.interval_max =
        pw::bytes::ConvertOrderTo(cpp20::endian::little, params.max_interval());
    payload.peripheral_latency =
        pw::bytes::ConvertOrderTo(cpp20::endian::little, params.max_latency());
    payload.timeout_multiplier = pw::bytes::ConvertOrderTo(
        cpp20::endian::little, params.supervision_timeout());

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
    pwemb::StatusCode status) {
  auto packet = hci::EmbossEventPacket::New<
      pwemb::LEConnectionUpdateCompleteSubeventWriter>(
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
                                pwemb::StatusCode reason) {
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
    hci_spec::ConnectionHandle handle, pwemb::StatusCode reason) {
  auto event =
      hci::EmbossEventPacket::New<pwemb::DisconnectionCompleteEventWriter>(
          hci_spec::kDisconnectionCompleteEventCode);
  event.view_t().status().Write(pwemb::StatusCode::SUCCESS);
  event.view_t().connection_handle().Write(handle);
  event.view_t().reason().Write(reason);
  SendCommandChannelPacket(event.data());
}

void FakeController::SendEncryptionChangeEvent(
    hci_spec::ConnectionHandle handle,
    pwemb::StatusCode status,
    pwemb::EncryptionStatus encryption_enabled) {
  auto response =
      hci::EmbossEventPacket::New<pwemb::EncryptionChangeEventV1Writer>(
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

void FakeController::SendAdvertisingReports() {
  if (!le_scan_state_.enabled || peers_.empty()) {
    return;
  }

  for (const auto& iter : peers_) {
    if (iter.second->send_advertising_report()) {
      SendAdvertisingReport(*iter.second);
      SendScanResponseReport(*iter.second);
    }
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

  DynamicByteBuffer buffer;
  if (advertising_procedure() == AdvertisingProcedure::kExtended) {
    buffer = peer.BuildExtendedAdvertisingReportEvent();
  } else {
    buffer = peer.BuildLegacyAdvertisingReportEvent();
  }

  SendCommandChannelPacket(buffer);
}

void FakeController::SendScanResponseReport(const FakePeer& peer) {
  if (!le_scan_state_.enabled || !peer.supports_le() ||
      !peer.advertising_enabled()) {
    return;
  }

  // We want to send scan response packets only during an active scan and if the
  // peer is scannable.
  bool is_active_scan = (le_scan_state_.scan_type == pwemb::LEScanType::ACTIVE);
  bool need_scan_rsp = (is_active_scan && peer.scannable());

  if (!need_scan_rsp) {
    return;
  }

  DynamicByteBuffer buffer;
  if (advertising_procedure() == AdvertisingProcedure::kExtended) {
    buffer = peer.BuildExtendedScanResponseEvent();
  } else {
    buffer = peer.BuildLegacyScanResponseReportEvent();
  }

  SendCommandChannelPacket(buffer);
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

void FakeController::CaptureLEConnectParams(
    const pwemb::LECreateConnectionCommandView& params) {
  le_connect_params_ = LEConnectParams();

  switch (params.initiator_filter_policy().Read()) {
    case pwemb::GenericEnableParam::ENABLE:
      le_connect_params_->use_filter_policy = true;
      break;
    case pwemb::GenericEnableParam::DISABLE:
      le_connect_params_->use_filter_policy = false;
      break;
  }

  le_connect_params_->own_address_type = params.own_address_type().Read();
  le_connect_params_->peer_address = DeviceAddress(
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read()),
      DeviceAddressBytes(params.peer_address()));

  LEConnectParams::Parameters& connect_params =
      le_connect_params_
          ->phy_conn_params[LEConnectParams::InitiatingPHYs::kLE_1M];
  connect_params.scan_interval = params.le_scan_interval().Read();
  connect_params.scan_window = params.le_scan_window().Read();
  connect_params.connection_interval_min =
      params.connection_interval_min().Read();
  connect_params.connection_interval_max =
      params.connection_interval_max().Read();
  connect_params.max_latency = params.max_latency().Read();
  connect_params.supervision_timeout = params.supervision_timeout().Read();
  connect_params.min_ce_length = params.min_connection_event_length().Read();
  connect_params.max_ce_length = params.max_connection_event_length().Read();
}

void FakeController::CaptureLEConnectParamsForPHY(
    const pwemb::LEExtendedCreateConnectionCommandV1View& params,
    LEConnectParams::InitiatingPHYs phy) {
  int index = static_cast<int>(phy);

  LEConnectParams::Parameters& connect_params =
      le_connect_params_->phy_conn_params[phy];
  connect_params.scan_interval = params.data()[index].scan_interval().Read();
  connect_params.scan_window = params.data()[index].scan_window().Read();
  connect_params.connection_interval_min =
      params.data()[index].connection_interval_min().Read();
  connect_params.connection_interval_min =
      params.data()[index].connection_interval_max().Read();
  connect_params.max_latency = params.data()[index].max_latency().Read();
  connect_params.supervision_timeout =
      params.data()[index].supervision_timeout().Read();
  connect_params.min_ce_length =
      params.data()[index].min_connection_event_length().Read();
  connect_params.max_ce_length =
      params.data()[index].max_connection_event_length().Read();
}

void FakeController::CaptureLEConnectParams(
    const pwemb::LEExtendedCreateConnectionCommandV1View& params) {
  le_connect_params_ = LEConnectParams();

  switch (params.initiator_filter_policy().Read()) {
    case pwemb::GenericEnableParam::ENABLE:
      le_connect_params_->use_filter_policy = true;
      break;
    case pwemb::GenericEnableParam::DISABLE:
      le_connect_params_->use_filter_policy = false;
      break;
  }

  le_connect_params_->own_address_type = params.own_address_type().Read();
  le_connect_params_->peer_address = DeviceAddress(
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read()),
      DeviceAddressBytes(params.peer_address()));

  CaptureLEConnectParamsForPHY(params, LEConnectParams::InitiatingPHYs::kLE_1M);
  CaptureLEConnectParamsForPHY(params, LEConnectParams::InitiatingPHYs::kLE_2M);
  CaptureLEConnectParamsForPHY(params,
                               LEConnectParams::InitiatingPHYs::kLE_Coded);
}

void FakeController::OnCreateConnectionCommandReceived(
    const pwemb::CreateConnectionCommandView& params) {
  acl_create_connection_command_count_++;

  // Cannot issue this command while a request is already pending.
  if (bredr_connect_pending_) {
    RespondWithCommandStatus(hci_spec::kCreateConnection,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));
  pwemb::StatusCode status = pwemb::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected()) {
      status = pwemb::StatusCode::CONNECTION_ALREADY_EXISTS;
    } else {
      status = peer->connect_status();
    }
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kCreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != pwemb::StatusCode::SUCCESS)
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

          auto response =
              hci::EmbossEventPacket::New<pwemb::ConnectionCompleteEventWriter>(
                  hci_spec::kConnectionCompleteEventCode);
          response.view_t().status().Write(pwemb::StatusCode::PAGE_TIMEOUT);
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
    status = pwemb::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  auto response =
      hci::EmbossEventPacket::New<pwemb::ConnectionCompleteEventWriter>(
          hci_spec::kConnectionCompleteEventCode);
  response.view_t().status().Write(status);
  response.view_t().bd_addr().CopyFrom(params.bd_addr());
  response.view_t().link_type().Write(pwemb::LinkType::ACL);
  response.view_t().encryption_enabled().Write(
      pwemb::GenericEnableParam::DISABLE);

  if (status == pwemb::StatusCode::SUCCESS) {
    hci_spec::ConnectionHandle handle = ++next_conn_handle_;
    response.view_t().connection_handle().Write(handle);
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect()) {
    return;
  }

  bredr_connect_rsp_task_.Cancel();
  bredr_connect_rsp_task_.set_function(
      [response, peer, this](pw::async::Context /*ctx*/,
                             pw::Status status) mutable {
        if (!status.ok()) {
          return;
        }
        bredr_connect_pending_ = false;

        if (response.view_t().status().Read() == pwemb::StatusCode::SUCCESS) {
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
    const pwemb::LECreateConnectionCommandView& params) {
  le_create_connection_command_count_++;

  if (advertising_procedure() == AdvertisingProcedure::kExtended) {
    RespondWithCommandStatus(hci_spec::kLECreateConnection,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (le_create_connection_cb_) {
    le_create_connection_cb_(params);
  }

  // Cannot issue this command while a request is already pending.
  if (le_connect_pending_) {
    RespondWithCommandStatus(hci_spec::kLECreateConnection,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  // The link is considered lost after connection_interval_max * 2. Connection
  // events (when data pdus are transmitted) must occur at least once within
  // that time frame.
  if (params.max_connection_event_length().Read() >
      2 * params.connection_interval_max().Read()) {
    RespondWithCommandStatus(hci_spec::kLECreateConnection,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  std::optional<DeviceAddress::Type> addr_type =
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read());
  BT_DEBUG_ASSERT(addr_type && *addr_type != DeviceAddress::Type::kBREDR);

  const DeviceAddress peer_address(*addr_type,
                                   DeviceAddressBytes(params.peer_address()));
  pw::bluetooth::emboss::StatusCode status =
      pw::bluetooth::emboss::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected()) {
      status = pwemb::StatusCode::CONNECTION_ALREADY_EXISTS;
    } else {
      status = peer->connect_status();
    }
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kLECreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != pwemb::StatusCode::SUCCESS) {
    return;
  }

  le_connect_pending_ = true;
  CaptureLEConnectParams(params);

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO,
           "fake-hci",
           "requested fake peer cannot be connected; request will time out");
    return;
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect()) {
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = pwemb::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  uint16_t interval_min = params.connection_interval_min().Read();
  uint16_t interval_max = params.connection_interval_max().Read();
  uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

  hci_spec::LEConnectionParameters conn_params(
      interval,
      params.max_latency().Read(),
      params.supervision_timeout().Read());
  peer->set_le_params(conn_params);

  bool use_enhanced_connection_complete = settings_.is_event_unmasked(
      hci_spec::LEEventMask::kLEEnhancedConnectionComplete);
  if (use_enhanced_connection_complete) {
    SendEnhancedConnectionCompleteEvent(status,
                                        params,
                                        interval,
                                        params.max_latency().Read(),
                                        params.supervision_timeout().Read());
  } else {
    SendConnectionCompleteEvent(status, params, interval);
  }
}

void FakeController::OnLEExtendedCreateConnectionCommandReceived(
    const pwemb::LEExtendedCreateConnectionCommandV1View& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(INFO,
           "fake-hci",
           "extended create connection command rejected, legacy advertising is "
           "in use");
    RespondWithCommandStatus(hci_spec::kLEExtendedCreateConnection,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (const auto& phys = params.initiating_phys();
      !phys.le_1m().Read() && !phys.le_2m().Read() && phys.le_coded().Read()) {
    RespondWithCommandStatus(hci_spec::kLEExtendedCreateConnection,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  }

  // Cannot issue this command while a request is already pending.
  if (le_connect_pending_) {
    RespondWithCommandStatus(hci_spec::kLEExtendedCreateConnection,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  // The link is considered lost after connection_interval_max * 2. Connection
  // events (when data pdus are transmitted) must occur at least once within
  // that time frame.
  if (params.data()[0].max_connection_event_length().Read() >
      2 * params.data()[0].connection_interval_max().Read()) {
    RespondWithCommandStatus(hci_spec::kLEExtendedCreateConnection,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  DeviceAddress::Type addr_type =
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read());
  const DeviceAddress peer_address(addr_type,
                                   DeviceAddressBytes(params.peer_address()));

  pwemb::StatusCode status = pwemb::StatusCode::SUCCESS;

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (peer) {
    if (peer->connected()) {
      status = pwemb::StatusCode::CONNECTION_ALREADY_EXISTS;
    } else {
      status = peer->connect_status();
    }
  }

  // First send the Command Status response.
  RespondWithCommandStatus(hci_spec::kLEExtendedCreateConnection, status);

  // If we just sent back an error status then the operation is complete.
  if (status != pwemb::StatusCode::SUCCESS) {
    return;
  }

  le_connect_pending_ = true;
  CaptureLEConnectParams(params);

  // The procedure was initiated successfully but the peer cannot be connected
  // because it either doesn't exist or isn't connectable.
  if (!peer || !peer->connectable()) {
    bt_log(INFO,
           "fake-hci",
           "requested fake peer cannot be connected; request will time out");
    return;
  }

  // Don't send a connection event if we were asked to force the request to
  // remain pending. This is used by test cases that operate during the pending
  // state.
  if (peer->force_pending_connect()) {
    return;
  }

  if (next_conn_handle_ == 0x0FFF) {
    // Ran out of handles
    status = pwemb::StatusCode::CONNECTION_LIMIT_EXCEEDED;
  } else {
    status = peer->connect_response();
  }

  uint16_t interval_min = params.data()[0].connection_interval_min().Read();
  uint16_t interval_max = params.data()[0].connection_interval_max().Read();
  uint16_t interval = interval_min + ((interval_max - interval_min) / 2);

  hci_spec::LEConnectionParameters conn_params(
      interval,
      params.data()[0].max_latency().Read(),
      params.data()[0].supervision_timeout().Read());
  peer->set_le_params(conn_params);

  SendEnhancedConnectionCompleteEvent(
      status,
      params,
      interval,
      params.data()[0].max_latency().Read(),
      params.data()[0].supervision_timeout().Read());
}

template <typename T>
void FakeController::SendEnhancedConnectionCompleteEvent(
    pwemb::StatusCode status,
    const T& params,
    uint16_t interval,
    uint16_t max_latency,
    uint16_t supervision_timeout) {
  DeviceAddress::Type addr_type =
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read());
  const DeviceAddress peer_address(addr_type,
                                   DeviceAddressBytes(params.peer_address()));

  auto packet = hci::EmbossEventPacket::New<
      pwemb::LEEnhancedConnectionCompleteSubeventV1Writer>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEEnhancedConnectionCompleteSubeventCode);
  view.status().Write(status);
  view.peer_address().CopyFrom(params.peer_address());
  view.peer_address_type().Write(DeviceAddress::DeviceAddrToLeAddr(addr_type));
  view.peripheral_latency().Write(max_latency);
  view.connection_interval().Write(interval);
  view.supervision_timeout().Write(supervision_timeout);
  view.role().Write(settings_.le_connection_role);
  view.connection_handle().Write(++next_conn_handle_);

  le_connect_rsp_task_.Cancel();
  le_connect_rsp_task_.set_function(
      [packet, address = peer_address, this](pw::async::Context /*ctx*/,
                                             pw::Status status) {
        auto peer = FindPeer(address);
        if (!peer || !status.ok()) {
          // The peer has been removed or dispatcher shut down; Ignore this
          // response
          return;
        }

        le_connect_pending_ = false;

        auto view =
            packet.view<pwemb::LEEnhancedConnectionCompleteSubeventV1View>();
        if (view.status().Read() == pwemb::StatusCode::SUCCESS) {
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

void FakeController::SendConnectionCompleteEvent(
    pwemb::StatusCode status,
    const pwemb::LECreateConnectionCommandView& params,
    uint16_t interval) {
  DeviceAddress::Type addr_type =
      DeviceAddress::LeAddrToDeviceAddr(params.peer_address_type().Read());
  const DeviceAddress peer_address(addr_type,
                                   DeviceAddressBytes(params.peer_address()));

  auto packet =
      hci::EmbossEventPacket::New<pwemb::LEConnectionCompleteSubeventWriter>(
          hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionCompleteSubeventCode);
  view.status().Write(status);
  view.peer_address().CopyFrom(params.peer_address());
  view.peer_address_type().Write(
      DeviceAddress::DeviceAddrToLePeerAddr(addr_type));

  view.peripheral_latency().CopyFrom(params.max_latency());
  view.connection_interval().Write(interval);
  view.supervision_timeout().CopyFrom(params.supervision_timeout());
  view.role().Write(settings_.le_connection_role);
  view.connection_handle().Write(++next_conn_handle_);

  le_connect_rsp_task_.Cancel();
  le_connect_rsp_task_.set_function(
      [packet, address = peer_address, this](pw::async::Context /*ctx*/,
                                             pw::Status status) {
        auto peer = FindPeer(address);
        if (!peer || !status.ok()) {
          // The peer has been removed or dispatcher shut down; Ignore this
          // response
          return;
        }

        le_connect_pending_ = false;

        auto view = packet.view<pwemb::LEConnectionCompleteSubeventView>();
        if (view.status().Read() == pwemb::StatusCode::SUCCESS) {
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
    const pwemb::LEConnectionUpdateCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  uint16_t min_interval = params.connection_interval_min().UncheckedRead();
  uint16_t max_interval = params.connection_interval_max().UncheckedRead();
  uint16_t max_latency = params.max_latency().UncheckedRead();
  uint16_t supv_timeout = params.supervision_timeout().UncheckedRead();

  if (min_interval > max_interval) {
    RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEConnectionUpdate,
                           pwemb::StatusCode::SUCCESS);

  hci_spec::LEConnectionParameters conn_params(
      min_interval + ((max_interval - min_interval) / 2),
      max_latency,
      supv_timeout);
  peer->set_le_params(conn_params);

  auto packet = hci::EmbossEventPacket::New<
      pwemb::LEConnectionUpdateCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEConnectionUpdateCompleteSubeventCode);
  view.connection_handle().CopyFrom(params.connection_handle());
  if (peer->supports_ll_conn_update_procedure()) {
    view.status().Write(pwemb::StatusCode::SUCCESS);
    view.connection_interval().UncheckedWrite(conn_params.interval());
    view.peripheral_latency().CopyFrom(params.max_latency());
    view.supervision_timeout().UncheckedCopyFrom(params.supervision_timeout());
  } else {
    view.status().Write(pwemb::StatusCode::UNSUPPORTED_REMOTE_FEATURE);
  }
  SendCommandChannelPacket(packet.data());

  NotifyLEConnectionParameters(peer->address(), conn_params);
}

void FakeController::OnDisconnectCommandReceived(
    const pwemb::DisconnectCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();

  // Find the peer that matches the disconnected handle.
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kDisconnect,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  BT_DEBUG_ASSERT(peer->connected());

  RespondWithCommandStatus(hci_spec::kDisconnect, pwemb::StatusCode::SUCCESS);

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
    const pwemb::WriteLEHostSupportCommandView& params) {
  if (params.le_supported_host().Read() == pwemb::GenericEnableParam::ENABLE) {
    SetBit(&settings_.lmp_features_page1,
           hci_spec::LMPFeature::kLESupportedHost);
  } else {
    UnsetBit(&settings_.lmp_features_page1,
             hci_spec::LMPFeature::kLESupportedHost);
  }

  RespondWithCommandComplete(hci_spec::kWriteLEHostSupport,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnWriteSecureConnectionsHostSupport(
    const pwemb::WriteSecureConnectionsHostSupportCommandView& params) {
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
    RespondWithCommandComplete(hci_spec::kWriteSecureConnectionsHostSupport,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (params.secure_connections_host_support().Read() ==
      pwemb::GenericEnableParam::ENABLE) {
    SetBit(&settings_.lmp_features_page1,
           hci_spec::LMPFeature::kSecureConnectionsHostSupport);
  } else {
    UnsetBit(&settings_.lmp_features_page1,
             hci_spec::LMPFeature::kSecureConnectionsHostSupport);
  }

  RespondWithCommandComplete(hci_spec::kWriteSecureConnectionsHostSupport,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReset() {
  // TODO(fxbug.dev/42159137): actually do some resetting of stuff here
  RespondWithCommandComplete(hci_spec::kReset, pwemb::StatusCode::SUCCESS);
}

void FakeController::OnInquiry(const pwemb::InquiryCommandView& params) {
  // Confirm that LAP array is equal to either kGIAC or kLIAC.
  if (params.lap().Read() != pwemb::InquiryAccessCode::GIAC &&
      params.lap().Read() != pwemb::InquiryAccessCode::LIAC) {
    RespondWithCommandStatus(hci_spec::kInquiry,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.inquiry_length().Read() == 0x00 ||
      params.inquiry_length().Read() > hci_spec::kInquiryLengthMax) {
    RespondWithCommandStatus(hci_spec::kInquiry,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  inquiry_num_responses_left_ = params.num_responses().Read();
  if (params.num_responses().Read() == 0) {
    inquiry_num_responses_left_ = -1;
  }

  RespondWithCommandStatus(hci_spec::kInquiry, pwemb::StatusCode::SUCCESS);

  bt_log(INFO, "fake-hci", "sending inquiry responses..");
  SendInquiryResponses();

  (void)heap_dispatcher().PostAfter(
      [this](pw::async::Context /*ctx*/, pw::Status status) {
        if (!status.ok()) {
          return;
        }
        auto output =
            hci::EmbossEventPacket::New<pwemb::InquiryCompleteEventWriter>(
                hci_spec::kInquiryCompleteEventCode);
        output.view_t().status().Write(pwemb::StatusCode::SUCCESS);
        SendCommandChannelPacket(output.data());
      },
      std::chrono::milliseconds(params.inquiry_length().Read()) * 1280);
}

void FakeController::OnLESetScanEnable(
    const pwemb::LESetScanEnableCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetScanEnable,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_scan_state_.enabled = false;
  if (params.le_scan_enable().Read() == pwemb::GenericEnableParam::ENABLE) {
    le_scan_state_.enabled = true;
  }

  le_scan_state_.filter_duplicates = false;
  if (params.filter_duplicates().Read() == pwemb::GenericEnableParam::ENABLE) {
    le_scan_state_.filter_duplicates = true;
  }

  // Post the scan state update before scheduling the HCI Command Complete
  // event. This guarantees that single-threaded unit tests receive the scan
  // state update BEFORE the HCI command sequence terminates.
  if (scan_state_cb_) {
    scan_state_cb_(le_scan_state_.enabled);
  }

  RespondWithCommandComplete(hci_spec::kLESetScanEnable,
                             pwemb::StatusCode::SUCCESS);

  if (le_scan_state_.enabled) {
    SendAdvertisingReports();
  }
}

void FakeController::OnLESetExtendedScanEnable(
    const pwemb::LESetExtendedScanEnableCommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetExtendedScanEnable,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_scan_state_.enabled = false;
  if (params.scanning_enabled().Read() == pwemb::GenericEnableParam::ENABLE) {
    le_scan_state_.enabled = true;
  }

  le_scan_state_.filter_duplicates = true;
  if (params.filter_duplicates().Read() ==
      pwemb::LEExtendedDuplicateFilteringOption::DISABLED) {
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
                             pwemb::StatusCode::SUCCESS);

  if (le_scan_state_.enabled) {
    SendAdvertisingReports();
  }
}

void FakeController::OnLESetScanParameters(
    const pwemb::LESetScanParametersCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetScanParameters,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (le_scan_state_.enabled) {
    RespondWithCommandComplete(hci_spec::kLESetScanParameters,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_scan_state_.own_address_type = params.own_address_type().Read();
  le_scan_state_.filter_policy = params.scanning_filter_policy().Read();
  le_scan_state_.scan_type = params.le_scan_type().Read();
  le_scan_state_.scan_interval = params.le_scan_interval().Read();
  le_scan_state_.scan_window = params.le_scan_window().Read();

  RespondWithCommandComplete(hci_spec::kLESetScanParameters,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnLESetExtendedScanParameters(
    const pwemb::LESetExtendedScanParametersCommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetScanParameters,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (le_scan_state_.enabled) {
    RespondWithCommandComplete(hci_spec::kLESetScanParameters,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (params.num_entries().Read() == 0) {
    RespondWithCommandComplete(
        hci_spec::kLESetScanParameters,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
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
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  le_scan_state_.scan_type = params.data()[0].scan_type().Read();
  le_scan_state_.scan_interval = params.data()[0].scan_interval().Read();
  le_scan_state_.scan_window = params.data()[0].scan_window().Read();
  RespondWithCommandComplete(hci_spec::kLESetExtendedScanParameters,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadLocalExtendedFeatures(
    const pwemb::ReadLocalExtendedFeaturesCommandView& params) {
  hci_spec::ReadLocalExtendedFeaturesReturnParams out_params;
  out_params.status = pwemb::StatusCode::SUCCESS;
  out_params.page_number = params.page_number().Read();
  out_params.maximum_page_number = 2;
  out_params.extended_lmp_features = 0;

  switch (params.page_number().Read()) {
    case 0:
      out_params.extended_lmp_features = pw::bytes::ConvertOrderTo(
          cpp20::endian::little, settings_.lmp_features_page0);
      break;
    case 1:
      out_params.extended_lmp_features = pw::bytes::ConvertOrderTo(
          cpp20::endian::little, settings_.lmp_features_page1);
      break;
    case 2:
      out_params.extended_lmp_features = pw::bytes::ConvertOrderTo(
          cpp20::endian::little, settings_.lmp_features_page2);
      break;
    default:
      out_params.status = pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS;
  }

  RespondWithCommandComplete(hci_spec::kReadLocalExtendedFeatures,
                             BufferView(&out_params, sizeof(out_params)));
}

void FakeController::OnSetEventMask(
    const pwemb::SetEventMaskCommandView& params) {
  settings_.event_mask = params.event_mask().Read();
  RespondWithCommandComplete(hci_spec::kSetEventMask,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnLESetEventMask(
    const pwemb::LESetEventMaskCommandView& params) {
  settings_.le_event_mask = params.le_event_mask().BackingStorage().ReadUInt();
  RespondWithCommandComplete(hci_spec::kLESetEventMask,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnLEReadBufferSizeV1() {
  hci_spec::LEReadBufferSizeV1ReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.hc_le_acl_data_packet_length = pw::bytes::ConvertOrderTo(
      cpp20::endian::little, settings_.le_acl_data_packet_length);
  params.hc_total_num_le_acl_data_packets =
      settings_.le_total_num_acl_data_packets;
  RespondWithCommandComplete(hci_spec::kLEReadBufferSizeV1,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadBufferSizeV2() {
  auto packet = hci::EmbossEventPacket::New<
      pwemb::LEReadBufferSizeV2CommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();

  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.le_acl_data_packet_length().Write(settings_.le_acl_data_packet_length);
  view.total_num_le_acl_data_packets().Write(
      settings_.le_total_num_acl_data_packets);
  view.iso_data_packet_length().Write(settings_.iso_data_packet_length);
  view.total_num_iso_data_packets().Write(settings_.total_num_iso_data_packets);

  RespondWithCommandComplete(hci_spec::kLEReadBufferSizeV2, &packet);
}

void FakeController::OnLEReadSupportedStates() {
  hci_spec::LEReadSupportedStatesReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.le_states = pw::bytes::ConvertOrderTo(cpp20::endian::little,
                                               settings_.le_supported_states);
  RespondWithCommandComplete(hci_spec::kLEReadSupportedStates,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLEReadLocalSupportedFeatures() {
  hci_spec::LEReadLocalSupportedFeaturesReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.le_features =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, settings_.le_features);
  RespondWithCommandComplete(hci_spec::kLEReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLECreateConnectionCancel() {
  if (!le_connect_pending_) {
    RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  le_connect_pending_ = false;
  le_connect_rsp_task_.Cancel();
  BT_DEBUG_ASSERT(le_connect_params_);

  NotifyConnectionState(le_connect_params_->peer_address,
                        0,
                        /*connected=*/false,
                        /*canceled=*/true);

  bool use_enhanced_connection_complete = settings_.is_event_unmasked(
      hci_spec::LEEventMask::kLEEnhancedConnectionComplete);
  if (use_enhanced_connection_complete) {
    auto packet = hci::EmbossEventPacket::New<
        pwemb::LEEnhancedConnectionCompleteSubeventV1Writer>(
        hci_spec::kLEMetaEventCode);
    auto params = packet.view_t();
    params.le_meta_event().subevent_code().Write(
        hci_spec::kLEEnhancedConnectionCompleteSubeventCode);
    params.status().Write(pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    params.peer_address().CopyFrom(
        le_connect_params_->peer_address.value().view());
    params.peer_address_type().Write(DeviceAddress::DeviceAddrToLeAddr(
        le_connect_params_->peer_address.type()));

    RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel,
                               pwemb::StatusCode::SUCCESS);
    SendCommandChannelPacket(packet.data());
  } else {
    auto packet =
        hci::EmbossEventPacket::New<pwemb::LEConnectionCompleteSubeventWriter>(
            hci_spec::kLEMetaEventCode);
    auto params = packet.view_t();
    params.le_meta_event().subevent_code().Write(
        hci_spec::kLEConnectionCompleteSubeventCode);
    params.status().Write(pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    params.peer_address().CopyFrom(
        le_connect_params_->peer_address.value().view());
    params.peer_address_type().Write(DeviceAddress::DeviceAddrToLePeerAddr(
        le_connect_params_->peer_address.type()));

    RespondWithCommandComplete(hci_spec::kLECreateConnectionCancel,
                               pwemb::StatusCode::SUCCESS);
    SendCommandChannelPacket(packet.data());
  }
}

void FakeController::OnWriteExtendedInquiryResponse(
    const pwemb::WriteExtendedInquiryResponseCommandView& params) {
  // As of now, we don't support FEC encoding enabled.
  if (params.fec_required().Read() != 0x00) {
    RespondWithCommandStatus(hci_spec::kWriteExtendedInquiryResponse,
                             pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  }

  RespondWithCommandComplete(hci_spec::kWriteExtendedInquiryResponse,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnWriteSimplePairingMode(
    const pwemb::WriteSimplePairingModeCommandView& params) {
  // "A host shall not set the Simple Pairing Mode to 'disabled'"
  // Spec 5.0 Vol 2 Part E Sec 7.3.59
  if (params.simple_pairing_mode().Read() !=
      pwemb::GenericEnableParam::ENABLE) {
    RespondWithCommandComplete(
        hci_spec::kWriteSimplePairingMode,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  SetBit(&settings_.lmp_features_page1,
         hci_spec::LMPFeature::kSecureSimplePairingHostSupport);
  RespondWithCommandComplete(hci_spec::kWriteSimplePairingMode,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadSimplePairingMode() {
  hci_spec::ReadSimplePairingModeReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  if (CheckBit(settings_.lmp_features_page1,
               hci_spec::LMPFeature::kSecureSimplePairingHostSupport)) {
    params.simple_pairing_mode = pwemb::GenericEnableParam::ENABLE;
  } else {
    params.simple_pairing_mode = pwemb::GenericEnableParam::DISABLE;
  }

  RespondWithCommandComplete(hci_spec::kReadSimplePairingMode,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWritePageScanType(
    const pwemb::WritePageScanTypeCommandView& params) {
  page_scan_type_ = params.page_scan_type().Read();
  RespondWithCommandComplete(hci_spec::kWritePageScanType,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanType() {
  hci_spec::ReadPageScanTypeReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.page_scan_type = page_scan_type_;
  RespondWithCommandComplete(hci_spec::kReadPageScanType,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteInquiryMode(
    const pwemb::WriteInquiryModeCommandView& params) {
  inquiry_mode_ = params.inquiry_mode().Read();
  RespondWithCommandComplete(hci_spec::kWriteInquiryMode,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadInquiryMode() {
  hci_spec::ReadInquiryModeReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.inquiry_mode = inquiry_mode_;
  RespondWithCommandComplete(hci_spec::kReadInquiryMode,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteClassOfDevice(
    const pwemb::WriteClassOfDeviceCommandView& params) {
  device_class_ =
      DeviceClass(params.class_of_device().BackingStorage().ReadUInt());
  NotifyControllerParametersChanged();
  RespondWithCommandComplete(hci_spec::kWriteClassOfDevice,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnWritePageScanActivity(
    const pwemb::WritePageScanActivityCommandView& params) {
  page_scan_interval_ = params.page_scan_interval().Read();
  page_scan_window_ = params.page_scan_window().Read();
  RespondWithCommandComplete(hci_spec::kWritePageScanActivity,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadPageScanActivity() {
  hci_spec::ReadPageScanActivityReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.page_scan_interval =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, page_scan_interval_);
  params.page_scan_window =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, page_scan_window_);
  RespondWithCommandComplete(hci_spec::kReadPageScanActivity,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteScanEnable(
    const pwemb::WriteScanEnableCommandView& params) {
  bredr_scan_state_ = params.scan_enable().BackingStorage().ReadUInt();
  RespondWithCommandComplete(hci_spec::kWriteScanEnable,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadScanEnable() {
  hci_spec::ReadScanEnableReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.scan_enable = bredr_scan_state_;
  RespondWithCommandComplete(hci_spec::kReadScanEnable,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalName() {
  hci_spec::ReadLocalNameReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  auto mut_view =
      MutableBufferView(params.local_name, hci_spec::kMaxNameLength);
  mut_view.Write((const uint8_t*)(local_name_.c_str()),
                 std::min(local_name_.length() + 1, hci_spec::kMaxNameLength));
  RespondWithCommandComplete(hci_spec::kReadLocalName,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnWriteLocalName(
    const pwemb::WriteLocalNameCommandView& params) {
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
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnCreateConnectionCancel() {
  hci_spec::CreateConnectionCancelReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.bd_addr = pending_bredr_connect_addr_.value();

  if (!bredr_connect_pending_) {
    // No request is currently pending.
    params.status = pwemb::StatusCode::UNKNOWN_CONNECTION_ID;
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

  auto response =
      hci::EmbossEventPacket::New<pwemb::ConnectionCompleteEventWriter>(
          hci_spec::kConnectionCompleteEventCode);
  response.view_t().status().Write(pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
  response.view_t().bd_addr().CopyFrom(
      pending_bredr_connect_addr_.value().view());
  SendCommandChannelPacket(response.data());
}

void FakeController::OnReadBufferSize() {
  hci_spec::ReadBufferSizeReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.hc_acl_data_packet_length = pw::bytes::ConvertOrderTo(
      cpp20::endian::little, settings_.acl_data_packet_length);
  params.hc_total_num_acl_data_packets = settings_.total_num_acl_data_packets;
  params.hc_synchronous_data_packet_length = pw::bytes::ConvertOrderTo(
      cpp20::endian::little,
      static_cast<uint8_t>(settings_.synchronous_data_packet_length));
  params.hc_total_num_synchronous_data_packets =
      settings_.total_num_synchronous_data_packets;
  RespondWithCommandComplete(hci_spec::kReadBufferSize,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadBRADDR() {
  hci_spec::ReadBDADDRReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.bd_addr = settings_.bd_addr.value();
  RespondWithCommandComplete(hci_spec::kReadBDADDR,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnLESetAdvertisingEnable(
    const pwemb::LESetAdvertisingEnableCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetAdvertisingEnable,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (legacy_advertising_state_.own_address_type ==
          pw::bluetooth::emboss::LEOwnAddressType::RANDOM &&
      !legacy_advertising_state_.random_address.has_value()) {
    bt_log(INFO,
           "fake-hci",
           "cannot enable, random address type requires a random address set");
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingEnable,
        pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  legacy_advertising_state_.enabled =
      params.advertising_enable().Read() == pwemb::GenericEnableParam::ENABLE;
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingEnable,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetScanResponseData(
    const pwemb::LESetScanResponseDataCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetScanResponseData,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

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
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingData(
    const pwemb::LESetAdvertisingDataCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetAdvertisingData,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

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
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetAdvertisingParameters(
    const pwemb::LESetAdvertisingParametersCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetAdvertisingParameters,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (legacy_advertising_state_.enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot set advertising parameters while advertising enabled");
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  uint16_t interval_min = params.advertising_interval_min().UncheckedRead();
  uint16_t interval_max = params.advertising_interval_max().UncheckedRead();

  // Core Spec Volume 4, Part E, Section 7.8.5: For high duty cycle directed
  // advertising, the Advertising_Interval_Min and Advertising_Interval_Max
  // parameters are not used and shall be ignored.
  if (params.adv_type().Read() !=
      pwemb::LEAdvertisingType::CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED) {
    if (interval_min >= interval_max) {
      bt_log(INFO,
             "fake-hci",
             "advertising interval min (%d) not strictly less than max (%d)",
             interval_min,
             interval_max);
      RespondWithCommandComplete(
          hci_spec::kLESetAdvertisingParameters,
          pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
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
          pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
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
          pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }
  }

  legacy_advertising_state_.own_address_type = params.own_address_type().Read();
  legacy_advertising_state_.interval_min = interval_min;
  legacy_advertising_state_.interval_max = interval_max;

  pwemb::LEAdvertisingType adv_type = params.adv_type().Read();
  switch (adv_type) {
    case pwemb::LEAdvertisingType::CONNECTABLE_AND_SCANNABLE_UNDIRECTED:
      legacy_advertising_state_.properties.scannable = true;
      legacy_advertising_state_.properties.connectable = true;
      break;
    case pwemb::LEAdvertisingType::CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED:
      legacy_advertising_state_.properties.directed = true;
      legacy_advertising_state_.properties.connectable = true;
      break;
    case pwemb::LEAdvertisingType::CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED:
      legacy_advertising_state_.properties
          .high_duty_cycle_directed_connectable = true;
      legacy_advertising_state_.properties.directed = true;
      legacy_advertising_state_.properties.connectable = true;
      break;
    case pwemb::LEAdvertisingType::SCANNABLE_UNDIRECTED:
      legacy_advertising_state_.properties.scannable = true;
      break;
    case pwemb::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED:
      break;
  }

  bt_log(INFO,
         "fake-hci",
         "start advertising using address type: %hhd",
         static_cast<char>(legacy_advertising_state_.own_address_type));

  RespondWithCommandComplete(hci_spec::kLESetAdvertisingParameters,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetRandomAddress(
    const pwemb::LESetRandomAddressCommandView& params) {
  if (!EnableLegacyAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "legacy advertising command rejected, extended advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetRandomAddress,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  if (legacy_advertising_state().enabled || le_scan_state().enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot set LE random address while scanning or advertising");
    RespondWithCommandComplete(hci_spec::kLESetRandomAddress,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  legacy_advertising_state_.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom,
                    DeviceAddressBytes(params.random_address()));
  RespondWithCommandComplete(hci_spec::kLESetRandomAddress,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnReadLocalSupportedFeatures() {
  hci_spec::ReadLocalSupportedFeaturesReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.lmp_features = pw::bytes::ConvertOrderTo(cpp20::endian::little,
                                                  settings_.lmp_features_page0);
  RespondWithCommandComplete(hci_spec::kReadLocalSupportedFeatures,
                             BufferView(&params, sizeof(params)));
}

void FakeController::OnReadLocalSupportedCommands() {
  hci_spec::ReadLocalSupportedCommandsReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
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
    const pwemb::RemoteNameRequestCommandView& params) {
  const DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                                   DeviceAddressBytes(params.bd_addr()));

  // Find the peer that matches the requested address.
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kRemoteNameRequest,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kRemoteNameRequest,
                           pwemb::StatusCode::SUCCESS);

  struct RemoteNameRequestCompleteEventParams {
    pwemb::StatusCode status;
    DeviceAddressBytes bd_addr;
    uint8_t remote_name[hci_spec::kMaxNameLength];
  } __attribute__((packed));
  RemoteNameRequestCompleteEventParams response = {};
  response.bd_addr = DeviceAddressBytes(params.bd_addr());
  std::strncpy((char*)response.remote_name,
               peer->name().c_str(),
               hci_spec::kMaxNameLength);
  response.status = pwemb::StatusCode::SUCCESS;
  SendEvent(hci_spec::kRemoteNameRequestCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteSupportedFeaturesCommandReceived(
    const pwemb::ReadRemoteSupportedFeaturesCommandView& params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteSupportedFeatures,
                           pwemb::StatusCode::SUCCESS);

  hci_spec::ReadRemoteSupportedFeaturesCompleteEventParams response = {};
  response.status = pwemb::StatusCode::SUCCESS;
  response.connection_handle = pw::bytes::ConvertOrderTo(
      cpp20::endian::little, params.connection_handle().Read());
  response.lmp_features = settings_.lmp_features_page0;
  SendEvent(hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
            BufferView(&response, sizeof(response)));
}

void FakeController::OnReadRemoteVersionInfoCommandReceived(
    const pwemb::ReadRemoteVersionInfoCommandView& params) {
  RespondWithCommandStatus(hci_spec::kReadRemoteVersionInfo,
                           pwemb::StatusCode::SUCCESS);
  auto response = hci::EmbossEventPacket::New<
      pwemb::ReadRemoteVersionInfoCompleteEventWriter>(
      hci_spec::kReadRemoteVersionInfoCompleteEventCode);
  auto view = response.view_t();
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.connection_handle().CopyFrom(params.connection_handle());
  view.version().Write(pwemb::CoreSpecificationVersion::V4_2);
  view.company_identifier().Write(0xFFFF);  // anything
  view.subversion().Write(0xADDE);          // anything
  SendCommandChannelPacket(response.data());
}

void FakeController::OnReadRemoteExtendedFeaturesCommandReceived(
    const pwemb::ReadRemoteExtendedFeaturesCommandView& params) {
  auto response = hci::EmbossEventPacket::New<
      pwemb::ReadRemoteExtendedFeaturesCompleteEventWriter>(
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
          pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
    }
  }

  RespondWithCommandStatus(hci_spec::kReadRemoteExtendedFeatures,
                           pwemb::StatusCode::SUCCESS);
  view.page_number().CopyFrom(params.page_number());
  view.max_page_number().Write(3);
  view.connection_handle().CopyFrom(params.connection_handle());
  view.status().Write(pwemb::StatusCode::SUCCESS);
  SendCommandChannelPacket(response.data());
}

void FakeController::OnAuthenticationRequestedCommandReceived(
    const pwemb::AuthenticationRequestedCommandView& params) {
  hci_spec::ConnectionHandle handle = params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kAuthenticationRequested,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kAuthenticationRequested,
                           pwemb::StatusCode::SUCCESS);

  auto event = hci::EmbossEventPacket::New<pwemb::LinkKeyRequestEventWriter>(
      hci_spec::kLinkKeyRequestEventCode);
  event.view_t().bd_addr().CopyFrom(peer->address_.value().view());
  SendCommandChannelPacket(event.data());
}

void FakeController::OnLinkKeyRequestReplyCommandReceived(
    const pwemb::LinkKeyRequestReplyCommandView& params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                             DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLinkKeyRequestReply,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLinkKeyRequestReply,
                           pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kLinkKeyRequestReply,
                             pwemb::StatusCode::SUCCESS);

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    auto event =
        hci::EmbossEventPacket::New<pwemb::AuthenticationCompleteEventWriter>(
            hci_spec::kAuthenticationCompleteEventCode);
    event.view_t().status().Write(pwemb::StatusCode::SUCCESS);
    event.view_t().connection_handle().Write(conn_handle);
    SendCommandChannelPacket(event.data());
  }
}

void FakeController::OnLinkKeyRequestNegativeReplyCommandReceived(
    const pwemb::LinkKeyRequestNegativeReplyCommandView& params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLinkKeyRequestNegativeReply,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }
  RespondWithCommandStatus(hci_spec::kLinkKeyRequestNegativeReply,
                           pwemb::StatusCode::SUCCESS);

  auto event =
      hci::EmbossEventPacket::New<pwemb::IoCapabilityRequestEventWriter>(
          hci_spec::kIOCapabilityRequestEventCode);
  event.view_t().bd_addr().CopyFrom(params.bd_addr());
  SendCommandChannelPacket(event.data());
}

void FakeController::OnIOCapabilityRequestReplyCommand(
    const pwemb::IoCapabilityRequestReplyCommandView& params) {
  RespondWithCommandStatus(hci_spec::kIOCapabilityRequestReply,
                           pwemb::StatusCode::SUCCESS);

  auto io_response =
      hci::EmbossEventPacket::New<pwemb::IoCapabilityResponseEventWriter>(
          hci_spec::kIOCapabilityResponseEventCode);
  io_response.view_t().bd_addr().CopyFrom(params.bd_addr());
  io_response.view_t().io_capability().Write(
      pwemb::IoCapability::NO_INPUT_NO_OUTPUT);
  io_response.view_t().oob_data_present().Write(
      pwemb::GenericPresenceParam::NOT_PRESENT);
  io_response.view_t().authentication_requirements().Write(
      pwemb::AuthenticationRequirements::GENERAL_BONDING);
  SendCommandChannelPacket(io_response.data());

  // Event type based on |params.io_capability| and |io_response.io_capability|.
  hci_spec::UserConfirmationRequestEventParams request = {};
  request.bd_addr = DeviceAddressBytes(params.bd_addr());
  request.numeric_value = 0;
  SendEvent(hci_spec::kUserConfirmationRequestEventCode,
            BufferView(&request, sizeof(request)));
}

void FakeController::OnUserConfirmationRequestReplyCommand(
    const pwemb::UserConfirmationRequestReplyCommandView& params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kUserConfirmationRequestReply,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestReply,
                           pwemb::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = DeviceAddressBytes(params.bd_addr());
  pairing_event.status = pwemb::StatusCode::SUCCESS;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));

  auto link_key_event =
      hci::EmbossEventPacket::New<pwemb::LinkKeyNotificationEventWriter>(
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
      pwemb::KeyType::UNAUTHENTICATED_COMBINATION_FROM_P192);
  SendCommandChannelPacket(link_key_event.data());

  BT_ASSERT(!peer->logical_links().empty());
  for (auto& conn_handle : peer->logical_links()) {
    auto event =
        hci::EmbossEventPacket::New<pwemb::AuthenticationCompleteEventWriter>(
            hci_spec::kAuthenticationCompleteEventCode);
    event.view_t().status().Write(pwemb::StatusCode::SUCCESS);
    event.view_t().connection_handle().Write(conn_handle);
    SendCommandChannelPacket(event.data());
  }
}

void FakeController::OnUserConfirmationRequestNegativeReplyCommand(
    const pwemb::UserConfirmationRequestNegativeReplyCommandView& params) {
  FakePeer* peer = FindPeer(DeviceAddress(
      DeviceAddress::Type::kBREDR, DeviceAddressBytes(params.bd_addr())));
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kUserConfirmationRequestNegativeReply,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kUserConfirmationRequestNegativeReply,
                           pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(hci_spec::kUserConfirmationRequestNegativeReply,
                             pwemb::StatusCode::SUCCESS);

  hci_spec::SimplePairingCompleteEventParams pairing_event;
  pairing_event.bd_addr = DeviceAddressBytes(params.bd_addr());
  pairing_event.status = pwemb::StatusCode::AUTHENTICATION_FAILURE;
  SendEvent(hci_spec::kSimplePairingCompleteEventCode,
            BufferView(&pairing_event, sizeof(pairing_event)));
}

void FakeController::OnSetConnectionEncryptionCommand(
    const pwemb::SetConnectionEncryptionCommandView& params) {
  RespondWithCommandStatus(hci_spec::kSetConnectionEncryption,
                           pwemb::StatusCode::SUCCESS);
  SendEncryptionChangeEvent(
      params.connection_handle().Read(),
      pwemb::StatusCode::SUCCESS,
      pwemb::EncryptionStatus::ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE);
}

void FakeController::OnReadEncryptionKeySizeCommand(
    const pwemb::ReadEncryptionKeySizeCommandView& params) {
  hci_spec::ReadEncryptionKeySizeReturnParams response;
  response.status = pwemb::StatusCode::SUCCESS;
  response.connection_handle = params.connection_handle().Read();
  response.key_size = 16;
  RespondWithCommandComplete(hci_spec::kReadEncryptionKeySize,
                             BufferView(&response, sizeof(response)));
}

void FakeController::OnEnhancedAcceptSynchronousConnectionRequestCommand(
    const pwemb::EnhancedAcceptSynchronousConnectionRequestCommandView&
        params) {
  DeviceAddress peer_address(DeviceAddress::Type::kBREDR,
                             DeviceAddressBytes(params.bd_addr()));
  FakePeer* peer = FindPeer(peer_address);
  if (!peer || !peer->last_connection_request_link_type().has_value()) {
    RespondWithCommandStatus(
        hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
        pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(
      hci_spec::kEnhancedAcceptSynchronousConnectionRequest,
      pwemb::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  auto packet = hci::EmbossEventPacket::New<
      pwemb::SynchronousConnectionCompleteEventWriter>(
      hci_spec::kSynchronousConnectionCompleteEventCode);
  auto view = packet.view_t();
  view.status().Write(pwemb::StatusCode::SUCCESS);
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
    const pwemb::EnhancedSetupSynchronousConnectionCommandView& params) {
  const hci_spec::ConnectionHandle acl_handle =
      params.connection_handle().Read();
  FakePeer* peer = FindByConnHandle(acl_handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kEnhancedSetupSynchronousConnection,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kEnhancedSetupSynchronousConnection,
                           pwemb::StatusCode::SUCCESS);

  hci_spec::ConnectionHandle sco_handle = ++next_conn_handle_;
  peer->AddLink(sco_handle);

  auto packet = hci::EmbossEventPacket::New<
      pwemb::SynchronousConnectionCompleteEventWriter>(
      hci_spec::kSynchronousConnectionCompleteEventCode);
  auto view = packet.view_t();
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.connection_handle().Write(sco_handle);
  view.bd_addr().CopyFrom(peer->address().value().view());
  view.link_type().Write(pwemb::LinkType::ESCO);
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

  const hci_spec::ConnectionHandle handle = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params.connection_handle);
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    RespondWithCommandStatus(hci_spec::kLEReadRemoteFeatures,
                             pwemb::StatusCode::UNKNOWN_CONNECTION_ID);
    return;
  }

  RespondWithCommandStatus(hci_spec::kLEReadRemoteFeatures,
                           pwemb::StatusCode::SUCCESS);

  auto response = hci::EmbossEventPacket::New<
      pwemb::LEReadRemoteFeaturesCompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = response.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode);
  view.connection_handle().Write(params.connection_handle);
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.le_features().BackingStorage().WriteUInt(
      peer->le_features().le_features);
  SendCommandChannelPacket(response.data());
}

void FakeController::OnLEStartEncryptionCommand(
    const pwemb::LEEnableEncryptionCommandView& params) {
  RespondWithCommandStatus(hci_spec::kLEStartEncryption,
                           pwemb::StatusCode::SUCCESS);
  SendEncryptionChangeEvent(
      params.connection_handle().Read(),
      pwemb::StatusCode::SUCCESS,
      pwemb::EncryptionStatus::ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE);
}

void FakeController::OnWriteSynchronousFlowControlEnableCommand(
    const pwemb::WriteSynchronousFlowControlEnableCommandView& params) {
  constexpr size_t flow_control_enable_octet = 10;
  bool supported =
      settings_.supported_commands[flow_control_enable_octet] &
      static_cast<uint8_t>(
          hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  if (!supported) {
    RespondWithCommandComplete(hci_spec::kWriteSynchronousFlowControlEnable,
                               pwemb::StatusCode::UNKNOWN_COMMAND);
    return;
  }
  RespondWithCommandComplete(hci_spec::kWriteSynchronousFlowControlEnable,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnLESetAdvertisingSetRandomAddress(
    const pwemb::LESetAdvertisingSetRandomAddressCommandView& params) {
  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetAdvertisingSetRandomAddress,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "unknown advertising handle (%d), "
           "use HCI_LE_Set_Extended_Advertising_Parameters to create one first",
           handle);
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.properties.connectable && state.enabled) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot set LE random address while connectable advertising enabled");
    RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  state.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom,
                    DeviceAddressBytes(params.random_address()));
  RespondWithCommandComplete(hci_spec::kLESetAdvertisingSetRandomAddress,
                             pwemb::StatusCode::SUCCESS);
}

void FakeController::OnLESetExtendedAdvertisingParameters(
    const pwemb::LESetExtendedAdvertisingParametersV1CommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetExtendedAdvertisingParameters,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // we cannot set parameters while an advertising set is currently enabled
  if (extended_advertising_states_.count(handle) != 0) {
    if (extended_advertising_states_[handle].enabled) {
      bt_log(INFO,
             "fake-hci",
             "cannot set parameters while advertising set is enabled");
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                                 pwemb::StatusCode::COMMAND_DISALLOWED);
      return;
    }
  }

  // ensure we can allocate memory for this advertising set if not already
  // present
  if (extended_advertising_states_.count(handle) == 0 &&
      extended_advertising_states_.size() >= num_supported_advertising_sets()) {
    bt_log(INFO,
           "fake-hci",
           "no available memory for new advertising set, handle: %d",
           handle);
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                               pwemb::StatusCode::MEMORY_CAPACITY_EXCEEDED);
    return;
  }

  // ensure we have a valid bit combination in the advertising event properties
  bool connectable = params.advertising_event_properties().connectable().Read();
  bool scannable = params.advertising_event_properties().scannable().Read();
  bool directed = params.advertising_event_properties().directed().Read();
  bool high_duty_cycle_directed_connectable =
      params.advertising_event_properties()
          .high_duty_cycle_directed_connectable()
          .Read();
  bool use_legacy_pdus =
      params.advertising_event_properties().use_legacy_pdus().Read();
  bool anonymous_advertising =
      params.advertising_event_properties().anonymous_advertising().Read();
  bool include_tx_power =
      params.advertising_event_properties().include_tx_power().Read();

  std::optional<pwemb::LEAdvertisingType> adv_type;
  if (use_legacy_pdus) {
    // ADV_IND
    if (!high_duty_cycle_directed_connectable && !directed && scannable &&
        connectable) {
      adv_type = pwemb::LEAdvertisingType::CONNECTABLE_AND_SCANNABLE_UNDIRECTED;
    }

    // ADV_DIRECT_IND
    if (!high_duty_cycle_directed_connectable && directed && !scannable &&
        connectable) {
      adv_type = pwemb::LEAdvertisingType::CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED;
    }

    // ADV_DIRECT_IND
    if (high_duty_cycle_directed_connectable && directed && !scannable &&
        connectable) {
      adv_type = pwemb::LEAdvertisingType::CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED;
    }

    // ADV_SCAN_IND
    if (!high_duty_cycle_directed_connectable && !directed && scannable &&
        !connectable) {
      adv_type = pwemb::LEAdvertisingType::SCANNABLE_UNDIRECTED;
    }

    // ADV_NONCONN_IND
    if (!high_duty_cycle_directed_connectable && !directed && !scannable &&
        !connectable) {
      adv_type = pwemb::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED;
    }

    if (!adv_type) {
      bt_log(INFO,
             "fake-hci",
             "invalid bit combination: %d",
             params.advertising_event_properties().BackingStorage().ReadUInt());
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingParameters,
          pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
    }

    // Core spec Volume 4, Part E, Section 7.8.53: if legacy advertising PDUs
    // are being used, the Primary_Advertising_PHY shall indicate the LE 1M PHY.
    if (params.primary_advertising_phy().Read() !=
        pwemb::LEPrimaryAdvertisingPHY::LE_1M) {
      bt_log(INFO,
             "fake-hci",
             "only legacy pdus are supported, requires advertising on 1M PHY");
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingParameters,
          pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
      return;
    }
  } else {
    // Core spec Volume 4, Part E, Section 7.8.53: If extended advertising PDU
    // types are being used (bit 4 = 0) then: The advertisement shall not be
    // both connectable and scannable.
    if (connectable && scannable) {
      bt_log(
          INFO,
          "fake-hci",
          "extended advertising pdus can't be both connectable and scannable");
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingParameters,
          pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
    }

    // Core spec Volume 4, Part E, Section 7.8.53: If extended advertising PDU
    // types are being used (bit 4 = 0) then: High duty cycle directed
    // connectable advertising (≤ 3.75 ms advertising interval) shall not be
    // used (bit 3 = 0).
    if (high_duty_cycle_directed_connectable) {
      bt_log(INFO,
             "fake-hci",
             "extended advertising pdus can't use the high duty cycle directed "
             "connectable type");
      RespondWithCommandComplete(
          hci_spec::kLESetExtendedAdvertisingParameters,
          pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
      return;
    }
  }

  // In case there is an error below, we want to reject all parameters instead
  // of storing a dead state and taking up an advertising handle. Avoid creating
  // the LEAdvertisingState directly in the map and add it in only once we have
  // made sure all is good.
  LEAdvertisingState state;
  state.properties.connectable = connectable;
  state.properties.scannable = scannable;
  state.properties.directed = directed;
  state.properties.high_duty_cycle_directed_connectable =
      high_duty_cycle_directed_connectable;
  state.properties.use_legacy_pdus = use_legacy_pdus;
  state.properties.anonymous_advertising = anonymous_advertising;
  state.properties.include_tx_power = include_tx_power;

  state.own_address_type = params.own_address_type().Read();
  state.interval_min = params.primary_advertising_interval_min().Read();
  state.interval_max = params.primary_advertising_interval_max().Read();

  if (state.interval_min >= state.interval_max) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) not strictly less than max (%d)",
           state.interval_min,
           state.interval_max);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (state.interval_min < hci_spec::kLEExtendedAdvertisingIntervalMin) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) less than spec min (%dstate.)",
           state.interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    return;
  }

  if (state.interval_max > hci_spec::kLEExtendedAdvertisingIntervalMax) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval max (%d) greater than spec max (%d)",
           state.interval_max,
           hci_spec::kLEAdvertisingIntervalMax);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingParameters,
        pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
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
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
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
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  // write full state back only at the end (we don't have a reference because we
  // only want to write if there are no errors)
  extended_advertising_states_[handle] = state;

  hci_spec::LESetExtendedAdvertisingParametersReturnParams return_params;
  return_params.status = pwemb::StatusCode::SUCCESS;
  return_params.selected_tx_power = hci_spec::kLEAdvertisingTxPowerMax;
  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingParameters,
                             BufferView(&return_params, sizeof(return_params)));
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedAdvertisingData(
    const pwemb::LESetExtendedAdvertisingDataCommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetExtendedAdvertisingData,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
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
                               pwemb::StatusCode::SUCCESS);
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
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.operation().Read() ==
      pwemb::LESetExtendedAdvDataOp::UNCHANGED_DATA) {
    RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                               pwemb::StatusCode::SUCCESS);
    return;
  }

  // For backwards compatibility with older devices, we support both legacy and
  // extended advertising pdus. Each pdu type has its own size limits.
  if (state.properties.use_legacy_pdus &&
      advertising_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than legacy PDU size limit",
           advertising_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (!state.properties.use_legacy_pdus &&
      advertising_data_length > pwemb::LESetExtendedAdvertisingDataCommand::
                                    advertising_data_length_max()) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than individual extended PDU size "
           "limit",
           advertising_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (!state.properties.use_legacy_pdus &&
      state.data_length + advertising_data_length >
          max_advertising_data_length_) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than total extended PDU size limit",
           advertising_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (state.properties.use_legacy_pdus ||
      params.operation().Read() == pwemb::LESetExtendedAdvDataOp::COMPLETE ||
      params.operation().Read() ==
          pwemb::LESetExtendedAdvDataOp::FIRST_FRAGMENT) {
    std::memcpy(state.data,
                params.advertising_data().BackingStorage().data(),
                advertising_data_length);
    state.data_length = static_cast<uint16_t>(advertising_data_length);
  } else {
    std::memcpy(state.data + state.data_length,
                params.advertising_data().BackingStorage().data(),
                advertising_data_length);
    state.data_length += advertising_data_length;
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingData,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedScanResponseData(
    const pwemb::LESetExtendedScanResponseDataCommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetExtendedScanResponseData,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error
  // conditions
  size_t scan_response_data_length = params.scan_response_data_length().Read();
  if (scan_response_data_length == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               pwemb::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.properties.scannable) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot provide scan response data for unscannable advertising types");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (params.operation().Read() ==
      pwemb::LESetExtendedAdvDataOp::UNCHANGED_DATA) {
    RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                               pwemb::StatusCode::SUCCESS);
    return;
  }

  // For backwards compatibility with older devices, we support both legacy and
  // extended advertising pdus. Each pdu type has its own size limits.
  if (state.properties.use_legacy_pdus &&
      scan_response_data_length > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than legacy PDU size limit",
           scan_response_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (!state.properties.use_legacy_pdus &&
      scan_response_data_length > pwemb::LESetExtendedAdvertisingDataCommand::
                                      advertising_data_length_max()) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than individual extended PDU size "
           "limit",
           scan_response_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (!state.properties.use_legacy_pdus &&
      state.scan_rsp_length + scan_response_data_length >
          max_advertising_data_length_) {
    bt_log(INFO,
           "fake-hci",
           "data length (%zu bytes) larger than total extended PDU size limit",
           scan_response_data_length);
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedScanResponseData,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (state.properties.use_legacy_pdus ||
      params.operation().Read() == pwemb::LESetExtendedAdvDataOp::COMPLETE ||
      params.operation().Read() ==
          pwemb::LESetExtendedAdvDataOp::FIRST_FRAGMENT) {
    std::memcpy(state.scan_rsp_data,
                params.scan_response_data().BackingStorage().data(),
                scan_response_data_length);
    state.scan_rsp_length = static_cast<uint16_t>(scan_response_data_length);
  } else {
    std::memcpy(state.scan_rsp_data + state.scan_rsp_length,
                params.scan_response_data().BackingStorage().data(),
                scan_response_data_length);
    state.scan_rsp_length += scan_response_data_length;
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedScanResponseData,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLESetExtendedAdvertisingEnable(
    const pwemb::LESetExtendedAdvertisingEnableCommandView& params) {
  if (!EnableExtendedAdvertising()) {
    bt_log(
        INFO,
        "fake-hci",
        "extended advertising command rejected, legacy advertising is in use");
    RespondWithCommandStatus(hci_spec::kLESetExtendedAdvertisingEnable,
                             pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

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
            pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
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
            pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
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
            pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
        return;
      }
    }
  }

  if (params.enable().Read() == pwemb::GenericEnableParam::DISABLE) {
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
                               pwemb::StatusCode::SUCCESS);
    NotifyAdvertisingState();
    return;
  }

  // rest of the function deals with enabling advertising for a given set of
  // advertising sets
  BT_ASSERT(params.enable().Read() == pwemb::GenericEnableParam::ENABLE);

  if (num_sets == 0) {
    bt_log(
        INFO, "fake-hci", "cannot enable with an empty advertising set list");
    RespondWithCommandComplete(
        hci_spec::kLESetExtendedAdvertisingEnable,
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
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
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                 pwemb::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    if (state.properties.scannable && state.scan_rsp_length == 0) {
      bt_log(INFO,
             "fake-hci",
             "cannot enable, requires scan response data but hasn't been set");
      RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                                 pwemb::StatusCode::COMMAND_DISALLOWED);
      return;
    }

    // TODO(fxbug.dev/42161900): if own address type is random, check that a
    // random address is set.

    state.enabled = true;
  }

  RespondWithCommandComplete(hci_spec::kLESetExtendedAdvertisingEnable,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadMaximumAdvertisingDataLength() {
  constexpr size_t octet = 36;
  constexpr hci_spec::SupportedCommand command =
      hci_spec::SupportedCommand::kLEReadMaximumAdvertisingDataLength;
  bool supported =
      settings_.supported_commands[octet] & static_cast<uint8_t>(command);
  if (!supported) {
    RespondWithCommandComplete(hci_spec::kLEReadMaximumAdvertisingDataLength,
                               pwemb::StatusCode::UNKNOWN_COMMAND);
  }

  auto response = hci::EmbossEventPacket::New<
      pwemb::LEReadMaximumAdvertisingDataLengthCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = response.view_t();
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.max_advertising_data_length().Write(max_advertising_data_length_);
  RespondWithCommandComplete(hci_spec::kLEReadMaximumAdvertisingDataLength,
                             &response);
}

void FakeController::OnLEReadNumberOfSupportedAdvertisingSets() {
  hci_spec::LEReadNumSupportedAdvertisingSetsReturnParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.num_supported_adv_sets = num_supported_advertising_sets_;
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
        pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);
    RespondWithCommandComplete(
        hci_spec::kLERemoveAdvertisingSet,
        pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    return;
  }

  if (extended_advertising_states_[handle].enabled) {
    bt_log(INFO,
           "fake-hci",
           "cannot remove enabled advertising set (handle: %d)",
           handle);
    RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                               pwemb::StatusCode::COMMAND_DISALLOWED);
    return;
  }

  extended_advertising_states_.erase(handle);
  RespondWithCommandComplete(hci_spec::kLERemoveAdvertisingSet,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEClearAdvertisingSets() {
  for (const auto& element : extended_advertising_states_) {
    if (element.second.enabled) {
      bt_log(INFO,
             "fake-hci",
             "cannot remove currently enabled advertising set (handle: %d)",
             element.second.enabled);
      RespondWithCommandComplete(hci_spec::kLEClearAdvertisingSets,
                                 pwemb::StatusCode::COMMAND_DISALLOWED);
      return;
    }
  }

  extended_advertising_states_.clear();
  RespondWithCommandComplete(hci_spec::kLEClearAdvertisingSets,
                             pwemb::StatusCode::SUCCESS);
  NotifyAdvertisingState();
}

void FakeController::OnLEReadAdvertisingChannelTxPower() {
  if (!respond_to_tx_power_read_) {
    return;
  }

  hci_spec::LEReadAdvertisingChannelTxPowerReturnParams params;
  // Send back arbitrary tx power.
  params.status = pwemb::StatusCode::SUCCESS;
  params.tx_power = 9;
  RespondWithCommandComplete(hci_spec::kLEReadAdvertisingChannelTxPower,
                             BufferView(&params, sizeof(params)));
}

void FakeController::SendLEAdvertisingSetTerminatedEvent(
    hci_spec::ConnectionHandle conn_handle,
    hci_spec::AdvertisingHandle adv_handle) {
  hci_spec::LEAdvertisingSetTerminatedSubeventParams params;
  params.status = pwemb::StatusCode::SUCCESS;
  params.connection_handle = conn_handle;
  params.adv_handle = adv_handle;
  SendLEMetaEvent(hci_spec::kLEAdvertisingSetTerminatedSubeventCode,
                  BufferView(&params, sizeof(params)));
}

void FakeController::SendAndroidLEMultipleAdvertisingStateChangeSubevent(
    hci_spec::ConnectionHandle conn_handle,
    hci_spec::AdvertisingHandle adv_handle) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtStateChangeSubeventWriter>(
      hci_spec::kVendorDebugEventCode);
  auto view = packet.view_t();
  view.vendor_event().subevent_code().Write(
      android_hci::kLEMultiAdvtStateChangeSubeventCode);
  view.advertising_handle().Write(adv_handle);
  view.status().Write(pwemb::StatusCode::SUCCESS);
  view.connection_handle().Write(conn_handle);
  SendCommandChannelPacket(packet.data());
}

void FakeController::OnReadLocalSupportedControllerDelay(
    const pwemb::ReadLocalSupportedControllerDelayCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      pwemb::ReadLocalSupportedControllerDelayCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto response_view = packet.view_t();
  constexpr size_t kReadLocalSupportedControllerDelayOctet = 45;
  if (settings_.supported_commands[kReadLocalSupportedControllerDelayOctet] &
      static_cast<uint8_t>(
          hci_spec::SupportedCommand::kReadLocalSupportedControllerDelay)) {
    response_view.status().Write(pwemb::StatusCode::SUCCESS);
    response_view.min_controller_delay().Write(0);  // no delay
    response_view.max_controller_delay().Write(
        pwemb::ReadLocalSupportedControllerDelayCommandCompleteEvent::
            max_delay_usecs());  // maximum allowable delay
  } else {
    response_view.status().Write(pwemb::StatusCode::UNKNOWN_COMMAND);
  }

  RespondWithCommandComplete(hci_spec::kReadLocalSupportedControllerDelay,
                             &packet);
}

void FakeController::OnCommandPacketReceived(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  hci_spec::OpCode opcode = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, command_packet.header().opcode);

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
  // We use the
  // android_emb::LEGetVendorCapabilitiesCommandCompleteEventWriter as
  // storage. This is the full HCI packet, including the header. Ensure we don't
  // accidentally send the header twice by using the overloaded
  // RespondWithCommandComplete that takes in an EmbossEventPacket. The one that
  // takes a BufferView allocates space for the header, assuming that it's been
  // sent only the payload.
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEGetVendorCapabilitiesCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  MutableBufferView buffer = packet.mutable_data();
  settings_.android_extension_settings.data().Copy(&buffer);
  RespondWithCommandComplete(android_hci::kLEGetVendorCapabilities, &packet);
}

void FakeController::OnAndroidStartA2dpOffload(
    const android_emb::StartA2dpOffloadCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::A2dpOffloadCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(android_emb::A2dpOffloadSubOpcode::START_LEGACY);

  // return in case A2DP offload already started
  if (offloaded_a2dp_channel_state_) {
    view.status().Write(pwemb::StatusCode::CONNECTION_ALREADY_EXISTS);
    RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
    return;
  }

  // SCMS-T is not currently supported
  if (params.scms_t_enable().enabled().Read() ==
      pwemb::GenericEnableParam::ENABLE) {
    view.status().Write(pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
    return;
  }

  // return in case any parameter has an invalid value
  view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);

  android_emb::A2dpCodecType const codec_type = params.codec_type().Read();
  switch (codec_type) {
    case android_emb::A2dpCodecType::SBC:
    case android_emb::A2dpCodecType::AAC:
    case android_emb::A2dpCodecType::APTX:
    case android_emb::A2dpCodecType::APTX_HD:
    case android_emb::A2dpCodecType::LDAC:
      break;
      RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
      return;
  }

  android_emb::A2dpSamplingFrequency const sampling_frequency =
      params.sampling_frequency().Read();
  switch (sampling_frequency) {
    case android_emb::A2dpSamplingFrequency::HZ_44100:
    case android_emb::A2dpSamplingFrequency::HZ_48000:
    case android_emb::A2dpSamplingFrequency::HZ_88200:
    case android_emb::A2dpSamplingFrequency::HZ_96000:
      break;
    default:
      RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
      return;
  }

  android_emb::A2dpBitsPerSample const bits_per_sample =
      static_cast<android_emb::A2dpBitsPerSample>(
          params.bits_per_sample().Read());
  switch (bits_per_sample) {
    case android_emb::A2dpBitsPerSample::BITS_PER_SAMPLE_16:
    case android_emb::A2dpBitsPerSample::BITS_PER_SAMPLE_24:
    case android_emb::A2dpBitsPerSample::BITS_PER_SAMPLE_32:
      break;
    default:
      RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
      return;
  }

  android_emb::A2dpChannelMode const channel_mode =
      static_cast<android_emb::A2dpChannelMode>(params.channel_mode().Read());
  switch (channel_mode) {
    case android_emb::A2dpChannelMode::MONO:
    case android_emb::A2dpChannelMode::STEREO:
      break;
    default:
      RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
      return;
  }

  uint32_t const encoded_audio_bitrate = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params.encoded_audio_bitrate().Read());
  // Bits 0x01000000 to 0xFFFFFFFF are reserved
  if (encoded_audio_bitrate >= 0x01000000) {
    RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
    return;
  }

  OffloadedA2dpChannel state;
  state.codec_type = codec_type;
  state.max_latency = pw::bytes::ConvertOrderFrom(cpp20::endian::little,
                                                  params.max_latency().Read());
  state.scms_t_enable.view().CopyFrom(params.scms_t_enable());
  state.sampling_frequency = sampling_frequency;
  state.bits_per_sample = bits_per_sample;
  state.channel_mode = channel_mode;
  state.encoded_audio_bitrate = encoded_audio_bitrate;
  state.connection_handle = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params.connection_handle().Read());
  state.l2cap_channel_id = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params.l2cap_channel_id().Read());
  state.l2cap_mtu_size = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, params.l2cap_mtu_size().Read());
  offloaded_a2dp_channel_state_ = state;

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
}

void FakeController::OnAndroidStopA2dpOffload() {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::A2dpOffloadCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(android_emb::A2dpOffloadSubOpcode::STOP_LEGACY);

  if (!offloaded_a2dp_channel_state_) {
    view.status().Write(pwemb::StatusCode::REPEATED_ATTEMPTS);
    RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
    return;
  }

  offloaded_a2dp_channel_state_ = std::nullopt;

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kA2dpOffloadCommand, &packet);
}

void FakeController::OnAndroidA2dpOffloadCommand(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case android_hci::kStartA2dpOffloadCommandSubopcode: {
      auto view = android_emb::MakeStartA2dpOffloadCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidStartA2dpOffload(view);
      break;
    }
    case android_hci::kStopA2dpOffloadCommandSubopcode:
      OnAndroidStopA2dpOffload();
      break;
    default:
      bt_log(WARN,
             "fake-hci",
             "unhandled android A2DP offload command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(subopcode, pwemb::StatusCode::UNKNOWN_COMMAND);
      break;
  }
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtParam(
    const android_emb::LEMultiAdvtSetAdvtParamCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(
      android_emb::LEMultiAdvtSubOpcode::SET_ADVERTISING_PARAMETERS);

  hci_spec::AdvertisingHandle handle = params.adv_handle().Read();
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
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

    view.status().Write(pwemb::StatusCode::MEMORY_CAPACITY_EXCEEDED);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  // In case there is an error below, we want to reject all parameters instead
  // of storing a dead state and taking up an advertising handle. Avoid creating
  // the LEAdvertisingState directly in the map and add it in only once we have
  // made sure all is good.
  LEAdvertisingState state;
  state.own_address_type = params.own_addr_type().Read();

  pwemb::LEAdvertisingType adv_type = params.adv_type().Read();
  switch (adv_type) {
    case pwemb::LEAdvertisingType::CONNECTABLE_AND_SCANNABLE_UNDIRECTED:
      state.properties.connectable = true;
      state.properties.scannable = true;
      break;
    case pwemb::LEAdvertisingType::CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED:
      state.properties.directed = true;
      state.properties.connectable = true;
      break;
    case pwemb::LEAdvertisingType::CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED:
      state.properties.high_duty_cycle_directed_connectable = true;
      state.properties.directed = true;
      state.properties.connectable = true;
      break;
    case pwemb::LEAdvertisingType::SCANNABLE_UNDIRECTED:
      state.properties.scannable = true;
      break;
    case pwemb::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED:
      break;
  }

  state.interval_min = params.adv_interval_min().Read();
  state.interval_max = params.adv_interval_max().Read();

  if (state.interval_min >= state.interval_max) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) not strictly less than max (%d)",
           state.interval_min,
           state.interval_max);

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (state.interval_min < hci_spec::kLEAdvertisingIntervalMin) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval min (%d) less than spec min (%d)",
           state.interval_min,
           hci_spec::kLEAdvertisingIntervalMin);
    view.status().Write(pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (state.interval_max > hci_spec::kLEAdvertisingIntervalMax) {
    bt_log(INFO,
           "fake-hci",
           "advertising interval max (%d) greater than spec max (%d)",
           state.interval_max,
           hci_spec::kLEAdvertisingIntervalMax);
    view.status().Write(pwemb::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  // write full state back only at the end (we don't have a reference because we
  // only want to write if there are no errors)
  extended_advertising_states_[handle] = state;

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetAdvtData(
    const android_emb::LEMultiAdvtSetAdvtDataCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(
      android_emb::LEMultiAdvtSubOpcode::SET_ADVERTISING_DATA);

  hci_spec::AdvertisingHandle handle = params.adv_handle().Read();
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    view.status().Write(pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing advertising data entirely doesn't require us to check for error
  // conditions
  if (params.adv_data_length().Read() == 0) {
    state.data_length = 0;
    std::memset(state.data, 0, sizeof(state.data));
    view.status().Write(pwemb::StatusCode::SUCCESS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    NotifyAdvertisingState();
    return;
  }

  // directed advertising doesn't support advertising data
  if (state.IsDirectedAdvertising()) {
    bt_log(INFO,
           "fake-hci",
           "cannot provide advertising data when using directed advertising");

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (params.adv_data_length().Read() > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%d bytes) larger than legacy PDU size limit",
           params.adv_data_length().Read());

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  state.data_length = params.adv_data_length().Read();
  std::memcpy(state.data,
              params.adv_data().BackingStorage().data(),
              params.adv_data_length().Read());

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetScanResp(
    const android_emb::LEMultiAdvtSetScanRespDataCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(
      android_emb::LEMultiAdvtSubOpcode::SET_SCAN_RESPONSE_DATA);

  hci_spec::AdvertisingHandle handle = params.adv_handle().Read();
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    view.status().Write(pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];

  // removing scan response data entirely doesn't require us to check for error
  // conditions
  if (params.scan_resp_length().Read() == 0) {
    state.scan_rsp_length = 0;
    std::memset(state.scan_rsp_data, 0, sizeof(state.scan_rsp_data));

    view.status().Write(pwemb::StatusCode::SUCCESS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    NotifyAdvertisingState();
    return;
  }

  // adding or changing scan response data, check for error conditions
  if (!state.properties.scannable) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot provide scan response data for unscannable advertising types");

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (params.scan_resp_length().Read() >
      hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(INFO,
           "fake-hci",
           "data length (%d bytes) larger than legacy PDU size limit",
           params.scan_resp_length().Read());

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  state.scan_rsp_length = params.scan_resp_length().Read();
  std::memcpy(state.scan_rsp_data,
              params.adv_data().BackingStorage().data(),
              params.scan_resp_length().Read());

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvtSetRandomAddr(
    const android_emb::LEMultiAdvtSetRandomAddrCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(
      android_emb::LEMultiAdvtSubOpcode::SET_RANDOM_ADDRESS);

  hci_spec::AdvertisingHandle handle = params.adv_handle().Read();
  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    view.status().Write(pwemb::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  if (extended_advertising_states_.count(handle) == 0) {
    bt_log(INFO,
           "fake-hci",
           "advertising handle (%d) maps to an unknown advertising set",
           handle);

    view.status().Write(pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  LEAdvertisingState& state = extended_advertising_states_[handle];
  if (state.properties.connectable && state.enabled) {
    bt_log(
        INFO,
        "fake-hci",
        "cannot set LE random address while connectable advertising enabled");

    view.status().Write(pwemb::StatusCode::COMMAND_DISALLOWED);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  state.random_address =
      DeviceAddress(DeviceAddress::Type::kLERandom,
                    DeviceAddressBytes(params.peer_address()));

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
}

void FakeController::OnAndroidLEMultiAdvtEnable(
    const android_emb::LEMultiAdvtEnableCommandView& params) {
  auto packet = hci::EmbossEventPacket::New<
      android_emb::LEMultiAdvtCommandCompleteEventWriter>(
      hci_spec::kCommandCompleteEventCode);
  auto view = packet.view_t();
  view.sub_opcode().Write(android_emb::LEMultiAdvtSubOpcode::ENABLE);

  hci_spec::AdvertisingHandle handle = params.advertising_handle().Read();

  if (!IsValidAdvertisingHandle(handle)) {
    bt_log(ERROR, "fake-hci", "advertising handle outside range: %d", handle);

    view.status().Write(pwemb::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER);
    RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
    return;
  }

  bool enabled = false;
  if (params.enable().Read() == pwemb::GenericEnableParam::ENABLE) {
    enabled = true;
  }

  extended_advertising_states_[handle].enabled = enabled;

  view.status().Write(pwemb::StatusCode::SUCCESS);
  RespondWithCommandComplete(android_hci::kLEMultiAdvt, &packet);
  NotifyAdvertisingState();
}

void FakeController::OnAndroidLEMultiAdvt(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  const auto& payload = command_packet.payload_data();

  uint8_t subopcode = payload.To<uint8_t>();
  switch (subopcode) {
    case android_hci::kLEMultiAdvtSetAdvtParamSubopcode: {
      auto params = android_emb::MakeLEMultiAdvtSetAdvtParamCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidLEMultiAdvtSetAdvtParam(params);
      break;
    }
    case android_hci::kLEMultiAdvtSetAdvtDataSubopcode: {
      auto params = android_emb::MakeLEMultiAdvtSetAdvtDataCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidLEMultiAdvtSetAdvtData(params);
      break;
    }
    case android_hci::kLEMultiAdvtSetScanRespSubopcode: {
      auto params = android_emb::MakeLEMultiAdvtSetScanRespDataCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidLEMultiAdvtSetScanResp(params);
      break;
    }
    case android_hci::kLEMultiAdvtSetRandomAddrSubopcode: {
      auto params = android_emb::MakeLEMultiAdvtSetRandomAddrCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidLEMultiAdvtSetRandomAddr(params);
      break;
    }
    case android_hci::kLEMultiAdvtEnableSubopcode: {
      auto view = android_emb::MakeLEMultiAdvtEnableCommandView(
          command_packet.data().data(), command_packet.data().size());
      OnAndroidLEMultiAdvtEnable(view);
      break;
    }
    default: {
      bt_log(WARN,
             "fake-hci",
             "unhandled android multiple advertising command, subopcode: %#.4x",
             subopcode);
      RespondWithCommandComplete(subopcode, pwemb::StatusCode::UNKNOWN_COMMAND);
      break;
    }
  }
}

void FakeController::OnVendorCommand(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  auto opcode = pw::bytes::ConvertOrderFrom(cpp20::endian::little,
                                            command_packet.header().opcode);

  switch (opcode) {
    case android_hci::kLEGetVendorCapabilities:
      OnAndroidLEGetVendorCapabilities();
      break;
    case android_hci::kA2dpOffloadCommand:
      OnAndroidA2dpOffloadCommand(command_packet);
      break;
    case android_hci::kLEMultiAdvt:
      OnAndroidLEMultiAdvt(command_packet);
      break;
    default:
      bt_log(WARN,
             "fake-hci",
             "received unhandled vendor command with opcode: %#.4x",
             opcode);
      RespondWithCommandComplete(opcode, pwemb::StatusCode::UNKNOWN_COMMAND);
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
      pw::bytes::ConvertOrderFrom(cpp20::endian::little,
                                  header.handle_and_flags) &
      0x0FFFF;
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
      pw::bytes::ConvertOrderFrom(cpp20::endian::little,
                                  header.handle_and_flags) &
      0x0FFFF;
  FakePeer* peer = FindByConnHandle(handle);
  if (!peer) {
    bt_log(WARN, "fake-hci", "SCO data received for unknown handle!");
    return;
  }

  if (auto_completed_packets_event_enabled_) {
    SendNumberOfCompletedPacketsEvent(handle, 1);
  }
}

void FakeController::OnIsoDataPacketReceived(
    const ByteBuffer& iso_data_packet) {
  if (iso_data_callback_) {
    iso_data_callback_(iso_data_packet);
  }

  if (iso_data_packet.size() < pwemb::IsoDataFrameHeader::MinSizeInBytes()) {
    bt_log(WARN, "fake-hci", "malformed ISO packet!");
    return;
  }

  auto iso_header_view = pwemb::MakeIsoDataFrameHeaderView(
      iso_data_packet.data(), iso_data_packet.size());
  hci_spec::ConnectionHandle handle =
      iso_header_view.connection_handle().Read();

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
  return properties.directed || properties.high_duty_cycle_directed_connectable;
}

bool FakeController::EnableLegacyAdvertising() {
  if (advertising_procedure() == AdvertisingProcedure::kExtended) {
    return false;
  }

  advertising_procedure_ = AdvertisingProcedure::kLegacy;
  return true;
}

bool FakeController::EnableExtendedAdvertising() {
  if (advertising_procedure() == AdvertisingProcedure::kLegacy) {
    return false;
  }

  advertising_procedure_ = AdvertisingProcedure::kExtended;
  return true;
}

void FakeController::HandleReceivedCommandPacket(
    const PacketView<hci_spec::CommandHeader>& command_packet) {
  hci_spec::OpCode opcode = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little, command_packet.header().opcode);

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

  // TODO(fxbug.dev/42175513): Validate size of payload to be the correct length
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
    case hci_spec::kLEReadBufferSizeV2: {
      OnLEReadBufferSizeV2();
      break;
    }
    case hci_spec::kReset: {
      OnReset();
      break;
    }
    case hci_spec::kLinkKeyRequestReply: {
      const auto& params =
          command_packet.payload<pwemb::LinkKeyRequestReplyCommandView>();
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
    case hci_spec::kLEExtendedCreateConnection:
    case hci_spec::kLEReadMaximumAdvertisingDataLength:
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
    case hci_spec::kReadLocalSupportedControllerDelay:
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
      auto emboss_packet =
          bt::hci::EmbossCommandPacket::New<pwemb::CommandHeaderView>(
              opcode, command_packet.size());
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
      RespondWithCommandComplete(opcode, pwemb::StatusCode::UNKNOWN_COMMAND);
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
    RespondWithCommandComplete(opcode, pwemb::StatusCode::UNKNOWN_COMMAND);
    return;
  }

  switch (opcode) {
    case hci_spec::kInquiry: {
      auto params = command_packet.view<pwemb::InquiryCommandView>();
      OnInquiry(params);
      break;
    }
    case hci_spec::kEnhancedAcceptSynchronousConnectionRequest: {
      auto params = command_packet.view<
          pwemb::EnhancedAcceptSynchronousConnectionRequestCommandView>();
      OnEnhancedAcceptSynchronousConnectionRequestCommand(params);
      break;
    }
    case hci_spec::kEnhancedSetupSynchronousConnection: {
      auto params =
          command_packet
              .view<pwemb::EnhancedSetupSynchronousConnectionCommandView>();
      OnEnhancedSetupSynchronousConnectionCommand(params);
      break;
    }
    case hci_spec::kCreateConnection: {
      const auto params =
          command_packet.view<pwemb::CreateConnectionCommandView>();
      OnCreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kDisconnect: {
      const auto params = command_packet.view<pwemb::DisconnectCommandView>();
      OnDisconnectCommandReceived(params);
      break;
    }
    case hci_spec::kLESetAdvertisingEnable: {
      const auto params =
          command_packet.view<pwemb::LESetAdvertisingEnableCommandView>();
      OnLESetAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingEnable: {
      const auto params =
          command_packet
              .view<pwemb::LESetExtendedAdvertisingEnableCommandView>();
      OnLESetExtendedAdvertisingEnable(params);
      break;
    }
    case hci_spec::kLinkKeyRequestNegativeReply: {
      const auto params =
          command_packet.view<pwemb::LinkKeyRequestNegativeReplyCommandView>();
      OnLinkKeyRequestNegativeReplyCommandReceived(params);
      break;
    }
    case hci_spec::kAuthenticationRequested: {
      const auto params =
          command_packet.view<pwemb::AuthenticationRequestedCommandView>();
      OnAuthenticationRequestedCommandReceived(params);
      break;
    }
    case hci_spec::kSetConnectionEncryption: {
      const auto params =
          command_packet.view<pwemb::SetConnectionEncryptionCommandView>();
      OnSetConnectionEncryptionCommand(params);
      break;
    }
    case hci_spec::kRemoteNameRequest: {
      const auto params =
          command_packet.view<pwemb::RemoteNameRequestCommandView>();
      OnReadRemoteNameRequestCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteSupportedFeatures: {
      const auto params =
          command_packet.view<pwemb::ReadRemoteSupportedFeaturesCommandView>();
      OnReadRemoteSupportedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteExtendedFeatures: {
      const auto params =
          command_packet.view<pwemb::ReadRemoteExtendedFeaturesCommandView>();
      OnReadRemoteExtendedFeaturesCommandReceived(params);
      break;
    }
    case hci_spec::kReadRemoteVersionInfo: {
      const auto params =
          command_packet.view<pwemb::ReadRemoteVersionInfoCommandView>();
      OnReadRemoteVersionInfoCommandReceived(params);
      break;
    }
    case hci_spec::kIOCapabilityRequestReply: {
      const auto params =
          command_packet.view<pwemb::IoCapabilityRequestReplyCommandView>();
      OnIOCapabilityRequestReplyCommand(params);
      break;
    }
    case hci_spec::kSetEventMask: {
      const auto params = command_packet.view<pwemb::SetEventMaskCommandView>();
      OnSetEventMask(params);
      break;
    }
    case hci_spec::kWriteLocalName: {
      const auto params =
          command_packet.view<pwemb::WriteLocalNameCommandView>();
      OnWriteLocalName(params);
      break;
    }
    case hci_spec::kWriteScanEnable: {
      const auto& params =
          command_packet.view<pwemb::WriteScanEnableCommandView>();
      OnWriteScanEnable(params);
      break;
    }
    case hci_spec::kWritePageScanActivity: {
      const auto& params =
          command_packet.view<pwemb::WritePageScanActivityCommandView>();
      OnWritePageScanActivity(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestReply: {
      const auto& params =
          command_packet.view<pwemb::UserConfirmationRequestReplyCommandView>();
      OnUserConfirmationRequestReplyCommand(params);
      break;
    }
    case hci_spec::kUserConfirmationRequestNegativeReply: {
      const auto& params =
          command_packet
              .view<pwemb::UserConfirmationRequestNegativeReplyCommandView>();
      OnUserConfirmationRequestNegativeReplyCommand(params);
      break;
    }
    case hci_spec::kWriteSynchronousFlowControlEnable: {
      const auto& params =
          command_packet
              .view<pwemb::WriteSynchronousFlowControlEnableCommandView>();
      OnWriteSynchronousFlowControlEnableCommand(params);
      break;
    }
    case hci_spec::kWriteExtendedInquiryResponse: {
      const auto& params =
          command_packet.view<pwemb::WriteExtendedInquiryResponseCommandView>();
      OnWriteExtendedInquiryResponse(params);
      break;
    }
    case hci_spec::kWriteSimplePairingMode: {
      const auto& params =
          command_packet.view<pwemb::WriteSimplePairingModeCommandView>();
      OnWriteSimplePairingMode(params);
      break;
    }
    case hci_spec::kWriteClassOfDevice: {
      const auto& params =
          command_packet.view<pwemb::WriteClassOfDeviceCommandView>();
      OnWriteClassOfDevice(params);
      break;
    }
    case hci_spec::kWriteInquiryMode: {
      const auto& params =
          command_packet.view<pwemb::WriteInquiryModeCommandView>();
      OnWriteInquiryMode(params);
      break;
    };
    case hci_spec::kWritePageScanType: {
      const auto& params =
          command_packet.view<pwemb::WritePageScanTypeCommandView>();
      OnWritePageScanType(params);
      break;
    }
    case hci_spec::kWriteLEHostSupport: {
      const auto& params =
          command_packet.view<pwemb::WriteLEHostSupportCommandView>();
      OnWriteLEHostSupportCommandReceived(params);
      break;
    }
    case hci_spec::kWriteSecureConnectionsHostSupport: {
      const auto& params =
          command_packet
              .view<pwemb::WriteSecureConnectionsHostSupportCommandView>();
      OnWriteSecureConnectionsHostSupport(params);
      break;
    }
    case hci_spec::kReadEncryptionKeySize: {
      const auto& params =
          command_packet.view<pwemb::ReadEncryptionKeySizeCommandView>();
      OnReadEncryptionKeySizeCommand(params);
      break;
    }
    case hci_spec::kLESetEventMask: {
      const auto& params =
          command_packet.view<pwemb::LESetEventMaskCommandView>();
      OnLESetEventMask(params);
      break;
    }
    case hci_spec::kLESetRandomAddress: {
      const auto& params =
          command_packet.view<pwemb::LESetRandomAddressCommandView>();
      OnLESetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetAdvertisingData: {
      const auto& params =
          command_packet.view<pwemb::LESetAdvertisingDataCommandView>();
      OnLESetAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetScanResponseData: {
      const auto& params =
          command_packet.view<pwemb::LESetScanResponseDataCommandView>();
      OnLESetScanResponseData(params);
      break;
    }
    case hci_spec::kLESetScanParameters: {
      const auto& params =
          command_packet.view<pwemb::LESetScanParametersCommandView>();
      OnLESetScanParameters(params);
      break;
    }
    case hci_spec::kLESetExtendedScanParameters: {
      const auto& params =
          command_packet.view<pwemb::LESetExtendedScanParametersCommandView>();
      OnLESetExtendedScanParameters(params);
      break;
    }
    case hci_spec::kLESetScanEnable: {
      const auto& params =
          command_packet.view<pwemb::LESetScanEnableCommandView>();
      OnLESetScanEnable(params);
      break;
    }
    case hci_spec::kLESetExtendedScanEnable: {
      const auto& params =
          command_packet.view<pwemb::LESetExtendedScanEnableCommandView>();
      OnLESetExtendedScanEnable(params);
      break;
    }
    case hci_spec::kLECreateConnection: {
      const auto& params =
          command_packet.view<pwemb::LECreateConnectionCommandView>();
      OnLECreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kLEExtendedCreateConnection: {
      const auto& params =
          command_packet.view<pwemb::LEExtendedCreateConnectionCommandV1View>();
      OnLEExtendedCreateConnectionCommandReceived(params);
      break;
    }
    case hci_spec::kLEConnectionUpdate: {
      const auto& params =
          command_packet.view<pwemb::LEConnectionUpdateCommandView>();
      OnLEConnectionUpdateCommandReceived(params);
      break;
    }
    case hci_spec::kLEStartEncryption: {
      const auto& params =
          command_packet.view<pwemb::LEEnableEncryptionCommandView>();
      OnLEStartEncryptionCommand(params);
      break;
    }
    case hci_spec::kReadLocalExtendedFeatures: {
      const auto& params =
          command_packet.view<pwemb::ReadLocalExtendedFeaturesCommandView>();
      OnReadLocalExtendedFeatures(params);
      break;
    }
    case hci_spec::kLESetAdvertisingParameters: {
      const auto& params =
          command_packet.view<pwemb::LESetAdvertisingParametersCommandView>();
      OnLESetAdvertisingParameters(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingData: {
      const auto& params =
          command_packet.view<pwemb::LESetExtendedAdvertisingDataCommandView>();
      OnLESetExtendedAdvertisingData(params);
      break;
    }
    case hci_spec::kLESetExtendedScanResponseData: {
      const auto& params =
          command_packet
              .view<pwemb::LESetExtendedScanResponseDataCommandView>();
      OnLESetExtendedScanResponseData(params);
      break;
    }
    case hci_spec::kLEReadMaximumAdvertisingDataLength: {
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
      const auto& params =
          command_packet
              .view<pwemb::LESetAdvertisingSetRandomAddressCommandView>();
      OnLESetAdvertisingSetRandomAddress(params);
      break;
    }
    case hci_spec::kLESetExtendedAdvertisingParameters: {
      const auto& params =
          command_packet
              .view<pwemb::LESetExtendedAdvertisingParametersV1CommandView>();
      OnLESetExtendedAdvertisingParameters(params);
      break;
    }
    case hci_spec::kReadLocalSupportedControllerDelay: {
      const auto& params =
          command_packet
              .view<pwemb::ReadLocalSupportedControllerDelayCommandView>();
      OnReadLocalSupportedControllerDelay(params);
      break;
    }
    default: {
      bt_log(WARN, "fake-hci", "opcode: %#.4x", opcode);
      break;
    }
  }
}
}  // namespace bt::testing
