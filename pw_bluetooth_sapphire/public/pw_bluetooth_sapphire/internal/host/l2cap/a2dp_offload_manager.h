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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

namespace bt::l2cap {

// Provides an API surface to start and stop A2DP offloading. A2dpOffloadManager
// tracks the state of A2DP offloading and allows at most one channel to be
// offloaded at a given time
class A2dpOffloadManager {
 public:
  // Configuration received from the profile server that needs to be converted
  // to a command packet in order to send the StartA2dpOffload command
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

  // Request the start of A2DP source offloading. |callback| will be called with
  // the result of the request. If offloading is already started or still
  // starting/stopping, the request will fail and |kInProgress| error will be
  // reported synchronously.
  void StartA2dpOffload(const Configuration& config,
                        ChannelId local_id,
                        ChannelId remote_id,
                        hci_spec::ConnectionHandle link_handle,
                        uint16_t max_tx_sdu_size,
                        hci::ResultCallback<> callback);

  // Request the stop of A2DP source offloading on a specific channel.
  // |callback| will be called with the result of the request.
  // If offloading was not started or the channel requested is not offloaded,
  // report success. Returns kInProgress error if channel offloading is
  // currently in the process of stopping.
  void RequestStopA2dpOffload(ChannelId local_id,
                              hci_spec::ConnectionHandle link_handle,
                              hci::ResultCallback<> callback);

  // Returns true if channel with |id| and |link_handle| is starting/has started
  // A2DP offloading
  bool IsChannelOffloaded(ChannelId id,
                          hci_spec::ConnectionHandle link_handle) const;

  WeakPtr<A2dpOffloadManager> GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  // Defines the state of A2DP offloading to the controller.
  enum class A2dpOffloadStatus : uint8_t {
    // The A2DP offload command was received and successfully started.
    kStarted,
    // The A2DP offload command was sent and the L2CAP channel is waiting for a
    // response.
    kStarting,
    // The A2DP offload stop command was sent and the L2CAP channel is waiting
    // for a response.
    kStopping,
    // Either an error or an A2DP offload command stopped offloading to the
    // controller.
    kStopped,
  };

  hci::CommandChannel::WeakPtr cmd_channel_;

  A2dpOffloadStatus a2dp_offload_status_ = A2dpOffloadStatus::kStopped;

  // Identifier for offloaded channel's endpoint on this device
  std::optional<ChannelId> offloaded_channel_id_;

  // Connection handle of the offloaded channel's underlying logical link
  std::optional<hci_spec::ConnectionHandle> offloaded_link_handle_;

  // Contains a callback if stop command was requested before offload status was
  // |kStarted|
  std::optional<hci::ResultCallback<>> pending_stop_a2dp_offload_request_;

  WeakSelf<A2dpOffloadManager> weak_self_{this};
};

}  // namespace bt::l2cap
