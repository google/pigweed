// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/extended_low_energy_advertiser.h"

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

ExtendedLowEnergyAdvertiser::ExtendedLowEnergyAdvertiser(hci::Transport::WeakPtr hci_ptr)
    : LowEnergyAdvertiser(std::move(hci_ptr)), weak_self_(this) {
  auto self = weak_self_.GetWeakPtr();
  set_terminated_event_handler_id_ = hci()->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLEAdvertisingSetTerminatedSubeventCode, [self](const EventPacket& event_packet) {
        if (self.is_alive()) {
          return self->OnAdvertisingSetTerminatedEvent(event_packet);
        }

        return CommandChannel::EventCallbackResult::kRemove;
      });
}

ExtendedLowEnergyAdvertiser::~ExtendedLowEnergyAdvertiser() {
  // This object is probably being destroyed because the stack is shutting down, in which case the
  // HCI layer may have already been destroyed.
  if (!hci().is_alive() || !hci()->command_channel()) {
    return;
  }
  hci()->command_channel()->RemoveEventHandler(set_terminated_event_handler_id_);
  // TODO(fxbug.dev/112157): This will only cancel one advertisement, after which the
  // SequentialCommandRunner will have been destroyed and no further commands will be sent.
  StopAdvertising();
}

std::optional<EmbossCommandPacket> ExtendedLowEnergyAdvertiser::BuildEnablePacket(
    const DeviceAddress& address, pw::bluetooth::emboss::GenericEnableParam enable) {
  // We only enable or disable a single address at a time. The multiply by 1 is set explicitly to
  // show that data[] within LESetExtendedAdvertisingEnableData is of size 1.
  constexpr size_t kPacketSize =
      pw::bluetooth::emboss::LESetExtendedAdvertisingEnableCommand::MinSizeInBytes() +
      (1 * pw::bluetooth::emboss::LESetExtendedAdvertisingEnableData::IntrinsicSizeInBytes());
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetExtendedAdvertisingEnableCommandWriter>(
      hci_spec::kLESetExtendedAdvertisingEnable, kPacketSize);
  auto packet_view = packet.view_t();
  packet_view.enable().Write(enable);
  packet_view.num_sets().Write(1);

  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);

  packet_view.data()[0].advertising_handle().Write(handle.value());
  packet_view.data()[0].duration().Write(hci_spec::kNoAdvertisingDuration);
  packet_view.data()[0].max_extended_advertising_events().Write(
      hci_spec::kNoMaxExtendedAdvertisingEvents);

  return packet;
}

std::unique_ptr<CommandPacket> ExtendedLowEnergyAdvertiser::BuildSetAdvertisingParams(
    const DeviceAddress& address, hci_spec::LEAdvertisingType type,
    hci_spec::LEOwnAddressType own_address_type, AdvertisingIntervalRange interval) {
  constexpr size_t kPayloadSize = sizeof(hci_spec::LESetExtendedAdvertisingParametersCommandParams);
  std::unique_ptr<CommandPacket> packet =
      CommandPacket::New(hci_spec::kLESetExtendedAdvertisingParameters, kPayloadSize);
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload =
      packet->mutable_payload<hci_spec::LESetExtendedAdvertisingParametersCommandParams>();

  // advertising handle
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.MapHandle(address);
  if (!handle) {
    bt_log(WARN, "hci-le",
           "could not allocate a new advertising handle for address: %s (all in use)",
           bt_str(address));
    return nullptr;
  }
  payload->adv_handle = handle.value();

  // advertising event properties
  std::optional<hci_spec::AdvertisingEventBits> bits = AdvertisingTypeToEventBits(type);
  if (!bits) {
    bt_log(WARN, "hci-le", "could not generate event bits for type: %hhu", type);
    return nullptr;
  }
  payload->adv_event_properties = bits.value();

  // advertising interval, NOTE: LE advertising parameters allow for up to 3 octets (10 ms to
  // 10428 s) to configure an advertising interval. However, we expose only the recommended
  // advertising interval configurations to users, as specified in the Bluetooth Spec Volume 3, Part
  // C, Appendix A. These values are expressed as uint16_t so we simply copy them (taking care of
  // endianness) into the 3 octets as is.
  hci_spec::EncodeLegacyAdvertisingInterval(interval.min(), payload->primary_adv_interval_min);
  hci_spec::EncodeLegacyAdvertisingInterval(interval.max(), payload->primary_adv_interval_max);

  // other settings
  payload->primary_adv_channel_map = hci_spec::kLEAdvertisingChannelAll;
  payload->own_address_type = own_address_type;
  payload->adv_filter_policy = hci_spec::LEAdvFilterPolicy::kAllowAll;
  payload->adv_tx_power = hci_spec::kLEExtendedAdvertisingTxPowerNoPreference;
  payload->scan_request_notification_enable = pw::bluetooth::emboss::GenericEnableParam::DISABLE;

  // TODO(fxbug.dev/81470): using legacy PDUs requires advertisements on the LE 1M PHY.
  payload->primary_adv_phy = hci_spec::LEPHY::kLE1M;
  payload->secondary_adv_phy = hci_spec::LEPHY::kLE1M;

  // Payload values were initialized to zero above. By not setting the values for the following
  // fields, we are purposely ignoring them:
  //
  // advertising_sid: We use only legacy PDUs, the controller ignores this field in that case
  // peer_address: We don't support directed advertising yet
  // peer_address_type: We don't support directed advertising yet
  // secondary_adv_max_skip: We use only legacy PDUs, the controller ignores this field in that case

  return packet;
}

CommandChannel::CommandPacketVariant ExtendedLowEnergyAdvertiser::BuildSetAdvertisingData(
    const DeviceAddress& address, const AdvertisingData& data, AdvFlags flags) {
  AdvertisingData adv_data;
  data.Copy(&adv_data);
  if (staged_advertising_parameters_.include_tx_power_level) {
    adv_data.SetTxPower(staged_advertising_parameters_.selected_tx_power_level);
  }
  size_t block_size = adv_data.CalculateBlockSize(/*include_flags=*/true);

  size_t kPayloadSize = sizeof(hci_spec::LESetExtendedAdvertisingDataCommandParams) + block_size;
  std::unique_ptr<CommandPacket> packet =
      CommandPacket::New(hci_spec::kLESetExtendedAdvertisingData, kPayloadSize);
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_spec::LESetExtendedAdvertisingDataCommandParams>();

  // advertising handle
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);
  payload->adv_handle = handle.value();

  // TODO(fxbug.dev/81470): We support only legacy PDUs and do not support fragmented extended
  // advertising data at this time.
  payload->operation = hci_spec::LESetExtendedAdvDataOp::kComplete;
  payload->fragment_preference = hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment;

  // advertising data
  payload->adv_data_length = static_cast<uint8_t>(block_size);
  MutableBufferView data_view(payload->adv_data, payload->adv_data_length);
  adv_data.WriteBlock(&data_view, flags);

  return packet;
}

std::unique_ptr<CommandPacket> ExtendedLowEnergyAdvertiser::BuildUnsetAdvertisingData(
    const DeviceAddress& address) {
  std::unique_ptr<CommandPacket> packet =
      CommandPacket::New(hci_spec::kLESetExtendedAdvertisingData,
                         sizeof(hci_spec::LESetExtendedAdvertisingDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_spec::LESetExtendedAdvertisingDataCommandParams>();

  // advertising handle
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);
  payload->adv_handle = handle.value();

  // TODO(fxbug.dev/81470): We support only legacy PDUs and do not support fragmented extended
  // advertising data at this time.
  payload->operation = hci_spec::LESetExtendedAdvDataOp::kComplete;
  payload->fragment_preference = hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment;
  payload->adv_data_length = 0;

  return packet;
}

std::unique_ptr<CommandPacket> ExtendedLowEnergyAdvertiser::BuildSetScanResponse(
    const DeviceAddress& address, const AdvertisingData& data) {
  AdvertisingData scan_rsp;
  data.Copy(&scan_rsp);
  if (staged_advertising_parameters_.include_tx_power_level) {
    scan_rsp.SetTxPower(staged_advertising_parameters_.selected_tx_power_level);
  }
  size_t block_size = scan_rsp.CalculateBlockSize();

  size_t kPayloadSize = sizeof(hci_spec::LESetExtendedScanResponseDataCommandParams) + block_size;
  std::unique_ptr<CommandPacket> packet =
      CommandPacket::New(hci_spec::kLESetExtendedScanResponseData, kPayloadSize);
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_spec::LESetExtendedScanResponseDataCommandParams>();

  // advertising handle
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);
  payload->adv_handle = handle.value();

  // TODO(fxbug.dev/81470): We support only legacy PDUs and do not support fragmented extended
  // advertising data at this time.
  payload->operation = hci_spec::LESetExtendedAdvDataOp::kComplete;
  payload->fragment_preference = hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment;

  // scan response data
  payload->scan_rsp_data_length = static_cast<uint8_t>(block_size);
  MutableBufferView scan_rsp_view(payload->scan_rsp_data, payload->scan_rsp_data_length);
  scan_rsp.WriteBlock(&scan_rsp_view, std::nullopt);

  return packet;
}

std::unique_ptr<CommandPacket> ExtendedLowEnergyAdvertiser::BuildUnsetScanResponse(
    const DeviceAddress& address) {
  std::unique_ptr<CommandPacket> packet =
      CommandPacket::New(hci_spec::kLESetExtendedScanResponseData,
                         sizeof(hci_spec::LESetExtendedScanResponseDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_spec::LESetExtendedScanResponseDataCommandParams>();

  // advertising handle
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);
  payload->adv_handle = handle.value();

  // TODO(fxbug.dev/81470): We support only legacy PDUs and do not support fragmented extended
  // advertising data at this time.
  payload->operation = hci_spec::LESetExtendedAdvDataOp::kComplete;
  payload->fragment_preference = hci_spec::LEExtendedAdvFragmentPreference::kShouldNotFragment;
  payload->scan_rsp_data_length = 0;

  return packet;
}

std::optional<EmbossCommandPacket> ExtendedLowEnergyAdvertiser::BuildRemoveAdvertisingSet(
    const DeviceAddress& address) {
  std::optional<hci_spec::AdvertisingHandle> handle = advertising_handle_map_.GetHandle(address);
  BT_ASSERT(handle);
  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LERemoveAdvertisingSetCommandWriter>(
          hci_spec::kLERemoveAdvertisingSet);
  auto packet_view = packet.view_t();
  packet_view.advertising_handle().Write(handle.value());

  return packet;
}

void ExtendedLowEnergyAdvertiser::OnSetAdvertisingParamsComplete(const EventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kCommandCompleteEventCode);
  BT_ASSERT(event.params<hci_spec::CommandCompleteEventParams>().command_opcode ==
            hci_spec::kLESetExtendedAdvertisingParameters);

  Result<> result = event.ToResult();
  if (bt_is_error(result, WARN, "hci-le", "set advertising parameters, error received: %s",
                  bt_str(result))) {
    return;  // full error handling done in super class, can just return here
  }

  auto params = event.return_params<hci_spec::LESetExtendedAdvertisingParametersReturnParams>();
  BT_ASSERT(params);

  if (staged_advertising_parameters_.include_tx_power_level) {
    staged_advertising_parameters_.selected_tx_power_level = params->selected_tx_power;
  }
}

void ExtendedLowEnergyAdvertiser::StartAdvertising(const DeviceAddress& address,
                                                   const AdvertisingData& data,
                                                   const AdvertisingData& scan_rsp,
                                                   AdvertisingOptions options,
                                                   ConnectionCallback connect_callback,
                                                   ResultFunction<> result_callback) {
  // if there is an operation currently in progress, enqueue this operation and we will get to it
  // the next time we have a chance
  if (!hci_cmd_runner().IsReady()) {
    bt_log(INFO, "hci-le", "hci cmd runner not ready, queuing advertisement commands for now");

    AdvertisingData copied_data;
    data.Copy(&copied_data);

    AdvertisingData copied_scan_rsp;
    scan_rsp.Copy(&copied_scan_rsp);

    op_queue_.push([this, address, data = std::move(copied_data),
                    scan_rsp = std::move(copied_scan_rsp), options,
                    conn_cb = std::move(connect_callback),
                    result_cb = std::move(result_callback)]() mutable {
      StartAdvertising(address, data, scan_rsp, options, std::move(conn_cb), std::move(result_cb));
    });

    return;
  }

  fit::result<HostError> result = CanStartAdvertising(address, data, scan_rsp, options);
  if (result.is_error()) {
    result_callback(ToResult(result.error_value()));
    return;
  }

  if (IsAdvertising(address)) {
    bt_log(DEBUG, "hci-le", "updating existing advertisement for %s", bt_str(address));
  }

  std::memset(&staged_advertising_parameters_, 0, sizeof(staged_advertising_parameters_));
  staged_advertising_parameters_.include_tx_power_level = options.include_tx_power_level;

  // Core Spec, Volume 4, Part E, Section 7.8.58: "the number of advertising sets that can be
  // supported is not fixed and the Controller can change it at any time. The memory used to store
  // advertising sets can also be used for other purposes."
  //
  // Depending on the memory profile of the controller, a new advertising set may or may not be
  // accepted. We could use HCI_LE_Read_Number_of_Supported_Advertising_Sets to check if the
  // controller has space for another advertising set. However, the value may change after the read
  // and before the addition of the advertising set. Furthermore, sending an extra HCI command
  // increases the latency of our stack. Instead, we simply attempt to add. If the controller is
  // unable to support another advertising set, it will respond with a memory capacity exceeded
  // error.
  StartAdvertisingInternal(address, data, scan_rsp, options.interval, options.flags,
                           std::move(connect_callback), std::move(result_callback));
}

void ExtendedLowEnergyAdvertiser::StopAdvertising() {
  LowEnergyAdvertiser::StopAdvertising();
  advertising_handle_map_.Clear();

  // std::queue doesn't have a clear method so we have to resort to this tomfoolery :(
  decltype(op_queue_) empty;
  std::swap(op_queue_, empty);
}

void ExtendedLowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address) {
  // if there is an operation currently in progress, enqueue this operation and we will get to it
  // the next time we have a chance
  if (!hci_cmd_runner().IsReady()) {
    bt_log(INFO, "hci-le", "hci cmd runner not ready, queueing stop advertising command for now");
    op_queue_.push([this, address]() { StopAdvertising(address); });
    return;
  }

  LowEnergyAdvertiser::StopAdvertisingInternal(address);
  advertising_handle_map_.RemoveAddress(address);
}

void ExtendedLowEnergyAdvertiser::OnIncomingConnection(
    hci_spec::ConnectionHandle handle, pw::bluetooth::emboss::ConnectionRole role,
    const DeviceAddress& peer_address, const hci_spec::LEConnectionParameters& conn_params) {
  // Core Spec Volume 4, Part E, Section 7.8.56: Incoming connections to LE Extended Advertising
  // occur through two events: HCI_LE_Connection_Complete and HCI_LE_Advertising_Set_Terminated.
  // This method is called as a result of the HCI_LE_Connection_Complete event. At this point, we
  // only have a connection handle but don't know the locally advertised address that the
  // connection is for. Until we receive the HCI_LE_Advertising_Set_Terminated event, we stage
  // these parameters.
  staged_connections_map_[handle] = {role, peer_address, conn_params};
}

// The HCI_LE_Advertising_Set_Terminated event contains the mapping between connection handle and
// advertising handle. After the HCI_LE_Advertising_Set_Terminated event, we have all the
// information necessary to create a connection object within the Host layer.
CommandChannel::EventCallbackResult ExtendedLowEnergyAdvertiser::OnAdvertisingSetTerminatedEvent(
    const EventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_ASSERT(event.params<hci_spec::LEMetaEventParams>().subevent_code ==
            hci_spec::kLEAdvertisingSetTerminatedSubeventCode);

  Result<> result = event.ToResult();
  if (bt_is_error(result, ERROR, "hci-le", "advertising set terminated event, error received %s",
                  bt_str(result))) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  auto params = event.subevent_params<hci_spec::LEAdvertisingSetTerminatedSubeventParams>();
  BT_ASSERT(params);

  hci_spec::ConnectionHandle connection_handle = params->connection_handle;
  auto staged_parameters_node = staged_connections_map_.extract(connection_handle);

  if (staged_parameters_node.empty()) {
    bt_log(ERROR, "hci-le",
           "advertising set terminated event, staged params not available "
           "(handle: %d)",
           params->adv_handle);
    return CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::AdvertisingHandle adv_handle = params->adv_handle;
  std::optional<DeviceAddress> opt_local_address = advertising_handle_map_.GetAddress(adv_handle);

  // We use the identity address as the local address if we aren't advertising or otherwise don't
  // know about this advertising set. This is obviously wrong. However, the link will be
  // disconnected in that case before it can propagate to higher layers.
  static DeviceAddress identity_address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  DeviceAddress local_address = identity_address;
  if (opt_local_address) {
    local_address = opt_local_address.value();
  }

  StagedConnectionParameters staged = staged_parameters_node.mapped();

  CompleteIncomingConnection(connection_handle, staged.role, local_address, staged.peer_address,
                             staged.conn_params);

  std::memset(&staged_advertising_parameters_, 0, sizeof(staged_advertising_parameters_));
  return CommandChannel::EventCallbackResult::kContinue;
}

void ExtendedLowEnergyAdvertiser::OnCurrentOperationComplete() {
  if (op_queue_.empty()) {
    return;  // no more queued operations so nothing to do
  }

  fit::closure closure = std::move(op_queue_.front());
  op_queue_.pop();
  closure();
}

}  // namespace bt::hci
