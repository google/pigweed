// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_TYPES_H_

#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/att/attribute.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/lib/fxl/macros.h"

namespace bt {
namespace gatt {

// TODO(armansito): Rename this file to "local_types.h" and add the Local*
// prefix to the types here.

class Characteristic;
using CharacteristicPtr = std::unique_ptr<Characteristic>;

// Represents a single remote or local GATT service. A Service object simply
// represents the composition/structure of a GATT service, such as its type,
// characteristics, includes, etc and is not intended to carry service state.
class Service final {
 public:
  Service(bool primary, const common::UUID& type);
  ~Service() = default;

  bool primary() const { return primary_; }
  const common::UUID& type() const { return type_; }

  // The list of characteristics that have been added to this service.
  const std::vector<CharacteristicPtr>& characteristics() const {
    return characteristics_;
  }

  // Passes the ownership of this service's characteristics to the caller.
  std::vector<CharacteristicPtr> ReleaseCharacteristics() {
    return std::move(characteristics_);
  }

  // Adds the given characteristic to this service.
  inline void AddCharacteristic(CharacteristicPtr&& chr) {
    characteristics_.push_back(std::forward<CharacteristicPtr>(chr));
  }

  // TODO(armansito): Support included services.

 private:
  bool primary_;
  common::UUID type_;
  std::vector<CharacteristicPtr> characteristics_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Service);
};

using ServicePtr = std::unique_ptr<Service>;

class Descriptor;
using DescriptorPtr = std::unique_ptr<Descriptor>;

// Represents a single remote or local GATT characteristic. This represents the
// composition/structure of a characteristic and is not intended to carry state.
class Characteristic final {
 public:
  Characteristic(IdType id,
                 const common::UUID& type,
                 uint8_t properties,
                 uint16_t extended_properties,
                 const att::AccessRequirements& read_permissions,
                 const att::AccessRequirements& write_permissions,
                 const att::AccessRequirements& update_permissions);
  ~Characteristic() = default;

  IdType id() const { return id_; }
  const common::UUID& type() const { return type_; }
  uint8_t properties() const { return properties_; }
  uint16_t extended_properties() const { return extended_properties_; }

  const att::AccessRequirements& read_permissions() const {
    return read_permissions_;
  }

  const att::AccessRequirements& write_permissions() const {
    return write_permissions_;
  }

  const att::AccessRequirements& update_permissions() const {
    return update_permissions_;
  }

  const std::vector<DescriptorPtr>& descriptors() const { return descriptors_; }

  // Passes the ownership of this characteristic's descriptors to the caller.
  std::vector<DescriptorPtr> ReleaseDescriptors() {
    return std::move(descriptors_);
  }

  inline void AddDescriptor(DescriptorPtr&& desc) {
    descriptors_.push_back(std::forward<DescriptorPtr>(desc));
  }

 private:
  IdType id_;
  common::UUID type_;
  uint8_t properties_;
  uint16_t extended_properties_;
  att::AccessRequirements read_permissions_;
  att::AccessRequirements write_permissions_;
  att::AccessRequirements update_permissions_;
  std::vector<DescriptorPtr> descriptors_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Characteristic);
};

// Represents a single remote or local GATT characteristic descriptor. This
// represents the composition/structure of a characteristic and is not intended
// to carry state.
class Descriptor final {
 public:
  Descriptor(IdType id,
             const common::UUID& type,
             const att::AccessRequirements& read_permissions,
             const att::AccessRequirements& write_permissions);
  ~Descriptor() = default;

  IdType id() const { return id_; }
  const common::UUID& type() const { return type_; }

  const att::AccessRequirements& read_permissions() const {
    return read_permissions_;
  }

  const att::AccessRequirements& write_permissions() const {
    return write_permissions_;
  }

 private:
  IdType id_;
  common::UUID type_;
  att::AccessRequirements read_permissions_;
  att::AccessRequirements write_permissions_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Descriptor);
};

}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_TYPES_H_
