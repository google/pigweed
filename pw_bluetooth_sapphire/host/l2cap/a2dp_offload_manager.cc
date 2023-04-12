// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "a2dp_offload_manager.h"

#include <utility>

#include "src/connectivity/bluetooth/core/bt-host/common/host_error.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"

namespace bt::l2cap {
namespace hci_android = bt::hci_spec::vendor::android;

void A2dpOffloadManager::StartA2dpOffload(const Configuration& config, ChannelId local_id,
                                          ChannelId remote_id,
                                          hci_spec::ConnectionHandle link_handle,
                                          uint16_t max_tx_sdu_size,
                                          hci::ResultCallback<> callback) {
  BT_DEBUG_ASSERT(cmd_channel_.is_alive());

  switch (a2dp_offload_status_) {
    case A2dpOffloadStatus::kStarted: {
      bt_log(
          WARN, "l2cap",
          "Only one channel can offload A2DP at a time; already offloaded (handle: %#.4x, local id: %#.4x",
          *offloaded_link_handle_, *offloaded_channel_id_);
      callback(ToResult(HostError::kFailed));
      return;
    }
    case A2dpOffloadStatus::kStarting: {
      bt_log(WARN, "l2cap", "A2DP offload is currently starting (status: %hhu)",
             a2dp_offload_status_);
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStopping: {
      bt_log(WARN, "l2cap",
             "A2DP offload is stopping... wait until stopped before starting (status: %hhu)",
             a2dp_offload_status_);
      callback(ToResult(HostError::kNotReady));
      return;
    }
    case A2dpOffloadStatus::kStopped:
      break;
  }

  offloaded_link_handle_ = link_handle;
  offloaded_channel_id_ = local_id;
  a2dp_offload_status_ = A2dpOffloadStatus::kStarting;

  std::unique_ptr<hci::CommandPacket> packet = hci::CommandPacket::New(
      hci_android::kA2dpOffloadCommand, sizeof(hci_android::StartA2dpOffloadCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_android::StartA2dpOffloadCommandParams>();
  payload->opcode = hci_android::kStartA2dpOffloadCommandSubopcode;
  payload->codec_type = static_cast<hci_android::A2dpCodecType>(htole32(config.codec));
  payload->max_latency = htole16(config.max_latency);
  payload->scms_t_enable = config.scms_t_enable;
  payload->sampling_frequency =
      static_cast<hci_android::A2dpSamplingFrequency>(htole32(config.sampling_frequency));
  payload->bits_per_sample = config.bits_per_sample;
  payload->channel_mode = config.channel_mode;
  payload->encoded_audio_bitrate = htole32(config.encoded_audio_bit_rate);
  payload->connection_handle = htole16(link_handle);
  payload->l2cap_channel_id = htole16(remote_id);
  payload->l2cap_mtu_size = htole16(max_tx_sdu_size);

  if (config.codec == hci_android::A2dpCodecType::kLdac) {
    payload->codec_information.ldac.vendor_id = htole32(config.codec_information.ldac.vendor_id);
    payload->codec_information.ldac.codec_id = htole16(config.codec_information.ldac.codec_id);
    payload->codec_information.ldac.bitrate_index = config.codec_information.ldac.bitrate_index;
    payload->codec_information.ldac.ldac_channel_mode =
        config.codec_information.ldac.ldac_channel_mode;
  } else {
    // A2dpOffloadCodecInformation does not require little endianness conversion for any other
    // codec type due to their fields being one byte only.
    payload->codec_information = config.codec_information;
  }

  cmd_channel_->SendCommand(
      std::move(packet), [cb = std::move(callback), id = local_id, handle = link_handle,
                          self = weak_self_.GetWeakPtr(),
                          this](auto /*transaction_id*/, const hci::EventPacket& event) mutable {
        if (!self.is_alive()) {
          return;
        }

        if (event.ToResult().is_error()) {
          bt_log(WARN, "l2cap",
                 "Start A2DP offload command failed (result: %s, handle: %#.4x, local id: %#.4x)",
                 bt_str(event.ToResult()), handle, id);
          a2dp_offload_status_ = A2dpOffloadStatus::kStopped;
        } else {
          bt_log(INFO, "l2cap", "A2DP offload started (handle: %#.4x, local id: %#.4x", handle, id);
          a2dp_offload_status_ = A2dpOffloadStatus::kStarted;
        }
        cb(event.ToResult());

        // If we tried to stop while A2DP was still starting, perform the stop command now
        if (pending_stop_a2dp_offload_request_.has_value()) {
          auto callback = std::move(pending_stop_a2dp_offload_request_.value());
          pending_stop_a2dp_offload_request_.reset();

          RequestStopA2dpOffload(id, handle, std::move(callback));
        }
      });
}

void A2dpOffloadManager::RequestStopA2dpOffload(ChannelId local_id,
                                                hci_spec::ConnectionHandle link_handle,
                                                hci::ResultCallback<> callback) {
  BT_DEBUG_ASSERT(cmd_channel_.is_alive());

  switch (a2dp_offload_status_) {
    case A2dpOffloadStatus::kStopped: {
      bt_log(WARN, "l2cap", "No channels are offloading A2DP (status: %hhu)", a2dp_offload_status_);
      callback(ToResult(HostError::kFailed));
      return;
    }
    case A2dpOffloadStatus::kStopping: {
      bt_log(WARN, "l2cap", "A2DP offload is currently stopping (status: %hhu)",
             a2dp_offload_status_);
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStarting:
    case A2dpOffloadStatus::kStarted:
      break;
  }

  if (!IsChannelOffloaded(local_id, link_handle)) {
    bt_log(WARN, "l2cap", "Channel is not offloaded (handle: %#.4x, local id: %#.4x",
           *offloaded_link_handle_, *offloaded_channel_id_);
    callback(ToResult(HostError::kFailed));
    return;
  }

  // Same channel that requested start A2DP offloading must request to stop offloading
  if (local_id != offloaded_channel_id_ || link_handle != offloaded_link_handle_) {
    bt_log(
        WARN, "l2cap",
        "Offloaded channel must request to stop A2DP offloading; offloaded channel (handle: %#.4x, local id: %#.4x",
        *offloaded_link_handle_, *offloaded_channel_id_);
    callback(ToResult(HostError::kFailed));
    return;
  }

  // Wait until offloading status is |kStarted| before sending stop command
  if (a2dp_offload_status_ == A2dpOffloadStatus::kStarting) {
    pending_stop_a2dp_offload_request_ = std::move(callback);
    return;
  }

  a2dp_offload_status_ = A2dpOffloadStatus::kStopping;

  std::unique_ptr<hci::CommandPacket> packet = hci::CommandPacket::New(
      hci_android::kA2dpOffloadCommand, sizeof(hci_android::StopA2dpOffloadCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto payload = packet->mutable_payload<hci_android::StopA2dpOffloadCommandParams>();
  payload->opcode = hci_android::kStopA2dpOffloadCommandSubopcode;

  cmd_channel_->SendCommand(
      std::move(packet),
      [cb = std::move(callback), self = weak_self_.GetWeakPtr(), id = local_id,
       handle = link_handle, this](auto /*transaction_id*/, const hci::EventPacket& event) mutable {
        if (!self.is_alive()) {
          return;
        }

        if (event.ToResult().is_error()) {
          bt_log(WARN, "l2cap",
                 "Stop A2DP offload command failed (result: %s, handle: %#.4x, local id: %#.4x)",
                 bt_str(event.ToResult()), handle, id);
        } else {
          bt_log(INFO, "l2cap", "A2DP offload stopped (handle: %#.4x, local id: %#.4x", handle, id);
        }
        cb(event.ToResult());

        a2dp_offload_status_ = A2dpOffloadStatus::kStopped;
      });
}

}  // namespace bt::l2cap
