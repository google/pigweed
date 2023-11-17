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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVICE_RECORD_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVICE_RECORD_H_

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pw_bluetooth_sapphire/internal/host/sdp/sdp.h"

namespace bt::sdp {

// A ServiceRecord represents a service record in a SDP database.
// The service has a number of attributes identified by defined IDs and each
// attribute has a value.
class ServiceRecord {
 public:
  // Create a new service record.
  // Generates a UUID and sets the Service ID attribute.
  ServiceRecord();

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ServiceRecord);
  // Allow move.
  ServiceRecord(ServiceRecord&&) = default;

  // Directly sets an attribute to a specific DataElement
  void SetAttribute(AttributeId id, DataElement value);

  // Get the value of an attribute. The attribute must be set.
  // Use HasAttribute() to detect if an attribute is set.
  const DataElement& GetAttribute(AttributeId id) const;

  // Returns true if there is an attribute with |id| in this record.
  bool HasAttribute(AttributeId id) const;

  // Removes the attribute identified by |id|. Idempotent.
  void RemoveAttribute(AttributeId id);

  // Protocol-only services only reserve a protocol endpoint, and don't
  // advertise a service in the SDP database.
  bool IsProtocolOnly() const;

  // Returns true if the ServiceRecord contains the required fields
  // needed for SDP registration.
  bool IsRegisterable() const;

  // Returns the handle of this service.
  ServiceHandle handle() const { return handle_; }

  void SetHandle(ServiceHandle handle);

  // Returns the set of attributes in this record that are in
  // the range |start| - |end| inclusive.
  // If |start| > |end| or no attributes are present, returns a
  // an empty set.
  std::set<AttributeId> GetAttributesInRange(AttributeId start,
                                             AttributeId end) const;

  // Returns true if any value of the attributes in this service contain all
  // of the |uuids| given.  The uuids need not be in any specific attribute
  // value.
  bool FindUUID(const std::unordered_set<UUID>& uuids) const;

  // Convenience function to set the service class id list attribute.
  void SetServiceClassUUIDs(const std::vector<UUID>& classes);

  using ProtocolListId = uint8_t;

  constexpr static ProtocolListId kPrimaryProtocolList = 0x00;

  // Adds a protocol to a protocol descriptor list.
  // Convenience function for adding protocol descriptor list attributes.
  // |id| identifies the list to be added to.
  // |uuid| must be a protocol UUID.
  // |params| is either:
  //   - a DataElement sequence of parameters
  //   - a null DataElement, for which nothing will be appended
  //   - a single DataElement parameter
  // kPrimaryProtocolList is presented as the primary protocol.
  // Other protocol will be added to the additional protocol lists,
  void AddProtocolDescriptor(const ProtocolListId id,
                             const UUID& uuid,
                             DataElement params);

  // Adds a profile to the bluetooth profile descriptor list attribute.
  // |uuid| is the UUID of the profile. |major| and |minor| are the major and
  // minor versions of the profile supported.
  void AddProfile(const UUID& uuid, uint8_t major, uint8_t minor);

  // Adds a set of language attributes.
  // |language| is required (and must be two characters long)
  // At least one other attribute must be non-empty.
  // Empty attributes will be omitted.
  // All strings are UTF-8 encoded.
  // Returns true if attributes were added, false otherwise.
  bool AddInfo(const std::string& language_code,
               const std::string& name,
               const std::string& description,
               const std::string& provider);

  // Set the security level required to connect to this service.
  // See v5.0, Vol 3, Part C, Section 5.2.2.8
  void set_security_level(SecurityLevel security_level) {
    security_level_ = security_level;
  }
  SecurityLevel security_level() const { return security_level_; }

  // Debug representation of a service record used for Inspect.
  // Only includes kBluetoothProfileDescriptorList and kServiceClassIdList to
  // minimize log spam.
  std::string ToString() const;

 private:
  ServiceHandle handle_;

  std::map<AttributeId, DataElement> attributes_;

  // Additional protocol lists, by id.
  // Each one of these elements is a sequence of the form that would qualify as
  // a protocol list (a sequence of sequences of protocols and params)
  std::unordered_map<ProtocolListId, DataElement> addl_protocols_;

  SecurityLevel security_level_;
};

}  // namespace bt::sdp

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SERVICE_RECORD_H_
