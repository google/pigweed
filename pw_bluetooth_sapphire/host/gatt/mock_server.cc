// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gatt/mock_server.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"

namespace bt::gatt::testing {

MockServer::MockServer(PeerId peer_id, LocalServiceManager::WeakPtr local_services)
    : peer_id_(peer_id), local_services_(std::move(local_services)), weak_self_(this) {}

void MockServer::SendUpdate(IdType service_id, IdType chrc_id, BufferView value,
                            IndicationCallback indicate_cb) {
  if (update_handler_) {
    update_handler_(service_id, chrc_id, value, std::move(indicate_cb));
  } else {
    ADD_FAILURE() << "notification/indication sent without an update_handler_";
  }
}

}  // namespace bt::gatt::testing
