// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "a2dp_offload_manager.h"

#include <cstdint>
#include <utility>

#include "src/connectivity/bluetooth/core/bt-host/common/host_error.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"

#include <pw_bluetooth/hci_vendor.emb.h>

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
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStarting: {
      bt_log(WARN, "l2cap", "A2DP offload is currently starting (status: %hhu)",
             static_cast<unsigned char>(a2dp_offload_status_));
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStopping: {
      bt_log(WARN, "l2cap",
             "A2DP offload is stopping... wait until stopped before starting (status: %hhu)",
             static_cast<unsigned char>(a2dp_offload_status_));
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStopped:
      break;
  }

  offloaded_link_handle_ = link_handle;
  offloaded_channel_id_ = local_id;
  a2dp_offload_status_ = A2dpOffloadStatus::kStarting;

  constexpr size_t kPacketSize =
      pw::bluetooth::vendor::android_hci::StartA2dpOffloadCommand::MaxSizeInBytes();
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::vendor::android_hci::StartA2dpOffloadCommandWriter>(
      hci_android::kA2dpOffloadCommand, kPacketSize);
  auto packet_view = packet.view_t();

  packet_view.vendor_command().sub_opcode().Write(hci_android::kStartA2dpOffloadCommandSubopcode);
  packet_view.codec_type().Write(
      static_cast<pw::bluetooth::vendor::android_hci::A2dpCodecType>(config.codec));
  packet_view.max_latency().Write(config.max_latency);

  packet_view.scms_t_enable().enabled().Write(config.scms_t_enable.enabled);
  packet_view.scms_t_enable().header().Write(config.scms_t_enable.header);

  packet_view.sampling_frequency().Write(
      static_cast<pw::bluetooth::vendor::android_hci::A2dpSamplingFrequency>(
          config.sampling_frequency));
  packet_view.bits_per_sample().Write(
      static_cast<pw::bluetooth::vendor::android_hci::A2dpBitsPerSample>(config.bits_per_sample));
  packet_view.channel_mode().Write(
      static_cast<pw::bluetooth::vendor::android_hci::A2dpChannelMode>(config.channel_mode));
  packet_view.encoded_audio_bitrate().Write(config.encoded_audio_bit_rate);
  packet_view.connection_handle().Write(link_handle);
  packet_view.l2cap_channel_id().Write(remote_id);
  packet_view.l2cap_mtu_size().Write(max_tx_sdu_size);

  // kAptx and kAptxhd codecs not yet handled
  if (config.codec == hci_android::A2dpCodecType::kSbc) {
    auto sbc_codec_information = packet_view.sbc_codec_information().BackingStorage();
    MutableBufferView sbc_codec_information_buf =
        MutableBufferView(sbc_codec_information.data(), sbc_codec_information.SizeInBytes());
    sbc_codec_information_buf.Fill(0);
    sbc_codec_information_buf.WriteObj(config.codec_information.sbc);
  } else if (config.codec == hci_android::A2dpCodecType::kLdac) {
    auto ldac_codec_information = packet_view.ldac_codec_information().BackingStorage();
    MutableBufferView ldac_codec_information_buf =
        MutableBufferView(ldac_codec_information.data(), ldac_codec_information.SizeInBytes());
    ldac_codec_information_buf.Fill(0);
    ldac_codec_information_buf.WriteObj(config.codec_information.ldac);
  } else if (config.codec == hci_android::A2dpCodecType::kAac) {
    auto aac_codec_information = packet_view.aac_codec_information().BackingStorage();
    MutableBufferView aac_codec_information_buf =
        MutableBufferView(aac_codec_information.data(), aac_codec_information.SizeInBytes());
    aac_codec_information_buf.Fill(0);
    aac_codec_information_buf.WriteObj(config.codec_information.aac);
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
      bt_log(DEBUG, "l2cap", "No channels are offloading A2DP (status: %hhu)",
             static_cast<unsigned char>(a2dp_offload_status_));
      callback(fit::success());
      return;
    }
    case A2dpOffloadStatus::kStopping: {
      bt_log(WARN, "l2cap", "A2DP offload is currently stopping (status: %hhu)",
             static_cast<unsigned char>(a2dp_offload_status_));
      callback(ToResult(HostError::kInProgress));
      return;
    }
    case A2dpOffloadStatus::kStarting:
    case A2dpOffloadStatus::kStarted:
      break;
  }

  if (!IsChannelOffloaded(local_id, link_handle)) {
    callback(fit::success());
    return;
  }

  // Wait until offloading status is |kStarted| before sending stop command
  if (a2dp_offload_status_ == A2dpOffloadStatus::kStarting) {
    pending_stop_a2dp_offload_request_ = std::move(callback);
    return;
  }

  a2dp_offload_status_ = A2dpOffloadStatus::kStopping;

  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::vendor::android_hci::StopA2dpOffloadCommandWriter>(
      hci_android::kA2dpOffloadCommand);
  auto packet_view = packet.view_t();

  packet_view.vendor_command().sub_opcode().Write(hci_android::kStopA2dpOffloadCommandSubopcode);

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

bool A2dpOffloadManager::IsChannelOffloaded(ChannelId id,
                                            hci_spec::ConnectionHandle link_handle) const {
  if (!offloaded_channel_id_.has_value() || !offloaded_link_handle_.has_value()) {
    bt_log(DEBUG, "l2cap", "Channel is not offloaded (handle: %#.4x, local id: %#.4x) ",
           link_handle, id);
    return false;
  }

  // Same channel that requested start A2DP offloading must request stop offloading
  if (id != offloaded_channel_id_ || link_handle != offloaded_link_handle_) {
    bt_log(
        WARN, "l2cap",
        "Offloaded channel must request stop offloading; offloaded channel (handle: %#.4x, local id: %#.4x)",
        *offloaded_link_handle_, *offloaded_channel_id_);
    return false;
  }

  return id == *offloaded_channel_id_ && link_handle == *offloaded_link_handle_ &&
         (a2dp_offload_status_ == A2dpOffloadStatus::kStarted ||
          a2dp_offload_status_ == A2dpOffloadStatus::kStarting);
}

}  // namespace bt::l2cap
