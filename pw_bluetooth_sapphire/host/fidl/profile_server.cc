// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/status.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

namespace fidlbredr = fuchsia::bluetooth::bredr;
using fidlbredr::DataElement;
using fidlbredr::Profile;

namespace bthost {

namespace {

bt::l2cap::ChannelParameters FidlToChannelParameters(const fidlbredr::ChannelParameters& fidl) {
  bt::l2cap::ChannelParameters params;
  if (fidl.has_channel_mode()) {
    switch (fidl.channel_mode()) {
      case fidlbredr::ChannelMode::BASIC:
        params.mode = bt::l2cap::ChannelMode::kBasic;
        break;
      case fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION:
        params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
        break;
      default:
        ZX_PANIC("FIDL channel parameter contains invalid mode");
    }
  }
  if (fidl.has_max_rx_sdu_size()) {
    params.max_rx_sdu_size = fidl.max_rx_sdu_size();
  }
  return params;
}

fidlbredr::ChannelMode ChannelModeToFidl(bt::l2cap::ChannelMode mode) {
  switch (mode) {
    case bt::l2cap::ChannelMode::kBasic:
      return fidlbredr::ChannelMode::BASIC;
      break;
    case bt::l2cap::ChannelMode::kEnhancedRetransmission:
      return fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION;
      break;
    default:
      ZX_PANIC("Could not convert channel parameter mode to unsupported FIDL mode");
  }
}

fidlbredr::Channel ChannelSocketToFidlChannel(bt::l2cap::ChannelSocket chan_sock) {
  fidlbredr::Channel chan;
  if (!chan_sock) {
    return chan;
  }
  chan.set_socket(std::move(chan_sock.socket));
  chan.set_channel_mode(ChannelModeToFidl(chan_sock.params->mode));
  chan.set_max_tx_sdu_size(chan_sock.params->max_tx_sdu_size);
  return chan;
}

bool FidlToDataElement(const fidlbredr::DataElement& fidl, bt::sdp::DataElement* out) {
  ZX_DEBUG_ASSERT(out);
  switch (fidl.Which()) {
    case fidlbredr::DataElement::Tag::kInt8:
      out->Set(fidl.int8());
      break;
    case fidlbredr::DataElement::Tag::kInt16:
      out->Set(fidl.int16());
      break;
    case fidlbredr::DataElement::Tag::kInt32:
      out->Set(fidl.int32());
      break;
    case fidlbredr::DataElement::Tag::kInt64:
      out->Set(fidl.int64());
      break;
    case fidlbredr::DataElement::Tag::kUint8:
      out->Set(fidl.uint8());
      break;
    case fidlbredr::DataElement::Tag::kUint16:
      out->Set(fidl.uint16());
      break;
    case fidlbredr::DataElement::Tag::kUint32:
      out->Set(fidl.uint32());
      break;
    case fidlbredr::DataElement::Tag::kUint64:
      out->Set(fidl.uint64());
      break;
    case fidlbredr::DataElement::Tag::kStr:
      out->Set(fidl.str());
      break;
    case fidlbredr::DataElement::Tag::kB:
      out->Set(fidl.b());
      break;
    case fidlbredr::DataElement::Tag::kUuid:
      out->Set(fidl_helpers::UuidFromFidl(fidl.uuid()));
      break;
    case fidlbredr::DataElement::Tag::kSequence: {
      std::vector<bt::sdp::DataElement> seq;
      for (const auto& fidl_elem : fidl.sequence()) {
        bt::sdp::DataElement it;
        if (!FidlToDataElement(*fidl_elem, &it)) {
          return false;
        }
        seq.emplace_back(std::move(it));
      }
      out->Set(std::move(seq));
      break;
    }
    case fidlbredr::DataElement::Tag::kAlternatives: {
      std::vector<bt::sdp::DataElement> alts;
      for (const auto& fidl_elem : fidl.alternatives()) {
        bt::sdp::DataElement it;
        if (!FidlToDataElement(*fidl_elem, &it)) {
          return false;
        }
        alts.emplace_back(std::move(it));
      }
      out->SetAlternative(std::move(alts));
      break;
    }
    default:
      // Types not handled: Null datatype (never used) and Url data type (not supported by Set)
      bt_log(WARN, "profile_server", "Encountered FidlToDataElement type not handled.");
      return false;
  }
  return true;
}

fidlbredr::DataElementPtr DataElementToFidl(const bt::sdp::DataElement* in) {
  auto elem = fidlbredr::DataElement::New();
  bt_log(SPEW, "sdp", "DataElementToFidl: %s", in->ToString().c_str());
  ZX_DEBUG_ASSERT(in);
  switch (in->type()) {
    case bt::sdp::DataElement::Type::kUnsignedInt: {
      switch (in->size()) {
        case bt::sdp::DataElement::Size::kOneByte:
          elem->set_uint8(*in->Get<uint8_t>());
          break;
        case bt::sdp::DataElement::Size::kTwoBytes:
          elem->set_uint16(*in->Get<uint16_t>());
          break;
        case bt::sdp::DataElement::Size::kFourBytes:
          elem->set_uint32(*in->Get<uint32_t>());
          break;
        case bt::sdp::DataElement::Size::kEightBytes:
          elem->set_uint64(*in->Get<uint64_t>());
          break;
        default:
          bt_log(INFO, "profile_server", "no 128-bit integer support in FIDL yet");
          return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kSignedInt: {
      switch (in->size()) {
        case bt::sdp::DataElement::Size::kOneByte:
          elem->set_int8(*in->Get<int8_t>());
          break;
        case bt::sdp::DataElement::Size::kTwoBytes:
          elem->set_int16(*in->Get<int16_t>());
          break;
        case bt::sdp::DataElement::Size::kFourBytes:
          elem->set_int32(*in->Get<int32_t>());
          break;
        case bt::sdp::DataElement::Size::kEightBytes:
          elem->set_int64(*in->Get<int64_t>());
          break;
        default:
          bt_log(INFO, "profile_server", "no 128-bit integer support in FIDL yet");
          return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kUuid: {
      auto uuid = in->Get<bt::UUID>();
      ZX_DEBUG_ASSERT(uuid);
      elem->set_uuid(fidl_helpers::UuidToFidl(*uuid));
      return elem;
    }
    case bt::sdp::DataElement::Type::kString: {
      elem->set_str(*in->Get<std::string>());
      return elem;
    }
    case bt::sdp::DataElement::Type::kBoolean: {
      elem->set_b(*in->Get<bool>());
      return elem;
    }
    case bt::sdp::DataElement::Type::kSequence: {
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->set_sequence(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kAlternative: {
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->set_alternatives(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kUrl: {
      bt_log(INFO, "profile_server", "no support for Url types in DataElement yet");
      return nullptr;
    }
    case bt::sdp::DataElement::Type::kNull: {
      bt_log(INFO, "profile_server", "no support for null DataElement types in FIDL");
      return nullptr;
    }
  }
}

fidlbredr::ProtocolDescriptorPtr DataElementToProtocolDescriptor(const bt::sdp::DataElement* in) {
  auto desc = fidlbredr::ProtocolDescriptor::New();
  if (in->type() != bt::sdp::DataElement::Type::kSequence) {
    return nullptr;
  }
  const auto protocol_uuid = in->At(0)->Get<bt::UUID>();
  if (!protocol_uuid) {
    return nullptr;
  }
  desc->protocol = fidlbredr::ProtocolIdentifier(*protocol_uuid->As16Bit());
  const bt::sdp::DataElement* it;
  for (size_t idx = 1; (it = in->At(idx)); ++idx) {
    desc->params.push_back(std::move(*DataElementToFidl(it)));
  }

  return desc;
}

void AddProtocolDescriptorList(
    bt::sdp::ServiceRecord* rec, bt::sdp::ServiceRecord::ProtocolListId id,
    const ::std::vector<fidlbredr::ProtocolDescriptor>& descriptor_list) {
  bt_log(SPEW, "profile_server", "ProtocolDescriptorList %d", id);
  for (auto& descriptor : descriptor_list) {
    bt::sdp::DataElement protocol_params;
    if (descriptor.params.size() > 1) {
      std::vector<bt::sdp::DataElement> params;
      for (auto& fidl_param : descriptor.params) {
        bt::sdp::DataElement bt_param;
        FidlToDataElement(fidl_param, &bt_param);
        params.emplace_back(std::move(bt_param));
      }
      protocol_params.Set(std::move(params));
    } else if (descriptor.params.size() == 1) {
      FidlToDataElement(descriptor.params.front(), &protocol_params);
    }

    bt_log(SPEW, "profile_server", "%d : %s", fidl::ToUnderlying(descriptor.protocol),
           protocol_params.ToString().c_str());
    rec->AddProtocolDescriptor(id, bt::UUID(static_cast<uint16_t>(descriptor.protocol)),
                               std::move(protocol_params));
  }
}

}  // namespace

ProfileServer::ProfileServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                             fidl::InterfaceRequest<Profile> request)
    : ServerBase(this, std::move(request)),
      advertised_total_(0),
      searches_total_(0),
      adapter_(adapter),
      weak_ptr_factory_(this) {}

ProfileServer::~ProfileServer() {
  if (adapter()) {
    // Unregister anything that we have registered.
    auto sdp = adapter()->sdp_server();
    for (const auto& it : current_advertised_) {
      sdp->UnregisterService(it.second.service_handle);
    }
    auto conn_manager = adapter()->bredr_connection_manager();
    for (const auto& it : searches_) {
      conn_manager->RemoveServiceSearch(it.second.search_id);
    }
  }
}

void ProfileServer::Advertise(
    std::vector<fidlbredr::ServiceDefinition> definitions,
    fidlbredr::SecurityRequirements requirements, fidlbredr::ChannelParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver) {
  // TODO: check that the service definition is valid for useful error messages

  std::vector<bt::sdp::ServiceRecord> registering;

  for (auto& definition : definitions) {
    bt::sdp::ServiceRecord rec;
    std::vector<bt::UUID> classes;

    if (!definition.has_service_class_uuids()) {
      bt_log(INFO, "proile_server", "Advertised service contains no Service UUIDs");
      // Dropping receiver as we didn't register.
      return;
    }

    for (auto& uuid : definition.service_class_uuids()) {
      bt::UUID btuuid = fidl_helpers::UuidFromFidl(uuid);
      bt_log(SPEW, "profile_server", "Setting Service Class UUID %s", bt_str(btuuid));
      classes.emplace_back(std::move(btuuid));
    }

    rec.SetServiceClassUUIDs(classes);

    if (definition.has_protocol_descriptor_list()) {
      AddProtocolDescriptorList(&rec, bt::sdp::ServiceRecord::kPrimaryProtocolList,
                                definition.protocol_descriptor_list());
    }

    if (definition.has_additional_protocol_descriptor_lists()) {
      size_t protocol_list_id = 1;
      for (const auto& descriptor_list : definition.additional_protocol_descriptor_lists()) {
        AddProtocolDescriptorList(&rec, protocol_list_id, descriptor_list);
        protocol_list_id++;
      }
    }

    if (definition.has_profile_descriptors()) {
      for (const auto& profile : definition.profile_descriptors()) {
        bt_log(SPEW, "profile_server", "Adding Profile %#hx v%d.%d", profile.profile_id,
               profile.major_version, profile.minor_version);
        rec.AddProfile(bt::UUID(uint16_t(profile.profile_id)), profile.major_version,
                       profile.minor_version);
      }
    }

    if (definition.has_information()) {
      for (const auto& info : definition.information()) {
        if (!info.has_language()) {
          bt_log(INFO, "profile_server", "Adding information to service definition: no language!");
          // Dropping the receiver as it's not registered.
          return;
        }
        std::string language = info.language();
        std::string name, description, provider;
        if (info.has_name()) {
          name = info.name();
        }
        if (info.has_description()) {
          description = info.description();
        }
        if (info.has_provider()) {
          provider = info.provider();
        }
        bt_log(SPEW, "profile_server", "Adding Info (%s): (%s, %s, %s)", language.c_str(),
               name.c_str(), description.c_str(), provider.c_str());
        rec.AddInfo(language, name, description, provider);
      }
    }

    if (definition.has_additional_attributes()) {
      for (const auto& attribute : definition.additional_attributes()) {
        bt::sdp::DataElement elem;
        FidlToDataElement(attribute.element, &elem);
        bt_log(SPEW, "profile_server", "Adding attribute %#x : %s", attribute.id,
               elem.ToString().c_str());
        rec.SetAttribute(attribute.id, std::move(elem));
      }
    }

    registering.emplace_back(std::move(rec));
  }

  ZX_DEBUG_ASSERT(adapter());
  auto sdp = adapter()->sdp_server();
  ZX_DEBUG_ASSERT(sdp);

  uint64_t next = advertised_total_ + 1;

  // TODO(1346): register more than just the first one.
  auto sdp_handle = sdp->RegisterService(
      std::move(registering.front()), FidlToChannelParameters(parameters),
      [this, next](auto chan_sock, auto handle, const auto& protocol_list) {
        OnChannelConnected(next, std::move(chan_sock), handle, std::move(protocol_list));
      });

  if (!sdp_handle) {
    return;
  };

  auto receiverptr = receiver.Bind();

  receiverptr.set_error_handler(
      [this, next](zx_status_t status) { OnConnectionReceiverError(next, status); });

  current_advertised_.try_emplace(next, std::move(receiverptr), sdp_handle);
  advertised_total_ = next;
}

void ProfileServer::Search(
    fidlbredr::ServiceClassProfileIdentifier service_uuid, std::vector<uint16_t> attr_ids,
    fidl::InterfaceHandle<fuchsia::bluetooth::bredr::SearchResults> results) {
  bt::UUID search_uuid(static_cast<uint32_t>(service_uuid));
  std::unordered_set<bt::sdp::AttributeId> attributes(attr_ids.begin(), attr_ids.end());
  if (!attr_ids.empty()) {
    // Always request the ProfileDescriptor for the event
    attributes.insert(bt::sdp::kBluetoothProfileDescriptorList);
  }

  ZX_DEBUG_ASSERT(adapter());

  auto next = searches_total_ + 1;

  auto search_id = adapter()->bredr_connection_manager()->AddServiceSearch(
      search_uuid, std::move(attributes),
      [this, next](auto id, const auto& attrs) { OnServiceFound(next, id, attrs); });

  if (!search_id) {
    return;
  }

  auto results_ptr = results.Bind();
  results_ptr.set_error_handler(
      [this, next](zx_status_t status) { OnSearchResultError(next, status); });

  searches_.try_emplace(next, std::move(results_ptr), search_id);
  searches_total_ = next;
}

void ProfileServer::Connect(fuchsia::bluetooth::PeerId peer_id, uint16_t psm,
                            fidlbredr::ChannelParameters parameters, ConnectCallback callback) {
  bt::PeerId id{peer_id.value};

  auto connected_cb = [cb = callback.share()](auto chan_sock) {
    cb(fit::ok(ChannelSocketToFidlChannel(std::move(chan_sock))));
  };
  ZX_DEBUG_ASSERT(adapter());

  bool connecting = adapter()->bredr_connection_manager()->OpenL2capChannel(
      id, psm, FidlToChannelParameters(parameters), std::move(connected_cb),
      async_get_default_dispatcher());
  if (!connecting) {
    callback(fit::error(fuchsia::bluetooth::ErrorCode::NOT_FOUND));
  }
}

void ProfileServer::OnChannelConnected(uint64_t ad_id, bt::l2cap::ChannelSocket chan_sock,
                                       bt::hci::ConnectionHandle handle,
                                       const bt::sdp::DataElement& protocol_list) {
  auto it = current_advertised_.find(ad_id);
  if (it == current_advertised_.end()) {
    // The receiver has disappeared, do nothing.
    return;
  }

  ZX_DEBUG_ASSERT(adapter());
  auto id = adapter()->bredr_connection_manager()->GetPeerId(handle);

  // The protocol that is connected should be L2CAP, because that is the only thing that
  // we can connect. We can't say anything about what the higher level protocols will be.
  auto prot_seq = protocol_list.At(0);
  ZX_ASSERT(prot_seq);

  fidlbredr::ProtocolDescriptorPtr desc = DataElementToProtocolDescriptor(prot_seq);
  ZX_ASSERT(desc);

  fuchsia::bluetooth::PeerId peer_id{id.value()};

  std::vector<fidlbredr::ProtocolDescriptor> list;
  list.emplace_back(std::move(*desc));

  it->second.receiver->Connected(peer_id, ChannelSocketToFidlChannel(std::move(chan_sock)),
                                 std::move(list));
}

void ProfileServer::OnConnectionReceiverError(uint64_t ad_id, zx_status_t status) {
  bt_log(SPEW, "profile_server", "Connection receiver closed, ending advertisement %lu", ad_id);

  auto it = current_advertised_.find(ad_id);

  if (it == current_advertised_.end() || !adapter()) {
    return;
  }

  adapter()->sdp_server()->UnregisterService(it->second.service_handle);

  current_advertised_.erase(it);
}

void ProfileServer::OnSearchResultError(uint64_t search_id, zx_status_t status) {
  bt_log(SPEW, "profile_server", "Search result closed, ending search %lu", search_id);

  auto it = searches_.find(search_id);

  if (it == searches_.end() || !adapter()) {
    return;
  }

  adapter()->bredr_connection_manager()->RemoveServiceSearch(it->second.search_id);

  searches_.erase(it);
}

void ProfileServer::OnServiceFound(
    uint64_t search_id, bt::PeerId peer_id,
    const std::map<bt::sdp::AttributeId, bt::sdp::DataElement>& attributes) {
  auto search_it = searches_.find(search_id);
  if (search_it == searches_.end()) {
    // Search was de-registered.
    return;
  }

  // Convert ProfileDescriptor Attribute
  auto it = attributes.find(bt::sdp::kProtocolDescriptorList);

  fidl::VectorPtr<fidlbredr::ProtocolDescriptor> descriptor_list;

  if (it != attributes.end()) {
    std::vector<fidlbredr::ProtocolDescriptor> list;
    size_t idx = 0;
    auto* sdp_list_element = it->second.At(idx);
    while (sdp_list_element != nullptr) {
      fidlbredr::ProtocolDescriptorPtr desc = DataElementToProtocolDescriptor(sdp_list_element);
      if (!desc) {
        break;
      }
      list.push_back(std::move(*desc));
      sdp_list_element = it->second.At(++idx);
    }
    descriptor_list = std::move(list);
  }

  // Add the rest of the attributes
  std::vector<fidlbredr::Attribute> fidl_attrs;

  for (const auto& it : attributes) {
    auto attr = fidlbredr::Attribute::New();
    attr->id = it.first;
    attr->element = std::move(*DataElementToFidl(&it.second));
    fidl_attrs.emplace_back(std::move(*attr));
  }

  fuchsia::bluetooth::PeerId fidl_peer_id{peer_id.value()};

  search_it->second.results->ServiceFound(fidl_peer_id, std::move(descriptor_list),
                                          std::move(fidl_attrs), []() {});
}

}  // namespace bthost
