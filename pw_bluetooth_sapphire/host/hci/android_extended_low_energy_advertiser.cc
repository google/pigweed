// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/android_extended_low_energy_advertiser.h"

#include <pw_bluetooth/hci_common.emb.h>
#include <pw_bluetooth/hci_vendor.emb.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

constexpr int8_t kTransmitPower =
    -25;  // Android range -70 to +20, select the middle for now

namespace hci_android = hci_spec::vendor::android;

AndroidExtendedLowEnergyAdvertiser::AndroidExtendedLowEnergyAdvertiser(
    hci::Transport::WeakPtr hci_ptr, uint8_t max_advertisements)
    : LowEnergyAdvertiser(std::move(hci_ptr)),
      max_advertisements_(max_advertisements),
      advertising_handle_map_(max_advertisements_),
      weak_self_(this) {
  auto self = weak_self_.GetWeakPtr();
  state_changed_event_handler_id_ =
      hci()->command_channel()->AddVendorEventHandler(
          hci_android::kLEMultiAdvtStateChangeSubeventCode,
          [self](const EmbossEventPacket& event_packet) {
            if (self.is_alive()) {
              return self->OnAdvertisingStateChangedSubevent(event_packet);
            }

            return CommandChannel::EventCallbackResult::kRemove;
          });
}

AndroidExtendedLowEnergyAdvertiser::~AndroidExtendedLowEnergyAdvertiser() {
  // This object is probably being destroyed because the stack is shutting down,
  // in which case the HCI layer may have already been destroyed.
  if (!hci().is_alive() || !hci()->command_channel()) {
    return;
  }
  hci()->command_channel()->RemoveEventHandler(state_changed_event_handler_id_);
  // TODO(fxbug.dev/112157): This will only cancel one advertisement, after
  // which the SequentialCommandRunner will have been destroyed and no further
  // commands will be sent.
  StopAdvertising();
}

std::optional<EmbossCommandPacket>
AndroidExtendedLowEnergyAdvertiser::BuildEnablePacket(
    const DeviceAddress& address,
    pw::bluetooth::emboss::GenericEnableParam enable) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::vendor::android_hci::LEMultiAdvtEnableCommandWriter>(
      hci_android::kLEMultiAdvt);
  auto packet_view = packet.view_t();
  packet_view.vendor_command().sub_opcode().Write(
      hci_android::kLEMultiAdvtEnableSubopcode);
  packet_view.enable().Write(enable);
  packet_view.advertising_handle().Write(handle.value());
  return packet;
}

CommandChannel::CommandPacketVariant
AndroidExtendedLowEnergyAdvertiser::BuildSetAdvertisingParams(
    const DeviceAddress& address,
    pw::bluetooth::emboss::LEAdvertisingType type,
    pw::bluetooth::emboss::LEOwnAddressType own_address_type,
    AdvertisingIntervalRange interval) {
  std::unique_ptr<CommandPacket> packet = CommandPacket::New(
      hci_android::kLEMultiAdvt,
      sizeof(hci_android::LEMultiAdvtSetAdvtParamCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<
      hci_android::LEMultiAdvtSetAdvtParamCommandParams>();

  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.MapHandle(address);
  if (!handle) {
    bt_log(WARN,
           "hci-le",
           "could not (al)locate advertising handle for address: %s",
           bt_str(address));
    return std::unique_ptr<CommandPacket>();
  }

  payload->opcode = hci_android::kLEMultiAdvtSetAdvtParamSubopcode;
  payload->adv_interval_min = htole16(interval.min());
  payload->adv_interval_max = htole16(interval.max());
  payload->adv_type = type;
  payload->own_address_type = own_address_type;
  payload->adv_channel_map = hci_spec::kLEAdvertisingChannelAll;
  payload->adv_filter_policy = hci_spec::LEAdvFilterPolicy::kAllowAll;
  payload->adv_handle = handle.value();
  payload->adv_tx_power = hci_spec::kLEExtendedAdvertisingTxPowerNoPreference;

  // We don't support directed advertising yet, so leave peer_address and
  // peer_address_type as 0x00
  // (|packet| parameters are initialized to zero above).

  return packet;
}

CommandChannel::CommandPacketVariant
AndroidExtendedLowEnergyAdvertiser::BuildSetAdvertisingData(
    const DeviceAddress& address, const AdvertisingData& data, AdvFlags flags) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  std::unique_ptr<CommandPacket> packet = CommandPacket::New(
      hci_android::kLEMultiAdvt,
      sizeof(hci_android::LEMultiAdvtSetAdvtDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload =
      packet
          ->mutable_payload<hci_android::LEMultiAdvtSetAdvtDataCommandParams>();

  payload->opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
  payload->adv_data_length =
      static_cast<uint8_t>(data.CalculateBlockSize(/*include_flags=*/true));
  payload->adv_handle = handle.value();

  MutableBufferView data_view(payload->adv_data, payload->adv_data_length);
  data.WriteBlock(&data_view, flags);

  return packet;
}

CommandChannel::CommandPacketVariant
AndroidExtendedLowEnergyAdvertiser::BuildUnsetAdvertisingData(
    const DeviceAddress& address) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  std::unique_ptr<CommandPacket> packet = CommandPacket::New(
      hci_android::kLEMultiAdvt,
      sizeof(hci_android::LEMultiAdvtSetAdvtDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload =
      packet
          ->mutable_payload<hci_android::LEMultiAdvtSetAdvtDataCommandParams>();

  payload->opcode = hci_android::kLEMultiAdvtSetAdvtDataSubopcode;
  payload->adv_data_length = 0;
  payload->adv_handle = handle.value();

  return packet;
}

CommandChannel::CommandPacketVariant
AndroidExtendedLowEnergyAdvertiser::BuildSetScanResponse(
    const DeviceAddress& address, const AdvertisingData& scan_rsp) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  std::unique_ptr<CommandPacket> packet = CommandPacket::New(
      hci_android::kLEMultiAdvt,
      sizeof(hci_android::LEMultiAdvtSetScanRespCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload =
      packet
          ->mutable_payload<hci_android::LEMultiAdvtSetScanRespCommandParams>();

  payload->opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
  payload->scan_rsp_data_length =
      static_cast<uint8_t>(scan_rsp.CalculateBlockSize());
  payload->adv_handle = handle.value();

  MutableBufferView scan_rsp_view(payload->scan_rsp_data,
                                  payload->scan_rsp_data_length);
  scan_rsp.WriteBlock(&scan_rsp_view, std::nullopt);
  return packet;
}

CommandChannel::CommandPacketVariant
AndroidExtendedLowEnergyAdvertiser::BuildUnsetScanResponse(
    const DeviceAddress& address) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  std::unique_ptr<CommandPacket> packet = CommandPacket::New(
      hci_android::kLEMultiAdvt,
      sizeof(hci_android::LEMultiAdvtSetScanRespCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload =
      packet
          ->mutable_payload<hci_android::LEMultiAdvtSetScanRespCommandParams>();

  payload->opcode = hci_android::kLEMultiAdvtSetScanRespSubopcode;
  payload->scan_rsp_data_length = 0;
  payload->adv_handle = handle.value();

  return packet;
}

std::optional<EmbossCommandPacket>
AndroidExtendedLowEnergyAdvertiser::BuildRemoveAdvertisingSet(
    const DeviceAddress& address) {
  std::optional<hci_spec::AdvertisingHandle> handle =
      advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::vendor::android_hci::LEMultiAdvtEnableCommandWriter>(
      hci_android::kLEMultiAdvt);
  auto packet_view = packet.view_t();
  packet_view.vendor_command().sub_opcode().Write(
      hci_android::kLEMultiAdvtEnableSubopcode);
  packet_view.enable().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  packet_view.advertising_handle().Write(handle.value());
  return packet;
}

void AndroidExtendedLowEnergyAdvertiser::StartAdvertising(
    const DeviceAddress& address,
    const AdvertisingData& data,
    const AdvertisingData& scan_rsp,
    AdvertisingOptions options,
    ConnectionCallback connect_callback,
    ResultFunction<> result_callback) {
  AdvertisingData copied_data;
  data.Copy(&copied_data);

  AdvertisingData copied_scan_rsp;
  scan_rsp.Copy(&copied_scan_rsp);

  // if there is an operation currently in progress, enqueue this operation and
  // we will get to it the next time we have a chance
  if (!hci_cmd_runner().IsReady()) {
    bt_log(INFO,
           "hci-le",
           "hci cmd runner not ready, queuing advertisement commands for now");

    op_queue_.push([this,
                    address,
                    data = std::move(copied_data),
                    scan_rsp = std::move(copied_scan_rsp),
                    options,
                    conn_cb = std::move(connect_callback),
                    result_cb = std::move(result_callback)]() mutable {
      StartAdvertising(address,
                       data,
                       scan_rsp,
                       options,
                       std::move(conn_cb),
                       std::move(result_cb));
    });

    return;
  }

  fit::result<HostError> result =
      CanStartAdvertising(address, data, scan_rsp, options);
  if (result.is_error()) {
    result_callback(ToResult(result.error_value()));
    return;
  }

  if (IsAdvertising(address)) {
    bt_log(DEBUG,
           "hci-le",
           "updating existing advertisement for %s",
           bt_str(address));
  }

  if (options.include_tx_power_level) {
    copied_data.SetTxPower(kTransmitPower);
    copied_scan_rsp.SetTxPower(kTransmitPower);
  }

  StartAdvertisingInternal(address,
                           copied_data,
                           copied_scan_rsp,
                           options.interval,
                           options.flags,
                           std::move(connect_callback),
                           std::move(result_callback));
}

void AndroidExtendedLowEnergyAdvertiser::StopAdvertising() {
  LowEnergyAdvertiser::StopAdvertising();
  advertising_handle_map_.Clear();

  // std::queue doesn't have a clear method so we have to resort to this
  // tomfoolery :(
  decltype(op_queue_) empty;
  std::swap(op_queue_, empty);
}

void AndroidExtendedLowEnergyAdvertiser::StopAdvertising(
    const DeviceAddress& address) {
  // if there is an operation currently in progress, enqueue this operation and
  // we will get to it the next time we have a chance
  if (!hci_cmd_runner().IsReady()) {
    bt_log(
        INFO,
        "hci-le",
        "hci cmd runner not ready, queueing stop advertising command for now");
    op_queue_.push([this, address]() { StopAdvertising(address); });
    return;
  }

  LowEnergyAdvertiser::StopAdvertisingInternal(address);
  advertising_handle_map_.RemoveAddress(address);
}

void AndroidExtendedLowEnergyAdvertiser::OnIncomingConnection(
    hci_spec::ConnectionHandle handle,
    pw::bluetooth::emboss::ConnectionRole role,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& conn_params) {
  staged_connections_map_[handle] = {role, peer_address, conn_params};
}

// The LE multi-advertising state change subevent contains the mapping between
// connection handle and advertising handle. After the LE multi-advertising
// state change subevent, we have all the information necessary to create a
// connection object within the Host layer.
CommandChannel::EventCallbackResult
AndroidExtendedLowEnergyAdvertiser::OnAdvertisingStateChangedSubevent(
    const EmbossEventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kVendorDebugEventCode);
  BT_ASSERT(event.view<pw::bluetooth::emboss::VendorDebugEventView>()
                .subevent_code()
                .Read() == hci_android::kLEMultiAdvtStateChangeSubeventCode);

  Result<> result = event.ToResult();
  if (bt_is_error(result,
                  ERROR,
                  "hci-le",
                  "advertising state change event, error received %s",
                  bt_str(result))) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  auto view = event.view<
      pw::bluetooth::vendor::android_hci::LEMultiAdvtStateChangeSubeventView>();

  hci_spec::ConnectionHandle connection_handle =
      view.connection_handle().Read();
  auto staged_parameters_node =
      staged_connections_map_.extract(connection_handle);

  if (staged_parameters_node.empty()) {
    bt_log(ERROR,
           "hci-le",
           "advertising state change event, staged params not available "
           "(handle: %d)",
           view.advertising_handle().Read());
    return CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::AdvertisingHandle adv_handle = view.advertising_handle().Read();
  std::optional<DeviceAddress> opt_local_address =
      advertising_handle_map_.GetAddress(adv_handle);

  // We use the identity address as the local address if we aren't advertising
  // or otherwise don't know about this advertising set. This is obviously
  // wrong. However, the link will be disconnected in that case before it can
  // propagate to higher layers.
  static DeviceAddress identity_address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  DeviceAddress local_address = identity_address;
  if (opt_local_address) {
    local_address = opt_local_address.value();
  }

  StagedConnectionParameters staged = staged_parameters_node.mapped();

  CompleteIncomingConnection(connection_handle,
                             staged.role,
                             local_address,
                             staged.peer_address,
                             staged.conn_params);
  return CommandChannel::EventCallbackResult::kContinue;
}

void AndroidExtendedLowEnergyAdvertiser::OnCurrentOperationComplete() {
  if (op_queue_.empty()) {
    return;  // no more queued operations so nothing to do
  }

  fit::closure closure = std::move(op_queue_.front());
  op_queue_.pop();
  closure();
}

}  // namespace bt::hci
