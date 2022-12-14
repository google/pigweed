// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_CONSTANTS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_CONSTANTS_H_

// clang-format off

#include <array>
#include <cstddef>
#include <cstdint>

#include "lib/zx/time.h"

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"

// This file contains constants and numbers used in HCI packet payloads.

namespace bt::hci_spec {

// HCI_Version Assigned Values See the "Assigned Numbers" document for
// reference.
// (https://www.bluetooth.com/specifications/assigned-numbers/host-controller-interface)
enum class HCIVersion : uint8_t {
  // Bluetooth Core Specification 1.0b
  k1_0b = 0,

  // Bluetooth Core Specification 1.1
  k1_1,

  // Bluetooth Core Specification 1.2
  k1_2,

  // Bluetooth Core Specification 2.0 + EDR
  k2_0_EDR,

  // Bluetooth Core Specification 2.1 + EDR
  k2_1_EDR,

  // Bluetooth Core Specification 3.0 + HS
  k3_0_HS,

  // Bluetooth Core Specification 4.0
  k4_0,

  // Bluetooth Core Specification 4.1
  k4_1,

  // Bluetooth Core Specification 4.2
  k4_2,

  // Bluetooth Core Specification 5.0
  k5_0,

  kReserved
};

// Bitmask values for the 8-octet Local Supported LMP Features bit-field. See
// Core Specv5.0, Volume 2, Part C, Section 3.3 "Feature Mask Definition".
enum class LMPFeature : uint64_t {
  // Octet 0
  k3SlotPackets   = (1 << 0),
  k5SlotPackets   = (1 << 1),
  kEncryption     = (1 << 2),
  kSlotOffset     = (1 << 3),
  kTimingAccuracy = (1 << 4),
  kRoleSwitch     = (1 << 5),
  kHoldMode       = (1 << 6),
  kSniffMode      = (1 << 7),

  // Octet 1
  // Reserved (1 << 0 + 8)
  // TODO(armansito): Add definitions

  // Octet 2
  // TODO(armansito): Add definitions

  // Octet 3
  // Reserved (1ull << 24)
  kEDRACL2MbitMode        = (1ull << 25),
  kEDRACL3MbitMode        = (1ull << 26),
  kEnhancedInquiryScan    = (1ull << 27),
  kInterlacedInquiryScan  = (1ull << 28),
  kInterlacedPageScan     = (1ull << 29),
  kRSSIwithInquiryResults = (1ull << 30),
  kEV3Packets             = (1ull << 31),

  // Octet 4
  kEV4Packets              = (1ull << 32),
  kEV5Packets              = (1ull << 33),
  // Reserved
  kAFHCapablePeripheral    = (1ull << 35),
  kAFHClassPeripheral      = (1ull << 36),
  kBREDRNotSupported       = (1ull << 37),
  kLESupported             = (1ull << 38),
  k3SlotEDRACLPackets      = (1ull << 39),

  // Octet 5
  // TODO(armansito): Add definitions

  // Octet 6
  kExtendedInquiryResponse        = (1ull << 48),
  kSimultaneousLEAndBREDR         = (1ull << 49),
  // Reserved
  kSecureSimplePairing            = (1ull << 51),
  kEncapsulatedPDU                = (1ull << 52),
  kErroneousDataReporting         = (1ull << 53),
  kNonflushablePacketBoundaryFlag = (1ull << 54),
  // Reerved

  // Octet 7
  kLinkSupervisionTimeoutChangedEvent = (1ull << 56),
  kInquiryTxPowerLevel                = (1ull << 57),
  kEnhancedPowerControl               = (1ull << 58),
  // Reserved
  // Reserved
  // Reserved
  // Reserved
  kExtendedFeatures                   = (1ull << 63),

  // Extended features (Page 1)
  kSecureSimplePairingHostSupport = (1ull << 0),
  kLESupportedHost                = (1ull << 1),
  kSimultaneousLEAndBREDRHost     = (1ull << 2),
  kSecureConnectionsHost          = (1ull << 3),

  // Extended features (Page 2)
  // TODO(armansito): Add definitions
};

// Bitmask values for the 64-octet Supported Commands bit-field. See Core Spec
// v5.0, Volume 2, Part E, Section 6.27 "Supported Commands".
enum class SupportedCommand : uint8_t {
  // Octet 0
  kInquiry                 = (1 << 0),
  kInquiryCancel           = (1 << 1),
  kPeriodicInquiryMode     = (1 << 2),
  kExitPeriodicInquiryMode = (1 << 3),
  kCreateConnection        = (1 << 4),
  kDisconnect              = (1 << 5),
  kAddSCOConnection        = (1 << 6),  // deprecated
  kCreateConnectionCancel  = (1 << 7),

  // Octet 1
  kAcceptConnectionRequest     = (1 << 0),
  kRejectConnectionRequest     = (1 << 1),
  kLinkKeyRequestReply         = (1 << 2),
  kLinkKeyRequestNegativeReply = (1 << 3),
  kPINCodeRequestReply         = (1 << 4),
  kPINCodeRequestNegativeReply = (1 << 5),
  kChangeConnectionPacketType  = (1 << 6),
  kAuthenticationRequested     = (1 << 7),

  // Octet 2
  kSetConnectionEncryption      = (1 << 0),
  kChangeConnectionLinkKey      = (1 << 1),
  kLinkKeySelection             = (1 << 2),
  kRemoteNameRequest            = (1 << 3),
  kRemoteNameRequestCancel      = (1 << 4),
  kReadRemoteSupportedFeatures  = (1 << 5),
  kReadRemoteExtendedFeatures   = (1 << 6),
  kReadRemoteVersionInformation = (1 << 7),

  // Octet 3
  kReadClockOffset = (1 << 0),
  kReadLMPHandle   = (1 << 1),
  // kReserved     = (1 << 2),
  // kReserved     = (1 << 3),
  // kReserved     = (1 << 4),
  // kReserved     = (1 << 5),
  // kReserved     = (1 << 6),
  // kReserved     = (1 << 7),

  // Octet 4
  // kReserved   = (1 << 0),
  kHoldMode      = (1 << 1),
  kSniffMode     = (1 << 2),
  kExitSniffMode = (1 << 3),
  kParkState     = (1 << 4),  // reserved in 5.0
  kExitParkState = (1 << 5),  // reserved in 5.0
  kQOSSetup      = (1 << 6),
  kRoleDiscovery = (1 << 7),

  // Octet 5
  kSwitchRole                     = (1 << 0),
  kReadLinkPolicySettings         = (1 << 1),
  kWriteLinkPolicySettings        = (1 << 2),
  kReadDefaultLinkPolicySettings  = (1 << 3),
  kWriteDefaultLinkPolicySettings = (1 << 4),
  kFlowSpecification              = (1 << 5),
  kSetEventMask                   = (1 << 6),
  kReset                          = (1 << 7),

  // Octet 6
  kSetEventFilter       = (1 << 0),
  kFlush                = (1 << 1),
  kReadPINType          = (1 << 2),
  kWritePINType         = (1 << 3),
  kCreateNewUnitKey     = (1 << 4),  // reserved in 5.0
  kReadStoredLinkKey    = (1 << 5),
  kWriteStoredLinkKey   = (1 << 6),
  kDeletedStoredLinkKey = (1 << 7),

  // Octet 7
  kWriteLocalName                = (1 << 0),
  kReadLocalName                 = (1 << 1),
  kReadConnectionAttemptTimeout  = (1 << 2),
  kWriteConnectionAttemptTimeout = (1 << 3),
  kReadPageTimeout               = (1 << 4),
  kWritePageTimeout              = (1 << 5),
  kReadScanEnable                = (1 << 6),
  kWriteScanEnable               = (1 << 7),

  // Octet 8
  kReadPageScanActivity      = (1 << 0),
  kWritePageScanActivity     = (1 << 1),
  kReadInquiryScanActivity   = (1 << 2),
  kWriteInquiryScanActivity  = (1 << 3),
  kReadAuthenticationEnable  = (1 << 4),
  kWriteAuthenticationEnable = (1 << 5),
  kReadEncryptionMode        = (1 << 6),  // deprecated
  kWriteEncryptionMode       = (1 << 7),  // deprecated

  // Octet 9
  kReadClassOfDevice                = (1 << 0),
  kWriteClassOfDevice               = (1 << 1),
  kReadVoiceSetting                 = (1 << 2),
  kWriteVoiceSetting                = (1 << 3),
  kReadAutomaticFlushTimeout        = (1 << 4),
  kWriteAutomaticFlushTimeout       = (1 << 5),
  kReadNumBroadcastRetransmissions  = (1 << 6),
  kWriteNumBroadcastRetransmissions = (1 << 7),

  // Octet 10
  kReadHoldModeActivity              = (1 << 0),
  kWriteHoldModeActivity             = (1 << 1),
  kReadTransmitPowerLevel            = (1 << 2),
  kReadSynchronousFlowControlEnable  = (1 << 3),
  kWriteSynchronousFlowControlEnable = (1 << 4),
  kSetControllerToHostFlowControl    = (1 << 5),
  kHostBufferSize                    = (1 << 6),
  kHostNumberOfCompletedPackets      = (1 << 7),

  // Octet 11
  kReadLinkSupervisionTimeout        = (1 << 0),
  kWriteLinkSupervisionTimeout       = (1 << 1),
  kReadNumberOfSupportedIAC          = (1 << 2),
  kReadCurrentIACLAP                 = (1 << 3),
  kWriteCurrentIACLAP                = (1 << 4),
  kReadPageScanModePeriod            = (1 << 5),  // deprecated
  kWritePageScanModePeriod           = (1 << 6),  // deprecated
  kReadPageScanMode                  = (1 << 7),  // deprecated

  // Octet 12
  kWritePageScanMode               = (1 << 0),  // deprecated
  kSetAFHHostChannelClassification = (1 << 1),
  // kReserved                     = (1 << 2),
  // kReserved                     = (1 << 3),
  kReadInquiryScanType             = (1 << 4),
  kWriteInquiryScanType            = (1 << 5),
  kReadInquiryMode                 = (1 << 6),
  kWriteInquiryMode                = (1 << 7),

  // Octet 13
  kReadPageScanType              = (1 << 0),
  kWritePageScanType             = (1 << 1),
  kReadAFHChannelAssessmentMode  = (1 << 2),
  kWriteAFHChannelAssessmentMode = (1 << 3),
  // kReserved                   = (1 << 4),
  // kReserved                   = (1 << 5),
  // kReserved                   = (1 << 6),
  // kReserved                   = (1 << 7),

  // Octet 14
  // kReserved                 = (1 << 0),
  // kReserved                 = (1 << 1),
  // kReserved                 = (1 << 2),
  kReadLocalVersionInformation = (1 << 3),
  // kReserved                 = (1 << 4),
  kReadLocalSupportedFeatures  = (1 << 5),
  kReadLocalExtendedFeatures   = (1 << 6),
  kReadBufferSize              = (1 << 7),

  // Octet 15
  kReadCountryCode           = (1 << 0),  // deprecated
  kReadBDADDR                = (1 << 1),
  kReadFailedContactCounter  = (1 << 2),
  kResetFailedContactCOunter = (1 << 3),
  kReadLinkQuality           = (1 << 4),
  kReadRSSI                  = (1 << 5),
  kReadAFHChannelMap         = (1 << 6),
  kReadClock                 = (1 << 7),

  // Octet 16
  kReadLoopbackMode                   = (1 << 0),
  kWriteLoopbackMode                  = (1 << 1),
  kEnableDeviceUnderTestMode          = (1 << 2),
  kSetupSynchronousConnectionRequest  = (1 << 3),
  kAcceptSynchronousConnectionRequest = (1 << 4),
  kRejectSynchronousConnectionRequest = (1 << 5),
  // kReserved                        = (1 << 6),
  // kReserved                        = (1 << 7),

  // Octet 17
  kReadExtendedInquiryResponse  = (1 << 0),
  kWriteExtendedInquiryResponse = (1 << 1),
  kRefreshEncryptionKey         = (1 << 2),
  // kReserved                  = (1 << 3),
  kSniffSubrating               = (1 << 4),
  kReadSimplePairingMode        = (1 << 5),
  kWriteSimplePairingMode       = (1 << 6),
  kReadLocalOOBData             = (1 << 7),

  // Octet 18
  kReadInquiryResponseTransmitPowerLevel = (1 << 0),
  kWriteInquiryTransmitPowerLevel        = (1 << 1),
  kReadDefaultErroneousDataReporting     = (1 << 2),
  kWriteDefaultErroneousDataReporting    = (1 << 3),
  // kReserved                           = (1 << 4),
  // kReserved                           = (1 << 5),
  // kReserved                           = (1 << 6),
  kIOCapabilityRequestReply              = (1 << 7),

  // Octet 19
  kUserConfirmationRequestReply         = (1 << 0),
  kUserConfirmationRequestNegativeReply = (1 << 1),
  kUserPasskeyRequestReply              = (1 << 2),
  kUserPasskeyRequestNegativeReply      = (1 << 3),
  kRemoteOOBDataRequestReply            = (1 << 4),
  kWriteSimplePairingDebugMode          = (1 << 5),
  kEnhancedFlush                        = (1 << 6),
  kRemoteOOBDataRequestNegativeReply    = (1 << 7),

  // Octet 20
  // kReserved                      = (1 << 0),
  // kReserved                      = (1 << 1),
  kSendKeypressNotification         = (1 << 2),
  kIOCapabilityRequestNegativeReply = (1 << 3),
  kReadEncryptionKeySize            = (1 << 4),
  // kReserved                      = (1 << 5),
  // kReserved                      = (1 << 6),
  // kReserved                      = (1 << 7),

  // Octet 21
  kCreatePhysicalLink     = (1 << 0),
  kAcceptPhysicalLink     = (1 << 1),
  kDisconnectPhysicalLink = (1 << 2),
  kCreateLogicalLink      = (1 << 3),
  kAcceptLogicalLink      = (1 << 4),
  kDisconnectLogicalLink  = (1 << 5),
  kLogicalLinkCancel      = (1 << 6),
  kFlowSpecModify         = (1 << 7),

  // Octet 22
  kReadLogicalLinkAcceptTimeout  = (1 << 0),
  kWriteLogicalLinkAcceptTimeout = (1 << 1),
  kSetEventMaskPage2             = (1 << 2),
  kReadLocationData              = (1 << 3),
  kWriteLocationData             = (1 << 4),
  kReadLocalAMPInfo              = (1 << 5),
  kReadLocalAMPASSOC             = (1 << 6),
  kWriteRemoteAMPASSOC           = (1 << 7),

  // Octet 23
  kReadFlowControlMode      = (1 << 0),
  kWriteFlowControlMode     = (1 << 1),
  kReadDataBlockSize        = (1 << 2),
  // kReserved              = (1 << 3),
  // kReserved              = (1 << 4),
  kEnableAMPReceiverReports = (1 << 5),
  kAMPTestEnd               = (1 << 6),
  kAMPTest                  = (1 << 7),

  // Octet 24
  kReadEnhancedTransmitPowerLevel = (1 << 0),
  // kReserved                    = (1 << 1),
  kReadBestEffortFlushTimeout     = (1 << 2),
  kWriteBestEffortFlushTimeout    = (1 << 3),
  kShortRangeMode                 = (1 << 4),
  kReadLEHostSupported            = (1 << 5),
  kWriteLEHostSupport             = (1 << 6),
  // kReserved                    = (1 << 7),

  // Octet 25
  kLESetEventMask                  = (1 << 0),
  kLEReadBufferSizeV1              = (1 << 1),
  kLEReadLocalSupportedFeatures    = (1 << 2),
  // kReserved                     = (1 << 3),
  kLESetRandomAddress              = (1 << 4),
  kLESetAdvertisingParameters      = (1 << 5),
  kLEReadAdvertisingChannelTXPower = (1 << 6),
  kLESetAdvertisingData            = (1 << 7),

  // Octet 26
  kLESetScanResponseData      = (1 << 0),
  kLESetAdvertisingEnable     = (1 << 1),
  kLESetScanParameters        = (1 << 2),
  kLESetScanEnable            = (1 << 3),
  kLECreateConnection         = (1 << 4),
  kLECreateConnectionCancel   = (1 << 5),
  kLEReadFilterAcceptListSize = (1 << 6),
  kLEClearFilterAcceptList    = (1 << 7),

  // Octet 27
  kLEAddDeviceToFilterAcceptList      = (1 << 0),
  kLERemoveDeviceFromFilterAcceptList = (1 << 1),
  kLEConnectionUpdate                 = (1 << 2),
  kLESetHostChannelClassification     = (1 << 3),
  kLEReadChannelMap                   = (1 << 4),
  kLEReadRemoteFeatures               = (1 << 5),
  kLEEncrypt                          = (1 << 6),
  kLERand                             = (1 << 7),

  // Octet 28
  kLEStartEncryption                 = (1 << 0),
  kLELongTermKeyRequestReply         = (1 << 1),
  kLELongTermKeyRequestNegativeReply = (1 << 2),
  kLEReadSupportedStates             = (1 << 3),
  kLEReceiverTestV1                  = (1 << 4),
  kLETransmitterTestV1               = (1 << 5),
  kLETestEnd                         = (1 << 6),
  // kReserved                       = (1 << 7),

  // Octet 29
  // kReserved                         = (1 << 0),
  // kReserved                         = (1 << 1),
  // kReserved                         = (1 << 2),
  kEnhancedSetupSynchronousConnection  = (1 << 3),
  kEnhancedAcceptSynchronousConnection = (1 << 4),
  kReadLocalSupportedCodecs            = (1 << 5),
  kSetMWSChannelParameters             = (1 << 6),
  kSetExternalFrameConfiguration       = (1 << 7),

  // Octet 30
  kSetMWSSignaling                   = (1 << 0),
  kSetMWSTransportLayer              = (1 << 1),
  kSetMWSScanFrequencyTable          = (1 << 2),
  kGetMWSTransportLayerConfiguration = (1 << 3),
  kSetMWSPATTERNConfiguration        = (1 << 4),
  kSetTriggeredClockCapture          = (1 << 5),
  kTruncatedPage                     = (1 << 6),
  kTruncatedPageCancel               = (1 << 7),

  // Octet 31
  kSetConnectionlessPeripheralBroadcast        = (1 << 0),
  kSetConnectionlessPeripheralBroadcastReceive = (1 << 1),
  kStartSynchronizationTrain                   = (1 << 2),
  kReceiveSynchronizationTrain                 = (1 << 3),
  kSetReservedLTADDR                           = (1 << 4),
  kDeleteReservedLTADDR                        = (1 << 5),
  kSetConnectionlessPeripheralBroadcastData    = (1 << 6),
  kReadSynchronizationTrainParameters          = (1 << 7),

  // Octet 32
  kWriteSynchronizationTrainParameters = (1 << 0),
  kRemoteOOBExtendedDataRequestReply   = (1 << 1),
  kReadSecureConnectionsHostSupport    = (1 << 2),
  kWriteSecureConnectionsHostSupport   = (1 << 3),
  kReadAuthenticatedPayloadTimeout     = (1 << 4),
  kWriteAuthenticatedPayloadTimeout    = (1 << 5),
  kReadLocalOOBExtendedData            = (1 << 6),
  kWriteSecureConnectionsTestMode      = (1 << 7),

  // Octet 33
  kReadExtendedPageTimeout                         = (1 << 0),
  kWriteExtendedPageTimeout                        = (1 << 1),
  kReadExtendedInquiryLength                       = (1 << 2),
  kWriteExtendedInquiryLength                      = (1 << 3),
  kLERemoteConnectionParameterRequestReply         = (1 << 4),
  kLERemoteConnectionParameterRequestNegativeReply = (1 << 5),
  kLESetDataLength                                 = (1 << 6),
  kLEReadSuggestedDefaultDataLength                = (1 << 7),

  // Octet 34
  kLEWriteSuggestedDefaultDataLength = (1 << 0),
  kLEReadLocalP256PublicKey          = (1 << 1),
  kLEGenerateDHKeyV1                 = (1 << 2),
  kLEAddDeviceToResolvingList        = (1 << 3),
  kLERemoveDeviceFromResolvingList   = (1 << 4),
  kLEClearResolvingList              = (1 << 5),
  kLEReadResolvingListSize           = (1 << 6),
  kLEReadPeerResolvableAddress       = (1 << 7),

  // Octet 35
  kLEReadLocalResolvableAddress         = (1 << 0),
  kLESetAddressResolutionEnable         = (1 << 1),
  kLESetResolvablePrivateAddressTimeout = (1 << 2),
  kLEReadMaximumDataLength              = (1 << 3),
  kLEReadPHY                            = (1 << 4),
  kLESetDefaultPHY                      = (1 << 5),
  kLESetPHY                             = (1 << 6),
  kLEEnhancedReceiverTestV2             = (1 << 7),

  // Octet 36
  kLEEnhancedTransmitterTestV2              = (1 << 0),
  kLESetAdvertisingSetRandomAddress       = (1 << 1),
  kLESetExtendedAdvertisingParameters     = (1 << 2),
  kLESetExtendedAdvertisingData           = (1 << 3),
  kLESetExtendedScanResponseData          = (1 << 4),
  kLESetExtendedAdvertisingEnable         = (1 << 5),
  kLEReadMaximumAdvertisingDataLength     = (1 << 6),
  kLEReadNumberOfSupportedAdvertisingSets = (1 << 7),

  // Octet 37
  kLERemoveAdvertisingSet             = (1 << 0),
  kLEClearAdvertisingSets             = (1 << 1),
  kLESetPeriodicAdvertisingParameters = (1 << 2),
  kLESetPeriodicAdvertisingData       = (1 << 3),
  kLESetPeriodicAdvertisingEnable     = (1 << 4),
  kLESetExtendedScanParameters        = (1 << 5),
  kLESetExtendedScanEnable            = (1 << 6),
  kLEExtendedCreateConnection         = (1 << 7),

  // Octet 38
  kLEPeriodicAdvertisingCreateSync          = (1 << 0),
  kLEPeriodicAdvertisingCreateSyncCancel    = (1 << 1),
  kLEPeriodicAdvertisingTerminateSync       = (1 << 2),
  kLEAddDeviceToPeriodicAdvertiserList      = (1 << 3),
  kLERemoveDeviceFromPeriodicAdvertiserList = (1 << 4),
  kLEClearPeriodicAdvertiserList            = (1 << 5),
  kLEReadPeriodicAdvertiserListSize         = (1 << 6),
  kLEReadTransmitPower                      = (1 << 7),

  // Octet 39
  kLEReadRFPathCompensation                 = (1 << 0),
  kLEWriteRFPathCompensation                = (1 << 1),
  kLESetPrivacyMode                         = (1 << 2),
  kLEReceiverTestV3                         = (1 << 3),
  kLETransmitterTestV3                      = (1 << 4),
  kLESetConnectionlessCTETransmitParameters = (1 << 5),
  kLESetConnectionlessCTETransmitEnable     = (1 << 6),
  kLESetConnectionlessIQSamplingEnable      = (1 << 7),

  // Octet 40
  kLESetConnectionCTEReceiveParameters   = (1 << 0),
  kLESetConnectionCTETransmitParameters  = (1 << 1),
  kLEConnectionCTERequestEnable          = (1 << 2),
  kLEConnectionCTEResponseEnable         = (1 << 3),
  kLEReadAntennaInformation              = (1 << 4),
  kLESetPeriodicAdvertisingReceiveEnable = (1 << 5),
  kLEPeriodicAdvertisingSyncTransfer     = (1 << 6),
  kLEPeriodicAdvertisingSetInfoTransfer  = (1 << 7),

  // Octet 41
  kLESetPeriodicAdvertisingSyncTransferParameters        = (1 << 0),
  kLESetDefaultPeriodicAdvertisingSyncTransferParameters = (1 << 1),
  kLEGenerateDHKeyV3                                     = (1 << 2),
  kReadLocalSimplePairingOptions                         = (1 << 3),
  kLEModifySleepClockAccuracy                            = (1 << 4),
  kLEReadBufferSizeV2                                    = (1 << 5),
  kLEReadISOTXSync                                       = (1 << 6),
  kLESetCIGParameters                                    = (1 << 7),

  // Octet 42
  kLESetCIGParametersTest = (1 << 0),
  kLECreateCIS            = (1 << 1),
  kLERemoveCIG            = (1 << 2),
  kLEAcceptCISRequest     = (1 << 3),
  kLERejectCISRequest     = (1 << 4),
  kLECreateBIG            = (1 << 5),
  kLECreateBIGTest        = (1 << 6),
  kLETerminateBIG         = (1 << 7),

  // Octet 43
  kLEBIGCreateSync       = (1 << 0),
  kLEBIGTerminateSync    = (1 << 1),
  kLERequestPeerSCA      = (1 << 2),
  kLESetupISODataPath    = (1 << 3),
  kLERemoveISODataPath   = (1 << 4),
  kLEISOTransmitTest     = (1 << 5),
  kLEISOReceiveTest      = (1 << 6),
  kLEISOReadTestCounters = (1 << 7),

  // Octet 44
  kLEISOTestEnd                      = (1 << 0),
  kLESetHostFeature                  = (1 << 1),
  kLEReadISOLinkQuality              = (1 << 2),
  kLEEnhancedReadTransmitPowerLevel  = (1 << 3),
  kLEReadRemoteTransmitPowerLevel    = (1 << 4),
  kLESetPathLossReportingParameters  = (1 << 5),
  kLESetPathLossReportingEnable      = (1 << 6),
  kLESetTransmitPowerReportingEnable = (1 << 7),

  // Octet 45
  kLETransmitterTestV4                 = (1 << 0),
  kSetEcosystemBaseInterval            = (1 << 1),
  kReadLocalSupportedCodecsV2          = (1 << 2),
  kReadLocalSupportedCodecCapabilities = (1 << 3),
  kReadLocalSupportedControllerDelay   = (1 << 4),
  kConfigureDataPath                   = (1 << 5),
  // kReserved                         = (1 << 6),
  // kReserved                         = (1 << 7),
};

// Bitmask of 8-octet LE supported features field. See Core Spec
// v5.0, Volume 6, Part B, Section 4.6 "Feature Support".
struct LESupportedFeatures {
  uint64_t le_features;
} __PACKED;

// Bitmask values for the 8-octet LE Supported Features bit-field. See Core Spec
// v5.0, Volume 6, Part B, Section 4.6 "Feature Support".
enum class LESupportedFeature : uint64_t {
  kLEEncryption                         = (1 << 0),
  kConnectionParametersRequestProcedure = (1 << 1),
  kExtendedRejectIndication             = (1 << 2),
  kPeripheralInitiatedFeaturesExchange       = (1 << 3),
  kLEPing                               = (1 << 4),
  kLEDataPacketLengthExtension          = (1 << 5),
  kLLPrivacy                            = (1 << 6),
  kExtendedScannerFilterPolicies        = (1 << 7),

  // Added in 5.0
  kLE2MPHY                                  = (1 << 8),
  kStableModulationIndexTransmitter         = (1 << 9),
  kStableModulationIndexReceiver            = (1 << 10),
  kLECodedPHY                               = (1 << 11),
  kLEExtendedAdvertising                    = (1 << 12),
  kLEPeriodicAdvertising                    = (1 << 13),
  kChannelSelectionAlgorithm2               = (1 << 14),
  kLEPowerClass1                            = (1 << 15),
  kMinimumNumberOfUsedChannelsProcedure     = (1 << 16),

  // Added in 5.1
  kConnectionCTERequest                     = (1 << 17),
  kConnectionCTEResponse                    = (1 << 18),
  kConnectionlessCTETransmitter             = (1 << 19),
  kConnectionlessCTEReceiver                = (1 << 20),
  kAntennaSwitchingDuringCTETransmission    = (1 << 21),
  kAntennaSwitchingDuringCTEReception       = (1 << 22),
  kReceivingConstantToneExtensions          = (1 << 23),
  kPeriodicAdvertisingSyncTransferSender    = (1 << 24),
  kPeriodicAdvertisingSyncTransferRecipient = (1 << 25),
  kSleepClockAccuracyUpdates                = (1 << 26),
  kRemotePublicKeyValidation                = (1 << 27),

  // Added in 5.2
  kConnectedIsochronousStreamCentral    = (1 << 28),
  kConnectedIsochronousStreamPeripheral = (1 << 29),
  kIsochronousBoradcaster               = (1 << 30),
  kSynchronizedReceiver                 = (1ul << 31),
  kIsochronousChannels                  = (1ul << 32),
  kLEPowerControlRequest                = (1ul << 33),
  kLEPowerChangeIndication              = (1ul << 34),
  kLEPathLossMonitoring                 = (1ul << 35),

  // The rest is reserved for future use.
};

// Bitmask values for the 8-octet HCI_Set_Event_Mask command parameter.
enum class EventMask : uint64_t {
  kInquiryCompleteEvent                         = (1 << 0),
  kInquiryResultEvent                           = (1 << 1),
  kConnectionCompleteEvent                      = (1 << 2),
  kConnectionRequestEvent                       = (1 << 3),
  kDisconnectionCompleteEvent                   = (1 << 4),
  kAuthenticationCompleteEvent                  = (1 << 5),
  kRemoteNameRequestCompleteEvent               = (1 << 6),
  kEncryptionChangeEvent                        = (1 << 7),
  kChangeConnectionLinkKeyCompleteEvent         = (1 << 8),
  kLinkKeyTypeChangedEvent                      = (1 << 9),
  kReadRemoteSupportedFeaturesCompleteEvent     = (1 << 10),
  kReadRemoteVersionInformationCompleteEvent    = (1 << 11),
  kQoSSetupCompleteEvent                        = (1 << 12),
  // Reserved For Future Use: (1 << 13)
  // Reserved For Future Use: (1 << 14)
  kHardwareErrorEvent                           = (1 << 15),
  kFlushOccurredEvent                           = (1 << 16),
  kRoleChangeEvent                              = (1 << 17),
  // Reserved For Future Use: (1 << 18)
  kModeChangeEvent                              = (1 << 19),
  kReturnLinkKeysEvent                          = (1 << 20),
  kPINCodeRequestEvent                          = (1 << 21),
  kLinkKeyRequestEvent                          = (1 << 22),
  kLinkKeyNotificationEvent                     = (1 << 23),
  kLoopbackCommandEvent                         = (1 << 24),
  kDataBufferOverflowEvent                      = (1 << 25),
  kMaxSlotsChangeEvent                          = (1 << 26),
  kReadClockOffsetCompleteEvent                 = (1 << 27),
  kConnectionPacketTypeChangedEvent             = (1 << 28),
  kQoSViolationEvent                            = (1 << 29),
  kPageScanModeChangeEvent                      = (1 << 30),  // deprecated
  kPageScanRepetitionModeChangeEvent            = (1ull << 31),
  kFlowSpecificationCompleteEvent               = (1ull << 32),
  kInquiryResultWithRSSIEvent                   = (1ull << 33),
  kReadRemoteExtendedFeaturesCompleteEvent      = (1ull << 34),
  // Reserved For Future Use: (1ull << 35)
  // Reserved For Future Use: (1ull << 36)
  // Reserved For Future Use: (1ull << 37)
  // Reserved For Future Use: (1ull << 38)
  // Reserved For Future Use: (1ull << 39)
  // Reserved For Future Use: (1ull << 40)
  // Reserved For Future Use: (1ull << 41)
  // Reserved For Future Use: (1ull << 42)
  kSynchronousConnectionCompleteEvent           = (1ull << 43),
  kSynchronousConnectionChangedEvent            = (1ull << 44),
  kSniffSubratingEvent                          = (1ull << 45),
  kExtendedInquiryResultEvent                   = (1ull << 46),
  kEncryptionKeyRefreshCompleteEvent            = (1ull << 47),
  kIOCapabilityRequestEvent                     = (1ull << 48),
  kIOCapabilityResponseEvent                    = (1ull << 49),
  kUserConfirmationRequestEvent                 = (1ull << 50),
  kUserPasskeyRequestEvent                      = (1ull << 51),
  kRemoteOOBDataRequestEvent                    = (1ull << 52),
  kSimplePairingCompleteEvent                   = (1ull << 53),
  // Reserved For Future Use: (1ull << 54)
  kLinkSupervisionTimeoutChangedEvent           = (1ull << 55),
  kEnhancedFlushCompleteEvent                   = (1ull << 56),
  // Reserved For Future Use: (1ull << 57)
  kUserPasskeyNotificationEvent                 = (1ull << 58),
  kKeypressNotificationEvent                    = (1ull << 59),
  kRemoteHostSupportedFeaturesNotificationEvent = (1ull << 60),
  kLEMetaEvent                                  = (1ull << 61),
  // Reserved For Future Use: (1ull << 62)
  // Reserved For Future Use: (1ull << 63)
};

// Bitmask values for the 8-octet HCI_Set_Event_Mask_Page_2 command parameter.
enum class EventMaskPage2 : uint64_t {
  kPhysicalLinkCompleteEvent                              = (1 << 0),
  kChannelSelectedEvent                                   = (1 << 1),
  kDisconnectionPhysicalLinkCompleteEvent                 = (1 << 2),
  kPhysicalLinkLossEarlyWarningEvent                      = (1 << 3),
  kPhysicalLinkRecoveryEvent                              = (1 << 4),
  kLogicalLinkCompleteEvent                               = (1 << 5),
  kDisconnectionLogicalLinkCompleteEvent                  = (1 << 6),
  kFlowSpecModifyCompleteEvent                            = (1 << 7),
  kNumberOfCompletedDataBlocksEvent                       = (1 << 8),
  kAMPStartTestEvent                                      = (1 << 9),
  kAMPTestEndEvent                                        = (1 << 10),
  kAMPReceiverReportEvent                                 = (1 << 11),
  kShortRangeModeChangeCompleteEvent                      = (1 << 12),
  kAMPStatusChangeEvent                                   = (1 << 13),
  kTriggeredClockCaptureEvent                             = (1 << 14),
  kSynchronizationTrainCompleteEvent                      = (1 << 15),
  kSynchronizationTrainReceivedEvent                      = (1 << 16),
  kConnectionlessPeripheralBroadcastReceiveEvent          = (1 << 17),
  kConnectionlessPeripheralBroadcastTimeoutEvent          = (1 << 18),
  kTruncatedPageCompleteEvent                             = (1 << 19),
  kPeripheralPageResponseTimeoutEvent                     = (1 << 20),
  kConnectionlessPeripheralBroadcastChannelMapChangeEvent = (1 << 21),
  kInquiryResponseNotificationEvent                       = (1 << 22),
  kAuthenticatedPayloadTimeoutExpiredEvent                = (1 << 23),
  kSAMStatusChangeEvent                                   = (1 << 24),
};

// Bitmask values for the 8-octet HCI_LE_Set_Event_Mask command parameter.
enum class LEEventMask : uint64_t {
  kLEConnectionComplete                 = (1 << 0),
  kLEAdvertisingReport                  = (1 << 1),
  kLEConnectionUpdateComplete           = (1 << 2),
  kLEReadRemoteFeaturesComplete         = (1 << 3),
  kLELongTermKeyRequest                 = (1 << 4),
  kLERemoteConnectionParameterRequest   = (1 << 5),
  kLEDataLengthChange                   = (1 << 6),
  kLEReadLocalP256PublicKeyComplete     = (1 << 7),
  kLEGenerateDHKeyComplete              = (1 << 8),
  kLEEnhancedConnectionComplete         = (1 << 9),
  kLEDirectedAdvertisingReport          = (1 << 10),
  kLEPHYUpdateComplete                  = (1 << 11),
  kLEExtendedAdvertisingReport          = (1 << 12),
  kLEPeriodicAdvertisingSyncEstablished = (1 << 13),
  kLEPeriodicAdvertisingReport          = (1 << 14),
  kLEPeriodicAdvertisingSyncLost        = (1 << 15),
  kLEExtendedScanTimeout                = (1 << 16),
  kLEExtendedAdvertisingSetTerminated   = (1 << 17),
  kLEScanRequestReceived                = (1 << 18),
  kLEChannelSelectionAlgorithm          = (1 << 19),
};

// Values that can be passed to the Type parameter in a
// HCI_Read_Transmit_Power_Level command.
enum class ReadTransmitPowerType : uint8_t {
  // Read Current Transmit Power Level.
  kCurrent = 0x00,

  // Read Maximum Transmit Power Level.
  kMax = 0x01,
};

// Possible values for the Encryption_Enabled parameter in a HCI_Encryption_Change event
// (see Vol 2, Part E, 7.7.8).
enum class EncryptionStatus : uint8_t {
  // Link Level Encryption is OFF.
  kOff = 0x00,

  // Link Level Encryption is ON with E0 for BR/EDR and AES-CCM for LE.
  kOn = 0x01,

  // Link Level Encryption is ON with AES-CCM for BR/EDR.
  kBredrSecureConnections = 0x02,
};

// HCI command timeout interval (milliseconds)
// TODO(fxbug.dev/1196,fxbug.dev/1197) This was increased to handle flaking integration tests.
// We may want to reduce this to something lower again once we have a better
// resolution to this issue.
constexpr zx::duration kCommandTimeout = zx::sec(10);

// The minimum and maximum range values for the LE advertising interval
// parameters.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5)
constexpr uint16_t kLEAdvertisingIntervalMin = 0x0020;
constexpr uint16_t kLEAdvertisingIntervalMax = 0x4000;

// The minimum and maximum range values for the LE periodic advertising interval
// parameters.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.61)
constexpr uint16_t kLEPeriodicAdvertisingIntervalMin = 0x0006;
constexpr uint16_t kLEPeriodicAdvertisingIntervalMax = 0xFFFF;

// The minimum and maximum range values for the LE extended advertising interval
// parameters.
constexpr uint32_t kLEExtendedAdvertisingIntervalMin = 0x000020;
constexpr uint32_t kLEExtendedAdvertisingIntervalMax = 0xFFFFFF;

// The default LE advertising interval parameter value, corresponding to 1.28
// seconds (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5).
constexpr uint16_t kLEAdvertisingIntervalDefault = 0x0800;

// The minimum and maximum range values for the LE scan interval parameters.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.10)
constexpr uint16_t kLEScanIntervalMin = 0x0004;
constexpr uint16_t kLEScanIntervalMax = 0x4000;

// The minimum and maximum range values for the LE extended scan interval
// parameters.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.64)
constexpr uint16_t kLEExtendedScanIntervalMin = 0x0004;
constexpr uint16_t kLEExtendedScanIntervalMax = 0xFFFF;

// The default LE scan interval parameter value, corresponding to 10
// milliseconds (see Core Spec v5.0, Vol 2, Part E, Section 7.8.10).
constexpr uint16_t kLEScanIntervalDefault = 0x0010;

// The minimum and maximum range values for the LE connection interval parameters.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.12)
constexpr uint16_t kLEConnectionIntervalMin = 0x0006;
constexpr uint16_t kLEConnectionIntervalMax = 0x0C80;

// The maximum value that can be used for the |conn_latency| parameter in a
// HCI_LE_Create_Connection
// command (see Core Spec v5.0, Vol 2, Part E, Section 7.8.12).
constexpr uint16_t kLEConnectionLatencyMax = 0x01F3;

// The minimum and maximum range values for LE connection supervision timeout
// parameters.
constexpr uint16_t kLEConnectionSupervisionTimeoutMin = 0x000A;
constexpr uint16_t kLEConnectionSupervisionTimeoutMax = 0x0C80;

// The minimum and maximum range values for LE link layer tx PDU used on
// connections.
constexpr uint16_t kLEMaxTxOctetsMin = 0x001B;
constexpr uint16_t kLEMaxTxOctetsMax = 0x00FB;

// The minimum and maximum range values for LE link layer tx maximum packet
// transmission time used on connections.
constexpr uint16_t kLEMaxTxTimeMin = 0x0148;
constexpr uint16_t kLEMaxTxTimeMax = 0x4290;

// Minimum, maximum, default values for the Resolvable Private Address timeout
// parameter.
constexpr uint16_t kLERPATimeoutMin = 0x0001;      // 1 second
constexpr uint16_t kLERPATimeoutMax = 0xA1B8;      // Approx. 11.5 hours
constexpr uint16_t kLERPATimeoutDefault = 0x0384;  // 900 seconds or 15 minutes.

// The maximum length of advertising data that can get passed to the
// HCI_LE_Set_Advertising_Data command.
//
// This constant should be used on pre-5.0 controllers. On controllers that
// support 5.0+ the host should use the
// HCI_LE_Read_Maximum_Advertising_Data_Length command to obtain this
// information.
constexpr size_t kMaxLEAdvertisingDataLength = 0x1F;  // (31)

// The maximum length of advertising data that can get passed to the
// HCI_LE_Set_Extended_Advertising_Data command. The advertised data can be
// larger than this value (based on the value returned for the LE Read Maximum
// Advertising Data Length parameter), however must be fragmented among multiple
// command packets.
constexpr size_t kMaxLEExtendedAdvertisingDataLength = 251;

// Tx Power values, See Core Spec v5.0 Vol 4, Part E, 7.8.6.
constexpr int8_t kTxPowerInvalid = 127;
constexpr int8_t kLEAdvertisingTxPowerMin = -127;
constexpr int8_t kLEAdvertisingTxPowerMax = 20;
constexpr int8_t kLEExtendedAdvertisingTxPowerNoPreference = 0x7F; // Vol 4, Part E, 7.8.53

// Values used in enabling extended advertising. See Core Spec v5.0 Vol 4, Part E, 7.8.56.
constexpr uint8_t kMaxAdvertisingHandle = 0xEF;
constexpr uint8_t kNoMaxExtendedAdvertisingEvents = 0;
constexpr uint8_t kNoAdvertisingDuration = 0;

// LE advertising types (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5).
enum class LEAdvertisingType : uint8_t {
  // ADV_IND: Connectable and scannable undirected advertising (the default
  // value used by the controller).
  kAdvInd = 0x00,

  // ADV_DIRECT_IND (high duty cycle): Connectable high duty cycle directed
  // advertising.
  kAdvDirectIndHighDutyCycle = 0x01,

  // ADV_SCAN_IND: Scannable undirected advertising.
  kAdvScanInd = 0x02,

  // ADV_NONCONN_IND: Non-connectable undirected advertising.
  kAdvNonConnInd = 0x03,

  // ADV_DIRECT_IND (low duty cycle): Connectable low duty cycle advertising.
  kAdvDirectIndLowDutyCycle = 0x04,

  // The rest is reserved for future use
};

// LE Advertising event types that can be reported in a LE Advertising Report
// event.
enum class LEAdvertisingEventType : uint8_t {
  // Connectable and scannable undirected advertising (ADV_IND)
  kAdvInd = 0x00,

  // Connectable directed advertising (ADV_DIRECT_IND)
  kAdvDirectInd = 0x01,

  // Scannable undirected advertising (ADV_SCAN_IND)
  kAdvScanInd = 0x02,

  // Non connectable undirected advertising (ADV_NONCONN_IND)
  kAdvNonConnInd = 0x03,

  // Scan Response (SCAN_RSP)
  kScanRsp = 0x04,

  // The rest is reserved for future use
};

// Possible values that can be reported for the |address_type| parameter in a LE
// Advertising Report event.
enum class LEAddressType : uint8_t {
  // Public Device Address
  kPublic = 0x00,

  // Random Device Address
  kRandom = 0x01,

  // Public Identity Address (Corresponds to Resolved Private Address)
  kPublicIdentity = 0x02,

  // Random (static) Identity Address (Corresponds to Resolved Private Address)
  kRandomIdentity = 0x03,

  // This is a special value used in LE Extended Advertising Report events to
  // indicate a random address that the controller was unable to resolve.
  kRandomUnresolved = 0xFE,

  // This is a special value that is only used in LE Directed Advertising Report
  // events.
  // Meaning: No address provided (anonymous advertisement)
  kAnonymous = 0xFF,
};

// Possible values that can be used for the |own_address_type| parameter in a
// HCI_LE_Set_Advertising_Parameters or a HCI_LE_Set_Scan_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Sections 7.8.5 and 7.8.10)
enum class LEOwnAddressType : uint8_t {
  // Public device address (default)
  kPublic = 0x00,

  // Random device address
  kRandom = 0x01,

  // Controller generates Resolvable Private Address based on the local IRK from
  // the resolving list. If the resolving list contains no matching entry, use
  // the public address.
  kPrivateDefaultToPublic = 0x02,

  // Controller generates Resolvable Private Address based on the local IRK from
  // the resolving list. If the resolving list contains no matching entry, use
  // the random address from LE_Set_Random_Address.
  kPrivateDefaultToRandom = 0x03,

  // The rest is reserved for future use
};

// Possible values that can be used for the |peer_address_type| parameter in a
// HCI_LE_Set_Advertising_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5)
enum class LEPeerAddressType : uint8_t {
  // Public Device Address (default) or Public Identity Address
  kPublic = 0x00,

  // Random Device Address or Random (static) Identity Address
  kRandom = 0x01,

  // This is a special value that should only be used with the
  // HCI_LE_Add_Device_To_Filter_Accept_List and HCI_LE_Remove_Device_From_Filter_Accept_List
  // commands for peer devices sending anonymous advertisements.
  kAnonymous = 0xFF,
};

// Possible values that can be used for the |adv_channel_map| bitfield in a
// HCI_LE_Set_Advertising_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5)
constexpr uint8_t kLEAdvertisingChannel37 = 0x01;
constexpr uint8_t kLEAdvertisingChannel38 = 0x02;
constexpr uint8_t kLEAdvertisingChannel39 = 0x04;
constexpr uint8_t kLEAdvertisingChannelAll = 0x07;

// Possible values that can be used for the |adv_filter_policy| parameter in a
// HCI_LE_Set_Advertising_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.5)
enum class LEAdvFilterPolicy : uint8_t {
  // Process scan and connection requests from all devices (i.e., the Filter Accept List is not in
  // use) (default).
  kAllowAll = 0x00,

  // Process connection requests from all devices and only scan requests from
  // devices that are in the filter accept policy list.
  kConnAllScanFilterAcceptList = 0x01,

  // Process scan requests from all devices and only connection requests from devices that are in
  // the Filter Accept List.
  kScanAllConnFilterAcceptList = 0x02,

  // Process scan and connection requests only from devices in the Filter Accept List.
  kFilterAcceptListOnly = 0x03,

  // The rest is reserved for future use.
};

// Possible values that can be used for the Filter_Policy parameter in a
// HCI_LE_Periodic_Advertising_Create_Sync command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.67)
enum class LEPeriodicAdvFilterPolicy : uint8_t {
  // Use the Advertising_SID, Advertising_Address_Type, and Advertising_Address
  // parameters to determine which advertiser to listen to.
  kListNotUsed = 0x00,

  // Use the Periodic Advertiser List to determine which advertiser to listen to.
  kUsePeriodicAdvList = 0x01,
};

// Possible values that can be used for the |scan_type| parameter in a
// LE_Set_Scan_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.10)
enum class LEScanType : uint8_t {
  // Passive Scanning. No scanning PDUs shall be sent (default)
  kPassive = 0x00,

  // Active scanning. Scanning PDUs may be sent.
  kActive = 0x01,

  // The rest is reserved for future use.
};

// Possible values that can be used for the |filter_policy| parameter
// in a HCI_LE_Set_Scan_Parameters command.
// (see Core Spec v5.0, Vol 2, Part E, Section 7.8.10)
enum class LEScanFilterPolicy : uint8_t {
  // Accept all advertising packets except directed advertising packets not
  // addressed to this device (default).
  kNoFilterAcceptList = 0x00,

  // Accept only advertising packets from devices where the advertiser’s address is in the Filter
  // Accept List. Directed advertising packets which are not addressed to this device shall be
  // ignored.
  kUseFilterAcceptList = 0x01,

  // Accept all advertising packets except directed advertising packets where the initiator's
  // identity address does not address this device. Note: Directed advertising packets where the
  // initiator's address is a resolvable private address that cannot be resolved are also accepted.
  kNoFilterAcceptListWithPrivacy = 0x02,

  // Accept all advertising packets except:
  //
  //   - advertising packets where the advertiser's identity address is not in the Filter Accept
  //   List; and
  //
  //   - directed advertising packets where the initiator's identity address does not address this
  //   device
  //
  // Note: Directed advertising packets where the initiator's address is a resolvable private
  // address that cannot be resolved are also accepted.
  kUseFilterAcceptListWithPrivacy = 0x03,
};

// Possible values that can be used for the |filter_duplicates| parameter in a
// HCI_LE_Set_Extended_Scan_Enable command.
enum class LEExtendedDuplicateFilteringOption : uint8_t {
  kDisabled = 0x00,
  kEnabled = 0x01,

  // Duplicate advertisements in a single scan period should not be sent to the
  // Host in advertising report events; this setting shall only be used if the
  // Period parameter is non-zero.
  kEnabledResetForEachScanPeriod = 0x02,
};

// The PHY bitfield values that can be used in HCI_LE_Set_PHY and
// HCI_LE_Set_Default_PHY commands that can be used for the TX_PHYS and RX_PHYS
// parameters.
constexpr uint8_t kLEPHYBit1M = (1 << 0);
constexpr uint8_t kLEPHYBit2M = (1 << 1);
constexpr uint8_t kLEPHYBitCoded = (1 << 2);

// The PHY bitfield values that can be used in HCI_LE_Set_PHY and
// HCI_LE_Set_Default_PHY commands that can be used for the ALL_PHYS parameter.
constexpr uint8_t kLEAllPHYSBitTxNoPreference = (1 << 0);
constexpr uint8_t kLEAllPHYSBitRxNoPreference = (1 << 1);

// Potential values that can be used for the LE PHY parameters in HCI commands
// and events.
enum class LEPHY : uint8_t {
  kLE1M = 0x01,
  kLE2M = 0x02,

  // Only for the HCI_LE_Enhanced_Transmitter_Test command this value implies
  // S=8 data coding. Otherwise this refers to general LE Coded PHY.
  kLECoded = 0x03,

  // This should ony be used with the HCI_LE_Enhanced_Transmitter_Test command.
  kLECodedS2 = 0x04,

  // Some HCI events may use this value to indicate that no packets were sent
  // on a particular PHY, specifically the LE Extended Advertising Report event.
  kNone = 0x00,
};

// Potential values that can be used for the PHY_options parameter in a
// HCI_LE_Set_PHY command.
enum class LEPHYOptions : uint16_t {
  kNoPreferredEncoding = 0x00,
  kPreferS2Coding = 0x01,
  kPreferS8Coding = 0x02,
};

// Potential values that can be passed for the Modulation_Index parameter in a
// HCI_LE_Enhanced_Receiver_Test command.
enum class LETestModulationIndex : uint8_t {
  kAssumeStandard = 0x00,
  kAssumeStable = 0x01,
};

// Potential values for the Operation parameter in a
// HCI_LE_Set_Extended_Advertising_Data command.
enum class LESetExtendedAdvDataOp : uint8_t {
  // Intermediate fragment of fragmented extended advertising data.
  kIntermediateFragment = 0x00,

  // First fragment of fragmented extended advertising data.
  kFirstFragment = 0x01,

  // Last fragment of fragmented extended advertising data.
  kLastFragment = 0x02,

  // Complete extended advertising data.
  kComplete = 0x03,

  // Unchanged data (just update the Advertising DID)
  kUnchangedData = 0x04,
};

// Potential values for the Fragment_Preference parameter in a
// HCI_LE_Set_Extended_Advertising_Data command.
enum class LEExtendedAdvFragmentPreference : uint8_t {
  // The Controller may fragment all Host advertising data
  kMayFragment = 0x00,

  // The Controller should not fragment or should minimize fragmentation of Host
  // advertising data
  kShouldNotFragment = 0x01,
};

// The Advertising_Event_Properties bitfield values used in a
// HCI_LE_Set_Extended_Advertising_Parameters command.
constexpr uint16_t kLEAdvEventPropBitConnectable                      = (1 << 0);
constexpr uint16_t kLEAdvEventPropBitScannable                        = (1 << 1);
constexpr uint16_t kLEAdvEventPropBitDirected                         = (1 << 2);
constexpr uint16_t kLEAdvEventPropBitHighDutyCycleDirectedConnectable = (1 << 3);
constexpr uint16_t kLEAdvEventPropBitUseLegacyPDUs                    = (1 << 4);
constexpr uint16_t kLEAdvEventPropBitAnonymousAdvertising             = (1 << 5);
constexpr uint16_t kLEAdvEventPropBitIncludeTxPower                   = (1 << 6);

// The Event_Type bitfield values reported in a LE Extended Advertising Report Event.
constexpr uint16_t kLEExtendedAdvEventTypeConnectable  = (1 << 0);
constexpr uint16_t kLEExtendedAdvEventTypeScannable    = (1 << 1);
constexpr uint16_t kLEExtendedAdvEventTypeDirected     = (1 << 2);
constexpr uint16_t kLEExtendedAdvEventTypeScanResponse = (1 << 3);
constexpr uint16_t kLEExtendedAdvEventTypeLegacy       = (1 << 4);

// LE Advertising data status properties stored in bits 5 and 6 of the
// Event_Type bitfield of a LE Extended Advertising Report event and in a LE
// Periodic Advertising Report event.
enum class LEAdvertisingDataStatus : uint16_t {
  // Data is complete.
  kComplete = 0x00,

  // Data is incomplete, more data to come in future events.
  kIncomplete = 0x01,

  // Data is incomplete and truncated, no more data to come.
  kIncompleteTruncated = 0x02,
};

// The Periodic_Advertising_Properties bitfield used in a
// HCI_LE_Set_Periodic_Advertising_Parameters command.
constexpr uint16_t kLEPeriodicAdvPropBitIncludeTxPower = (1 << 6);

// Potential values for the Privacy_Mode parameter in a HCI_LE_Set_Privacy_Mode
// command.
enum class LEPrivacyMode : uint8_t {
  // Use Network Privacy Mode for this peer device (default).
  kNetwork = 0x00,

  // Use Device Privacy Mode for this peer device.
  kDevice = 0x01,
};

// The maximum length of LE data packets when the LE Data Packet Length Extension
// feature is supported. See v5.0, Vol 6, Part B, 4.5.10, Table 4.3.
constexpr size_t kMaxLEExtendedDataLength = 251;

// Maximum value of the Advertising SID subfield in the ADI field of the PDU
constexpr uint8_t kLEAdvertsingSIDMax = 0xEF;

// Invalid RSSI value.
constexpr int8_t kRSSIInvalid = 127;

// The maximum length of a friendly name that can be assigned to a BR/EDR
// controller, in octets.
constexpr size_t kMaxNameLength = bt::kMaxNameLength;

// The maximum number of bytes in a HCI Command Packet payload, excluding the
// header. See Core Spec v5.0 Vol 2, Part E, 5.4.1, paragraph 2.
constexpr size_t kMaxCommandPacketPayloadSize = 255;

// The maximum number of bytes in a HCI event Packet payload, excluding the
// header. See Core Spec v5.0 Vol 2, Part E, 5.4.4, paragraph 1.
constexpr size_t kMaxEventPacketPayloadSize = 255;

// The maximum number of bytes in a HCI ACL data packet payload supported by our
// stack.
constexpr size_t kMaxACLPayloadSize = 1024;

// The maximum number of bytes in a HCI Synchronous Data packet payload.
// This is based on the maximum value of the 1-byte Data_Total_Length field of a Synchronous Data packet.
constexpr size_t kMaxSynchronousDataPacketPayloadSize = 255;

// Values that can be used in HCI Read|WriteFlowControlMode commands.
enum class FlowControlMode : uint8_t {
  // Packet based data flow control mode (default for a Primary Controller)
  kPacketBased = 0x00,

  // Data block based flow control mode (default for an AMP Controller)
  kDataBlockBased = 0x01
};

// The Packet Boundary Flag is contained in bits 4 and 5 in the second octet of
// a HCI ACL Data packet.
enum class ACLPacketBoundaryFlag : uint8_t {
  kFirstNonFlushable  = 0x00,
  kContinuingFragment = 0x01,
  kFirstFlushable     = 0x02,
  kCompletePDU        = 0x03,
};

// The Broadcast Flag is contained in bits 6 and 7 in the second octet of a HCI
// ACL Data packet.
enum class ACLBroadcastFlag : uint8_t {
  kPointToPoint              = 0x00,
  kActivePeripheralBroadcast = 0x01,
};

// The Packet Status Flag is contained in bits 4 to 5 of the second octet of
// a HCI Synchronous Data packet (Core Spec v5.2, Vol 4, Part E, Sec 5.4.3).
enum class SynchronousDataPacketStatusFlag : uint8_t {
  kCorrectlyReceived  = 0x00,
  kPossiblyInvalid  = 0x01,
  kNoDataReceived  = 0x02,
  kDataPartiallyLost = 0x03,
};

// Possible values that can be reported for the Central_Clock_Accuracy and
// Advertiser_Clock_Accuracy parameters.
enum class LEClockAccuracy : uint8_t {
  k500Ppm = 0x00,
  k250Ppm = 0x01,
  k150Ppm = 0x02,
  k100Ppm = 0x03,
  k75Ppm  = 0x04,
  k50Ppm  = 0x05,
  k30Ppm  = 0x06,
  k20Ppm  = 0x07,
};

// Possible values that can be reported in a LE Channel Selection Algorithm event.
enum class LEChannelSelectionAlgorithm : uint8_t {
  kAlgorithm1 = 0x00,
  kAlgorithm2 = 0x01,
};

// "Hosts and Controllers shall be able to accept HCI ACL Data Packets with up
// to 27 bytes of data excluding the HCI ACL Data Packet header on Connection
// Handles associated with an LE-U logical link." (See Core Spec v5.0, Volume 2,
// Part E, Section 5.4.2)
constexpr size_t kMinLEACLDataBufferLength = 27;

// The maximum value that can be used for a 12-bit connection handle.
constexpr uint16_t kConnectionHandleMax = 0x0EFF;

// The maximum value that can ve used for a 8-bit advertising set handle.
constexpr uint8_t kAdvertisingHandleMax = 0xEF;

// The maximum value that can be set for the length of an Inquiry
constexpr uint8_t kInquiryLengthMax = 0x30;

// Bit 15, or "Clock_Offset_Valid_Flag" of the 16-bit clock offset field.
// Some HCI commands that require a clock offset expect this bit to be set (e.g.
// see HCI_Remote_Name_Request command, Vol 2, Part E, 7.1.19).
constexpr uint16_t kClockOffsetValidFlagBit = 0x8000;

// Masks the lower 15 bits of a Clock_Offset, excluding the bit 15 - the reserved/validity bit.
constexpr uint16_t kClockOffsetMask = 0x7FFF;

// Bitmask Values for the Scan_Enable parameter in a
// HCI_(Read,Write)_Scan_Enable command.
enum class ScanEnableBit : uint8_t {
  kInquiry  = (1 << 0), // Inquiry scan enabled
  kPage     = (1 << 1), // Page scan enabled.
};

using ScanEnableType = uint8_t;

// The minimum and maximum range values for Page Scan Interval (in time slices)
// (see Core Spec v5.0, Vol 2, Part E, Section 7.3.19)
constexpr uint16_t kPageScanIntervalMin = 0x0012;
constexpr uint16_t kPageScanIntervalMax = 0x1000;

// The minimum and maximum range valeus for Page Scan Window (in time slices)
// (see Core Spec v5.0, Vol 2, Part E, Section 7.3.19)
constexpr uint16_t kPageScanWindowMin = 0x0011;
constexpr uint16_t kPageScanWindowMax = 0x1000;

enum class PageScanType : uint8_t {
  kStandardScan = 0x00, // Standard scan (default) (mandatory)
  kInterlacedScan = 0x01, // Interlaced scan
};

// Constant values for common scanning modes
// See Spec 5.0, Vol 3, Part C, Section 4.2.2.1, Table 4.2
constexpr uint16_t kPageScanR0Interval = 0x0800; // 1.28s
constexpr uint16_t kPageScanR0Window = 0x0800; // 1.28s
constexpr uint16_t kPageScanR1Interval = 0x0800; // 1.28s
constexpr uint16_t kPageScanR1Window = 0x0011; // 10.625ms
constexpr uint16_t kPageScanR2Interval = 0x1000; // 2.56s
constexpr uint16_t kPageScanR2Window = 0x0011; // 10.625ms

// The minimum and maximum range values for Inquiry Scan Interval (in time slices)
// (see Core Spec v5.0, Vol 2, Part E, Section 7.3.21)
constexpr uint16_t kInquiryScanIntervalMin = 0x0012;
constexpr uint16_t kInquiryScanIntervalMax = 0x1000;

// The minimum and maximum range values for Inquiry Scan Window (in time slices)
// (see Core Spec v5.0, Vol 2, Part E, Section 7.3.21)
constexpr uint16_t kInquiryScanWindowMin = 0x0011;
constexpr uint16_t kInquiryScanWindowMax = 0x1000;

enum class InquiryScanType : uint8_t {
  kStandardScan = 0x00, // Standard scan (default) (mandatory)
  kInterlacedScan = 0x01, // Interlaced scan
};

// Link Types for BR/EDR connections.
enum class LinkType : uint8_t {
  kSCO = 0x00,         // SCO
  kACL = 0x01,         // ACL (data channel)
  kExtendedSCO = 0x02, // eSCO
};

enum class InquiryMode : uint8_t {
  kStandard = 0x00, // Standard Inquiry Result format (default)
  kRSSI = 0x01, // Inquiry Result format with RSSI
  kExtended = 0x02, // Inquiry Result format with RSSI or EIR format
};

// Length of the Extended Inquiry Response data. (Vol 3, Part C, Section 8)
constexpr size_t kExtendedInquiryResponseBytes = 240;

// Maximum length of a local name in the Extended Inquiry Response data.
// Length: 1 byte, DataType: 1 byte, Remaining buffer: 238 bytes.
// (Vol 3, Part C, Section 8)
constexpr size_t kExtendedInquiryResponseMaxNameBytes = kExtendedInquiryResponseBytes - 2;

// Minimum supported encryption key size for ACL-U links, as queried by Read
// Encryption Key Size. This isn't specified so the value is taken from the LE
// limit for SM Long Term Keys (v5.0 Vol 3, Part H, 2.3.4). This limit applies
// to the per-session encryption key, not the semi-permanent Link Key (v5.0
// Vol 2, Part H, 1).
constexpr uint8_t kMinEncryptionKeySize = 7;

// Key types for BR/EDR link encryption as reported to the host using the Link
// Key Notification event upon pairing or key changes (v5.0 Vol 2, Part E,
// Section 7.7.24).
//
// "Combination" refers to keys created from contributions of two devices
// according to v5.0 Vol 2, Part H, Section 3.2.4 and as opposed to "unit" keys
// that are generated on a single device but used by both parties (Section 3.2.3
// and deprecated in Section 3.1).
//
// Authenticated keys were generated using a challenge-response scheme described
// in v5.0 Vol 2, Part H, Section 5 to protect against man-in-the-middle (MITM)
// attacks.
//
// When Secure Connections is used, the key exchange uses keys generated from
// points on a 256-bit elliptic curve (v5.0 Vol 2, Part H, Section 7.1) and
// authentication uses Secure Authentication procedures described in Section 5.
enum class LinkKeyType : uint8_t {
  // Legacy pairing (pre-v2.1) key types
  kCombination = 0x00,
  kLocalUnit = 0x01,
  kRemoteUnit = 0x02,

  // Secure Simple Pairing key types
  kDebugCombination = 0x03,
  kUnauthenticatedCombination192 = 0x04,
  kAuthenticatedCombination192 = 0x05,

  // Special value indicating key generated due to a Change Connection Link Key
  // command. The actual key type is the same as before the change.
  kChangedCombination = 0x06,

  // Secure Simple Pairing with Secure Connections key types
  kUnauthenticatedCombination256 = 0x07,
  kAuthenticatedCombination256 = 0x08,
};

// Bitmask values for supported Packet Types
// Used for HCI_Create_Connection and HCI_Change_Connection_Packet_Type
enum class PacketTypeBits : uint16_t {
  // Reserved (1 << 0)
  kDisable2DH1 = (1 << 1),
  kDisable3DH1 = (1 << 2),
  kEnableDM1 = (1 << 3), // Note: always on in >= v1.2
  kEnableDH1 = (1 << 4),
  // Reserved (1 << 5)
  // Reserved (1 << 6)
  // Reserved (1 << 7)
  kDisable2DH3 = (1 << 8),
  kDisable3DH3 = (1 << 9),
  kEnableDM3 = (1 << 10),
  kEnableDH3 = (1 << 11),
  kDisable2DH5 = (1 << 12),
  kDisable3DH5 = (1 << 13),
  kEnableDM5 = (1 << 14),
  kEnableDH5 = (1 << 15),
};

using PacketTypeType = uint16_t;

enum class RoleSwitchBits : uint8_t {
  kDisallowRoleSwitch = 0x0,
  kAllowRoleSwitch = 0x1
};

enum class ScoRetransmissionEffort : uint8_t {
  // SCO or eSCO
  kNone = 0x00,

  // eSCO only
  kPowerOptimized  = 0x01,

  // eSCO only
  kQualityOptimized  = 0x02,

  // SCO or eSCO
  kDontCare = 0xFF,
};

// Flush Timeout = N * 0.625ms (Core Spec v5.2, Vol 4, Part E, Sec 7.3.30).
constexpr float kFlushTimeoutCommandParameterToMillisecondsConversionFactor = 0.625f;
constexpr float kFlushTimeoutMsToCommandParameterConversionFactor = 1.0f / kFlushTimeoutCommandParameterToMillisecondsConversionFactor;

// See Core Spec v5.2, Vol 4, Part E, Sec 7.3.30
constexpr uint16_t kMaxAutomaticFlushTimeoutCommandParameterValue = 0x07FF;
constexpr zx::duration kMaxAutomaticFlushTimeoutDuration = zx::msec(static_cast<int64_t>(kMaxAutomaticFlushTimeoutCommandParameterValue * kFlushTimeoutCommandParameterToMillisecondsConversionFactor));

// Page Timeout = N * 0.625 ms (Core Spec v5.2, Vol 4, Part E, Sec 7.3.16).
// The default is 5.12 sec.
constexpr zx::duration kDurationPerPageTimeoutUnit = zx::usec(625);
constexpr zx::duration kMinPageTimeoutDuration = kDurationPerPageTimeoutUnit * static_cast<uint16_t>(hci_spec::PageTimeout::MIN);
constexpr zx::duration kDefaultPageTimeoutDuration = kDurationPerPageTimeoutUnit * static_cast<uint16_t>(hci_spec::PageTimeout::DEFAULT);
constexpr zx::duration kMaxPageTimeoutDuration = kDurationPerPageTimeoutUnit * static_cast<uint16_t>(hci_spec::PageTimeout::MAX);

}  // namespace bt::hci_spec

#endif // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_CONSTANTS_H_
