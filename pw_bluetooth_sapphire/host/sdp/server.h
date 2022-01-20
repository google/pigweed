// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_

#include <lib/fit/function.h>
#include <lib/stdcompat/optional.h>
#include <lib/sys/inspect/cpp/component.h>
#include <lib/zx/socket.h>

#include <map>

#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sdp {

// The SDP server object owns the Service Database and all Service Records.
// Only one server is expected to exist per host.
// This object is not thread-safe.
// TODO(jamuraa): make calls thread-safe or ensure single-threadedness
class Server final {
 public:
  static constexpr const char* kInspectNodeName = "sdp_server";

  // A new SDP server, which starts with just a ServiceDiscoveryService record.
  // Registers itself with |l2cap| when created.
  explicit Server(fbl::RefPtr<l2cap::L2cap> l2cap);
  ~Server();

  // Attach SDP server inspect node as a child node of |parent|.
  void AttachInspect(inspect::Node& parent, std::string name = kInspectNodeName);

  // Initialize a new SDP profile connection with |peer_id| on |channel|.
  // Returns false if the channel cannot be activated.
  bool AddConnection(fbl::RefPtr<l2cap::Channel> channel);

  // An identifier for a set of services that have been registered at the same time.
  using RegistrationHandle = uint32_t;

  const RegistrationHandle kNotRegistered = 0x00000000;

  // Given incomplete ServiceRecords, register services that will be made available over SDP.
  // Takes ownership of |records|. Channels created for this service will be configured using the
  // preferred parameters in |chan_params|.
  //
  // A non-zero RegistrationHandle will be returned if the service was successfully registered.
  //
  // If any record in |records| fails registration checks, none of the services will be registered.
  //
  // |conn_cb| will be called for any connections made to any of the services in |records| with a
  // connected channel and the descriptor list for the endpoint which was connected.
  using ConnectCallback = fit::function<void(fbl::RefPtr<l2cap::Channel>, const DataElement&)>;
  RegistrationHandle RegisterService(std::vector<ServiceRecord> records,
                                     l2cap::ChannelParameters chan_params, ConnectCallback conn_cb);

  // Unregister services previously registered with RegisterService. Idempotent.
  // Returns |true| if any records were removed.
  bool UnregisterService(RegistrationHandle handle);

  // Define the ServiceDiscoveryService record for the SDP server object.
  // This method is public for testing purposes.
  ServiceRecord MakeServiceDiscoveryService();

  // Construct a response based on input packet |sdu| and max size
  // |max_tx_sdu_size|. Note that this function can both be called by means of
  // connecting an l2cap::Channel and directly querying its database. As long
  // as the database does not change between requests, both of these approaches
  // are compatible.
  // This function will drop the packet if the PDU is too short, and it will
  // handle most errors by returning a valid packet with an ErrorResponse.
  cpp17::optional<ByteBufferPtr> HandleRequest(ByteBufferPtr sdu, uint16_t max_tx_sdu_size);

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

  // An array of PSM to ServiceHandle assignments that are used to represent
  // the services that need to be registered in Server::QueueService.
  using ProtocolQueue = std::vector<std::pair<l2cap::PSM, ServiceHandle>>;

  // Given a complete ServiceRecord, extracts the PSM, ProtocolDescriptorList, and
  // any AditionalProtocolDescriptorList information.
  // Inserts the extracted info into |psm_to_register|.
  //
  // Returns |true| if the protocols are successfully validated and queued,
  // |false| otherwise.
  bool QueueService(ServiceRecord* record, ProtocolQueue* protocols_to_register);

  // l2cap::Channel callbacks
  void OnChannelClosed(l2cap::Channel::UniqueId channel_id);

  // Updates the property values associated with the |sdp_server_node_|.
  void UpdateInspectProperties();

  // Send |bytes| over the channel associated with the connection handle
  // |conn|. Logs an error if channel not found.
  void Send(l2cap::Channel::UniqueId channel_id, ByteBufferPtr bytes);

  // The data domain that owns the L2CAP layer.  Used to register callbacks for
  // the channels of services registered.
  fbl::RefPtr<l2cap::L2cap> l2cap_;

  struct InspectProperties {
    // Inspect hierarchy node representing the sdp server.
    inspect::Node sdp_server_node;

    // Each ServiceRecord has it's record and nodes associated wth the registered PSMs.
    struct InspectServiceRecordProperties {
      InspectServiceRecordProperties(std::string record, std::unordered_set<l2cap::PSM> psms);
      void AttachInspect(inspect::Node& parent, std::string name);
      inspect::Node node;
      // The record description.
      const std::string record;
      inspect::StringProperty record_property;
      // The node for the registered PSMs.
      inspect::Node psms_node;
      // The currently registered PSMs.
      const std::unordered_set<l2cap::PSM> psms;
      std::vector<std::pair<inspect::Node, inspect::StringProperty>> psm_nodes;
    };

    // The currently registered ServiceRecords.
    std::vector<InspectServiceRecordProperties> svc_record_properties;
  };
  InspectProperties inspect_properties_;

  // Map of channels that are opened to the server.  Keyed by the channels unique id.
  std::unordered_map<l2cap::Channel::UniqueId, l2cap::ScopedChannel> channels_;
  // The map of ServiceHandles that are associated with ServiceRecords.
  // This is a 1:1 mapping.
  std::unordered_map<ServiceHandle, ServiceRecord> records_;

  // Which PSMs are registered to services. Multiple ServiceHandles can be registered
  // to a single PSM.
  std::unordered_map<l2cap::PSM, std::unordered_set<ServiceHandle>> psm_to_service_;
  // The set of PSMs that are registered to a service.
  std::unordered_map<ServiceHandle, std::unordered_set<l2cap::PSM>> service_to_psms_;

  // The next available ServiceHandle.
  ServiceHandle next_handle_;

  // The set of ServiceHandles that are registered together, identified by a RegistrationHandle.
  std::unordered_map<RegistrationHandle, std::set<ServiceHandle>> reg_to_service_;

  // The service database state tracker.
  uint32_t db_state_ __UNUSED;

  fxl::WeakPtrFactory<Server> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Server);
};

}  // namespace bt::sdp

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVER_H_
