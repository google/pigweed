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

#pragma once
#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/att/bearer.h"
#include "pw_bluetooth_sapphire/internal/host/att/database.h"
#include "pw_bluetooth_sapphire/internal/host/common/uuid.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/local_service_manager.h"

namespace bt {

namespace att {
class Attribute;
class Database;
class PacketReader;
}  // namespace att

namespace gatt {
using IndicationCallback = att::ResultCallback<>;

// A GATT Server implements the server-role of the ATT protocol over a single
// ATT Bearer. A unique Server instance should exist for each logical link that
// supports GATT.
//
// A Server responds to incoming requests by querying the database that it
// is initialized with. Each Server shares an att::Bearer with a Client.
class Server {
 public:
  // Constructs a new Server bearer.
  // |peer_id| is the unique system identifier for the peer device.
  // |local_services| will be used to resolve inbound/outbound transactions.
  // |bearer| is the ATT data bearer that this Server operates on. It must
  // outlive this Server.
  static std::unique_ptr<Server> Create(
      PeerId peer_id,
      LocalServiceManager::WeakPtr local_services,
      att::Bearer::WeakPtr bearer);
  // Servers can be constructed without production att::Bearers (e.g. for
  // testing), so the FactoryFunction type reflects that.
  using FactoryFunction = fit::function<std::unique_ptr<Server>(
      PeerId, LocalServiceManager::WeakPtr)>;

  virtual ~Server() = default;

  // Sends a Handle-Value notification or indication PDU on the given |chrc_id|
  // within |service_id|. If |indicate_cb| is nullptr, a notification is sent.
  // Otherwise, an indication is sent, and indicate_cb is called with the result
  // of the indication. The underlying att::Bearer will disconnect the link if a
  // confirmation is not received in a timely manner.
  virtual void SendUpdate(IdType service_id,
                          IdType chrc_id,
                          BufferView value,
                          IndicationCallback indicate_cb) = 0;

  // Shuts down the transport on which this Server operates, which may also
  // disconnect any other objects using the same transport, like the
  // gatt::Client.
  virtual void ShutDown() = 0;
};

}  // namespace gatt
}  // namespace bt
