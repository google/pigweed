// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "legacy_low_energy_advertiser.h"

#include <endian.h>
#include <lib/async/default.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"

namespace bt {
namespace hci {

namespace {

// The size, in bytes, of the serialized TX Power Level.
constexpr size_t kTxPowerLevelTLVSize = 3;

// Helpers for building HCI command packets:

std::unique_ptr<CommandPacket> BuildEnablePacket(GenericEnableParam enable) {
  constexpr size_t kPayloadSize = sizeof(LESetAdvertisingEnableCommandParams);
  auto packet = CommandPacket::New(kLESetAdvertisingEnable, kPayloadSize);
  packet->mutable_payload<LESetAdvertisingEnableCommandParams>()->advertising_enable = enable;
  ZX_ASSERT(packet);
  return packet;
}

std::unique_ptr<CommandPacket> BuildSetAdvertisingData(const AdvertisingData& data,
                                                       AdvFlags flags) {
  auto packet =
      CommandPacket::New(kLESetAdvertisingData, sizeof(LESetAdvertisingDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetAdvertisingDataCommandParams>();
  params->adv_data_length = data.CalculateBlockSize(/*include_flags=*/true);

  MutableBufferView adv_view(params->adv_data, params->adv_data_length);
  data.WriteBlock(&adv_view, flags);

  return packet;
}

std::unique_ptr<CommandPacket> BuildSetScanResponse(const AdvertisingData& scan_rsp) {
  auto packet =
      CommandPacket::New(kLESetScanResponseData, sizeof(LESetScanResponseDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetScanResponseDataCommandParams>();
  params->scan_rsp_data_length = scan_rsp.CalculateBlockSize();

  MutableBufferView scan_data_view(params->scan_rsp_data, sizeof(params->scan_rsp_data));
  scan_rsp.WriteBlock(&scan_data_view, std::nullopt);

  return packet;
}

std::unique_ptr<CommandPacket> BuildSetAdvertisingParams(LEAdvertisingType type,
                                                         LEOwnAddressType own_address_type,
                                                         AdvertisingIntervalRange interval) {
  auto packet = CommandPacket::New(kLESetAdvertisingParameters,
                                   sizeof(LESetAdvertisingParametersCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetAdvertisingParametersCommandParams>();
  params->adv_interval_min = htole16(interval.min());
  params->adv_interval_max = htole16(interval.max());
  params->adv_type = type;
  params->own_address_type = own_address_type;
  params->adv_channel_map = kLEAdvertisingChannelAll;
  params->adv_filter_policy = LEAdvFilterPolicy::kAllowAll;

  // We don't support directed advertising yet, so leave peer_address as 0x00
  // (|packet| parameters are initialized to zero above).

  return packet;
}

std::unique_ptr<CommandPacket> BuildReadAdvertisingTxPower() {
  auto packet = CommandPacket::New(kLEReadAdvertisingChannelTxPower);
  ZX_ASSERT(packet);
  return packet;
}

}  // namespace

LegacyLowEnergyAdvertiser::LegacyLowEnergyAdvertiser(fxl::WeakPtr<Transport> hci)
    : hci_(std::move(hci)), starting_(false), connect_callback_(nullptr) {
  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(async_get_default_dispatcher(), hci_);
}

LegacyLowEnergyAdvertiser::~LegacyLowEnergyAdvertiser() { StopAdvertisingInternal(); }

size_t LegacyLowEnergyAdvertiser::GetSizeLimit() { return kMaxLEAdvertisingDataLength; }

bool LegacyLowEnergyAdvertiser::AllowsRandomAddressChange() const {
  return !starting_ && !advertising();
}

void LegacyLowEnergyAdvertiser::StartAdvertisingInternal(
    const DeviceAddress& address, const AdvertisingData& data, const AdvertisingData& scan_rsp,
    AdvertisingIntervalRange interval, AdvFlags flags, ConnectionCallback connect_callback,
    StatusCallback callback) {
  if (advertising()) {
    // Temporarily disable advertising so we can tweak the parameters.
    hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kDisable));
  }

  // Set advertising and scan response data. If either data is empty then it
  // will be cleared accordingly.
  hci_cmd_runner_->QueueCommand(BuildSetAdvertisingData(data, flags));
  hci_cmd_runner_->QueueCommand(BuildSetScanResponse(scan_rsp));

  // Set advertising parameters
  LEAdvertisingType type = LEAdvertisingType::kAdvNonConnInd;
  if (connect_callback) {
    type = LEAdvertisingType::kAdvInd;
  } else if (scan_rsp.CalculateBlockSize() > 0) {
    type = LEAdvertisingType::kAdvScanInd;
  }

  LEOwnAddressType own_addr_type;
  if (address.type() == DeviceAddress::Type::kLEPublic) {
    own_addr_type = LEOwnAddressType::kPublic;
  } else {
    own_addr_type = LEOwnAddressType::kRandom;
  }

  hci_cmd_runner_->QueueCommand(BuildSetAdvertisingParams(type, own_addr_type, interval));

  // Enable advertising.
  hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kEnable));

  hci_cmd_runner_->RunCommands(
      [this, address, callback = std::move(callback),
       connect_callback = std::move(connect_callback)](Status status) mutable {
        ZX_DEBUG_ASSERT(starting_);
        starting_ = false;

        if (bt_is_error(status, ERROR, "hci-le", "failed to start advertising")) {
          // Clear out the advertising data if it partially succeeded.
          StopAdvertisingInternal();
        } else {
          bt_log(INFO, "hci-le", "advertising enabled");
          advertised_ = address;
          connect_callback_ = std::move(connect_callback);
        }

        callback(status);
      });
}

void LegacyLowEnergyAdvertiser::StartAdvertising(
    const DeviceAddress& address, const AdvertisingData& data, const AdvertisingData& scan_rsp,
    AdvertisingOptions adv_options, ConnectionCallback connect_callback, StatusCallback callback) {
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(address.type() != DeviceAddress::Type::kBREDR);

  if (adv_options.anonymous) {
    bt_log(DEBUG, "hci-le", "anonymous advertising not supported");
    callback(Status(HostError::kNotSupported));
    return;
  }

  if (advertising()) {
    if (address != advertised_) {
      bt_log(DEBUG, "hci-le", "already advertising");
      callback(Status(HostError::kNotSupported));
      return;
    }
    bt_log(DEBUG, "hci-le", "updating existing advertisement");
  }

  // If the TX Power Level is requested, ensure both buffers have enough space.
  size_t size_limit = GetSizeLimit();
  if (adv_options.include_tx_power_level)
    size_limit -= kTxPowerLevelTLVSize;

  if (data.CalculateBlockSize(/*include_flags=*/true) > size_limit) {
    bt_log(DEBUG, "hci-le", "advertising data too large");
    callback(Status(HostError::kAdvertisingDataTooLong));
    return;
  }

  if (scan_rsp.CalculateBlockSize() > size_limit) {
    bt_log(DEBUG, "hci-le", "scan response too large");
    callback(Status(HostError::kScanResponseTooLong));
    return;
  }

  // Midst of a TX power level read - send a cancel over the previous status callback.
  if (staged_params_.has_value()) {
    auto status_cb = std::move(staged_params_.value().callback);
    status_cb(Status(HostError::kCanceled));
  }

  // If the TX Power level is requested, then stage the parameters for the read operation.
  // If there already is an outstanding TX Power Level read request, return early.
  // Advertising on the outstanding call will now use the updated |staged_params_|.
  if (adv_options.include_tx_power_level) {
    AdvertisingData data_copy, scan_rsp_copy;
    data.Copy(&data_copy);
    scan_rsp.Copy(&scan_rsp_copy);
    staged_params_ = StagedParams{address,
                                  adv_options.interval,
                                  adv_options.flags,
                                  std::move(data_copy),
                                  std::move(scan_rsp_copy),
                                  std::move(connect_callback),
                                  std::move(callback)};
    if (starting_ && hci_cmd_runner_->IsReady()) {
      return;
    }
  }

  if (!hci_cmd_runner_->IsReady()) {
    // Abort any remaining commands from the current stop sequence. If we got
    // here then the controller MUST receive our request to disable advertising,
    // so the commands that we send next will overwrite the current advertising
    // settings and re-enable it.
    hci_cmd_runner_->Cancel();
  }

  starting_ = true;

  // If the TX Power Level is requested, read it from the controller, update the data buf, and
  // proceed with starting advertising.
  //
  // If advertising was canceled during the TX power level read (either |starting_| was
  // reset or the |callback| was moved), return early.
  if (adv_options.include_tx_power_level) {
    auto power_cb = [this](auto, const hci::EventPacket& event) mutable {
      ZX_ASSERT(staged_params_.has_value());
      if ((!starting_) || (!staged_params_.value().callback)) {
        bt_log(INFO, "hci-le", "Advertising canceled during TX Power Level read.");
        return;
      }

      if (hci_is_error(event, WARN, "hci-le", "read TX power level failed")) {
        staged_params_.value().callback(event.ToStatus());
        staged_params_ = {};
        starting_ = false;
        return;
      }

      const auto& params = event.return_params<hci::LEReadAdvertisingChannelTxPowerReturnParams>();

      // Update the advertising and scan response data with the TX power level.
      auto staged_params = std::move(staged_params_.value());
      staged_params.data.SetTxPower(params->tx_power);
      if (staged_params.scan_rsp.CalculateBlockSize()) {
        staged_params.scan_rsp.SetTxPower(params->tx_power);
      }
      // Reset the |staged_params_| as it is no longer in use.
      staged_params_ = {};

      StartAdvertisingInternal(staged_params.address, staged_params.data, staged_params.scan_rsp,
                               staged_params.interval, staged_params.flags,
                               std::move(staged_params.connect_callback),
                               std::move(staged_params.callback));
    };

    hci_->command_channel()->SendCommand(BuildReadAdvertisingTxPower(), std::move(power_cb));
    return;
  }

  StartAdvertisingInternal(address, data, scan_rsp, adv_options.interval, adv_options.flags,
                           std::move(connect_callback), std::move(callback));
}

// TODO(fxbug.dev/50542): StopAdvertising() should cancel outstanding calls to StartAdvertising()
// and clean up state.
bool LegacyLowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address) {
  if (advertised_ != address) {
    // not advertising, or not on this address.
    return false;
  }
  StopAdvertisingInternal();
  return true;
}

void LegacyLowEnergyAdvertiser::StopAdvertisingInternal() {
  connect_callback_ = nullptr;

  if (!hci_cmd_runner_->IsReady()) {
    if (!starting_) {
      bt_log(DEBUG, "hci-le", "already stopping");

      // The advertised address must have been cleared in this state.
      ZX_DEBUG_ASSERT(!advertising());
      return;
    }

    // Cancel the pending start
    ZX_DEBUG_ASSERT(starting_);
    hci_cmd_runner_->Cancel();
    starting_ = false;
  }

  // Even on failure, we want to consider us not advertising. Clear the
  // advertised address here so that new advertisements can be requested right
  // away.
  advertised_ = {};

  // Disable advertising
  hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kDisable));

  // Unset advertising data
  auto data_packet =
      CommandPacket::New(kLESetAdvertisingData, sizeof(LESetAdvertisingDataCommandParams));
  data_packet->mutable_view()->mutable_payload_data().SetToZeros();
  hci_cmd_runner_->QueueCommand(std::move(data_packet));

  // Set scan response data
  auto scan_rsp_packet =
      CommandPacket::New(kLESetScanResponseData, sizeof(LESetScanResponseDataCommandParams));
  scan_rsp_packet->mutable_view()->mutable_payload_data().SetToZeros();
  hci_cmd_runner_->QueueCommand(std::move(scan_rsp_packet));

  hci_cmd_runner_->RunCommands(
      [](Status status) { bt_log(INFO, "hci-le", "advertising stopped: %s", bt_str(status)); });
}

void LegacyLowEnergyAdvertiser::OnIncomingConnection(ConnectionHandle handle, Connection::Role role,
                                                     const DeviceAddress& peer_address,
                                                     const LEConnectionParameters& conn_params) {
  // Immediately construct a Connection object. If this object goes out of scope following the error
  // checks below, it will send the a command to disconnect the link. We assign |advertised_| as the
  // local address however this address may be invalid if we're not advertising. This is OK as the
  // link will be disconnected in that case before it can propagate to higher layers.
  //
  // TODO(fxbug.dev/2761): We can't assign the default address since an LE connection cannot have a BR/EDR
  // type. This temporary default won't be necessary was we remove transport from the address type.
  auto local_address =
      advertising() ? advertised_ : DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  auto link = Connection::CreateLE(handle, role, local_address, peer_address, conn_params, hci_);

  if (!advertising()) {
    bt_log(DEBUG, "hci-le", "connection received without advertising!");
    return;
  }

  if (!connect_callback_) {
    bt_log(DEBUG, "hci-le", "connection received when not connectable!");
    return;
  }

  // Assign the currently advertised address as the local address of the
  // connection.
  auto callback = std::move(connect_callback_);
  StopAdvertisingInternal();

  // Pass on the ownership.
  callback(std::move(link));
}

}  // namespace hci
}  // namespace bt
