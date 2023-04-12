// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_A2DP_OFFLOAD_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_A2DP_OFFLOAD_MANAGER_H_

#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt::l2cap {

// Provides an API surface to start and stop A2DP offloading. A2dpOffloadManager tracks the state of
// A2DP offloading and allows at most one channel to be offloaded at a given time
class A2dpOffloadManager {
 public:
  // Configuration received from the profile server that needs to be converted to a command packet
  // in order to send the StartA2dpOffload command
  struct Configuration {
    hci_spec::vendor::android::A2dpCodecType codec;
    uint16_t max_latency;
    hci_spec::vendor::android::A2dpScmsTEnable scms_t_enable;
    hci_spec::vendor::android::A2dpSamplingFrequency sampling_frequency;
    hci_spec::vendor::android::A2dpBitsPerSample bits_per_sample;
    hci_spec::vendor::android::A2dpChannelMode channel_mode;
    uint32_t encoded_audio_bit_rate;
    hci_spec::vendor::android::A2dpOffloadCodecInformation codec_information;
  };

  explicit A2dpOffloadManager(hci::CommandChannel::WeakPtr cmd_channel)
      : cmd_channel_(std::move(cmd_channel)) {}

  // Request the start of A2DP source offloading. |callback| will be called with the result of the
  // request. If offloading is already started or is starting, the request will fail and an error
  // will be reported synchronously.
  void StartA2dpOffload(const Configuration& config, ChannelId local_id, ChannelId remote_id,
                        hci_spec::ConnectionHandle link_handle, uint16_t max_tx_sdu_size,
                        hci::ResultCallback<> callback);

  // Request the stop of A2DP source offloading. |callback| will be called with the result of the
  // request. If offloading is already stopped, report success.
  void RequestStopA2dpOffload(ChannelId local_id, hci_spec::ConnectionHandle link_handle,
                              hci::ResultCallback<> callback);

  // Returns true if channel with |id| and |link_handle| is starting/has started A2DP offloading
  bool IsChannelOffloaded(ChannelId id, hci_spec::ConnectionHandle link_handle) const {
    if (!offloaded_channel_id_.has_value() || !offloaded_link_handle_.has_value()) {
      return false;
    }
    return id == *offloaded_channel_id_ && link_handle == *offloaded_link_handle_ &&
           (a2dp_offload_status_ == A2dpOffloadStatus::kStarted ||
            a2dp_offload_status_ == A2dpOffloadStatus::kStarting);
  }

  WeakPtr<A2dpOffloadManager> GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  // Defines the state of A2DP offloading to the controller.
  enum class A2dpOffloadStatus : uint8_t {
    // The A2DP offload command was received and successfully started.
    kStarted,
    // The A2DP offload command was sent and the L2CAP channel is waiting for a response.
    kStarting,
    // The A2DP offload stop command was sent and the L2CAP channel is waiting for a response.
    kStopping,
    // Either an error or an A2DP offload command stopped offloading to the controller.
    kStopped,
  };

  hci::CommandChannel::WeakPtr cmd_channel_;

  A2dpOffloadStatus a2dp_offload_status_ = A2dpOffloadStatus::kStopped;

  // Identifier for offloaded channel's endpoint on this device
  std::optional<ChannelId> offloaded_channel_id_;

  // Connection handle of the offloaded channel's underlying logical link
  std::optional<hci_spec::ConnectionHandle> offloaded_link_handle_;

  // Contains a callback if stop command was requested before offload status was |kStarted|
  std::optional<hci::ResultCallback<>> pending_stop_a2dp_offload_request_;

  WeakSelf<A2dpOffloadManager> weak_self_{this};
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_A2DP_OFFLOAD_MANAGER_H_
