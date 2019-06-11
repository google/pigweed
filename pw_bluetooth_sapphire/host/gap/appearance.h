// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_APPEARANCE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_APPEARANCE_H_

namespace bt {
namespace gap {

// https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.gap.appearance.xml
enum class AppearanceCategory : uint16_t {
  kUnknown = 0,
  kGenericPhone = 64,
  kGenericComputer = 128,
  kGenericWatch = 192,
  kWatchSportsWatch = 193,
  kGenericClock = 256,
  kGenericDisplay = 320,
  kGenericRemoteControl = 384,
  kGenericEyeglasses = 448,
  kGenericTag = 512,
  kGenericKeyring = 576,
  kGenericMediaPlayer = 640,
  kGenericBarcodeScanner = 704,
  kGenericThermometer = 768,
  kThermometerEar = 769,
  kGenericHeartrateSensor = 832,
  kHeartRateSensorHeartRateBelt = 833,
  kGenericBloodPressure = 896,
  kBloodPressureArm = 897,
  kBloodPressureWrist = 898,
  kHumanInterfaceDevice = 960,
  kKeyboard = 961,
  kMouse = 962,
  kJoystick = 963,
  kGamepad = 964,
  kDigitizerTablet = 965,
  kCardReader = 966,
  kDigitalPen = 967,
  kBarcodeScanner = 968,
  kGenericGlucoseMeter = 1024,
  kGenericRunningWalkingSensor = 1088,
  kRunningWalkingSensorInShoe = 1089,
  kRunningWalkingSensorOnShoe = 1090,
  kRunningWalkingSensorOnHip = 1091,
  kGenericCycling = 1152,
  kCyclingCyclingComputer = 1153,
  kCyclingSpeedSensor = 1154,
  kCyclingCadenceSensor = 1155,
  kCyclingPowerSensor = 1156,
  kCyclingSpeedandCadenceSensor = 1157,
  kGenericPulseOximeter = 3136,
  kFingertip = 3137,
  kWristWorn = 3138,
  kGenericWeightScale = 3200,
  kGenericPersonalMobilityDevice = 3264,
  kPoweredWheelchair = 3265,
  kMobilityScooter = 3266,
  kGenericContinuousGlucoseMonitor = 3328,
  kGenericInsulinPump = 3392,
  kInsulinPumpDurablePump = 3393,
  kInsulinPumpPatchPump = 3396,
  kInsulinPen = 3400,
  kGenericMedicationDelivery = 3456,
  kGenericOutdoorSportsActivity = 5184,
  kLocationDisplayDevice = 5185,
  kLocationandNavigationDisplayDevice = 5186,
  kLocationPod = 5187,
  kLocationandNavigationPod = 5188,
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_APPEARANCE_H_
