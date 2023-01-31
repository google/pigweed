// Copyright 2022 The Pigweed Authors
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

#include <cstdint>

#include "pw_bluetooth/assigned_uuids.h"
#include "pw_bluetooth/gatt/error.h"
#include "pw_bluetooth/gatt/server.h"
#include "pw_bluetooth/gatt/types.h"
#include "pw_span/span.h"

namespace pw::bluetooth_profiles {

// Device information to be exposed by the Device Information Service, according
// to the DIS spec 1.1. All fields are optional.
struct DeviceInfo {
  // Bitmask of the fields present in the DeviceInfoService, each one
  // corresponding to one of the possible characteristics in the Device
  // Information Service.
  enum class Field : uint16_t {
    kManufacturerName = 1u << 0,
    kModelNumber = 1u << 1,
    kSerialNumber = 1u << 2,
    kHardwareRevision = 1u << 3,
    kFirmwareRevision = 1u << 4,
    kSoftwareRevision = 1u << 5,
    kSystemId = 1u << 6,
    kRegulatoryInformation = 1u << 7,
    kPnpId = 1u << 8,
  };

  // Manufacturer Name String
  span<const std::byte> manufacturer_name;

  // Model Number String
  span<const std::byte> model_number;

  // Serial Number String
  span<const std::byte> serial_number;

  // Hardware Revision String
  span<const std::byte> hardware_revision;

  // Firmware Revision String
  span<const std::byte> firmware_revision;

  // Software Revision String
  span<const std::byte> software_revision;

  // System ID
  span<const std::byte> system_id;

  // IEEE 11073-20601 Regulatory Certification Data List
  span<const std::byte> regulatory_information;

  // PnP ID
  span<const std::byte> pnp_id;
};

// Helper operator| to allow combining multiple DeviceInfo::Field values.
static inline constexpr DeviceInfo::Field operator|(DeviceInfo::Field left,
                                                    DeviceInfo::Field right) {
  return static_cast<DeviceInfo::Field>(static_cast<uint16_t>(left) |
                                        static_cast<uint16_t>(right));
}

static inline constexpr bool operator&(DeviceInfo::Field left,
                                       DeviceInfo::Field right) {
  return (static_cast<uint16_t>(left) & static_cast<uint16_t>(right)) != 0;
}

// Shared implementation of the DeviceInfoService<> template class of elements
// that don't depend on the template parameters.
class DeviceInfoServiceImpl {
 public:
  DeviceInfoServiceImpl(const bluetooth::gatt::LocalServiceInfo& service_info,
                        span<const span<const std::byte>> values)
      : service_info_(service_info), delegate_(values) {}

  // Publish this service on the passed gatt::Server. The service may be
  // published only on one Server at a time.
  void PublishService(
      bluetooth::gatt::Server* gatt_server,
      Callback<
          void(bluetooth::Result<bluetooth::gatt::Server::PublishServiceError>)>
          result_callback);

 protected:
  using GattCharacteristicUuid = bluetooth::GattCharacteristicUuid;

  // A struct for describing each one of the optional characteristics available.
  struct FieldDescriptor {
    DeviceInfo::Field field_value;
    span<const std::byte> DeviceInfo::*field_pointer;
    bluetooth::Uuid characteristic_type;
  };

  // List of all the fields / characteristics available in the DIS, mapping the
  // characteristic UUID type to the corresponding field in the DeviceInfo
  // struct.
  static constexpr size_t kNumFields = 9;
  static constexpr std::array<FieldDescriptor, kNumFields>
      kCharacteristicFields = {{
          {DeviceInfo::Field::kManufacturerName,
           &DeviceInfo::manufacturer_name,
           GattCharacteristicUuid::kManufacturerNameString},
          {DeviceInfo::Field::kModelNumber,
           &DeviceInfo::model_number,
           GattCharacteristicUuid::kModelNumberString},
          {DeviceInfo::Field::kSerialNumber,
           &DeviceInfo::serial_number,
           GattCharacteristicUuid::kSerialNumberString},
          {DeviceInfo::Field::kHardwareRevision,
           &DeviceInfo::hardware_revision,
           GattCharacteristicUuid::kHardwareRevisionString},
          {DeviceInfo::Field::kFirmwareRevision,
           &DeviceInfo::firmware_revision,
           GattCharacteristicUuid::kFirmwareRevisionString},
          {DeviceInfo::Field::kSoftwareRevision,
           &DeviceInfo::software_revision,
           GattCharacteristicUuid::kSoftwareRevisionString},
          {DeviceInfo::Field::kSystemId,
           &DeviceInfo::system_id,
           GattCharacteristicUuid::kSystemId},
          {DeviceInfo::Field::kRegulatoryInformation,
           &DeviceInfo::regulatory_information,
           GattCharacteristicUuid::
               kIeee1107320601RegulatoryCertificationDataList},
          {DeviceInfo::Field::kPnpId,
           &DeviceInfo::pnp_id,
           GattCharacteristicUuid::kPnpId},
      }};

 private:
  class Delegate : public bluetooth::gatt::LocalServiceDelegate {
   public:
    explicit Delegate(span<const span<const std::byte>> values)
        : values_(values) {}
    ~Delegate() override = default;

    void SetServicePtr(bluetooth::gatt::LocalService::Ptr service) {
      local_service_ = std::move(service);
    }

    // LocalServiceDelegate overrides
    void OnError(bluetooth::gatt::Error error) override;

    void ReadValue(bluetooth::PeerId peer_id,
                   bluetooth::gatt::Handle handle,
                   uint32_t offset,
                   Function<void(bluetooth::Result<bluetooth::gatt::Error,
                                                   span<const std::byte>>)>&&
                       result_callback) override;

    void WriteValue(bluetooth::PeerId /* peer_id */,
                    bluetooth::gatt::Handle /* handle */,
                    uint32_t /* offset */,
                    span<const std::byte> /* value */,
                    Function<void(bluetooth::Result<bluetooth::gatt::Error>)>&&
                        status_callback) override {
      status_callback(bluetooth::gatt::Error::kUnlikelyError);
    }

    void CharacteristicConfiguration(bluetooth::PeerId /* peer_id */,
                                     bluetooth::gatt::Handle /* handle */,
                                     bool /* notify */,
                                     bool /* indicate */) override {
      // No indications or notifications are supported by this service.
    }

    void MtuUpdate(bluetooth::PeerId /* peer_id*/,
                   uint16_t /* mtu */) override {
      // MTU is ignored.
    }

   private:
    // LocalService smart pointer returned by the pw_bluetooth API once the
    // service is published. This field is unused since we don't generate any
    // Notification or Indication, but deleting this object unpublishes the
    // service.
    bluetooth::gatt::LocalService::Ptr local_service_;

    // Device information values for the service_info_ characteristics. The
    // characteristic Handle is the index into the values_ span.
    span<const span<const std::byte>> values_;
  };

  // GATT service info.
  const bluetooth::gatt::LocalServiceInfo& service_info_;

  // Callback to be called after the service is published.
  Callback<void(
      bluetooth::Result<bluetooth::gatt::Server::PublishServiceError>)>
      publish_service_callback_;

  // The LocalServiceDelegate implementation.
  Delegate delegate_;
};

// Device Information Service exposing only the subset of characteristics
// specified by the bitmask kPresentFields.
template <DeviceInfo::Field kPresentFields,
          bluetooth::gatt::Handle kServiceHandle>
class DeviceInfoService : public DeviceInfoServiceImpl {
 public:
  // Handle used to reference this service from other services.
  static constexpr bluetooth::gatt::Handle kHandle = kServiceHandle;

  // Construct a DeviceInfoService exposing the values provided in the
  // `device_info` for the subset of characteristics selected by kPresentFields.
  // DeviceInfo fields for characteristics not selected by kPresentFields are
  // ignored. The `device_info` reference doesn't need to be kept alive after
  // the constructor returns, however the data pointed to by the various fields
  // in `device_info` must be kept available while the service is published.
  explicit constexpr DeviceInfoService(const DeviceInfo& device_info)
      : DeviceInfoServiceImpl(kServiceInfo, span{values_}) {
    size_t count = 0;
    // Get the device information only for the fields we care about.
    for (const auto& field : kCharacteristicFields) {
      if (field.field_value & kPresentFields) {
        values_[count] = device_info.*(field.field_pointer);
        count++;
      }
    }
  }

 private:
  // Return the total number of selected characteristics on this service.
  static constexpr size_t NumCharacteristics() {
    size_t ret = 0;
    for (const auto& field : kCharacteristicFields) {
      if (field.field_value & kPresentFields) {
        ret++;
      }
    }
    return ret;
  }
  static constexpr size_t kNumCharacteristics = NumCharacteristics();

  // Factory method to build the list of characteristics needed for a given
  // subset of fields.
  static constexpr std::array<bluetooth::gatt::Characteristic,
                              kNumCharacteristics>
  BuildServiceInfoCharacteristics() {
    std::array<bluetooth::gatt::Characteristic, kNumCharacteristics> ret = {};
    size_t count = 0;
    for (const auto& field : kCharacteristicFields) {
      if (field.field_value & kPresentFields) {
        ret[count] = bluetooth::gatt::Characteristic{
            .handle = bluetooth::gatt::Handle(count),
            .type = field.characteristic_type,
            .properties = bluetooth::gatt::CharacteristicPropertyBits::kRead,
            .permissions = bluetooth::gatt::AttributePermissions{},
            .descriptors = {},
        };
        count++;
      }
    }
    return ret;
  }
  // Static constexpr array of characteristics for the current subset of fields
  // kPresentFields.
  static constexpr auto kServiceInfoCharacteristics =
      BuildServiceInfoCharacteristics();

  // GATT Service information.
  static constexpr auto kServiceInfo = bluetooth::gatt::LocalServiceInfo{
      .handle = kServiceHandle,
      .primary = true,
      .type = bluetooth::GattServiceUuid::kDeviceInformation,
      .characteristics = kServiceInfoCharacteristics,
      .includes = {},
  };

  // Storage std::array for the span<const std::byte> of values.
  std::array<span<const std::byte>, kNumCharacteristics> values_;
};

}  // namespace pw::bluetooth_profiles
