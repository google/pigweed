// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/sco_connection.h"

#include "src/connectivity/bluetooth/core/bt-host/data/socket_channel_relay.cc"

namespace bt {
namespace data {

template class SocketChannelRelay<gap::ScoConnection>;

}  // namespace data
}  // namespace bt
