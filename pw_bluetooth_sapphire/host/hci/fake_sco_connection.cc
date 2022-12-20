// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_sco_connection.h"

namespace bt::hci::testing {

FakeScoConnection::FakeScoConnection(hci_spec::ConnectionHandle handle,
                                     const DeviceAddress& local_address,
                                     const DeviceAddress& peer_address,
                                     const hci::Transport::WeakPtr& hci)
    : ScoConnection(handle, local_address, peer_address, hci) {}

}  // namespace bt::hci::testing
