// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"

namespace bt {
namespace l2cap {

// A Channel wrapper that automatically deactivates a channel when it gets
// deleted.
class ScopedChannel final {
 public:
  explicit ScopedChannel(fbl::RefPtr<Channel> channel);
  ScopedChannel() = default;
  ~ScopedChannel();

  // Resets the underlying channel to the one that is provided. Any previous
  // channel will be deactivated.
  void Reset(fbl::RefPtr<Channel> new_channel);

  inline void operator=(decltype(nullptr)) { Close(); }
  inline operator bool() const { return static_cast<bool>(chan_); }

  inline Channel* get() const { return chan_.get(); }
  inline Channel* operator->() const { return get(); }

  // Returns a copy of the underlying Channel reference without releasing
  // ownership.  The channel will still be deactivated when this goes out
  // of scope.
  inline fbl::RefPtr<Channel> share() const { return chan_; }

 private:
  void Close();

  fbl::RefPtr<Channel> chan_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScopedChannel);
};

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SCOPED_CHANNEL_H_
