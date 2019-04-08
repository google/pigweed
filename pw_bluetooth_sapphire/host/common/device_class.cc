// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_class.h"

#include <zircon/assert.h>

namespace bt {
namespace common {

namespace {

DeviceClass::ServiceClass bit_no_to_service_class(uint8_t bit_no) {
  ZX_DEBUG_ASSERT(bit_no >= 16);
  ZX_DEBUG_ASSERT(bit_no < 24);
  switch (bit_no) {
    case 16:
      return DeviceClass::ServiceClass::kPositioning;
    case 17:
      return DeviceClass::ServiceClass::kNetworking;
    case 18:
      return DeviceClass::ServiceClass::kRendering;
    case 19:
      return DeviceClass::ServiceClass::kCapturing;
    case 20:
      return DeviceClass::ServiceClass::kObjectTransfer;
    case 21:
      return DeviceClass::ServiceClass::kAudio;
    case 22:
      return DeviceClass::ServiceClass::kTelephony;
    case 23:
      return DeviceClass::ServiceClass::kInformation;
  };
  // Should be unreachable.
  return DeviceClass::ServiceClass::kInformation;
}

std::string service_class_to_string(const DeviceClass::ServiceClass& serv) {
  switch (serv) {
    case DeviceClass::ServiceClass::kPositioning:
      return "Positioning";
    case DeviceClass::ServiceClass::kNetworking:
      return "Networking";
    case DeviceClass::ServiceClass::kRendering:
      return "Rendering";
    case DeviceClass::ServiceClass::kCapturing:
      return "Capturing";
    case DeviceClass::ServiceClass::kObjectTransfer:
      return "Object Transfer";
    case DeviceClass::ServiceClass::kAudio:
      return "Audio";
    case DeviceClass::ServiceClass::kTelephony:
      return "Telephony";
    case DeviceClass::ServiceClass::kInformation:
      return "Information";
  }
}

}  // namespace

DeviceClass::DeviceClass() : DeviceClass(MajorClass::kUnspecified) {}

DeviceClass::DeviceClass(MajorClass major_class)
    : bytes_{0x00, static_cast<uint8_t>(major_class), 0x00} {}

DeviceClass::DeviceClass(std::initializer_list<uint8_t> bytes) {
  ZX_DEBUG_ASSERT(bytes.size() == bytes_.size());
  std::copy(bytes.begin(), bytes.end(), bytes_.begin());
}

void DeviceClass::SetServiceClasses(
    const std::unordered_set<ServiceClass>& classes) {
  for (const auto& c : classes) {
    uint8_t bit = static_cast<uint8_t>(c);
    if (bit >= 16) {
      bytes_[2] |= 0x01 << (bit - 16);
    } else if (bit >= 8) {
      bytes_[1] |= 0x01 << (bit - 8);
    }
  }
}

std::unordered_set<DeviceClass::ServiceClass> DeviceClass::GetServiceClasses()
    const {
  std::unordered_set<ServiceClass> classes;
  for (uint8_t bit_no = 16; bit_no < 24; bit_no++) {
    if (bytes_[2] & (0x01 << (bit_no - 16))) {
      classes.emplace(bit_no_to_service_class(bit_no));
    }
  }
  return classes;
}

std::string DeviceClass::ToString() const {
  std::string service_desc;
  auto classes = GetServiceClasses();
  if (!classes.empty()) {
    auto it = classes.begin();
    service_desc = " (" + service_class_to_string(*it);
    ++it;
    for (; it != classes.end(); ++it) {
      service_desc = service_desc + ", " + service_class_to_string(*it);
    }
    service_desc = service_desc + ")";
  }
  switch (major_class()) {
    case MajorClass::kMiscellaneous:
      return "Miscellaneous" + service_desc;
    case MajorClass::kComputer:
      return "Computer" + service_desc;
    case MajorClass::kPhone:
      return "Phone" + service_desc;
    case MajorClass::kLAN:
      return "LAN" + service_desc;
    case MajorClass::kAudioVideo:
      return "A/V" + service_desc;
    case MajorClass::kPeripheral:
      return "Peripheral" + service_desc;
    case MajorClass::kImaging:
      return "Imaging" + service_desc;
    case MajorClass::kWearable:
      return "Wearable" + service_desc;
    case MajorClass::kToy:
      return "Toy" + service_desc;
    case MajorClass::kHealth:
      return "Health Device" + service_desc;
    case MajorClass::kUnspecified:
      return "Unspecified" + service_desc;
  };

  return "(unknown)";
}

}  // namespace common
}  // namespace bt
