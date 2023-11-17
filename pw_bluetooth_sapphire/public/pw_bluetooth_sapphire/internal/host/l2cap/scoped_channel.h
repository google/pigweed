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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"

namespace bt::l2cap {

// A Channel wrapper that automatically deactivates a channel when it gets
// deleted.
class ScopedChannel final {
 public:
  explicit ScopedChannel(Channel::WeakPtr channel);
  ScopedChannel() = default;
  ScopedChannel(ScopedChannel&& other);
  ~ScopedChannel();

  // Returns true if there is an open underlying channel.
  [[nodiscard]] bool is_active() const { return chan_.is_alive(); }

  // Resets the underlying channel to the one that is provided. Any previous
  // channel will be deactivated.
  void Reset(Channel::WeakPtr new_channel);

  ScopedChannel& operator=(decltype(nullptr)) {
    Close();
    return *this;
  }
  explicit operator bool() const { return is_active(); }

  const Channel::WeakPtr& get() const { return chan_; }
  Channel* operator->() const { return &chan_.get(); }

  // Returns a copy of the underlying Channel reference without releasing
  // ownership.  The channel will still be deactivated when this goes out
  // of scope.
  inline Channel::WeakPtr share() const { return chan_; }

 private:
  void Close();

  Channel::WeakPtr chan_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScopedChannel);
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
