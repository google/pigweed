// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_proxy/internal/l2cap_status_tracker.h"

#include <mutex>

#include "pw_containers/algorithm.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

void L2capStatusTracker::RegisterDelegate(L2capStatusDelegate& delegate) {
  std::lock_guard lock(mutex_);
  delegates_.push_front(delegate);
}

void L2capStatusTracker::UnregisterDelegate(L2capStatusDelegate& delegate) {
  std::lock_guard lock(mutex_);
  delegates_.remove(delegate);
}

void L2capStatusTracker::HandleConnectionComplete(
    const L2capChannelConnectionInfo& info) {
  std::lock_guard lock(mutex_);
  bool track = false;
  for (L2capStatusDelegate& delegate : delegates_) {
    if (!delegate.ShouldTrackPsm(info.psm)) {
      continue;
    }

    track = true;
    delegate.HandleConnectionComplete(info);
  }

  if (track) {
    if (connected_channel_infos_.full()) {
      // TODO: https://pwbug.dev/379558046 - Let client know we won't be able to
      // notify on disconnect.
      PW_LOG_ERROR(
          "Couldn't track l2cap channel connection as requested, so will not "
          "be able to send disconnect event to client.");
      return;
    }
    connected_channel_infos_.push_back(info);
  }
}

void L2capStatusTracker::HandleDisconnectionComplete(
    uint16_t connection_handle) {
  std::lock_guard lock(mutex_);
  for (size_t i = 0; i < connected_channel_infos_.size();) {
    L2capChannelConnectionInfo& info = connected_channel_infos_[i];

    if (info.connection_handle == connection_handle) {
      containers::ForEach(delegates_, [info](L2capStatusDelegate& delegate) {
        if (delegate.ShouldTrackPsm(info.psm)) {
          delegate.HandleDisconnectionComplete(info);
        }
      });
      // Deleting this entry in Vector, so do not increment index.
      connected_channel_infos_.erase(&info);
    } else {
      // Not deleting this entry in Vector, so increment index.
      ++i;
    }
  }
}

void L2capStatusTracker::HandleDisconnectionComplete(
    const DisconnectParams& params) {
  std::lock_guard lock(mutex_);
  for (L2capStatusDelegate& delegate : delegates_) {
    auto match = [&params](const L2capChannelConnectionInfo& i) {
      return params.connection_handle == i.connection_handle &&
             params.remote_cid == i.remote_cid &&
             params.local_cid == i.local_cid;
    };
    auto connection_it = std::find_if(connected_channel_infos_.begin(),
                                      connected_channel_infos_.end(),
                                      match);
    if (connection_it == connected_channel_infos_.end()) {
      return;
    }

    delegate.HandleDisconnectionComplete(*connection_it);
    connected_channel_infos_.erase(connection_it);
  }
}

}  // namespace pw::bluetooth::proxy
