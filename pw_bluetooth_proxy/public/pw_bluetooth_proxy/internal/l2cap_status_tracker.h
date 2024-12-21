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

#pragma once

#include <cstdint>

#include "pw_bluetooth_proxy/l2cap_status_delegate.h"
#include "pw_containers/vector.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

/// Thread safe collection of service delegates and l2cap channel connections
/// currently being tracked.
///
/// TODO: https://pwbug.dev/382138082 - Tracked connections should be cleaned up
/// when their ACL connection goes away.
class L2capStatusTracker {
 public:
  void RegisterDelegate(L2capStatusDelegate& delegate)
      PW_LOCKS_EXCLUDED(mutex_);

  void UnregisterDelegate(L2capStatusDelegate& delegate)
      PW_LOCKS_EXCLUDED(mutex_);

  void HandleConnectionComplete(const L2capChannelConnectionInfo& info)
      PW_LOCKS_EXCLUDED(mutex_);

  struct DisconnectParams {
    uint16_t connection_handle;
    uint16_t remote_cid;
    uint16_t local_cid;
  };

  void HandleDisconnectionComplete(uint16_t connection_handle)
      PW_LOCKS_EXCLUDED(mutex_);

  void HandleDisconnectionComplete(const DisconnectParams& params)
      PW_LOCKS_EXCLUDED(mutex_);

 private:
  IntrusiveForwardList<L2capStatusDelegate> delegates_ PW_GUARDED_BY(mutex_);

  // TODO: https://pwbug.dev/369849508 - Use allocator to let client control max
  // here.
  static constexpr size_t kMaxTrackedConnections = 10;

  // Contains an entry for each channel connection open that has a delegate
  // registered for it's PSM.
  Vector<L2capChannelConnectionInfo, kMaxTrackedConnections>
      connected_channel_infos_ PW_GUARDED_BY(mutex_){};

  sync::Mutex mutex_;
};

}  // namespace pw::bluetooth::proxy
