// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/data/socket_factory.cc"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"

namespace bt {
namespace data {
namespace internal {

template class SocketFactory<l2cap::Channel>;

}  // namespace internal
}  // namespace data
}  // namespace bt
