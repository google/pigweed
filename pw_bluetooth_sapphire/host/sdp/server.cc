// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <lib/async/default.h>

#include <cstdint>
#include <cstdio>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"

namespace bt::sdp {

using RegistrationHandle = Server::RegistrationHandle;

namespace {

constexpr const char* kInspectRegisteredPsmName = "registered_psms";
constexpr const char* kInspectPsmName = "psm";
constexpr const char* kInspectRecordName = "record";

bool IsQueuedPsm(const std::vector<std::pair<l2cap::Psm, ServiceHandle>>* queued_psms,
                 l2cap::Psm psm) {
  auto is_queued = [target = psm](const auto& psm_pair) { return target == psm_pair.first; };
  auto iter = std::find_if(queued_psms->begin(), queued_psms->end(), is_queued);
  return iter != queued_psms->end();
}

// Returns true if the |psm| is considered valid.
bool IsValidPsm(l2cap::Psm psm) {
  // The least significant bit of the most significant octet must be 0
  // (Core 5.4, Vol 3, Part A, 4.2).
  constexpr uint16_t MS_OCTET_MASK = 0x0100;
  if (psm & MS_OCTET_MASK) {
    return false;
  }

  // The least significant bit of all other octets must be 1
  // (Core 5.4, Vol 3, Part A, 4.2).
  constexpr uint16_t LOWER_OCTET_MASK = 0x0001;
  if ((psm & LOWER_OCTET_MASK) != LOWER_OCTET_MASK) {
    return false;
  }
  return true;
}

// Updates the |protocol_list| with the provided dynamic |psm|.
// Returns true if the list was updated, false if the list couldn't be updated.
bool UpdateProtocolWithPsm(DataElement& protocol_list, l2cap::Psm psm) {
  const auto* l2cap_protocol = protocol_list.At(0);
  BT_DEBUG_ASSERT(l2cap_protocol);
  const auto* prot_uuid = l2cap_protocol->At(0);
  if (!prot_uuid || prot_uuid->type() != DataElement::Type::kUuid ||
      *prot_uuid->Get<UUID>() != protocol::kL2CAP) {
    bt_log(TRACE, "sdp", "ProtocolDescriptorList is not valid or not L2CAP");
    return false;
  }
  std::vector<DataElement> result;
  // Rebuild the L2CAP protocol by adding UUID & new PSM.
  result.emplace_back(prot_uuid->Clone());
  result.emplace_back(DataElement(uint16_t{psm}));

  // Copy over the remaining protocol descriptors.
  const DataElement* it;
  for (size_t idx = 1; nullptr != (it = protocol_list.At(idx)); idx++) {
    result.emplace_back(it->Clone());
  }

  protocol_list = DataElement(std::move(result));
  bt_log(TRACE, "sdp", "Updated protocol list with dynamic PSM %s",
         protocol_list.ToString().c_str());
  return true;
}

// Finds the PSM that is specified in a ProtocolDescriptorList
// Returns l2cap::kInvalidPsm if none is found or the list is invalid
l2cap::Psm FindProtocolListPsm(const DataElement& protocol_list) {
  bt_log(TRACE, "sdp", "Trying to find PSM from %s", protocol_list.ToString().c_str());
  const auto* l2cap_protocol = protocol_list.At(0);
  BT_DEBUG_ASSERT(l2cap_protocol);
  const auto* prot_uuid = l2cap_protocol->At(0);
  if (!prot_uuid || prot_uuid->type() != DataElement::Type::kUuid ||
      *prot_uuid->Get<UUID>() != protocol::kL2CAP) {
    bt_log(TRACE, "sdp", "ProtocolDescriptorList is not valid or not L2CAP");
    return l2cap::kInvalidPsm;
  }

  const auto* psm_elem = l2cap_protocol->At(1);
  if (psm_elem && psm_elem->Get<uint16_t>()) {
    return *psm_elem->Get<uint16_t>();
  }
  if (psm_elem) {
    bt_log(TRACE, "sdp", "ProtocolDescriptorList invalid L2CAP parameter type");
    return l2cap::kInvalidPsm;
  }

  // The PSM is missing, determined by the next protocol.
  const auto* next_protocol = protocol_list.At(1);
  if (!next_protocol) {
    bt_log(TRACE, "sdp", "L2CAP has no PSM and no additional protocol");
    return l2cap::kInvalidPsm;
  }
  const auto* next_protocol_uuid = next_protocol->At(0);
  if (!next_protocol_uuid || next_protocol_uuid->type() != DataElement::Type::kUuid) {
    bt_log(TRACE, "sdp", "L2CAP has no PSM and additional protocol invalid");
    return l2cap::kInvalidPsm;
  }
  UUID protocol_uuid = *next_protocol_uuid->Get<UUID>();
  // When it's RFCOMM, the L2CAP protocol descriptor omits the PSM parameter
  // See example in the SPP Spec, v1.2
  if (protocol_uuid == protocol::kRFCOMM) {
    return l2cap::kRFCOMM;
  }
  bt_log(TRACE, "sdp", "Can't determine L2CAP PSM from protocol");
  return l2cap::kInvalidPsm;
}

l2cap::Psm PsmFromProtocolList(const DataElement* protocol_list) {
  const auto* primary_protocol = protocol_list->At(0);
  if (!primary_protocol) {
    bt_log(TRACE, "sdp", "ProtocolDescriptorList is not a sequence");
    return l2cap::kInvalidPsm;
  }

  const auto* prot_uuid = primary_protocol->At(0);
  if (!prot_uuid || prot_uuid->type() != DataElement::Type::kUuid) {
    bt_log(TRACE, "sdp", "ProtocolDescriptorList is not valid");
    return l2cap::kInvalidPsm;
  }

  // We do nothing for primary protocols that are not L2CAP
  if (*prot_uuid->Get<UUID>() != protocol::kL2CAP) {
    return l2cap::kInvalidPsm;
  }

  l2cap::Psm psm = FindProtocolListPsm(*protocol_list);
  if (psm == l2cap::kInvalidPsm) {
    bt_log(TRACE, "sdp", "Couldn't find PSM from ProtocolDescriptorList");
    return l2cap::kInvalidPsm;
  }

  return psm;
}

// Sets the browse group list of the record to be the top-level group.
void SetBrowseGroupList(ServiceRecord* record) {
  std::vector<DataElement> browse_list;
  browse_list.emplace_back(kPublicBrowseRootUuid);
  record->SetAttribute(kBrowseGroupList, DataElement(std::move(browse_list)));
}

}  // namespace

// The VersionNumberList value. (5.0, Vol 3, Part B, 5.2.3)
constexpr uint16_t kVersion = 0x0100;  // Version 1.0

// The initial ServiceDatabaseState
constexpr uint32_t kInitialDbState = 0;

// Populates the ServiceDiscoveryService record.
ServiceRecord Server::MakeServiceDiscoveryService() {
  ServiceRecord sdp;
  sdp.SetHandle(kSDPHandle);

  // ServiceClassIDList attribute should have the
  // ServiceDiscoveryServerServiceClassID
  // See v5.0, Vol 3, Part B, Sec 5.2.2
  sdp.SetServiceClassUUIDs({profile::kServiceDiscoveryClass});

  // The VersionNumberList attribute. See v5.0, Vol 3, Part B, Sec 5.2.3
  // Version 1.0
  std::vector<DataElement> version_attribute;
  version_attribute.emplace_back(kVersion);
  sdp.SetAttribute(kSDP_VersionNumberList, DataElement(std::move(version_attribute)));

  // ServiceDatabaseState attribute. Changes when a service gets added or
  // removed.
  sdp.SetAttribute(kSDP_ServiceDatabaseState, DataElement(kInitialDbState));

  return sdp;
}

Server::Server(l2cap::ChannelManager* l2cap)
    : l2cap_(l2cap), next_handle_(kFirstUnreservedHandle), db_state_(0), weak_ptr_factory_(this) {
  BT_ASSERT(l2cap_);

  records_.emplace(kSDPHandle, Server::MakeServiceDiscoveryService());

  // Register SDP
  l2cap::ChannelParameters sdp_chan_params;
  sdp_chan_params.mode = l2cap::ChannelMode::kBasic;
  l2cap_->RegisterService(l2cap::kSDP, sdp_chan_params,
                          [self = weak_ptr_factory_.GetWeakPtr()](auto channel) {
                            if (self.is_alive())
                              self->AddConnection(channel);
                          });

  // SDP is used by SDP server.
  psm_to_service_.emplace(l2cap::kSDP, std::unordered_set<ServiceHandle>({kSDPHandle}));
  service_to_psms_.emplace(kSDPHandle, std::unordered_set<l2cap::Psm>({l2cap::kSDP}));

  // Update the inspect properties after Server initialization.
  UpdateInspectProperties();
}

Server::~Server() { l2cap_->UnregisterService(l2cap::kSDP); }

void Server::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_properties_.sdp_server_node = parent.CreateChild(name);
  UpdateInspectProperties();
}

bool Server::AddConnection(l2cap::Channel::WeakPtr channel) {
  BT_ASSERT(channel.is_alive());
  hci_spec::ConnectionHandle handle = channel->link_handle();
  bt_log(DEBUG, "sdp", "add connection handle %#.4x", handle);

  l2cap::Channel::UniqueId chan_id = channel->unique_id();
  auto iter = channels_.find(chan_id);
  if (iter != channels_.end()) {
    bt_log(WARN, "sdp", "l2cap channel to %#.4x already connected", handle);
    return false;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  bool activated = channel->Activate(
      [self, chan_id, max_tx_sdu_size = channel->max_tx_sdu_size()](ByteBufferPtr sdu) {
        if (self.is_alive()) {
          auto packet = self->HandleRequest(std::move(sdu), max_tx_sdu_size);
          if (packet) {
            self->Send(chan_id, std::move(packet.value()));
          }
        }
      },
      [self, chan_id] {
        if (self.is_alive()) {
          self->OnChannelClosed(chan_id);
        }
      });
  if (!activated) {
    bt_log(WARN, "sdp", "failed to activate channel (handle %#.4x)", handle);
    return false;
  }
  self->channels_.emplace(chan_id, std::move(channel));
  return true;
}

bool Server::AddPsmToProtocol(ProtocolQueue* protocols_to_register, l2cap::Psm psm,
                              ServiceHandle handle) const {
  if (psm == l2cap::kInvalidPsm) {
    return false;
  }

  if (IsAllocated(psm)) {
    bt_log(TRACE, "sdp", "L2CAP PSM %#.4x is already allocated", psm);
    return false;
  }

  auto data = std::make_pair(psm, handle);
  protocols_to_register->emplace_back(std::move(data));
  return true;
}

l2cap::Psm Server::GetDynamicPsm(const ProtocolQueue* queued_psms) const {
  // Generate a random PSM in the valid range of PSMs.
  // RNG(Range(MIN, MAX)) = MIN + RNG(MAX-MIN) where MIN = kMinDynamicPSM = 0x1001. MAX = 0xffff.
  uint16_t offset = 0;
  constexpr uint16_t MAX_MINUS_MIN = 0xeffe;
  random_generator()->GetInt(offset, MAX_MINUS_MIN);
  uint16_t psm = l2cap::kMinDynamicPsm + offset;
  // LSB of upper octet must be 0. LSB of lower octet must be 1.
  constexpr uint16_t UPPER_OCTET_MASK = 0xFEFF;
  constexpr uint16_t LOWER_OCTET_MASK = 0x0001;
  psm &= UPPER_OCTET_MASK;
  psm |= LOWER_OCTET_MASK;
  bt_log(DEBUG, "sdp", "Trying random dynamic PSM %#.4x", psm);

  // Check if the PSM is valid (e.g. valid construction, not allocated, & not queued).
  if ((IsValidPsm(psm)) && (!IsAllocated(psm)) && (!IsQueuedPsm(queued_psms, psm))) {
    bt_log(TRACE, "sdp", "Generated random dynamic PSM %#.4x", psm);
    return psm;
  }

  // Otherwise, fall back to sequentially finding the next available PSM.
  bool search_wrapped = false;
  for (uint16_t next_psm = psm + 2; next_psm <= UINT16_MAX; next_psm += 2) {
    if ((IsValidPsm(next_psm)) && (!IsAllocated(next_psm)) &&
        (!IsQueuedPsm(queued_psms, next_psm))) {
      bt_log(TRACE, "sdp", "Generated sequential dynamic PSM %#.4x", next_psm);
      return next_psm;
    }

    // If we reach the max valid PSM, wrap around to the minimum valid dynamic PSM. Only try this
    // once.
    if (next_psm == 0xFEFF) {
      next_psm = l2cap::kMinDynamicPsm;
      if (search_wrapped) {
        break;
      }
      search_wrapped = true;
    }
  }
  bt_log(WARN, "sdp", "Couldn't find an available dynamic PSM");
  return l2cap::kInvalidPsm;
}

bool Server::QueueService(ServiceRecord* record, ProtocolQueue* protocols_to_register) {
  // ProtocolDescriptorList handling:
  if (record->HasAttribute(kProtocolDescriptorList)) {
    const auto& primary_protocol = record->GetAttribute(kProtocolDescriptorList);
    auto psm = PsmFromProtocolList(&primary_protocol);
    if (psm == kDynamicPsm) {
      bt_log(TRACE, "sdp", "Primary protocol contains dynamic PSM");
      auto primary_protocol_copy = primary_protocol.Clone();
      psm = GetDynamicPsm(protocols_to_register);
      if (!UpdateProtocolWithPsm(primary_protocol_copy, psm)) {
        return false;
      }
      record->SetAttribute(kProtocolDescriptorList, std::move(primary_protocol_copy));
    }
    if (!AddPsmToProtocol(protocols_to_register, psm, record->handle())) {
      return false;
    }
  }

  // AdditionalProtocolDescriptorList handling:
  if (record->HasAttribute(kAdditionalProtocolDescriptorList)) {
    // |additional_list| is a list of ProtocolDescriptorLists.
    const auto& additional_list = record->GetAttribute(kAdditionalProtocolDescriptorList);
    size_t attribute_id = 0;
    const auto* additional = additional_list.At(attribute_id);

    // If `kAdditionalProtocolDescriptorList` exists, there should be at least one
    // protocol provided.
    if (!additional) {
      bt_log(TRACE, "sdp", "AdditionalProtocolDescriptorList provided but empty");
      return false;
    }

    // Add valid additional PSMs to the register queue. Because some additional protocols may need
    // dynamic PSM assignment, modify the relevant protocols and rebuild the list.
    std::vector<DataElement> additional_protocols;
    while (additional) {
      auto psm = PsmFromProtocolList(additional);
      auto additional_protocol_copy = additional->Clone();
      if (psm == kDynamicPsm) {
        bt_log(TRACE, "sdp", "Additional protocol contains dynamic PSM");
        psm = GetDynamicPsm(protocols_to_register);
        if (!UpdateProtocolWithPsm(additional_protocol_copy, psm)) {
          return l2cap::kInvalidPsm;
        }
      }
      if (!AddPsmToProtocol(protocols_to_register, psm, record->handle())) {
        return false;
      }

      attribute_id++;
      additional_protocols.emplace_back(std::move(additional_protocol_copy));
      additional = additional_list.At(attribute_id);
    }
    record->SetAttribute(kAdditionalProtocolDescriptorList,
                         DataElement(std::move(additional_protocols)));
  }

  // For some services that depend on OBEX, the L2CAP PSM is specified in the GoepL2capPsm
  // attribute.
  bool has_obex = record->FindUUID(std::unordered_set<UUID>({protocol::kOBEX}));
  if (has_obex && record->HasAttribute(kGoepL2capPsm)) {
    const auto& attribute = record->GetAttribute(kGoepL2capPsm);
    if (attribute.Get<uint16_t>()) {
      auto psm = *attribute.Get<uint16_t>();
      // If a dynamic PSM was requested, attempt to allocate the next available PSM.
      if (psm == kDynamicPsm) {
        bt_log(TRACE, "sdp", "GoepL2capAttribute contains dynamic PSM");
        psm = GetDynamicPsm(protocols_to_register);
        record->SetAttribute(kGoepL2capPsm, DataElement(uint16_t{psm}));
      }
      if (!AddPsmToProtocol(protocols_to_register, psm, record->handle())) {
        return false;
      }
    }
  }

  return true;
}

RegistrationHandle Server::RegisterService(std::vector<ServiceRecord> records,
                                           l2cap::ChannelParameters chan_params,
                                           ConnectCallback conn_cb) {
  if (records.empty()) {
    return 0;
  }

  // The PSMs and their ServiceHandles to register.
  ProtocolQueue protocols_to_register;

  // The ServiceHandles that are assigned to each ServiceRecord.
  // There should be one ServiceHandle per ServiceRecord in |records|.
  std::set<ServiceHandle> assigned_handles;

  for (auto& record : records) {
    ServiceHandle next = GetNextHandle();
    if (!next) {
      return 0;
    }
    // Assign a new handle for the service record.
    record.SetHandle(next);

    if (!record.IsProtocolOnly()) {
      // Place record in a browse group.
      SetBrowseGroupList(&record);

      // Validate the |ServiceRecord|.
      if (!record.IsRegisterable()) {
        return 0;
      }
    }

    // Attempt to queue the |record| for registration.
    // Note: Since the validation & queueing operations for ALL the records
    // occur before registration, multiple ServiceRecords can share the same PSM.
    //
    // If any |record| is not parsable, exit the registration process early.
    if (!QueueService(&record, &protocols_to_register)) {
      return 0;
    }

    // For every ServiceRecord, there will be one ServiceHandle assigned.
    assigned_handles.emplace(next);
  }

  BT_ASSERT(assigned_handles.size() == records.size());

  // The RegistrationHandle is the smallest ServiceHandle that was assigned.
  RegistrationHandle reg_handle = *assigned_handles.begin();

  // Multiple ServiceRecords in |records| can request the same PSM. However,
  // |l2cap_| expects a single target for each PSM to go to. Consequently,
  // only the first occurrence of a PSM needs to be registered with the |l2cap_|.
  std::unordered_set<l2cap::Psm> psms_to_register;

  // All PSMs have assigned handles and will be registered.
  for (auto& [psm, handle] : protocols_to_register) {
    psm_to_service_[psm].insert(handle);
    service_to_psms_[handle].insert(psm);

    // Add unique PSMs to the data domain registration queue.
    psms_to_register.insert(psm);
  }

  for (const auto& psm : psms_to_register) {
    bt_log(TRACE, "sdp", "Allocating PSM %#.4x for new service", psm);
    l2cap_->RegisterService(
        psm, chan_params,
        [psm = psm, conn_cb = conn_cb.share()](l2cap::Channel::WeakPtr channel) mutable {
          bt_log(TRACE, "sdp", "Channel connected to %#.4x", psm);
          // Build the L2CAP descriptor
          std::vector<DataElement> protocol_l2cap;
          protocol_l2cap.emplace_back(protocol::kL2CAP);
          protocol_l2cap.emplace_back(psm);
          std::vector<DataElement> protocol;
          protocol.emplace_back(std::move(protocol_l2cap));
          conn_cb(std::move(channel), DataElement(std::move(protocol)));
        });
  }

  // Store the complete records.
  for (auto& record : records) {
    auto [it, success] = records_.emplace(record.handle(), std::move(record));
    BT_DEBUG_ASSERT(success);
    const ServiceRecord& placed_record = it->second;
    if (placed_record.IsProtocolOnly()) {
      bt_log(TRACE, "sdp", "registered protocol-only service %#.8x, Protocol: %s",
             placed_record.handle(), bt_str(placed_record.GetAttribute(kProtocolDescriptorList)));
    } else {
      bt_log(TRACE, "sdp", "registered service %#.8x, classes: %s", placed_record.handle(),
             bt_str(placed_record.GetAttribute(kServiceClassIdList)));
    }
  }

  // Store the RegistrationHandle that represents the set of services that were registered.
  reg_to_service_[reg_handle] = std::move(assigned_handles);

  // Update the inspect properties.
  UpdateInspectProperties();

  return reg_handle;
}

bool Server::UnregisterService(RegistrationHandle handle) {
  if (handle == kNotRegistered) {
    return false;
  }

  auto handles_it = reg_to_service_.extract(handle);
  if (!handles_it) {
    return false;
  }

  for (const auto& svc_h : handles_it.mapped()) {
    BT_ASSERT(svc_h != kSDPHandle);
    BT_ASSERT(records_.find(svc_h) != records_.end());
    bt_log(DEBUG, "sdp", "unregistering service (handle: %#.8x)", svc_h);

    // Unregister any service callbacks from L2CAP
    auto psms_it = service_to_psms_.extract(svc_h);
    if (psms_it) {
      for (const auto& psm : psms_it.mapped()) {
        bt_log(DEBUG, "sdp", "removing registration for psm %#.4x", psm);
        l2cap_->UnregisterService(psm);
        psm_to_service_.erase(psm);
      }
    }

    records_.erase(svc_h);
  }

  // Update the inspect properties as the registered PSMs may have changed.
  UpdateInspectProperties();

  return true;
}

ServiceHandle Server::GetNextHandle() {
  ServiceHandle initial_next_handle = next_handle_;
  // We expect most of these to be free.
  // Safeguard against possibly having to wrap-around and reuse handles.
  while (records_.count(next_handle_)) {
    if (next_handle_ == kLastHandle) {
      bt_log(WARN, "sdp", "service handle wrapped to start");
      next_handle_ = kFirstUnreservedHandle;
    } else {
      next_handle_++;
    }
    if (next_handle_ == initial_next_handle) {
      return 0;
    }
  }
  return next_handle_++;
}

ServiceSearchResponse Server::SearchServices(const std::unordered_set<UUID>& pattern) const {
  ServiceSearchResponse resp;
  std::vector<ServiceHandle> matched;
  for (const auto& it : records_) {
    if (it.second.FindUUID(pattern) && !it.second.IsProtocolOnly()) {
      matched.push_back(it.first);
    }
  }
  bt_log(TRACE, "sdp", "ServiceSearch matched %zu records", matched.size());
  resp.set_service_record_handle_list(matched);
  return resp;
}

ServiceAttributeResponse Server::GetServiceAttributes(
    ServiceHandle handle, const std::list<AttributeRange>& ranges) const {
  ServiceAttributeResponse resp;
  const auto& record = records_.at(handle);
  for (const auto& range : ranges) {
    auto attrs = record.GetAttributesInRange(range.start, range.end);
    for (const auto& attr : attrs) {
      resp.set_attribute(attr, record.GetAttribute(attr).Clone());
    }
  }
  bt_log(TRACE, "sdp", "ServiceAttribute %zu attributes", resp.attributes().size());
  return resp;
}

ServiceSearchAttributeResponse Server::SearchAllServiceAttributes(
    const std::unordered_set<UUID>& search_pattern,
    const std::list<AttributeRange>& attribute_ranges) const {
  ServiceSearchAttributeResponse resp;
  for (const auto& it : records_) {
    const auto& rec = it.second;
    if (rec.IsProtocolOnly()) {
      continue;
    }
    if (rec.FindUUID(search_pattern)) {
      for (const auto& range : attribute_ranges) {
        auto attrs = rec.GetAttributesInRange(range.start, range.end);
        for (const auto& attr : attrs) {
          resp.SetAttribute(it.first, attr, rec.GetAttribute(attr).Clone());
        }
      }
    }
  }

  bt_log(TRACE, "sdp", "ServiceSearchAttribute %zu records", resp.num_attribute_lists());
  return resp;
}

void Server::OnChannelClosed(l2cap::Channel::UniqueId channel_id) { channels_.erase(channel_id); }

std::optional<ByteBufferPtr> Server::HandleRequest(ByteBufferPtr sdu, uint16_t max_tx_sdu_size) {
  BT_DEBUG_ASSERT(sdu);
  TRACE_DURATION("bluetooth", "sdp::Server::HandleRequest");
  if (sdu->size() < sizeof(Header)) {
    bt_log(DEBUG, "sdp", "PDU too short; dropping");
    return std::nullopt;
  }
  PacketView<Header> packet(sdu.get());
  TransactionId tid = betoh16(packet.header().tid);
  uint16_t param_length = betoh16(packet.header().param_length);
  auto error_response_builder = [tid, max_tx_sdu_size](ErrorCode code) -> ByteBufferPtr {
    return ErrorResponse(code).GetPDU(0 /* ignored */, tid, max_tx_sdu_size, BufferView());
  };
  if (param_length != (sdu->size() - sizeof(Header))) {
    bt_log(TRACE, "sdp", "request isn't the correct size (%hu != %zu)", param_length,
           sdu->size() - sizeof(Header));
    return error_response_builder(ErrorCode::kInvalidSize);
  }
  packet.Resize(param_length);
  switch (packet.header().pdu_id) {
    case kServiceSearchRequest: {
      ServiceSearchRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(DEBUG, "sdp", "ServiceSearchRequest not valid");
        return error_response_builder(ErrorCode::kInvalidRequestSyntax);
      }
      auto resp = SearchServices(request.service_search_pattern());

      auto bytes = resp.GetPDU(request.max_service_record_count(), tid, max_tx_sdu_size,
                               request.ContinuationState());
      if (!bytes) {
        return error_response_builder(ErrorCode::kInvalidContinuationState);
      }
      return std::move(bytes);
    }
    case kServiceAttributeRequest: {
      ServiceAttributeRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(TRACE, "sdp", "ServiceAttributeRequest not valid");
        return error_response_builder(ErrorCode::kInvalidRequestSyntax);
      }
      auto handle = request.service_record_handle();
      auto record_it = records_.find(handle);
      if (record_it == records_.end() || record_it->second.IsProtocolOnly()) {
        bt_log(TRACE, "sdp", "ServiceAttributeRequest can't find handle %#.8x", handle);
        return error_response_builder(ErrorCode::kInvalidRecordHandle);
      }
      auto resp = GetServiceAttributes(handle, request.attribute_ranges());
      auto bytes = resp.GetPDU(request.max_attribute_byte_count(), tid, max_tx_sdu_size,
                               request.ContinuationState());
      if (!bytes) {
        return error_response_builder(ErrorCode::kInvalidContinuationState);
      }
      return std::move(bytes);
    }
    case kServiceSearchAttributeRequest: {
      ServiceSearchAttributeRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(TRACE, "sdp", "ServiceSearchAttributeRequest not valid");
        return error_response_builder(ErrorCode::kInvalidRequestSyntax);
      }
      auto resp =
          SearchAllServiceAttributes(request.service_search_pattern(), request.attribute_ranges());
      auto bytes = resp.GetPDU(request.max_attribute_byte_count(), tid, max_tx_sdu_size,
                               request.ContinuationState());
      if (!bytes) {
        return error_response_builder(ErrorCode::kInvalidContinuationState);
      }
      return std::move(bytes);
    }
    case kErrorResponse: {
      bt_log(TRACE, "sdp", "ErrorResponse isn't allowed as a request");
      return error_response_builder(ErrorCode::kInvalidRequestSyntax);
    }
    default: {
      bt_log(TRACE, "sdp", "unhandled request, returning InvalidRequest");
      return error_response_builder(ErrorCode::kInvalidRequestSyntax);
    }
  }
}

void Server::Send(l2cap::Channel::UniqueId channel_id, ByteBufferPtr bytes) {
  auto it = channels_.find(channel_id);
  if (it == channels_.end()) {
    bt_log(ERROR, "sdp", "can't find peer to respond to; dropping");
    return;
  }
  l2cap::Channel::WeakPtr chan = it->second.get();
  chan->Send(std::move(bytes));
}

void Server::UpdateInspectProperties() {
  // Skip update if node has not been attached.
  if (!inspect_properties_.sdp_server_node) {
    return;
  }

  // Clear the previous inspect data.
  inspect_properties_.svc_record_properties.clear();

  for (const auto& svc_record : records_) {
    auto record_string = svc_record.second.ToString();
    auto psms_it = service_to_psms_.find(svc_record.first);
    std::unordered_set<l2cap::Psm> psm_set;
    if (psms_it != service_to_psms_.end()) {
      psm_set = psms_it->second;
    }

    InspectProperties::InspectServiceRecordProperties svc_rec_props(std::move(record_string),
                                                                    std::move(psm_set));
    auto& parent = inspect_properties_.sdp_server_node;
    svc_rec_props.AttachInspect(parent, parent.UniqueName(kInspectRecordName));

    inspect_properties_.svc_record_properties.push_back(std::move(svc_rec_props));
  }
}

std::set<l2cap::Psm> Server::AllocatedPsmsForTest() const {
  std::set<l2cap::Psm> allocated;
  for (auto it = psm_to_service_.begin(); it != psm_to_service_.end(); ++it) {
    allocated.insert(it->first);
  }
  return allocated;
}

Server::InspectProperties::InspectServiceRecordProperties::InspectServiceRecordProperties(
    std::string record, std::unordered_set<l2cap::Psm> psms)
    : record(std::move(record)), psms(std::move(psms)) {}

void Server::InspectProperties::InspectServiceRecordProperties::AttachInspect(inspect::Node& parent,
                                                                              std::string name) {
  node = parent.CreateChild(name);
  record_property = node.CreateString(kInspectRecordName, record);
  psms_node = node.CreateChild(kInspectRegisteredPsmName);
  psm_nodes.clear();
  for (const auto& psm : psms) {
    auto psm_node = psms_node.CreateChild(psms_node.UniqueName(kInspectPsmName));
    auto psm_string = psm_node.CreateString(kInspectPsmName, l2cap::PsmToString(psm));
    psm_nodes.emplace_back(std::move(psm_node), std::move(psm_string));
  }
}

}  // namespace bt::sdp
