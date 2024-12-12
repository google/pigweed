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

#pragma once
#include <array>
#include <initializer_list>
#include <string>
#include <unordered_set>

namespace bt {

// Represents a 24-bit "Class of Device/Service" field.
// This data structure can be directly serialized into HCI command payloads.
// See the Bluetooth SIG Assigned Numbers for the Baseband
// (https://www.bluetooth.com/specifications/assigned-numbers/baseband)
// for the format.
class DeviceClass {
 public:
  using Bytes = std::array<uint8_t, 3>;

  enum class MajorClass : uint8_t {
    kMiscellaneous = 0x00,
    kComputer = 0x01,
    kPhone = 0x02,
    kLAN = 0x03,
    kAudioVideo = 0x04,
    kPeripheral = 0x05,
    kImaging = 0x06,
    kWearable = 0x07,
    kToy = 0x08,
    kHealth = 0x09,
    kUnspecified = 0x1F,
  };

  enum class ServiceClass : uint8_t {
    kLimitedDiscoverableMode = 13,
    kLEAudio = 14,
    kReserved = 15,
    kPositioning = 16,
    kNetworking = 17,
    kRendering = 18,
    kCapturing = 19,
    kObjectTransfer = 20,
    kAudio = 21,
    kTelephony = 22,
    kInformation = 23,
  };

  // Initializes the device to an uncategorized device with no services.
  DeviceClass();

  // Initializes the contents from |bytes|, as they are represented from the
  // controller (little-endian)
  DeviceClass(std::initializer_list<uint8_t> bytes);

  // Initializes the contents from |uint32_t|
  explicit DeviceClass(uint32_t value);

  // Initializes the contents using the given |major_class|.
  explicit DeviceClass(MajorClass major_class);

  MajorClass major_class() const { return MajorClass(bytes_[1] & 0b1'1111); }

  uint8_t minor_class() const { return (bytes_[0] >> 2) & 0b11'1111; }

  const Bytes& bytes() const { return bytes_; }

  // Converts the DeviceClass into an integer with host-endianness. Only the
  // lower 24 bits are used, and the highest 8 bits will be 0.
  uint32_t to_int() const;

  // Sets the major service classes of this.
  // Clears any service classes that are not set.
  void SetServiceClasses(const std::unordered_set<ServiceClass>& classes);

  // Gets a set representing the major service classes that are set.
  std::unordered_set<ServiceClass> GetServiceClasses() const;

  // Returns a string describing the device, like "Computer" or "Headphones"
  std::string ToString() const;

  // Equality operators
  bool operator==(const DeviceClass& rhs) const { return rhs.bytes_ == bytes_; }
  bool operator!=(const DeviceClass& rhs) const { return rhs.bytes_ != bytes_; }

  // TODO(jamuraa): add MinorClass
 private:
  Bytes bytes_;
};

static_assert(sizeof(DeviceClass) == 3,
              "DeviceClass must take up exactly 3 bytes");

}  // namespace bt
