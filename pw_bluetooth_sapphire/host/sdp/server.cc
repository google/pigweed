// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/rfcomm.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"

namespace bt {
namespace sdp {

namespace {

// The VersionNumberList value. (5.0, Vol 3, Part B, 5.2.3)
constexpr uint16_t kVersion = 0x0100;  // Version 1.0

// The initial ServiceDatabaseState
constexpr uint32_t kInitialDbState = 0;

// Populates the ServiceDiscoveryService record.
ServiceRecord MakeServiceDiscoveryService() {
  ServiceRecord sdp;
  sdp.SetHandle(kSDPHandle);

  // ServiceClassIDList attribute should have the
  // ServiceDiscoveryServerServiceClassID
  // See v5.0, Vol 3, Part B, Sec 5.2.2
  sdp.SetServiceClassUUIDs({profile::kServiceDiscoveryClass});

  // The VersionNumberList attribute. See v5.0, Vol 3, Part B, Sec 5.2.3
  // Version 1.0
  sdp.SetAttribute(kSDP_VersionNumberList, DataElement(std::vector<DataElement>{kVersion}));

  // ServiceDatabaseState attribute. Changes when a service gets added or
  // removed.
  sdp.SetAttribute(kSDP_ServiceDatabaseState, DataElement(kInitialDbState));

  return sdp;
}

void SendErrorResponse(l2cap::Channel* chan, TransactionId tid, uint16_t tx_mtu, ErrorCode code) {
  ErrorResponse response(code);
  chan->Send(response.GetPDU(0 /* ignored */, tid, tx_mtu, BufferView()));
}

// Finds the PSM that is specified in a ProtocolDescriptorList
// Returns l2cap::kInvalidPSM if none is found or the list is invalid
l2cap::PSM FindProtocolListPSM(const DataElement& protocol_list) {
  bt_log(SPEW, "sdp", "Trying to find PSM from %s", protocol_list.ToString().c_str());
  const auto* l2cap_protocol = protocol_list.At(0);
  ZX_DEBUG_ASSERT(l2cap_protocol);
  const auto* prot_uuid = l2cap_protocol->At(0);
  if (!prot_uuid || prot_uuid->type() != DataElement::Type::kUuid ||
      *prot_uuid->Get<UUID>() != protocol::kL2CAP) {
    bt_log(SPEW, "sdp", "ProtocolDescriptorList is not valid or not L2CAP");
    return l2cap::kInvalidPSM;
  }

  const auto* psm_elem = l2cap_protocol->At(1);
  if (psm_elem && psm_elem->type() == DataElement::Type::kUnsignedInt) {
    return *psm_elem->Get<uint16_t>();
  } else if (psm_elem) {
    bt_log(SPEW, "sdp", "ProtocolDescriptorList invalid L2CAP parameter type");
    return l2cap::kInvalidPSM;
  }

  // The PSM is missing, determined by the next protocol.
  const auto* next_protocol = protocol_list.At(1);
  if (!next_protocol) {
    bt_log(SPEW, "sdp", "L2CAP has no PSM and no additional protocol");
    return l2cap::kInvalidPSM;
  }
  const auto* next_protocol_uuid = next_protocol->At(0);
  if (!next_protocol_uuid || next_protocol_uuid->type() != DataElement::Type::kUuid) {
    bt_log(SPEW, "sdp", "L2CAP has no PSM and additional protocol invalid");
    return l2cap::kInvalidPSM;
  }
  UUID protocol_uuid = *next_protocol_uuid->Get<UUID>();
  // When it's RFCOMM, the L2CAP protocol descriptor omits the PSM parameter
  if (protocol_uuid == protocol::kRFCOMM) {
    return l2cap::kRFCOMM;
  }
  bt_log(SPEW, "sdp", "Can't determine L2CAP PSM from protocol");
  return l2cap::kInvalidPSM;
}

// Writes the RFCOMM channel into a ProtocolDescriptorList
DataElement WriteRFCOMMChannel(const DataElement& protocol_list, rfcomm::ServerChannel channel) {
  return protocol_list.Clone();
}

}  // namespace

Server::Server(fbl::RefPtr<data::Domain> data_domain)
    : data_domain_(data_domain),
      next_handle_(kFirstUnreservedHandle),
      db_state_(0),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(data_domain_);

  records_.emplace(kSDPHandle, MakeServiceDiscoveryService());

  // Register SDP
  l2cap::ChannelParameters sdp_chan_params;
  sdp_chan_params.mode = l2cap::ChannelMode::kBasic;
  data_domain_->RegisterService(
      l2cap::kSDP, sdp_chan_params,
      [self = weak_ptr_factory_.GetWeakPtr()](auto channel) {
        if (self)
          self->AddConnection(channel);
      },
      async_get_default_dispatcher());

  // SDP and RFCOMM are used by SDP server.
  psm_to_service_.emplace(l2cap::kSDP, kSDPHandle);
  psm_to_service_.emplace(l2cap::kRFCOMM, kSDPHandle);
}

Server::~Server() { data_domain_->UnregisterService(l2cap::kSDP); }

bool Server::AddConnection(fbl::RefPtr<l2cap::Channel> channel) {
  bt_log(TRACE, "sdp", "add connection handle %#.4x", channel->link_handle());

  hci::ConnectionHandle handle = channel->link_handle();
  auto iter = channels_.find(channel->link_handle());
  if (iter != channels_.end()) {
    bt_log(WARN, "sdp", "handle %#.4x already connected", handle);
    return false;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  bool activated = channel->ActivateWithDispatcher(
      [self, handle, tx_mtu = channel->tx_mtu()](ByteBufferPtr sdu) {
        if (self) {
          self->OnRxBFrame(handle, std::move(sdu), tx_mtu);
        }
      },
      [self, handle] {
        if (self) {
          self->OnChannelClosed(handle);
        }
      },
      async_get_default_dispatcher());
  if (!activated) {
    bt_log(WARN, "sdp", "failed to activate channel (handle %#.4x)", handle);
    return false;
  }
  self->channels_.emplace(handle, std::move(channel));
  return true;
}

ServiceHandle Server::RegisterService(ServiceRecord record, l2cap::ChannelParameters chan_params,
                                      ConnectCallback conn_cb) {
  ServiceHandle next = GetNextHandle();
  if (!next) {
    return 0;
  }

  record.SetHandle(next);

  // Services must at least have a ServiceClassIDList (5.0, Vol 3, Part B, 5.1)
  if (!record.HasAttribute(kServiceClassIdList)) {
    bt_log(SPEW, "sdp", "new record doesn't have a ServiceClass");
    return 0;
  }
  // Class ID list is a data element sequence in which each data element is
  // a UUID representing the service classes that a given service record
  // conforms to. (5.0, Vol 3, Part B, 5.1.2)
  const DataElement& class_id_list = record.GetAttribute(kServiceClassIdList);
  if (class_id_list.type() != DataElement::Type::kSequence) {
    bt_log(SPEW, "sdp", "class ID list isn't a sequence");
    return 0;
  }
  size_t idx;
  const DataElement* elem;
  for (idx = 0; nullptr != (elem = class_id_list.At(idx)); idx++) {
    if (elem->type() != DataElement::Type::kUuid) {
      bt_log(SPEW, "sdp", "class ID list elements are not all UUIDs");
      return 0;
    }
  }
  if (idx == 0) {
    bt_log(SPEW, "sdp", "no elements in the Class ID list (need at least 1)");
    return 0;
  }

  // All services are placed in the top-level browse group.
  std::vector<DataElement> browse_list;
  browse_list.emplace_back(DataElement(kPublicBrowseRootUuid));
  record.SetAttribute(kBrowseGroupList, DataElement(std::move(browse_list)));

  // ProtocolDescriptorList handling:
  if (record.HasAttribute(kProtocolDescriptorList)) {
    const auto& primary_list = record.GetAttribute(kProtocolDescriptorList);
    const auto* primary_protocol = primary_list.At(0);
    if (!primary_protocol) {
      bt_log(SPEW, "sdp", "ProtocolDescriptorList is not a sequence");
      return 0;
    }

    const auto* prot_uuid = primary_protocol->At(0);
    if (!prot_uuid || prot_uuid->type() != DataElement::Type::kUuid) {
      bt_log(SPEW, "sdp", "ProtocolDescriptorList is not valid");
      return 0;
    }

    // We do nothing for primary protocols that are not L2CAP
    if (*prot_uuid->Get<UUID>() == protocol::kL2CAP) {
      l2cap::PSM psm = FindProtocolListPSM(primary_list);
      if (psm == l2cap::kInvalidPSM) {
        bt_log(SPEW, "sdp", "Couldn't find PSM from ProtocolDescriptorList");
        return 0;
      }

      if (!conn_cb) {
        bt_log(SPEW, "sdp", "Connection expected but no conn_cb provided");
        return 0;
      }

      if (psm == l2cap::kRFCOMM) {
        const auto* rfcomm_protocol = primary_list.At(1);
        if (!rfcomm_protocol) {
          bt_log(SPEW, "sdp", "ProtocolDesciptorList missing RFCOMM protocol");
          return 0;
        }
        const auto* rfcomm_uuid = rfcomm_protocol->At(0);
        if (!rfcomm_uuid || rfcomm_uuid->type() != DataElement::Type::kUuid ||
            *rfcomm_uuid->Get<UUID>() != protocol::kRFCOMM) {
          bt_log(SPEW, "sdp", "L2CAP is RFCOMM, but RFCOMM is not specified");
          return 0;
        }
        // TODO(NET-1015): allocate an actual RFCOMM channel with RFCOMM
        rfcomm::ServerChannel rfcomm_channel = 0;
        record.SetAttribute(kProtocolDescriptorList,
                            WriteRFCOMMChannel(primary_list, rfcomm_channel));
      } else if (psm_to_service_.count(psm)) {
        bt_log(SPEW, "sdp", "L2CAP PSM %#.4x is already allocated", psm);
        return 0;
      } else {
        bt_log(SPEW, "sdp", "Allocating PSM %#.4x for new service", psm);
        psm_to_service_.emplace(psm, next);
        data_domain_->RegisterService(
            psm, chan_params,
            [psm, protocol = primary_list.Clone(), conn_cb = std::move(conn_cb)](
                auto chan_sock, auto handle) mutable {
              bt_log(SPEW, "sdp", "Channel connected to %#.4x", psm);
              conn_cb(std::move(chan_sock), handle, std::move(protocol));
            },
            async_get_default_dispatcher());
        auto psm_place = service_to_psms_.emplace(next, std::unordered_set<l2cap::PSM>{psm});
        if (!psm_place.second) {
          psm_place.first->second.insert(psm);
        }
      }
    }
  }

  auto placement = records_.emplace(next, std::move(record));
  ZX_DEBUG_ASSERT(placement.second);
  bt_log(SPEW, "sdp", "registered service %#.8x, classes: %s", next,
         placement.first->second.GetAttribute(kServiceClassIdList).ToString().c_str());
  return next;
}

bool Server::UnregisterService(ServiceHandle handle) {
  if (handle == kSDPHandle || records_.find(handle) == records_.end()) {
    return false;
  }
  bt_log(TRACE, "sdp", "unregistering service (handle: %#.8x)", handle);

  // Unregister any service callbacks from L2CAP
  auto psms_it = service_to_psms_.find(handle);
  if (psms_it != service_to_psms_.end()) {
    for (const auto& psm : psms_it->second) {
      bt_log(TRACE, "sdp", "removing registration for psm %#.4x", psm);
      data_domain_->UnregisterService(psm);
      psm_to_service_.erase(psm);
    }
    service_to_psms_.erase(psms_it);
  }

  records_.erase(handle);
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
    if (it.second.FindUUID(pattern)) {
      matched.push_back(it.first);
    }
  }
  bt_log(SPEW, "sdp", "ServiceSearch matched %zu records", matched.size());
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
  bt_log(SPEW, "sdp", "ServiceAttribute %zu attributes", resp.attributes().size());
  return resp;
}

ServiceSearchAttributeResponse Server::SearchAllServiceAttributes(
    const std::unordered_set<UUID>& search_pattern,
    const std::list<AttributeRange>& attribute_ranges) const {
  ServiceSearchAttributeResponse resp;
  for (const auto& it : records_) {
    const auto& rec = it.second;
    if (rec.FindUUID(search_pattern)) {
      for (const auto& range : attribute_ranges) {
        auto attrs = rec.GetAttributesInRange(range.start, range.end);
        for (const auto& attr : attrs) {
          resp.SetAttribute(it.first, attr, rec.GetAttribute(attr).Clone());
        }
      }
    }
  }

  bt_log(SPEW, "sdp", "ServiceSearchAttribute %zu records", resp.num_attribute_lists());
  return resp;
}

void Server::OnChannelClosed(const hci::ConnectionHandle& handle) { channels_.erase(handle); }

void Server::OnRxBFrame(const hci::ConnectionHandle& handle, ByteBufferPtr sdu, uint16_t tx_mtu) {
  ZX_DEBUG_ASSERT(sdu);
  uint16_t length = sdu->size();
  if (length < sizeof(Header)) {
    bt_log(TRACE, "sdp", "PDU too short; dropping");
    return;
  }

  auto it = channels_.find(handle);
  if (it == channels_.end()) {
    bt_log(TRACE, "sdp", "can't find peer to respond to; dropping");
    return;
  }

  l2cap::Channel* chan = it->second.get();
  PacketView<Header> packet(sdu.get());
  TransactionId tid = betoh16(packet.header().tid);
  uint16_t param_length = betoh16(packet.header().param_length);

  if (param_length != (sdu->size() - sizeof(Header))) {
    bt_log(SPEW, "sdp", "request isn't the correct size (%hu != %zu)", param_length,
           sdu->size() - sizeof(Header));
    SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidSize);
    return;
  }

  packet.Resize(param_length);

  switch (packet.header().pdu_id) {
    case kServiceSearchRequest: {
      ServiceSearchRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(TRACE, "sdp", "ServiceSearchRequest not valid");
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRequestSyntax);
        return;
      }
      auto resp = SearchServices(request.service_search_pattern());

      auto bytes =
          resp.GetPDU(request.max_service_record_count(), tid, tx_mtu, request.ContinuationState());
      if (!bytes) {
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidContinuationState);
        return;
      }
      chan->Send(std::move(bytes));
      return;
    }
    case kServiceAttributeRequest: {
      ServiceAttributeRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(SPEW, "sdp", "ServiceAttributeRequest not valid");
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRequestSyntax);
        return;
      }
      auto handle = request.service_record_handle();
      if (records_.find(handle) == records_.end()) {
        bt_log(SPEW, "sdp", "ServiceAttributeRequest can't find handle %#.8x", handle);
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRecordHandle);
        return;
      }
      auto resp = GetServiceAttributes(handle, request.attribute_ranges());
      auto bytes =
          resp.GetPDU(request.max_attribute_byte_count(), tid, tx_mtu, request.ContinuationState());
      if (!bytes) {
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidContinuationState);
        return;
      }
      chan->Send(std::move(bytes));
      return;
    }
    case kServiceSearchAttributeRequest: {
      ServiceSearchAttributeRequest request(packet.payload_data());
      if (!request.valid()) {
        bt_log(SPEW, "sdp", "ServiceSearchAttributeRequest not valid");
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRequestSyntax);
        return;
      }
      auto resp =
          SearchAllServiceAttributes(request.service_search_pattern(), request.attribute_ranges());
      auto bytes =
          resp.GetPDU(request.max_attribute_byte_count(), tid, tx_mtu, request.ContinuationState());
      if (!bytes) {
        SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidContinuationState);
        return;
      }
      chan->Send(std::move(bytes));
      return;
    }
    case kErrorResponse: {
      bt_log(SPEW, "sdp", "ErrorResponse isn't allowed as a request");
      SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRequestSyntax);
      return;
    }
    default: {
      bt_log(SPEW, "sdp", "unhandled request, returning InvalidRequest");
      SendErrorResponse(chan, tid, tx_mtu, ErrorCode::kInvalidRequestSyntax);
      return;
    }
  }
}

}  // namespace sdp
}  // namespace bt
