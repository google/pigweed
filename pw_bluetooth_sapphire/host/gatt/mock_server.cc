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

#include "pw_bluetooth_sapphire/internal/host/gatt/mock_server.h"

#include "pw_bluetooth_sapphire/internal/host/att/att.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"
#include "pw_unit_test/framework.h"

namespace bt::gatt::testing {

MockServer::MockServer(PeerId peer_id,
                       LocalServiceManager::WeakPtr local_services)
    : peer_id_(peer_id),
      local_services_(std::move(local_services)),
      weak_self_(this) {}

void MockServer::SendUpdate(IdType service_id,
                            IdType chrc_id,
                            BufferView value,
                            IndicationCallback indicate_cb) {
  if (update_handler_) {
    update_handler_(service_id, chrc_id, value, std::move(indicate_cb));
  } else {
    ADD_FAILURE() << "notification/indication sent without an update_handler_";
  }
}

}  // namespace bt::gatt::testing
