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

#include "pw_bluetooth_proxy/direction.h"
#include "pw_containers/intrusive_forward_list.h"

namespace pw::bluetooth::proxy {

struct L2capChannelConnectionInfo {
  Direction direction;
  uint16_t psm;
  uint16_t connection_handle;
  // Otherwise known as source_cid
  uint16_t remote_cid;
  // Otherwise known as destination_cid
  uint16_t local_cid;
};

class L2capStatusDelegate
    : public IntrusiveForwardList<L2capStatusDelegate>::Item {
 public:
  virtual ~L2capStatusDelegate() = default;

  /// Should return true if the implementor is interested in l2cap channel
  /// connections for this psm.
  virtual bool ShouldTrackPsm(uint16_t psm) = 0;

  /// Called when a l2cap channel connection successfully made. Note: this
  /// doesn't currently handle credit based l2cap channels.
  virtual void HandleConnectionComplete(
      const L2capChannelConnectionInfo& info) = 0;

  /// Called when a l2cap channel connection is disconnected.
  ///
  /// Note you cannot Register or Unregister a delegate in this method.
  virtual void HandleDisconnectionComplete(
      const L2capChannelConnectionInfo& info) = 0;
};

}  // namespace pw::bluetooth::proxy
