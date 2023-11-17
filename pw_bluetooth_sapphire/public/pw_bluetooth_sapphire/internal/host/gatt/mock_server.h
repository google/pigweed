// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_MOCK_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_MOCK_SERVER_H_

#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/server.h"

namespace bt::gatt::testing {
using UpdateHandler = fit::function<void(IdType service_id,
                                         IdType chrc_id,
                                         BufferView value,
                                         IndicationCallback indicate_cb)>;

// A mock implementation of a gatt::Server object. Can be used to mock outbound
// notifications/ indications without a production att::Bearer in tests.
class MockServer : public Server {
 public:
  MockServer(PeerId peer_id, LocalServiceManager::WeakPtr local_services);

  void set_update_handler(UpdateHandler handler) {
    update_handler_ = std::move(handler);
  }

  using WeakPtr = WeakSelf<MockServer>::WeakPtr;
  MockServer::WeakPtr AsMockWeakPtr() { return weak_self_.GetWeakPtr(); }

  bool was_shut_down() const { return was_shut_down_; }

 private:
  // Server overrides:
  void SendUpdate(IdType service_id,
                  IdType chrc_id,
                  BufferView value,
                  IndicationCallback indicate_cb) override;
  void ShutDown() override { was_shut_down_ = true; }

  PeerId peer_id_;
  LocalServiceManager::WeakPtr local_services_;
  UpdateHandler update_handler_ = nullptr;
  bool was_shut_down_ = false;
  WeakSelf<MockServer> weak_self_;
};

}  // namespace bt::gatt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_MOCK_SERVER_H_
