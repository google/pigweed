// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_DEFS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_DEFS_H_

// clang-format off

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#include <pw_chrono/system_clock.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/frame_headers.h"

namespace bt::l2cap {

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. Note that we assume there is no flush timeout
// on the underlying logical link.
static constexpr auto kErtmReceiverReadyPollTimerDuration = std::chrono::seconds(2);
static_assert(kErtmReceiverReadyPollTimerDuration <= std::chrono::milliseconds(std::numeric_limits<uint16_t>::max()));
static constexpr uint16_t kErtmReceiverReadyPollTimerMsecs = static_cast<uint16_t>(std::chrono::duration_cast<std::chrono::milliseconds>(kErtmReceiverReadyPollTimerDuration).count());

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. Note that we assume there is no flush timeout
// on the underlying logical link. If the link _does_ have a flush timeout, then our implementation
// will be slower to trigger the monitor timeout than the specification recommends.
static constexpr auto kErtmMonitorTimerDuration = std::chrono::seconds(12);
static_assert(kErtmMonitorTimerDuration <= std::chrono::milliseconds(std::numeric_limits<uint16_t>::max()));
static constexpr uint16_t kErtmMonitorTimerMsecs = static_cast<uint16_t>(std::chrono::duration_cast<std::chrono::milliseconds>(kErtmMonitorTimerDuration).count());

// See Core Spec v5.0, Volume 3, Part A, Sec 6.2.1. This is the initial value of the timeout duration.
// Although Signaling Channel packets are not sent as automatically flushable, Signaling Channel packets
// may not receive a response for reasons other than packet loss (e.g. peer is slow to respond due to pairing).
// As such, L2CAP uses the "at least double" back-off scheme to increase this timeout after retransmissions.
static constexpr auto kSignalingChannelResponseTimeout = std::chrono::seconds(1);
static_assert(kSignalingChannelResponseTimeout >= std::chrono::seconds(1));
static_assert(kSignalingChannelResponseTimeout <= std::chrono::seconds(60));

// Selected so that total time between initial transmission and last retransmission timout is less
// than 60 seconds when using the exponential back-off scheme.
static constexpr size_t kMaxSignalingChannelTransmissions = 5;

// See Core Spec v5.0, Volume 3, Part A, Sec 6.2.2. This initial value is the only timeout duration
// used because Signaling Channel packets are not to be sent as automatically flushable and thus
// requests will not be retransmitted at the L2CAP level per its "at least double" back-off scheme.
static constexpr auto kSignalingChannelExtendedResponseTimeout = std::chrono::seconds(60);
static_assert(kSignalingChannelExtendedResponseTimeout >= std::chrono::seconds(60));
static_assert(kSignalingChannelExtendedResponseTimeout <= std::chrono::seconds(300));
static constexpr pw::chrono::SystemClock::duration kPwSignalingChannelExtendedResponseTimeout = std::chrono::seconds(60);

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
} __attribute__((packed));

// Frame Check Sequence (FCS) footer. This is computed for S- and I-frames per Core Spec v5.0, Vol
// 3, Part A, Section 3.3.5.
struct FrameCheckSequence {
  uint16_t fcs;
} __attribute__((packed));

// Initial state of the FCS generating circuit is all zeroes per v5.0, Vol 3, Part A, Section 3.3.5,
// Figure 3.5.
constexpr FrameCheckSequence kInitialFcsValue = {0};

// The L2CAP MTU defines the maximum SDU size and is asymmetric. The following are the minimum and
// default MTU sizes that a L2CAP implementation must support (see Core Spec v5.0, Vol 3, Part A,
// Section 5.1).
constexpr uint16_t kDefaultMTU = 672;
constexpr uint16_t kMinACLMTU = 48;
constexpr uint16_t kMinLEMTU = 23;
constexpr uint16_t kMaxMTU = 0xFFFF;

// The maximum length of a L2CAP B-frame information payload.
constexpr uint16_t kMaxBasicFramePayloadSize = 65535;

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. This is the minimum permissible value of
// "TxWindow size" in the Retransmission & Flow Control Configuration Option.
static constexpr uint8_t kErtmMinUnackedInboundFrames = 1;

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. We do not have a limit on inbound data that we
// can receive in bursts based on memory constraints or other considerations, so this is simply the
// maximum permissible value.
static constexpr uint8_t kErtmMaxUnackedInboundFrames = 63;

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. We rely on the ERTM Monitor Timeout and the
// ACL-U Link Supervision Timeout to terminate links based on data loss rather than rely on the peer
// to handle unacked ERTM frames in the peer-to-local direction.
static constexpr uint8_t kErtmMaxInboundRetransmissions = 0;  // Infinite retransmissions

// See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. We can receive as large of a PDU as the peer
// can encode and transmit. However, this value is for the information payload field of an I-Frame,
// which is bounded by the 16-bit length field together with frame header and footer overhead.
static constexpr uint16_t kMaxInboundPduPayloadSize = std::numeric_limits<uint16_t>::max() -
                                                      sizeof(internal::EnhancedControlField) -
                                                      sizeof(FrameCheckSequence);

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
  kPsmNotSupported = 0x0002,
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

// Channel modes available in a L2CAP_CONFIGURATION_REQ packet. These are not
// the full set of possible channel modes, see CreditBasedFlowControlMode.
enum class RetransmissionAndFlowControlMode : uint8_t {
  kBasic = 0x00,
  kRetransmission = 0x01,
  kFlowControl = 0x02,
  kEnhancedRetransmission = 0x03,
  kStreaming = 0x04,
};

// Channel modes defined by an associated channel establishment packet rather
// than an L2CAP_CONFIGURATION_REQ packet. The values here are the signaling
// packet code of the connection establishment request packet associated with
// the mode. These are not the full set of possible channel modes, see
// RetransmissionAndFlowControlMode.
enum class CreditBasedFlowControlMode : uint8_t {
  kLeCreditBasedFlowControl = 0x14,
  kEnhancedCreditBasedFlowControl = 0x17,
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
  kPsmNotSupported = 0x0002,
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
using Psm = uint16_t;
constexpr Psm kInvalidPsm = 0x0000;
// The minimum PSM value in the dynamic range of PSMs.
// Defined in 5.2, Vol 3, Part A, 4.2.
constexpr Psm kMinDynamicPsm = 0x1001;

// Well-known Protocol and Service Multiplexer values defined by the Bluetooth
// SIG in Logical Link Control Assigned Numbers
// https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control
constexpr Psm kSDP = 0x0001;
constexpr Psm kRFCOMM = 0x0003;
constexpr Psm kTCSBIN = 0x0005; // Telephony Control Specification
constexpr Psm kTCSBINCordless = 0x0007;
constexpr Psm kBNEP = 0x0009; // Bluetooth Network Encapsulation Protocol
constexpr Psm kHIDControl = 0x0011; // Human Interface Device
constexpr Psm kHIDInteerup = 0x0013; // Human Interface Device
constexpr Psm kAVCTP = 0x0017; // Audio/Video Control Transport Protocol
constexpr Psm kAVDTP = 0x0019; // Audio/Video Distribution Transport Protocol
constexpr Psm kAVCTP_Browse = 0x001B; // Audio/Video Remote Control Profile (Browsing)
constexpr Psm kATT = 0x001F; // ATT
constexpr Psm k3DSP = 0x0021; // 3D Synchronization Profile
constexpr Psm kLE_IPSP = 0x0023; // Internet Protocol Support Profile
constexpr Psm kOTS = 0x0025; // Object Transfer Service

// Convenience function for visualizing a PSM. Used for Inspect and logging.
// Returns string formatted |psm| if not recognized.
inline std::string PsmToString(l2cap::Psm psm) {
  switch (psm) {
    case kInvalidPsm:
      return "InvalidPsm";
    case kSDP:
      return "SDP";
    case kRFCOMM:
      return "RFCOMM";
    case kTCSBIN:
      return "TCSBIN";
    case kTCSBINCordless:
      return "TCSBINCordless";
    case kBNEP:
      return "BNEP";
    case kHIDControl:
      return "HIDControl";
    case kHIDInteerup:
      return "HIDInteerup";
    case kAVCTP:
      return "AVCTP";
    case kAVDTP:
      return "AVDTP";
    case kAVCTP_Browse:
      return "AVCTP_Browse";
    case kATT:
      return "ATT";
    case k3DSP:
      return "3DSP";
    case kLE_IPSP:
      return "LE_IPSP";
    case kOTS:
      return "OTS";
  }
  return "PSM:" + std::to_string(psm);
}

// Identifier assigned to each signaling transaction. This is used to match each
// signaling channel request with a response.
using CommandId = uint8_t;

constexpr CommandId kInvalidCommandId = 0x00;

// Signaling command header.
struct CommandHeader {
  CommandCode code;
  CommandId id;
  uint16_t length;  // Length of the remaining payload
} __attribute__((packed));

// ACL-U & LE-U
constexpr CommandCode kCommandRejectCode = 0x01;
constexpr size_t kCommandRejectMaxDataLength = 4;
struct CommandRejectPayload {
  // See RejectReason for possible values.
  uint16_t reason;

  // Followed by up to 4 octets of optional data (see Vol 3, Part A, Section 4.1)
} __attribute__((packed));

// Payload of Command Reject (see Vol 3, Part A, Section 4.1).
struct InvalidCIDPayload {
  // Source CID (relative to rejecter)
  ChannelId src_cid;

  // Destination CID (relative to rejecter)
  ChannelId dst_cid;
} __attribute__((packed));

// ACL-U
constexpr CommandCode kConnectionRequest = 0x02;
struct ConnectionRequestPayload {
  uint16_t psm;
  ChannelId src_cid;
} __attribute__((packed));

// ACL-U
constexpr CommandCode kConnectionResponse = 0x03;
struct ConnectionResponsePayload {
  ChannelId dst_cid;
  ChannelId src_cid;
  ConnectionResult result;
  ConnectionStatus status;
} __attribute__((packed));

// ACL-U
constexpr CommandCode kConfigurationRequest = 0x04;
constexpr size_t kConfigurationOptionMaxDataLength = 22;

// Element of configuration payload data (see Vol 3, Part A, Section 5)
struct ConfigurationOption {
  OptionType type;
  uint8_t length;

  // Followed by configuration option-specific data
} __attribute__((packed));

// Payload of Configuration Option (see Vol 3, Part A, Section 5.1)
struct MtuOptionPayload {
  uint16_t mtu;
} __attribute__((packed));


// Payload of Configuration Option (see Vol 3, Part A, Section 5.2)
struct FlushTimeoutOptionPayload {
  uint16_t flush_timeout;
} __attribute__((packed));

// Payload of Configuration Option (see Vol 3, Part A, Section 5.4)
struct RetransmissionAndFlowControlOptionPayload {
  RetransmissionAndFlowControlMode mode;
  uint8_t tx_window_size;
  uint8_t max_transmit;
  uint16_t rtx_timeout;
  uint16_t monitor_timeout;
  uint16_t mps;
} __attribute__((packed));

struct ConfigurationRequestPayload {
  ChannelId dst_cid;
  uint16_t flags;

  // Followed by zero or more configuration options of varying length
} __attribute__((packed));

// ACL-U
constexpr CommandCode kConfigurationResponse = 0x05;
struct ConfigurationResponsePayload {
  ChannelId src_cid;
  uint16_t flags;
  ConfigurationResult result;

  // Followed by zero or more configuration options of varying length
} __attribute__((packed));

// ACL-U & LE-U
constexpr CommandCode kDisconnectionRequest = 0x06;
struct DisconnectionRequestPayload {
  ChannelId dst_cid;
  ChannelId src_cid;
} __attribute__((packed));

// ACL-U & LE-U
constexpr CommandCode kDisconnectionResponse = 0x07;
struct DisconnectionResponsePayload {
  ChannelId dst_cid;
  ChannelId src_cid;
} __attribute__((packed));

// ACL-U
constexpr CommandCode kEchoRequest = 0x08;

// ACL-U
constexpr CommandCode kEchoResponse = 0x09;

// ACL-U
constexpr CommandCode kInformationRequest = 0x0A;
struct InformationRequestPayload {
  InformationType type;
} __attribute__((packed));

// ACL-U
constexpr CommandCode kInformationResponse = 0x0B;
constexpr size_t kInformationResponseMaxDataLength = 8;
struct InformationResponsePayload {
  InformationType type;
  InformationResult result;

  // Up to 8 octets of optional data (see Vol 3, Part A, Section 4.11)
} __attribute__((packed));

// LE-U
constexpr CommandCode kConnectionParameterUpdateRequest = 0x12;
struct ConnectionParameterUpdateRequestPayload {
  uint16_t interval_min;
  uint16_t interval_max;
  uint16_t peripheral_latency;
  uint16_t timeout_multiplier;
} __attribute__((packed));

// LE-U
constexpr CommandCode kConnectionParameterUpdateResponse = 0x13;
struct ConnectionParameterUpdateResponsePayload {
  ConnectionParameterUpdateResult result;
} __attribute__((packed));

// LE-U
constexpr CommandCode kLECreditBasedConnectionRequest = 0x14;
struct LECreditBasedConnectionRequestPayload {
  uint16_t le_psm;
  ChannelId src_cid;
  uint16_t mtu;  // Max. SDU size
  uint16_t mps;  // Max. PDU size
  uint16_t initial_credits;
} __attribute__((packed));

// LE-U
constexpr CommandCode kLECreditBasedConnectionResponse = 0x15;
struct LECreditBasedConnectionResponsePayload {
  ChannelId dst_cid;
  uint16_t mtu;  // Max. SDU size
  uint16_t mps;  // Max. PDU size
  uint16_t initial_credits;
  LECreditBasedConnectionResult result;
} __attribute__((packed));

// LE-U
constexpr CommandCode kLEFlowControlCredit = 0x16;
struct LEFlowControlCreditParams {
  ChannelId cid;
  uint16_t credits;
} __attribute__((packed));

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_DEFS_H_
