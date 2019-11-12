// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_

// clang-format off

#include <cstdint>

#include <fbl/macros.h>
#include <fbl/ref_ptr.h>
#include <lib/fit/function.h>
#include <zircon/compiler.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace l2cap {

class Channel;
using ChannelCallback = fit::function<void(fbl::RefPtr<Channel>)>;

// Callback invoked when a logical link should be closed due to an error.
using LinkErrorCallback = fit::closure;

// Callback called to notify LE preferred connection parameters during the "LE
// Connection Parameter Update" procedure.
using LEConnectionParameterUpdateCallback =
    fit::function<void(const hci::LEPreferredConnectionParameters&)>;

// Callback used to deliver LE fixed channels that are created when a LE link is
// registered with L2CAP.
using LEFixedChannelsCallback =
    fit::function<void(fbl::RefPtr<Channel> att, fbl::RefPtr<Channel> smp)>;

// Callback used to request a security upgrade for an active logical link.
// Invokes its |callback| argument with the result of the operation.
using SecurityUpgradeCallback = fit::function<void(
    hci::ConnectionHandle ll_handle, sm::SecurityLevel level, sm::StatusCallback callback)>;

// L2CAP channel identifier uniquely identifies fixed and connection-oriented
// channels over a logical link.
// (see Core Spec v5.0, Vol 3, Part A, Section 2.1)
using ChannelId = uint16_t;

// Null ID, "never be used as a destination endpoint"
constexpr ChannelId kInvalidChannelId = 0x0000;

// Fixed channel identifiers used in BR/EDR & AMP (i.e. ACL-U, ASB-U, and AMP-U
// logical links)
constexpr ChannelId kSignalingChannelId = 0x0001;
constexpr ChannelId kConnectionlessChannelId = 0x0002;
constexpr ChannelId kAMPManagerChannelId = 0x0003;
constexpr ChannelId kSMPChannelId = 0x0007;
constexpr ChannelId kAMPTestManagerChannelId = 0x003F;

// Fixed channel identifiers used in LE
constexpr ChannelId kATTChannelId = 0x0004;
constexpr ChannelId kLESignalingChannelId = 0x0005;
constexpr ChannelId kLESMPChannelId = 0x0006;


// Range of dynamic channel identifiers; each logical link has its own set of
// channel IDs (except for ACL-U and AMP-U, which share a namespace)
// (see Tables 2.1 and 2.2 in v5.0, Vol 3, Part A, Section 2.1)
constexpr ChannelId kFirstDynamicChannelId = 0x0040;
constexpr ChannelId kLastACLDynamicChannelId = 0xFFFF;
constexpr ChannelId kLastLEDynamicChannelId = 0x007F;

// Basic L2CAP header. This corresponds to the header used in a B-frame (Basic Information Frame)
// and is the basis of all other frame types.
struct BasicHeader {
  uint16_t length;
  ChannelId channel_id;
} __PACKED;

// Frame Check Sequence (FCS) footer. This is computed for S- and I-frames per Core Spec v5.0, Vol
// 3, Part A, Section 3.3.5.
struct FrameCheckSequence {
  uint16_t fcs;
} __PACKED;

// Initial state of the FCS generating circuit is all zeroes per v5.0, Vol 3, Part A, Section 3.3.5,
// Figure 3.5.
constexpr FrameCheckSequence kInitialFcsValue = {0};

// The L2CAP MTU defines the maximum SDU size and is asymmetric. The following are the minimum and
// default MTU sizes that a L2CAP implementation must support (see Core Spec v5.0, Vol 3, Part A,
// Section 5.1).
constexpr uint16_t kDefaultMTU = 672;
constexpr uint16_t kMinACLMTU = 48;
constexpr uint16_t kMinLEMTU = 23;

// The maximum length of a L2CAP B-frame information payload.
constexpr uint16_t kMaxBasicFramePayloadSize = 65535;

// Channel configuration option type field (Core Spec v5.1, Vol 3, Part A, Section 5):
enum class OptionType : uint8_t {
  kMTU = 0x01,
  kFlushTimeout = 0x02,
  kQoS = 0x03,
  kRetransmissionAndFlowControl = 0x04,
  kFCS = 0x05,
  kExtendedFlowSpecification = 0x06,
  kExtendedWindowSize = 0x07,
};

// Signaling packet formats (Core Spec v5.0, Vol 3, Part A, Section 4):

using CommandCode = uint8_t;

enum class RejectReason : uint16_t {
  kNotUnderstood = 0x0000,
  kSignalingMTUExceeded = 0x0001,
  kInvalidCID = 0x0002,
};

// Results field in Connection Response and Create Channel Response
enum class ConnectionResult : uint16_t {
  kSuccess = 0x0000,
  kPending = 0x0001,
  kPSMNotSupported = 0x0002,
  kSecurityBlock = 0x0003,
  kNoResources = 0x0004,
  kControllerNotSupported = 0x0005,  // for Create Channel only
  kInvalidSourceCID = 0x0006,
  kSourceCIDAlreadyAllocated = 0x0007,
};

enum class ConnectionStatus : uint16_t {
  kNoInfoAvailable = 0x0000,
  kAuthenticationPending = 0x0001,
  kAuthorizationPending = 0x0002,
};

// Flags field in Configuration request and response, continuation bit mask
constexpr uint16_t kConfigurationContinuation = 0x0001;

enum class ConfigurationResult : uint16_t {
  kSuccess = 0x0000,
  kUnacceptableParameters = 0x0001,
  kRejected = 0x0002,
  kUnknownOptions = 0x0003,
  kPending = 0x0004,
  kFlowSpecRejected = 0x0005,
};

enum class ChannelMode : uint8_t {
  kBasic = 0x00,
  kRetransmission = 0x01,
  kFlowControl = 0x02,
  kEnhancedRetransmission = 0x03,
  kStreaming = 0x04
};

enum class InformationType : uint16_t {
  kConnectionlessMTU = 0x0001,
  kExtendedFeaturesSupported = 0x0002,
  kFixedChannelsSupported = 0x0003,
};

enum class InformationResult : uint16_t {
  kSuccess = 0x0000,
  kNotSupported = 0x0001,
};

// Type and bit masks for Extended Features Supported in the Information
// Response data field (Vol 3, Part A, Section 4.12)
using ExtendedFeatures = uint32_t;
constexpr ExtendedFeatures kExtendedFeaturesBitFlowControl = 1 << 0;
constexpr ExtendedFeatures kExtendedFeaturesBitRetransmission = 1 << 1;
constexpr ExtendedFeatures kExtendedFeaturesBitBidirectionalQoS = 1 << 2;
constexpr ExtendedFeatures kExtendedFeaturesBitEnhancedRetransmission = 1 << 3;
constexpr ExtendedFeatures kExtendedFeaturesBitStreaming = 1 << 4;
constexpr ExtendedFeatures kExtendedFeaturesBitFCSOption = 1 << 5;
constexpr ExtendedFeatures kExtendedFeaturesBitExtendedFlowSpecification = 1 << 6;
constexpr ExtendedFeatures kExtendedFeaturesBitFixedChannels = 1 << 7;
constexpr ExtendedFeatures kExtendedFeaturesBitExtendedWindowSize = 1 << 8;
constexpr ExtendedFeatures kExtendedFeaturesBitUnicastConnectionlessDataRx = 1 << 9;

// Type and bit masks for Fixed Channels Supported in the Information Response
// data field (Vol 3, Part A, Section 4.12)
using FixedChannelsSupported = uint64_t;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitNull = 1ULL << 0;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitSignaling = 1ULL << 1;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitConnectionless = 1ULL << 2;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitAMPManager = 1ULL << 3;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitATT = 1ULL << 4;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitLESignaling = 1ULL << 5;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitSMP = 1ULL << 6;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitSM = 1ULL << 7;
constexpr FixedChannelsSupported kFixedChannelsSupportedBitAMPTestManager = 1ULL << 63;

enum class ConnectionParameterUpdateResult : uint16_t {
  kAccepted = 0x0000,
  kRejected = 0x0001,
};

enum class LECreditBasedConnectionResult : uint16_t {
  kSuccess = 0x0000,
  kPSMNotSupported = 0x0002,
  kNoResources = 0x0004,
  kInsufficientAuthentication = 0x0005,
  kInsufficientAuthorization = 0x0006,
  kInsufficientEncryptionKeySize = 0x0007,
  kInsufficientEncryption = 0x0008,
  kInvalidSourceCID = 0x0009,
  kSourceCIDAlreadyAllocated = 0x000A,
  kUnacceptableParameters = 0x000B,
};

// Type used for all Protocol and Service Multiplexer (PSM) identifiers,
// including those dynamically-assigned/-obtained
using PSM = uint16_t;
constexpr PSM kInvalidPSM = 0x0000;

// Well-known Protocol and Service Multiplexer values defined by the Bluetooth
// SIG in Logical Link Control Assigned Numbers
// https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control
constexpr PSM kSDP = 0x0001;
constexpr PSM kRFCOMM = 0x0003;
constexpr PSM kTCSBIN = 0x0005; // Telephony Control Specification
constexpr PSM kTCSBINCordless = 0x0007;
constexpr PSM kBNEP = 0x0009; // Bluetooth Network Encapsulation Protocol
constexpr PSM kHIDControl = 0x0011; // Human Interface Device
constexpr PSM kHIDInteerup = 0x0013; // Human Interface Device
constexpr PSM kAVCTP = 0x0017; // Audio/Video Control Transport Protocol
constexpr PSM kAVDTP = 0x0019; // Audio/Video Distribution Transport Protocol
constexpr PSM kAVCTP_Browse = 0x001B; // Audio/Video Remote Control Profile (Browsing)
constexpr PSM kATT = 0x001F; // ATT
constexpr PSM k3DSP = 0x0021; // 3D Synchronization Profile
constexpr PSM kLE_IPSP = 0x0023; // Internet Protocol Support Profile
constexpr PSM kOTS = 0x0025; // Object Transfer Service

// Identifier assigned to each signaling transaction. This is used to match each
// signaling channel request with a response.
using CommandId = uint8_t;

constexpr CommandId kInvalidCommandId = 0x00;

// Signaling command header.
struct CommandHeader {
  CommandCode code;
  CommandId id;
  uint16_t length;  // Length of the remaining payload
} __PACKED;

// ACL-U & LE-U
constexpr CommandCode kCommandRejectCode = 0x01;
constexpr size_t kCommandRejectMaxDataLength = 4;
struct CommandRejectPayload {
  CommandRejectPayload() = delete;
  DISALLOW_COPY_ASSIGN_AND_MOVE(CommandRejectPayload);

  // See RejectReason for possible values.
  uint16_t reason;

  // Up to 4 octets of optional data (see Vol 3, Part A, Section 4.1)
  uint8_t data[];
} __PACKED;

// ACL-U
constexpr CommandCode kConnectionRequest = 0x02;
struct ConnectionRequestPayload {
  uint16_t psm;
  ChannelId src_cid;
} __PACKED;

// ACL-U
constexpr CommandCode kConnectionResponse = 0x03;
struct ConnectionResponsePayload {
  ChannelId dst_cid;
  ChannelId src_cid;
  ConnectionResult result;
  ConnectionStatus status;
} __PACKED;

// ACL-U
constexpr CommandCode kConfigurationRequest = 0x04;
constexpr size_t kConfigurationOptionMaxDataLength = 22;

// Element of configuration payload data (see Vol 3, Part A, Section 5)
struct ConfigurationOption {
  ConfigurationOption() = delete;
  DISALLOW_COPY_ASSIGN_AND_MOVE(ConfigurationOption);

  OptionType type;
  uint8_t length;

  // Up to 22 octets of data
  uint8_t data[];
} __PACKED;

// Payload of ConfigurationOption (see Vol 3, Part A, Section 5.1)
struct MtuOptionPayload {
  uint16_t mtu;
} __PACKED;

// Payload of ConfigurationOption (see Vol 3, Part A, Section 5.4)
struct RetransmissionAndFlowControlOptionPayload {
  ChannelMode mode;
  uint8_t tx_window_size;
  uint8_t max_transmit;
  uint16_t rtx_timeout;
  uint16_t monitor_timeout;
  // Maximum PDU size
  uint16_t mps;
} __PACKED;

struct ConfigurationRequestPayload {
  ConfigurationRequestPayload() = delete;
  DISALLOW_COPY_ASSIGN_AND_MOVE(ConfigurationRequestPayload);

  ChannelId dst_cid;
  uint16_t flags;

  // Followed by zero or more configuration options of varying length
  uint8_t data[];
} __PACKED;

// ACL-U
constexpr CommandCode kConfigurationResponse = 0x05;
struct ConfigurationResponsePayload {
  ConfigurationResponsePayload() = delete;
  DISALLOW_COPY_ASSIGN_AND_MOVE(ConfigurationResponsePayload);

  ChannelId src_cid;
  uint16_t flags;
  ConfigurationResult result;

  // Followed by zero or more configuration options of varying length
  uint8_t data[];
} __PACKED;

// ACL-U & LE-U
constexpr CommandCode kDisconnectionRequest = 0x06;
struct DisconnectionRequestPayload {
  ChannelId dst_cid;
  ChannelId src_cid;
} __PACKED;

// ACL-U & LE-U
constexpr CommandCode kDisconnectionResponse = 0x07;
struct DisconnectionResponsePayload {
  ChannelId dst_cid;
  ChannelId src_cid;
} __PACKED;

// ACL-U
constexpr CommandCode kEchoRequest = 0x08;

// ACL-U
constexpr CommandCode kEchoResponse = 0x09;

// ACL-U
constexpr CommandCode kInformationRequest = 0x0A;
struct InformationRequestPayload {
  InformationType type;
} __PACKED;

// ACL-U
constexpr CommandCode kInformationResponse = 0x0B;
constexpr size_t kInformationResponseMaxDataLength = 8;
struct InformationResponsePayload {
  InformationResponsePayload() = delete;
  DISALLOW_COPY_ASSIGN_AND_MOVE(InformationResponsePayload);

  InformationType type;
  InformationResult result;

  // Up to 8 octets of optional data (see Vol 3, Part A, Section 4.11)
  uint8_t data[];
} __PACKED;

// LE-U
constexpr CommandCode kConnectionParameterUpdateRequest = 0x12;
struct ConnectionParameterUpdateRequestPayload {
  uint16_t interval_min;
  uint16_t interval_max;
  uint16_t slave_latency;
  uint16_t timeout_multiplier;
} __PACKED;

// LE-U
constexpr CommandCode kConnectionParameterUpdateResponse = 0x13;
struct ConnectionParameterUpdateResponsePayload {
  ConnectionParameterUpdateResult result;
} __PACKED;

// LE-U
constexpr CommandCode kLECreditBasedConnectionRequest = 0x14;
struct LECreditBasedConnectionRequestPayload {
  uint16_t le_psm;
  ChannelId src_cid;
  uint16_t mtu;  // Max. SDU size
  uint16_t mps;  // Max. PDU size
  uint16_t initial_credits;
} __PACKED;

// LE-U
constexpr CommandCode kLECreditBasedConnectionResponse = 0x15;
struct LECreditBasedConnectionResponsePayload {
  ChannelId dst_cid;
  uint16_t mtu;  // Max. SDU size
  uint16_t mps;  // Max. PDU size
  uint16_t initial_credits;
  LECreditBasedConnectionResult result;
} __PACKED;

// LE-U
constexpr CommandCode kLEFlowControlCredit = 0x16;
struct LEFlowControlCreditParams {
  ChannelId cid;
  uint16_t credits;
} __PACKED;

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_
