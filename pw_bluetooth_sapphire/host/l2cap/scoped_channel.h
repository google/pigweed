// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"

namespace bt::l2cap {

// A Channel wrapper that automatically deactivates a channel when it gets
// deleted.
class ScopedChannel final {
 public:
  explicit ScopedChannel(fxl::WeakPtr<Channel> channel);
  ScopedChannel() = default;
  ~ScopedChannel();

  // Returns true if there is an open underlying channel.
  [[nodiscard]] bool is_active() const { return static_cast<bool>(chan_); }

  // Resets the underlying channel to the one that is provided. Any previous
  // channel will be deactivated.
  void Reset(fxl::WeakPtr<Channel> new_channel);

  void operator=(decltype(nullptr)) { Close(); }
  explicit operator bool() const { return is_active(); }

  Channel* get() const { return chan_.get(); }
  Channel* operator->() const { return get(); }

  // Returns a copy of the underlying Channel reference without releasing
  // ownership.  The channel will still be deactivated when this goes out
  // of scope.
  inline fxl::WeakPtr<Channel> share() const { return chan_; }

 private:
  void Close();

  fxl::WeakPtr<Channel> chan_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScopedChannel);
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
