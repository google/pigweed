// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_SERVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_SERVICE_H_

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/string.h>
#include <lib/fidl/cpp/vector.h>
#include <src/lib/fxl/memory/weak_ptr.h>

#include <unordered_set>

#include "appearance.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"

namespace bt {
namespace gap {

// Generic Access Service which exposes the Device Name, Appearance and
// Peripheral Preferred Connection Parameters characteristics.
//
// See Bluetooth Core Spec 5.1 Vol 3 Part C Section 12.
class GenericAccessService final {
 public:
  // See assigned numbers for GATT services and characteristics.
  // https://www.bluetooth.com/specifications/gatt/services
  // https://www.bluetooth.com/specifications/gatt/characteristics
  //
  // Service UUID for Generic Access (org.bluetooth.service.generic_access).
  // Generic Access Service's assigned number is 0x1800.
  // The Base UUID is 00000000-0000-1000-8000-00805F9B34FB
  //
  // See Assigned Numbers for Generic Access Service.
  // https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.generic_access.xml
  static constexpr UUID kServiceUUID = UUID(uint16_t{0x1800});
  static constexpr gatt::IdType kDisplayNameId = 0;
  static constexpr UUID kDisplayNameUUID = UUID(uint16_t{0x2a00});
  static constexpr gatt::IdType kAppearanceId = 1;
  static constexpr UUID kAppearanceUUID = UUID(uint16_t{0x2a01});
  static constexpr gatt::IdType kPeripheralPreferredConnectionParametersId = 2;
  static constexpr UUID kPeripheralPreferredConnectionParametersUUID =
      UUID(uint16_t{0x2a04});

  // Max device name length as per Core v5.1 Vol 3 Part C Section 12.1.
  static constexpr size_t kMaxDeviceNameLength = 248;

  explicit GenericAccessService(fbl::RefPtr<gatt::GATT> gatt);
  ~GenericAccessService() { Unregister(); };

  // Get current values.
  const std::string &GetDeviceName() { return device_name_; };
  AppearanceCategory GetAppearance() { return appearance_; };
  const std::optional<hci::LEPreferredConnectionParameters>
      &GetPreferredConnectionParameters() {
    return preferred_connection_parameters_;
  };

  // Update generic access service characteristics.

  // If device name cant be more than 248 octets as per Core v5.1 Vol 3
  // Part C Section 12.1. If |device_name| is longer, it will be truncated.
  void UpdateDeviceName(std::string device_name);
  void UpdateAppearance(AppearanceCategory appearance);
  // Returns true if the provided parameters are valid (within range, or
  // unspecified if an unspecified value is allowed as per Core v5.1 Vol 3 Part
  // C Section 12.3. Specifically:
  //   Conn_Interval_Min
  //     - range of 0x0006 to 0x0C80
  //     - value of 0xFFFF indicates no specific minimum
  //   Conn_Interval_Max
  //     - range of 0x0006 to 0x0C80
  //     - value of 0xFFFF indicates no specific minimum
  //     - must be equal to or grater than Conn_Interval_Min
  //   Latency_Max
  //     - range of 0x0000 to 0x01F3
  //   Supervision_Timeout
  //     - range of 0x0006 to 0x0C80
  //     - value of 0xFFFF indicates no specific minimum
  // If false is returned, then no changes were made to the parameters this
  // service provides to clients.
  bool UpdatePreferredConnectionParameters(
      std::optional<hci::LEPreferredConnectionParameters> parameters);

  // Handle a read of a characteristic.
  void OnReadValue(gatt::IdType id, int32_t offset,
                   gatt::ReadResponder callback);

 private:
  // Register the generic access service GATT service so remote clients can
  // send it requests.
  //
  // |Register| must only be called after the GATT profile as been
  // successfully initialized.
  void Register();

  // Unregister the generic access service GATT service.
  //
  // If the service is not registered, this function does nothing.
  void Unregister();

  void OnReadDeviceName(gatt::ReadResponder callback);
  void OnReadAppearance(gatt::ReadResponder callback);
  void OnReadPreferredConnectionParameters(gatt::ReadResponder callback);

  fxl::WeakPtrFactory<GenericAccessService> weak_factory_;

  // The GATT profile. We use this reference to register this service.
  fbl::RefPtr<gatt::GATT> gatt_;

  // The service id of this generic access service.
  gatt::IdType service_id_;

  // Device name.
  std::string device_name_;

  // Appearance.
  AppearanceCategory appearance_;

  // Preferred Connection Parameters.
  std::optional<hci::LEPreferredConnectionParameters>
      preferred_connection_parameters_;

  FXL_DISALLOW_IMPLICIT_CONSTRUCTORS(GenericAccessService);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_SERVICE_H_
