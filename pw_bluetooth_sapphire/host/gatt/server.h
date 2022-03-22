// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_SERVER_H_

#include <lib/fit/function.h>

#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/att/bearer.h"
#include "src/connectivity/bluetooth/core/bt-host/att/database.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/lib/fxl/memory/weak_ptr.h"

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
  // |bearer| is the ATT data bearer that this Server operates on.
  static std::unique_ptr<Server> Create(PeerId peer_id,
                                        fxl::WeakPtr<LocalServiceManager> local_services,
                                        fbl::RefPtr<att::Bearer> bearer);
  // Servers can be constructed without production att::Bearers (e.g. for testing), so the
  // FactoryFunction type reflects that.
  using FactoryFunction =
      fit::function<std::unique_ptr<Server>(PeerId, fxl::WeakPtr<LocalServiceManager>)>;

  virtual ~Server() = default;

  // Sends a Handle-Value notification or indication PDU on the given |chrc_id| within |service_id|.
  // If |indicate_cb| is nullptr, a notification is sent. Otherwise, an indication is sent, and
  // indicate_cb is called with the result of the indication. The underlying att::Bearer will
  // disconnect the link if a confirmation is not received in a timely manner.
  virtual void SendUpdate(IdType service_id, IdType chrc_id, BufferView value,
                          IndicationCallback indicate_cb) = 0;

  // Shuts down the transport on which this Server operates, which may also disconnect any other
  // objects using the same transport, like the gatt::Client.
  virtual void ShutDown() = 0;
};

}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_SERVER_H_
