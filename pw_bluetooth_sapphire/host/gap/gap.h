// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_

#include <cstdint>

#include <pw_chrono/system_clock.h>

#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"

// This file contains constants and numbers that are part of the Generic Access
// Profile specification.

namespace bt::gap {

// Bluetooth technologies that a device can support.
enum class TechnologyType {
  kLowEnergy,
  kClassic,
  kDualMode,
};
const char* TechnologyTypeToString(TechnologyType type);

enum class Mode {
  // Use the legacy HCI command set.
  kLegacy,

  // Use the extended HCI command set introduced in version 5.0
  kExtended,
};

// Enum for the supported values of the BR/EDR Security Mode as defined in
// Core Spec v5.4, Vol 3, Part C, 4.2.2.
enum class BrEdrSecurityMode {
  // Mode 4 entails possibly encrypted, possibly authenticated communication.
  Mode4,
  // Secure Connections Only mode enforces that all encrypted transmissions use 128-bit,
  // SC-generated and authenticated encryption keys.
  SecureConnectionsOnly,
};
const char* BrEdrSecurityModeToString(BrEdrSecurityMode mode);

// Enum for the supported values of the LE Security Mode as defined in spec v5.2 Vol 3 Part C 10.2.
enum class LESecurityMode {
  // Mode 1 entails possibly encrypted, possibly authenticated communication.
  Mode1,
  // Secure Connections Only mode enforces that all encrypted transmissions use 128-bit,
  // SC-generated and authenticated encryption keys.
  SecureConnectionsOnly,
};
const char* LeSecurityModeToString(LESecurityMode mode);

// Placeholder assigned as the local name when gap::Adapter is initialized.
constexpr char kDefaultLocalName[] = "fuchsia";

// Constants used in BR/EDR Inquiry (Core Spec v5.0, Vol 2, Part C, Appendix A)
// Default cycles value for length of Inquiry. See T_gap(100).
// This is in 1.28s time slice units, and is 10.24 seconds.
constexpr uint8_t kInquiryLengthDefault = 0x08;

// The inquiry scan interval and window used by our stack. The unit for these values is
// controller timeslices (N) where Time in ms = N * 0.625ms
constexpr uint16_t kInquiryScanInterval = 0x01E0;  // 300 ms
constexpr uint16_t kInquiryScanWindow = 0x0012;    // 11.25 ms

// Constants used in Low Energy (see Core Spec v5.0, Vol 3, Part C, Appendix A).

constexpr pw::chrono::SystemClock::duration kLEGeneralDiscoveryScanMin =
    std::chrono::milliseconds(10240);
constexpr pw::chrono::SystemClock::duration kPwLEGeneralDiscoveryScanMin =
    std::chrono::milliseconds(10240);
constexpr zx::duration kLEGeneralDiscoveryScanMinCoded = zx::msec(30720);
constexpr zx::duration kLEScanFastPeriod = zx::msec(30720);

// The HCI spec defines the time conversion as follows: Time =  N * 0.625 ms,
// where N is the value of the constant.
constexpr float kHciScanIntervalToMs = 0.625f;
constexpr float HciScanIntervalToMs(uint16_t i) {
  return static_cast<float>(i) * kHciScanIntervalToMs;
}
constexpr float HciScanWindowToMs(uint16_t w) { return HciScanIntervalToMs(w); }

// Recommended scan and advertising parameters that can be passed directly to the HCI commands.
// A constant that contans the word "Coded" is recommended when using the LE
// Coded PHY. Otherwise the constant is recommended when using the LE 1M PHY.
// See Core Spec v5.2, Vol. 3, Part C, Table A for ranges and descriptions.

// For user-initiated scanning
constexpr uint16_t kLEScanFastInterval = 0x0060;       // 60 ms
constexpr uint16_t kLEScanFastIntervalCoded = 0x0120;  // 180 ms
constexpr uint16_t kLEScanFastWindow = 0x0030;         // 30 ms
constexpr uint16_t kLEScanFastWindowCoded = 0x90;      // 90 ms

// For background scanning
constexpr uint16_t kLEScanSlowInterval1 = 0x0800;       // 1.28 s
constexpr uint16_t kLEScanSlowInterval1Coded = 0x1800;  // 3.84 s
constexpr uint16_t kLEScanSlowWindow1 = 0x0012;         // 11.25 ms
constexpr uint16_t kLEScanSlowWindow1Coded = 0x0036;    // 33.75 ms
constexpr uint16_t kLEScanSlowInterval2 = 0x1000;       // 2.56 s
constexpr uint16_t kLEScanSlowInterval2Coded = 0x3000;  // 7.68 s
constexpr uint16_t kLEScanSlowWindow2 = 0x0024;         // 22.5 ms
constexpr uint16_t kLEScanSlowWindow2Coded = 0x006C;    // 67.5 ms

// Advertising parameters
constexpr uint16_t kLEAdvertisingFastIntervalMin1 = 0x0030;       // 30 ms
constexpr uint16_t kLEAdvertisingFastIntervalMax1 = 0x0060;       // 60 ms
constexpr uint16_t kLEAdvertisingFastIntervalMin2 = 0x00A0;       // 100 ms
constexpr uint16_t kLEAdvertisingFastIntervalMax2 = 0x00F0;       // 150 ms
constexpr uint16_t kLEAdvertisingFastIntervalCodedMin1 = 0x0090;  // 90 ms
constexpr uint16_t kLEAdvertisingFastIntervalCodedMax1 = 0x0120;  // 180 ms
constexpr uint16_t kLEAdvertisingFastIntervalCodedMin2 = 0x01E0;  // 300 ms
constexpr uint16_t kLEAdvertisingFastIntervalCodedMax2 = 0x02D0;  // 450 ms

constexpr uint16_t kLEAdvertisingSlowIntervalMin = 0x0640;       // 1 s
constexpr uint16_t kLEAdvertisingSlowIntervalMax = 0x0780;       // 1.2 s
constexpr uint16_t kLEAdvertisingSlowIntervalCodedMin = 0x12C0;  // 3 s
constexpr uint16_t kLEAdvertisingSlowIntervalCodedMax = 0x1680;  // 3.6 s

// Timeout used for the LE Create Connection command.
constexpr pw::chrono::SystemClock::duration kLECreateConnectionTimeout = std::chrono::seconds(20);
constexpr pw::chrono::SystemClock::duration kPwLECreateConnectionTimeout = std::chrono::seconds(20);
// Timeout used for the Br/Edr Create Connection command.
constexpr pw::chrono::SystemClock::duration kBrEdrCreateConnectionTimeout =
    std::chrono::seconds(20);
constexpr pw::chrono::SystemClock::duration kPwBrEdrCreateConnectionTimeout =
    std::chrono::seconds(20);

// Timeout used for scanning during LE General CEP. Selected to be longer than the scan period.
constexpr pw::chrono::SystemClock::duration kLEGeneralCepScanTimeout = std::chrono::seconds(20);
constexpr pw::chrono::SystemClock::duration kPwLEGeneralCepScanTimeout = std::chrono::seconds(20);

// Connection Interval Timing Parameters (see v5.0, Vol 3, Part C,
// Section 9.3.12 and Appendix A)
constexpr zx::duration kLEConnectionParameterTimeout = zx::sec(30);
// Recommended minimum time upon connection establishment before the central starts a connection
// update procedure.
constexpr pw::chrono::SystemClock::duration kLEConnectionPauseCentral = std::chrono::seconds(1);
constexpr pw::chrono::SystemClock::duration kPwLEConnectionPauseCentral = std::chrono::seconds(1);
// Recommended minimum time upon connection establishment before the peripheral starts a connection
// update procedure.
constexpr pw::chrono::SystemClock::duration kLEConnectionPausePeripheral = std::chrono::seconds(5);
constexpr pw::chrono::SystemClock::duration kPwLEConnectionPausePeripheral =
    std::chrono::seconds(5);

constexpr uint16_t kLEInitialConnIntervalMin = 0x0018;       // 30 ms
constexpr uint16_t kLEInitialConnIntervalMax = 0x0028;       // 50 ms
constexpr uint16_t kLEInitialConnIntervalCodedMin = 0x0048;  // 90 ms
constexpr uint16_t kLEInitialConnIntervalCodedMax = 0x0078;  // 150 ms

// Time interval that must expire before a temporary device is removed from the
// cache.
constexpr pw::chrono::SystemClock::duration kCacheTimeout = std::chrono::seconds(60);
constexpr pw::chrono::SystemClock::duration kPwCacheTimeout = std::chrono::seconds(60);

// Time interval between random address changes when privacy is enabled (see
// T_GAP(private_addr_int) in 5.0 Vol 3, Part C, Appendix A)
constexpr pw::chrono::SystemClock::duration kPrivateAddressTimeout = std::chrono::minutes(15);
constexpr pw::chrono::SystemClock::duration kPwPrivateAddressTimeout = std::chrono::minutes(15);

// Maximum duration for which a scannable advertisement will be stored and not reported to
// clients until a corresponding scan response is received.
//
// This number has been determined empirically but over a limited number of devices. According to
// Core Spec. v5.2 Vol 6, Part B, Section 4.4 and in practice, the typical gap between the two
// events from the same peer is <=10ms. However in practice it's possible to see gaps as high as
// 1.5 seconds or more.
constexpr zx::duration kLEScanResponseTimeout = zx::sec(2);
constexpr pw::chrono::SystemClock::duration kPwLEScanResponseTimeout = std::chrono::seconds(2);

// GATT types used in the GAP service.
constexpr UUID kGenericAccessService(uint16_t{0x1800});
constexpr UUID kDeviceNameCharacteristic(uint16_t{0x2A00});
constexpr UUID kAppearanceCharacteristic(uint16_t{0x2A01});
constexpr UUID kPeripheralPreferredConnectionParametersCharacteristic(uint16_t{0x2A04});

// The Peripheral Preferred Connection Parameters Characteristic is optionally included in the GAP
// service of a peripheral (Core Spec v5.2, Vol 3, Part C, Sec 9.12.3).
// See hci_spec::LEConnectionParameters for a description of these fields.
struct PeripheralPreferredConnectionParametersCharacteristicValue {
  uint16_t min_interval;
  uint16_t max_interval;
  uint16_t max_latency;
  uint16_t supervision_timeout;
} __attribute__((packed));

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_
