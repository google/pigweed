// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SDP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SDP_H_

#include <zircon/compiler.h>

#include <list>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"

namespace bt::sdp {

class Status;

// Each service has a service record handle that uniquely identifies that
// service within an SDP server.
using ServiceHandle = uint32_t;

// Handle to service record representing SDP itself. (Vol 3, Part B, 2.2)
constexpr ServiceHandle kSDPHandle = 0x00000000;
// Handles 0x0000001 - 0x0000FFFF are reserved (Vol 3, Part B, 5.1.1)
constexpr ServiceHandle kFirstUnreservedHandle = 0x00010000;
constexpr ServiceHandle kLastHandle = 0xFFFFFFFF;

// Valid security levels for services. (Vol 3, Part C, 5.2.2)
enum class SecurityLevel : uint8_t {
  kNone = 0,
  kEncOptional = 1,
  kEncRequired = 2,
  kMITMProtected = 3,
  kHighStrength = 4,
};

// Vol 3, Part B, 2.3.1
using AttributeId = uint16_t;

// Referred to as "PDU ID" in the spec.
using OpCode = uint8_t;

using TransactionId = uint16_t;

// Header for each SDP PDU.
// v5.0, Vol 3, Part B, 4.2
struct Header {
  OpCode pdu_id;
  TransactionId tid;
  uint16_t param_length;
} __PACKED;

// v5.0, Vol 3, Part B, 4.4.1
enum class ErrorCode : uint16_t {
  kReserved = 0x0000,
  kUnsupportedVersion = 0x0001,
  kInvalidRecordHandle = 0x0002,
  kInvalidRequestSyntax = 0x0003,
  kInvalidSize = 0x0004,
  kInvalidContinuationState = 0x0005,
  kInsufficientResources = 0x0006,
};

// ===== SDP PDUs =====
constexpr OpCode kReserved = 0x00;

// Error Handling
constexpr OpCode kErrorResponse = 0x01;

// Service Search Transaction
constexpr OpCode kServiceSearchRequest = 0x02;
constexpr OpCode kServiceSearchResponse = 0x03;

// Service Attribute Transaction
constexpr OpCode kServiceAttributeRequest = 0x04;
constexpr OpCode kServiceAttributeResponse = 0x05;

// Service Search Attribute Transaction
constexpr OpCode kServiceSearchAttributeRequest = 0x06;
constexpr OpCode kServiceSearchAttributeResponse = 0x07;

// ====== SDP Protocol UUIDs ======
// Defined in the Bluetooth Assigned Numbers:
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery

namespace protocol {

constexpr UUID kSDP(uint16_t{0x0001});
constexpr UUID kRFCOMM(uint16_t{0x0003});
constexpr UUID kATT(uint16_t{0x0007});
constexpr UUID kOBEX(uint16_t{0x0008});  // IrDA Interop
constexpr UUID kBNEP(uint16_t{0x000F});
constexpr UUID kHIDP(uint16_t{0x0011});
constexpr UUID kAVCTP(uint16_t{0x0017});
constexpr UUID kAVDTP(uint16_t{0x0019});
constexpr UUID kL2CAP(uint16_t{0x0100});

}  // namespace protocol

// ====== SDP Profile / Class UUIDs =====
// Defined in the Bluetooth Assigned Numbers:
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery

namespace profile {

// Service Discovery Profile (SDP)
constexpr UUID kServiceDiscoveryClass(uint16_t{0x1000});
constexpr UUID kBrowseGroupClass(uint16_t{0x1001});
// Serial Port Profile (SPP)
constexpr UUID kSerialPort(uint16_t{0x1101});
// Dial-up Networking Profile (DUN)
constexpr UUID kDialupNetworking(uint16_t{0x1103});
// Object Push Profile (OPP)
constexpr UUID kObexObjectPush(uint16_t{0x1105});
// File Transfer Profile (FTP)
constexpr UUID kObexFileTransfer(uint16_t{0x1106});
// Headset Profile (HSP)
constexpr UUID kHeadset(uint16_t{0x1108});
constexpr UUID kHeadsetAudioGateway(uint16_t{0x1112});
constexpr UUID kHeadsetHS(uint16_t{0x1131});
// Advanced Audio Distribution Profile (A2DP)
constexpr UUID kAudioSource(uint16_t{0x110A});
constexpr UUID kAudioSink(uint16_t{0x110B});
constexpr UUID kAdvancedAudioDistribution(uint16_t{0x110D});
// Audio/Video Remote Control Profile (AVRCP)
constexpr UUID kAVRemoteControlTarget(uint16_t{0x110C});
constexpr UUID kAVRemoteControl(uint16_t{0x110E});
constexpr UUID kAVRemoteControlController(uint16_t{0x110F});
// Personal Area Networking (PAN)
constexpr UUID kPANU(uint16_t{0x1115});
constexpr UUID kNAP(uint16_t{0x1116});
constexpr UUID kGN(uint16_t{0x1117});
// Basic Printing and Basic Imaging Profiles omitted (unsupported)
// Hands-Free Profile (HFP)
constexpr UUID kHandsfree(uint16_t{0x111E});
constexpr UUID kHandsfreeAudioGateway(uint16_t{0x111F});
// Human Interface Device omitted (unsupported)
// Hardcopy Cable Replacement Profile omitted (unsupported)
// Sim Access Profile (SAP)
constexpr UUID kSIM_Access(uint16_t{0x112D});
// Phonebook Access Profile (PBAP)
constexpr UUID kPhonebookPCE(uint16_t{0x112E});
constexpr UUID kPhonebookPSE(uint16_t{0x112F});
constexpr UUID kPhonebook(uint16_t{0x1130});
// Message Access Profile (MAP)
constexpr UUID kMessageAccessServer(uint16_t{0x1132});
constexpr UUID kMessageNotificationServer(uint16_t{0x1133});
constexpr UUID kMessageAccessProfile(uint16_t{0x1134});
// GNSS and 3DSP omitted (unsupported)
// Multi-Profile Specification (MPS)
constexpr UUID kMPSProfile(uint16_t{0x113A});
constexpr UUID kMPSClass(uint16_t{0x113B});
// Calendar, Task, and Notes Profile omitted (unsupported)
// Device ID
constexpr UUID kPeerIdentification(uint16_t{0x1200});
// Video Distribution Profile (VDP)
constexpr UUID kVideoSource(uint16_t{0x1303});
constexpr UUID kVideoSink(uint16_t{0x1304});
constexpr UUID kVideoDistribution(uint16_t{0x1305});
// Health Device Profile (HDP)
constexpr UUID kHDP(uint16_t{0x1400});
constexpr UUID kHDPSource(uint16_t{0x1401});
constexpr UUID kHDPSink(uint16_t{0x1402});

}  // namespace profile

// ====== SDP Attribute IDs ======

// ====== Universal Attribute Definitions =====
// v5.0, Vol 3, Part B, Sec 5.1

// Service Record Handle
constexpr AttributeId kServiceRecordHandle = 0x0000;

using ServiceRecordHandleValueType = uint32_t;

// Service Class ID List
constexpr AttributeId kServiceClassIdList = 0x0001;

// A seqeunce of UUIDs.
// Must contain at least one UUID.
using ServiceClassIdListValueType = std::vector<DataElement>;

// Service Record State
// Used to facilitate caching. If any part of the service record changes,
// this value must change.
constexpr AttributeId kServiceRecordState = 0x0002;

using ServiceRecordStateValueType = uint32_t;

// Service ID
constexpr AttributeId kServiceId = 0x0003;

using ServiceIdValueType = UUID;

// Protocol Descriptor List
constexpr AttributeId kProtocolDescriptorList = 0x0004;

// This is a list of DataElementSequences, of which each has as it's first
// element a Protocol UUID, followed by prtoocol-specific parameters.
// See v5.0, Vol 3, Part B, Sec 5.1.5
using ProtocolDescriptorListValueType = std::vector<DataElement>;

// AdditionalProtocolDescriptorList
constexpr AttributeId kAdditionalProtocolDescriptorList = 0x000D;

// This is a sequence of Protocol Descriptor Lists
using AdditionalProtocolDescriptorListValueType = std::vector<DataElement>;

// Browse Group List
// Browse Group lists are described in v5.0, Vol 3, Part B, Sec 2.6
constexpr AttributeId kBrowseGroupList = 0x0005;

// This is a sequence which is composed of UUIDs of the groups that this
// service belongs to.
using BrowseGroupListValueType = std::vector<DataElement>;

// The UUID used for the root of the browsing hierarchy
constexpr UUID kPublicBrowseRootUuid(uint16_t{0x1002});

// Language Base Attribute Id List
constexpr AttributeId kLanguageBaseAttributeIdList = 0x0006;

// A sequence of uint16_t triplets containing:
//  - An identifier for a natural language from ISO 639:1988 (E/F)
//  - A character encoding to use for the language (UTF-8 is 106)
//  - An attribute ID as the base attribute for offset for human-readable
//    information about the service.
using LanguageBaseAttributeIdListValueType = std::vector<DataElement>;

// Service Info TTL
// Number of seconds that the service record is expected to be valid and
// unchanged.
constexpr AttributeId kServiceInfoTimeToLive = 0x0007;

using ServiceInfoTimeToLiveValueType = uint32_t;

// Service Availability
// Represents the relative ability of the service to accept additional clients.
// 0x00 means no clients can connect, 0xFF means no one is using it.
// See Vol 3, Part B, 5.1.10
constexpr AttributeId kServiceAvailability = 0x0008;

using ServiceAvailabilityValueType = uint8_t;

// Bluetooth Profile Descriptor List
constexpr AttributeId kBluetoothProfileDescriptorList = 0x0009;

// A Sequence of Sequences with:
//  - a UUID for a profile
//  - A 16-bit unsigned version number with:
//     - 8 MSbits major version
//     - 8 LSbits minor version
using BluetoothProfileDescriptorListValueType = std::vector<DataElement>;

// TODO(jamuraa): Documentation URL, ClientExecutalbleURL, IconURL
// When we support the URL type.

// ##### Language Attribute Offsets #####
// These must be added to the attribute ID retrieved from the
// LanguageBaseAttributeIdList

// Service Name
constexpr AttributeId kServiceNameOffset = 0x0000;

using ServiceNameValueType = std::string;

// Service Description
constexpr AttributeId kServiceDescriptionOffset = 0x0001;

using ServiceDescriptionValueType = std::string;

// Provider Name
constexpr AttributeId kProviderNameOffset = 0x0002;

using ProviderNameValueType = std::string;

// ===== ServiceDiscoveryServer Service Class Attribute Definitions ======
// These attributes are defined as valid for the ServiceDiscoveryServer.
// See Spec v5.0, Vol 3, Part B, Section 5.2

// VersionNumberList is a list of the versions supported by the SDP server.
// See v5.0, Vol 3, Part B, Section 5.2.3
constexpr AttributeId kSDP_VersionNumberList = 0x0200;

using SDP_VersionNumberListType = std::vector<DataElement>;

// ServiceDatabaseState is a 32-bit integer that is changed whenever any other
// service records are added or deleted from the database.
constexpr AttributeId kSDP_ServiceDatabaseState = 0x0201;

// ===== Advanced Audio Distribution Profile Attribute Definitions ======
// These attributes are defined as valid for the AudioSource and AudioSink
// Service Class UUIDs in the Assigned Numbers for SDP
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
constexpr AttributeId kA2DP_SupportedFeatures = 0x0311;

}  // namespace bt::sdp

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_SDP_H_
