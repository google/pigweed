// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/status.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

namespace fidlbredr = fuchsia::bluetooth::bredr;
using fidlbredr::DataElementType;
using fidlbredr::Profile;

namespace bthost {

namespace {

bool FidlToDataElement(const fidlbredr::DataElement& fidl,
                       bt::sdp::DataElement* out) {
  ZX_DEBUG_ASSERT(out);
  switch (fidl.type) {
    case DataElementType::NOTHING:
      out->Set(nullptr);
      return true;
    case DataElementType::UNSIGNED_INTEGER: {
      if (!fidl.data.is_integer()) {
        return false;
      }
      if (fidl.size == 1) {
        out->Set(uint8_t(fidl.data.integer()));
      } else if (fidl.size == 2) {
        out->Set(uint16_t(fidl.data.integer()));
      } else if (fidl.size == 4) {
        out->Set(uint32_t(fidl.data.integer()));
      } else if (fidl.size == 8) {
        out->Set(uint64_t(fidl.data.integer()));
      } else {
        return false;
      }
      return true;
    }
    case DataElementType::SIGNED_INTEGER: {
      if (!fidl.data.is_integer()) {
        return false;
      }
      if (fidl.size == 1) {
        out->Set(int8_t(fidl.data.integer()));
      } else if (fidl.size == 2) {
        out->Set(int16_t(fidl.data.integer()));
      } else if (fidl.size == 4) {
        out->Set(int32_t(fidl.data.integer()));
      } else if (fidl.size == 8) {
        out->Set(int64_t(fidl.data.integer()));
      } else {
        return false;
      }
      return true;
    }
    case DataElementType::UUID: {
      if (!fidl.data.is_uuid()) {
        return false;
      }
      bt::UUID uuid;
      bool success = StringToUuid(fidl.data.uuid(), &uuid);
      if (!success) {
        return false;
      }
      out->Set(uuid);
      return true;
    }
    case DataElementType::STRING: {
      if (!fidl.data.is_str()) {
        return false;
      }
      out->Set(*fidl.data.str());
      return true;
    }
    case DataElementType::BOOLEAN: {
      if (!fidl.data.is_b()) {
        return false;
      }
      out->Set(fidl.data.b());
      return true;
    }
    case DataElementType::SEQUENCE: {
      if (!fidl.data.is_sequence()) {
        return false;
      }
      bool success = true;
      std::vector<bt::sdp::DataElement> elems;
      for (const auto& fidl_elem : fidl.data.sequence()) {
        bt::sdp::DataElement it;
        success = FidlToDataElement(*fidl_elem, &it);
        if (!success) {
          return false;
        }
        elems.emplace_back(std::move(it));
      }
      out->Set(std::move(elems));
      return true;
    }
    default:
      return false;
  }
}

fidlbredr::DataElementPtr DataElementToFidl(const bt::sdp::DataElement* in) {
  auto elem = fidlbredr::DataElement::New();
  bt_log(SPEW, "sdp", "DataElementToFidl: %s", in->ToString().c_str());
  ZX_DEBUG_ASSERT(in);
  switch (in->type()) {
    case bt::sdp::DataElement::Type::kNull:
      elem->type = DataElementType::NOTHING;
      return elem;
    case bt::sdp::DataElement::Type::kUnsignedInt: {
      elem->type = DataElementType::UNSIGNED_INTEGER;
      auto size = in->size();
      if (size == bt::sdp::DataElement::Size::kOneByte) {
        elem->data.set_integer(*in->Get<uint8_t>());
      } else if (size == bt::sdp::DataElement::Size::kTwoBytes) {
        elem->data.set_integer(*in->Get<uint16_t>());
      } else if (size == bt::sdp::DataElement::Size::kFourBytes) {
        elem->data.set_integer(*in->Get<uint32_t>());
      } else if (size == bt::sdp::DataElement::Size::kEightBytes) {
        elem->data.set_integer(*in->Get<uint64_t>());
      } else {
        // TODO: handle 128-bit integers
        bt_log(DEBUG, "profile_server", "no 128-bit integer type yet");
        return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kSignedInt: {
      elem->type = DataElementType::SIGNED_INTEGER;
      auto size = in->size();
      if (size == bt::sdp::DataElement::Size::kOneByte) {
        elem->data.set_integer(*in->Get<int8_t>());
      } else if (size == bt::sdp::DataElement::Size::kTwoBytes) {
        elem->data.set_integer(*in->Get<int16_t>());
      } else if (size == bt::sdp::DataElement::Size::kFourBytes) {
        elem->data.set_integer(*in->Get<int32_t>());
      } else if (size == bt::sdp::DataElement::Size::kEightBytes) {
        elem->data.set_integer(*in->Get<int64_t>());
      } else {
        // TODO: handle 128-bit integers
        bt_log(DEBUG, "profile_server", "no 128-bit integer type yet");
        return nullptr;
      }
      return elem;
    }
    case bt::sdp::DataElement::Type::kUuid: {
      elem->type = DataElementType::UUID;
      auto uuid = in->Get<bt::UUID>();
      ZX_DEBUG_ASSERT(uuid);
      elem->data.uuid() = uuid->ToString();
      return elem;
    }
    case bt::sdp::DataElement::Type::kString: {
      elem->type = DataElementType::STRING;
      elem->data.str() = *in->Get<std::string>();
      return elem;
    }
    case bt::sdp::DataElement::Type::kBoolean: {
      elem->type = DataElementType::BOOLEAN;
      elem->data.set_b(*in->Get<bool>());
      return elem;
    }
    case bt::sdp::DataElement::Type::kSequence: {
      elem->type = DataElementType::SEQUENCE;
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->data.set_sequence(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kAlternative: {
      elem->type = DataElementType::ALTERNATIVE;
      std::vector<fidlbredr::DataElementPtr> elems;
      const bt::sdp::DataElement* it;
      for (size_t idx = 0; (it = in->At(idx)); ++idx) {
        elems.emplace_back(DataElementToFidl(it));
      }
      elem->data.set_sequence(std::move(elems));
      return elem;
    }
    case bt::sdp::DataElement::Type::kUrl: {
      ZX_PANIC("not implemented");
      break;
    }
  }
}

fidlbredr::ProtocolDescriptorPtr DataElementToProtocolDescriptor(
    const bt::sdp::DataElement* in) {
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

fidlbredr::ProfileDescriptorPtr DataElementToProfileDescriptor(
    const bt::sdp::DataElement* in) {
  auto desc = fidlbredr::ProfileDescriptor::New();
  if (in->type() != bt::sdp::DataElement::Type::kSequence) {
    return nullptr;
  }

  const bt::sdp::DataElement* profile_desc = in->At(0);
  const bt::sdp::DataElement* profile_elem = profile_desc->At(0);
  if (!profile_elem) {
    return nullptr;
  }
  const auto profile_uuid = profile_elem->Get<bt::UUID>();
  if (!profile_uuid) {
    return nullptr;
  }
  desc->profile_id =
      fidlbredr::ServiceClassProfileIdentifier(*profile_uuid->As16Bit());

  const bt::sdp::DataElement* version_elem = profile_desc->At(1);
  if (!version_elem) {
    return nullptr;
  }
  const auto version = version_elem->Get<uint16_t>();
  if (!version) {
    return nullptr;
  }

  desc->major_version = static_cast<uint8_t>(*version >> 8);
  desc->minor_version = static_cast<uint8_t>(*version & 0xFF);

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

    bt_log(SPEW, "profile_server", "%d : %s",
           fidl::ToUnderlying(descriptor.protocol),
           protocol_params.ToString().c_str());
    rec->AddProtocolDescriptor(
        id, bt::UUID(static_cast<uint16_t>(descriptor.protocol)),
        std::move(protocol_params));
  }
}

}  // namespace

ProfileServer::ProfileServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                             fidl::InterfaceRequest<Profile> request)
    : AdapterServerBase(adapter, this, std::move(request)),
      last_service_id_(0),
      weak_ptr_factory_(this) {}

ProfileServer::~ProfileServer() {
  // Unregister anything that we have registered.
  auto sdp = adapter()->sdp_server();
  for (const auto& it : registered_) {
    sdp->UnregisterService(it.second);
  }
  for (const auto& search_id : searches_) {
    adapter()->bredr_connection_manager()->RemoveServiceSearch(search_id);
  }
}

void ProfileServer::AddService(fidlbredr::ServiceDefinition definition,
                               fidlbredr::SecurityLevel sec_level, bool devices,
                               AddServiceCallback callback) {
  // TODO: check that the service definition is valid for useful error messages

  auto sdp = adapter()->sdp_server();
  ZX_DEBUG_ASSERT(sdp);

  bt::sdp::ServiceRecord rec;
  std::vector<bt::UUID> classes;
  for (auto& uuid_str : definition.service_class_uuids) {
    bt::UUID uuid;
    bt_log(SPEW, "profile_server", "Setting Service Class UUID %s",
           uuid_str.c_str());
    bool success = bt::StringToUuid(uuid_str, &uuid);
    ZX_DEBUG_ASSERT(success);
    classes.emplace_back(std::move(uuid));
  }

  rec.SetServiceClassUUIDs(classes);

  AddProtocolDescriptorList(&rec, bt::sdp::ServiceRecord::kPrimaryProtocolList,
                            definition.protocol_descriptors);

  size_t protocol_list_id = 1;
  for (const auto& descriptor_list :
       *definition.additional_protocol_descriptors) {
    AddProtocolDescriptorList(&rec, protocol_list_id, descriptor_list);
    protocol_list_id++;
  }

  for (const auto& profile : definition.profile_descriptors) {
    bt_log(SPEW, "profile_server", "Adding Profile %#x v%d.%d",
           profile.profile_id, profile.major_version, profile.minor_version);
    rec.AddProfile(bt::UUID(uint16_t(profile.profile_id)),
                   profile.major_version, profile.minor_version);
  }

  for (const auto& info : definition.information) {
    bt_log(SPEW, "profile_server", "Adding Info (%s): (%s, %s, %s)",
           info.language.c_str(), info.name->c_str(), info.description->c_str(),
           info.provider->c_str());
    rec.AddInfo(info.language, info.name, info.description, info.provider);
  }

  for (const auto& attribute : *definition.additional_attributes) {
    bt::sdp::DataElement elem;
    FidlToDataElement(attribute.element, &elem);
    bt_log(SPEW, "profile_server", "Adding attribute %#x : %s", attribute.id,
           elem.ToString().c_str());
    rec.SetAttribute(attribute.id, std::move(elem));
  }

  uint64_t next = last_service_id_ + 1;

  auto handle = sdp->RegisterService(
      std::move(rec),
      [this, next](auto sock, auto handle, const auto& protocol_list) {
        OnChannelConnected(next, std::move(sock), handle,
                           std::move(protocol_list));
      });

  if (!handle) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS,
                                        "Service definition was not valid"),
             0);
    return;
  };

  registered_.emplace(next, handle);
  last_service_id_ = next;
  callback(fidl_helpers::StatusToFidl(bt::sdp::Status()), next);
}

void ProfileServer::RemoveService(uint64_t service_id) {
  auto it = registered_.find(service_id);
  if (it == registered_.end()) {
    bt_log(INFO, "profile_server", "RemoveService with unused id %lu",
           service_id);
    return;
  }
  auto server = adapter()->sdp_server();
  ZX_DEBUG_ASSERT(server);
  bool removed = server->UnregisterService(it->second);
  ZX_DEBUG_ASSERT(removed);
  registered_.erase(it);
}

void ProfileServer::AddSearch(
    fidlbredr::ServiceClassProfileIdentifier service_uuid,
    std::vector<uint16_t> attr_ids) {
  bt::UUID search_uuid(static_cast<uint32_t>(service_uuid));
  std::unordered_set<bt::sdp::AttributeId> attributes(attr_ids.begin(),
                                                      attr_ids.end());
  if (!attr_ids.empty()) {
    // Always request the ProfileDescriptor for the event
    attributes.insert(bt::sdp::kBluetoothProfileDescriptorList);
  }

  auto search_id = adapter()->bredr_connection_manager()->AddServiceSearch(
      search_uuid, std::move(attributes),
      [this](auto id, const auto& attrs) { OnServiceFound(id, attrs); });

  if (search_id) {
    searches_.emplace_back(search_id);
  }
}

void ProfileServer::ConnectL2cap(std::string peer_id, uint16_t channel,
                                 ConnectL2capCallback callback) {
  auto dev_id = fidl_helpers::PeerIdFromString(peer_id);
  if (!dev_id.has_value()) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS,
                                        "invalid device ID"),
             zx::socket());
    return;
  }

  auto connected_cb = [cb = callback.share()](zx::socket channel) {
    cb(fidl_helpers::StatusToFidl(bt::sdp::Status()), std::move(channel));
  };
  bool connecting = adapter()->bredr_connection_manager()->OpenL2capChannel(
      *dev_id, channel, std::move(connected_cb),
      async_get_default_dispatcher());
  if (!connecting) {
    callback(
        fidl_helpers::NewFidlError(
            ErrorCode::NOT_FOUND, "Remote device not found - is it connected?"),
        zx::socket());
  }
}

void ProfileServer::OnChannelConnected(
    uint64_t service_id, zx::socket socket, bt::hci::ConnectionHandle handle,
    const bt::sdp::DataElement& protocol_list) {
  auto id = adapter()->bredr_connection_manager()->GetPeerId(handle);

  const auto* prot_seq = protocol_list.At(1);

  // If there isn't a second-level protocol, return the l2cap protocol
  if (!prot_seq) {
    prot_seq = protocol_list.At(0);
  }

  ZX_DEBUG_ASSERT(prot_seq);

  fidlbredr::ProtocolDescriptorPtr desc =
      DataElementToProtocolDescriptor(prot_seq);

  ZX_DEBUG_ASSERT(desc);

  binding()->events().OnConnected(id.ToString(), service_id, std::move(socket),
                                  std::move(*desc));
}

void ProfileServer::OnServiceFound(
    bt::PeerId peer_id,
    const std::map<bt::sdp::AttributeId, bt::sdp::DataElement>& attributes) {
  // Convert ProfileDescriptor Attribute
  auto it = attributes.find(bt::sdp::kBluetoothProfileDescriptorList);
  if (it == attributes.end()) {
    bt_log(WARN, "profile_server",
           "Found service on %s didn't contain profile descriptor, dropping",
           bt_str(peer_id));
    return;
  }
  fidlbredr::ProfileDescriptorPtr desc =
      DataElementToProfileDescriptor(&it->second);

  // Add the rest of the attributes
  std::vector<fidlbredr::Attribute> fidl_attrs;

  for (const auto& it : attributes) {
    auto attr = fidlbredr::Attribute::New();
    attr->id = it.first;
    attr->element = std::move(*DataElementToFidl(&it.second));
    fidl_attrs.emplace_back(std::move(*attr));
  }

  binding()->events().OnServiceFound(peer_id.ToString(), std::move(*desc),
                                     std::move(fidl_attrs));
}

}  // namespace bthost
