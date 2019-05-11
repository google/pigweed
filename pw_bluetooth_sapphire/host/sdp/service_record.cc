// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/service_record.h"

#include <endian.h>

#include <set>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/lib/uuid/uuid.h"

namespace bt {
namespace sdp {

namespace {

// Adds all UUIDs that it finds in |elem| to |out|, recursing through
// sequences and alternatives if necessary.
void AddAllUUIDs(const DataElement& elem, std::unordered_set<UUID>* out) {
  DataElement::Type type = elem.type();
  if (type == DataElement::Type::kUuid) {
    out->emplace(*elem.Get<UUID>());
  } else if (type == DataElement::Type::kSequence ||
             type == DataElement::Type::kAlternative) {
    const DataElement* it;
    for (size_t idx = 0; nullptr != (it = elem.At(idx)); idx++) {
      AddAllUUIDs(*it, out);
    }
  }
}

}  // namespace

ServiceRecord::ServiceRecord() {
  UUID service_uuid;
  StringToUuid(uuid::Generate(), &service_uuid);
  SetAttribute(kServiceId, DataElement(service_uuid));
}

void ServiceRecord::SetAttribute(AttributeId id, DataElement value) {
  attributes_.erase(id);
  attributes_.emplace(id, std::move(value));
}

const DataElement& ServiceRecord::GetAttribute(AttributeId id) const {
  auto it = attributes_.find(id);
  ZX_DEBUG_ASSERT_MSG(it != attributes_.end(), "attribute %#.4x not set!", id);
  return it->second;
}

bool ServiceRecord::HasAttribute(AttributeId id) const {
  return attributes_.count(id) == 1;
}

void ServiceRecord::RemoveAttribute(AttributeId id) { attributes_.erase(id); }

void ServiceRecord::SetHandle(ServiceHandle handle) {
  handle_ = handle;
  SetAttribute(kServiceRecordHandle, DataElement(uint32_t(handle_)));
}

std::set<AttributeId> ServiceRecord::GetAttributesInRange(
    AttributeId start, AttributeId end) const {
  std::set<AttributeId> attrs;
  if (start > end) {
    return attrs;
  }
  for (auto it = attributes_.lower_bound(start);
       it != attributes_.end() && (it->first <= end); ++it) {
    attrs.emplace(it->first);
  }

  return attrs;
}

bool ServiceRecord::FindUUID(const std::unordered_set<UUID>& uuids) const {
  if (uuids.size() == 0) {
    return true;
  }
  // Gather all the UUIDs in the attributes
  std::unordered_set<UUID> attribute_uuids;
  for (const auto& it : attributes_) {
    AddAllUUIDs(it.second, &attribute_uuids);
  }
  for (const auto& uuid : uuids) {
    if (attribute_uuids.count(uuid) == 0) {
      return false;
    }
  }
  return true;
}

void ServiceRecord::SetServiceClassUUIDs(const std::vector<UUID>& classes) {
  std::vector<DataElement> class_uuids;
  for (const auto& uuid : classes) {
    class_uuids.emplace_back(DataElement(uuid));
  }
  DataElement class_id_list_val(std::move(class_uuids));
  SetAttribute(kServiceClassIdList, std::move(class_id_list_val));
}

void ServiceRecord::AddProtocolDescriptor(const ProtocolListId id,
                                          const UUID& uuid,
                                          DataElement params) {
  std::vector<DataElement> seq;
  if (id == kPrimaryProtocolList) {
    auto list_it = attributes_.find(kProtocolDescriptorList);
    if (list_it != attributes_.end()) {
      auto v = list_it->second.Get<std::vector<DataElement>>();
      seq = std::move(*v);
    }
  } else if (addl_protocols_.count(id)) {
    auto v = addl_protocols_[id].Get<std::vector<DataElement>>();
    seq = std::move(*v);
  }

  std::vector<DataElement> protocol_desc;
  protocol_desc.emplace_back(DataElement(uuid));
  if (params.type() == DataElement::Type::kSequence) {
    auto v = params.Get<std::vector<DataElement>>();
    auto param_seq = std::move(*v);
    std::move(std::begin(param_seq), std::end(param_seq),
              std::back_inserter(protocol_desc));
  } else if (params.type() != DataElement::Type::kNull) {
    protocol_desc.emplace_back(std::move(params));
  }

  seq.emplace_back(DataElement(std::move(protocol_desc)));

  if (id == kPrimaryProtocolList) {
    SetAttribute(kProtocolDescriptorList, DataElement(std::move(seq)));
  } else {
    addl_protocols_.erase(id);
    addl_protocols_.emplace(id, DataElement(std::move(seq)));

    std::vector<DataElement> addl_protocol_seq;
    for (const auto& it : addl_protocols_) {
      addl_protocol_seq.emplace_back(it.second.Clone());
    }

    SetAttribute(kAdditionalProtocolDescriptorList,
                 DataElement(std::move(addl_protocol_seq)));
  }
}

void ServiceRecord::AddProfile(const UUID& uuid, uint8_t major, uint8_t minor) {
  std::vector<DataElement> seq;
  auto list_it = attributes_.find(kBluetoothProfileDescriptorList);
  if (list_it != attributes_.end()) {
    auto v = list_it->second.Get<std::vector<DataElement>>();
    seq = std::move(*v);
  }

  std::vector<DataElement> profile_desc;
  profile_desc.emplace_back(DataElement(uuid));

  uint16_t profile_version = (major << 8) | minor;
  profile_desc.emplace_back(DataElement(profile_version));

  seq.emplace_back(DataElement(std::move(profile_desc)));

  SetAttribute(kBluetoothProfileDescriptorList, DataElement(std::move(seq)));
}

bool ServiceRecord::AddInfo(const std::string& language_code,
                            const std::string& name,
                            const std::string& description,
                            const std::string& provider) {
  if ((name.empty() && description.empty() && provider.empty()) ||
      (language_code.size() != 2)) {
    return false;
  }
  AttributeId base_attrid = 0x0100;
  std::vector<DataElement> base_attr_list;
  auto it = attributes_.find(kLanguageBaseAttributeIdList);
  if (it != attributes_.end()) {
    auto v = it->second.Get<std::vector<DataElement>>();
    base_attr_list = std::move(*v);
    ZX_DEBUG_ASSERT(base_attr_list.size() % 3 == 0);
    // 0x0100 is guaranteed to be taken, start counting from higher.
    base_attrid = 0x9000;
  }

  // Find the first base_attrid that's not taken
  while (HasAttribute(base_attrid + kServiceNameOffset) ||
         HasAttribute(base_attrid + kServiceDescriptionOffset) ||
         HasAttribute(base_attrid + kProviderNameOffset)) {
    base_attrid++;
    if (base_attrid == 0xFFFF) {
      return false;
    }
  }

  // Core Spec v5.0, Vol 3, Part B, Sect 5.1.8: "The LanguageBaseAttributeIDList
  // attribute consists of a data element sequence in which each element is a
  // 16-bit unsigned integer."
  // The language code consists of two byte characters in left-to-right order,
  // so it may be considered a 16-bit big-endian integer that can be converted
  // to host byte order.
  uint16_t lang_encoded = betoh16(*((uint16_t*)(language_code.data())));
  base_attr_list.emplace_back(DataElement(lang_encoded));
  base_attr_list.emplace_back(DataElement(uint16_t(106)));  // UTF-8
  base_attr_list.emplace_back(DataElement(base_attrid));

  if (!name.empty()) {
    SetAttribute(base_attrid + kServiceNameOffset, DataElement(name));
  }
  if (!description.empty()) {
    SetAttribute(base_attrid + kServiceDescriptionOffset,
                 DataElement(description));
  }
  if (!provider.empty()) {
    SetAttribute(base_attrid + kProviderNameOffset, DataElement(provider));
  }

  SetAttribute(kLanguageBaseAttributeIdList,
               DataElement(std::move(base_attr_list)));
  return true;
}

}  // namespace sdp
}  // namespace bt
