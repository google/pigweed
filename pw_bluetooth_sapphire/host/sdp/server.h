// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_

#include <lib/fit/function.h>
#include <lib/zx/socket.h>

#include <map>

#include <fbl/function.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sdp {

// The SDP server object owns the Service Database and all Service Records.
// Only one server is expected to exist per host.
// This object is not thread-safe.
// TODO(jamuraa): make calls thread-safe or ensure single-threadedness
class Server final {
 public:
  // A new SDP server, which starts with just a ServiceDiscoveryService record.
  // Registers itself with |l2cap| when created.
  explicit Server(fbl::RefPtr<data::Domain> data_domain);
  ~Server();

  // Initialize a new SDP profile connection with |peer_id| on |channel|.
  // Returns false if the channel cannot be activated.
  bool AddConnection(fbl::RefPtr<l2cap::Channel> channel);

  // Given an incomplete ServiceRecord, register a service that will be made available over SDP.
  // Takes ownership of |record|. Channels created for this service will be configured using the
  // preferred parameters in |chan_params|.
  //
  // A non-zero ServiceHandle will be returned if the service was successfully registered. Any
  // service handle previously set in |record| is ignored and overwritten.
  //
  // |conn_cb| will be called for any connections made to the registered service with a connected
  // socket, the accepted channel parameters, the connection handle the channel was opened on, and
  // the descriptor list for the endpoint which was connected.
  // TODO: possibly combine these into a struct later
  using ConnectCallback =
      fit::function<void(l2cap::ChannelSocket, hci::ConnectionHandle, const DataElement&)>;
  ServiceHandle RegisterService(ServiceRecord record, l2cap::ChannelParameters chan_params,
                                ConnectCallback conn_cb);

  // Unregister a service from the database. Idempotent.
  // Returns |true| if a record was removed.
  bool UnregisterService(ServiceHandle handle);

  // Define the ServiceDiscoveryService record for the SDP server object.
  // This method is public for testing purposes.
  ServiceRecord MakeServiceDiscoveryService();

 private:
  // Returns the next unused Service Handle, or 0 if none are available.
  ServiceHandle GetNextHandle();

  // Performs a Service Search, returning any service record that contains
  // all UUID from the |search_pattern|
  ServiceSearchResponse SearchServices(const std::unordered_set<UUID>& pattern) const;

  // Gets Service Attributes in the |attribute_ranges| from the service record
  // with |handle|.
  ServiceAttributeResponse GetServiceAttributes(ServiceHandle handle,
                                                const std::list<AttributeRange>& ranges) const;

  // Retrieves Service Attributes in the |attribute_ranges|, using the pattern
  // to search for the services that contain all UUIDs from the |search_pattern|
  ServiceSearchAttributeResponse SearchAllServiceAttributes(
      const std::unordered_set<UUID>& search_pattern,
      const std::list<AttributeRange>& attribute_ranges) const;

  // Attempts to extract the PSM from the protocol list.
  // Returns the PSM if successful, otherwise |kInvalidPSM|.
  l2cap::PSM PSMFromProtocolList(ServiceRecord* record, const DataElement* protocol_list);

  // l2cap::Channel callbacks
  void OnChannelClosed(const hci::ConnectionHandle& handle);
  void OnRxBFrame(const hci::ConnectionHandle& handle, ByteBufferPtr sdu, uint16_t max_tx_sdu_size);

  // The data domain that owns the L2CAP layer.  Used to register callbacks for
  // the channels of services registered.
  fbl::RefPtr<data::Domain> data_domain_;

  std::unordered_map<hci::ConnectionHandle, l2cap::ScopedChannel> channels_;
  std::unordered_map<ServiceHandle, ServiceRecord> records_;

  // Which PSMs are registered to services.
  std::unordered_map<l2cap::PSM, ServiceHandle> psm_to_service_;
  // The set of PSMs that are registered to a service.
  std::unordered_map<ServiceHandle, std::unordered_set<l2cap::PSM>> service_to_psms_;

  // The next available ServiceHandle.
  ServiceHandle next_handle_;

  // The service database state tracker.
  uint32_t db_state_ __UNUSED;

  fxl::WeakPtrFactory<Server> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Server);
};

}  // namespace sdp
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_
