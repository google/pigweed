// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_

#include <cstdint>

#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"

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

// Enum for the supported values of the LE Security Mode as defined in spec v5.2 Vol 3 Part C 10.2.
enum class LeSecurityMode {
  // Mode 1 entails possibly encrypted, possibly authenticated communication.
  Mode1,
  // Secure Connections Only mode enforces that all encrypted transmissions use 128-bit,
  // SC-generated and authenticated encryption keys.
  SecureConnectionsOnly,
};
const char* LeSecurityModeToString(LeSecurityMode mode);

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

constexpr zx::duration kLEGeneralDiscoveryScanMin = zx::msec(10240);
constexpr zx::duration kLEGeneralDiscoveryScanMinCoded = zx::msec(30720);
constexpr zx::duration kLEScanFastPeriod = zx::msec(30720);

// Recommended scan and advertising parameters that can be passed directly to the HCI commands.
// The HCI spec defines the time conversion as follows: Time =  N * 0.625 ms,
// where N is the value of the constant.
//
// A constant that contans the word "Coded" is recommended when using the LE
// Coded PHY. Otherwise the constant is recommended when using the LE 1M PHY.

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
constexpr zx::duration kLECreateConnectionTimeout = zx::sec(20);
// Timeout used for the Br/Edr Create Connection command.
constexpr zx::duration kBrEdrCreateConnectionTimeout = zx::sec(20);

// Timeout used for scanning during LE General CEP. Selected to be longer than the scan period.
constexpr zx::duration kLEGeneralCepScanTimeout = zx::sec(15);

// Connection Interval Timing Parameters (see v5.0, Vol 3, Part C,
// Section 9.3.12 and Appendix A)
constexpr zx::duration kLEConnectionParameterTimeout = zx::sec(30);
// Recommended minimum time upon connection establishment before the central starts a connection
// update procedure.
constexpr zx::duration kLEConnectionPauseCentral = zx::sec(1);
// Recommended minimum time upon connection establishment before the peripheral starts a connection
// update procedure.
constexpr zx::duration kLEConnectionPausePeripheral = zx::sec(5);

constexpr uint16_t kLEInitialConnIntervalMin = 0x0018;       // 30 ms
constexpr uint16_t kLEInitialConnIntervalMax = 0x0028;       // 50 ms
constexpr uint16_t kLEInitialConnIntervalCodedMin = 0x0048;  // 90 ms
constexpr uint16_t kLEInitialConnIntervalCodedMax = 0x0078;  // 150 ms

// Time interval that must expire before a temporary device is removed from the
// cache.
constexpr zx::duration kCacheTimeout = zx::sec(60);

// Time interval between random address changes when privacy is enabled (see
// T_GAP(private_addr_int) in 5.0 Vol 3, Part C, Appendix A)
constexpr zx::duration kPrivateAddressTimeout = zx::min(15);

// Maximum duration for which a scannable advertisement will be stored and not reported to
// clients until a corresponding scan response is received.
//
// This number has been determined empirically but over a limited number of devices. According to
// Core Spec. v5.2 Vol 6, Part B, Section 4.4 and in practice, the typical gap between the two
// events from the same peer is <=10ms. However in practice it's possible to see gaps as high as
// 1.5 seconds or more.
constexpr zx::duration kLEScanResponseTimeout = zx::sec(2);

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GAP_H_
