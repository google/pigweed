// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_EVENT_MASKS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_EVENT_MASKS_H_

#include <cstdint>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

namespace bt::gap {

// Builds and returns the HCI event mask based on our supported host side
// features and controller capabilities. This is used to mask events that we
// do not know how to handle.
constexpr uint64_t BuildEventMask() {
  uint64_t event_mask = 0;

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci_spec::EventMask::event)

  // Enable events that are needed for basic functionality. (alphabetic)
  ENABLE_EVT(kAuthenticationCompleteEvent);
  ENABLE_EVT(kConnectionCompleteEvent);
  ENABLE_EVT(kConnectionRequestEvent);
  ENABLE_EVT(kDataBufferOverflowEvent);
  ENABLE_EVT(kDisconnectionCompleteEvent);
  ENABLE_EVT(kEncryptionChangeEvent);
  ENABLE_EVT(kEncryptionKeyRefreshCompleteEvent);
  ENABLE_EVT(kExtendedInquiryResultEvent);
  ENABLE_EVT(kHardwareErrorEvent);
  ENABLE_EVT(kInquiryCompleteEvent);
  ENABLE_EVT(kInquiryResultEvent);
  ENABLE_EVT(kInquiryResultWithRSSIEvent);
  ENABLE_EVT(kIOCapabilityRequestEvent);
  ENABLE_EVT(kIOCapabilityResponseEvent);
  ENABLE_EVT(kLEMetaEvent);
  ENABLE_EVT(kLinkKeyRequestEvent);
  ENABLE_EVT(kLinkKeyNotificationEvent);
  ENABLE_EVT(kRemoteOOBDataRequestEvent);
  ENABLE_EVT(kRemoteNameRequestCompleteEvent);
  ENABLE_EVT(kReadRemoteSupportedFeaturesCompleteEvent);
  ENABLE_EVT(kReadRemoteVersionInformationCompleteEvent);
  ENABLE_EVT(kReadRemoteExtendedFeaturesCompleteEvent);
  ENABLE_EVT(kRoleChangeEvent);
  ENABLE_EVT(kSimplePairingCompleteEvent);
  ENABLE_EVT(kSynchronousConnectionCompleteEvent);
  ENABLE_EVT(kUserConfirmationRequestEvent);
  ENABLE_EVT(kUserPasskeyRequestEvent);
  ENABLE_EVT(kUserPasskeyNotificationEvent);

#undef ENABLE_EVT

  return event_mask;
}

// Builds and returns the LE event mask based on our supported host side
// features and controller capabilities. This is used to mask LE events that
// we do not know how to handle.
constexpr uint64_t BuildLEEventMask() {
  uint64_t event_mask = 0;

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci_spec::LEEventMask::event)

  ENABLE_EVT(kLEAdvertisingReport);
  ENABLE_EVT(kLEConnectionComplete);
  ENABLE_EVT(kLEConnectionUpdateComplete);
  ENABLE_EVT(kLEExtendedAdvertisingSetTerminated);
  ENABLE_EVT(kLELongTermKeyRequest);
  ENABLE_EVT(kLEReadRemoteFeaturesComplete);

#undef ENABLE_EVT

  return event_mask;
}

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_EVENT_MASKS_H_
