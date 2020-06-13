// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_3.h"

#include <zircon/assert.h>

#include <optional>
#include <type_traits>

#include "lib/fit/function.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/active_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace sm {

Phase3::Phase3(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
               PairingFeatures features, SecurityProperties le_sec,
               Phase3CompleteCallback on_complete)
    : ActivePhase(std::move(chan), std::move(listener), role),
      features_(features),
      le_sec_(le_sec),
      obtained_remote_keys_(0),
      sent_local_keys_(false),
      ltk_bytes_(std::nullopt),
      ltk_(std::nullopt),
      irk_(std::nullopt),
      identity_address_(std::nullopt),
      on_complete_(std::move(on_complete)),
      weak_ptr_factory_(this) {
  // LTKs may not be distributed during Secure Connections.
  ZX_ASSERT_MSG(!(features_.secure_connections && (ShouldSendLtk() || ShouldReceiveLtk())),
                "Phase 3 may not distribute the LTK in Secure Connections pairing");
  // There should be some keys to distribute if Phase 3 exists.
  ZX_ASSERT(features_.local_key_distribution || features_.remote_key_distribution);
  // The link must be encrypted with at least an STK in order for Phase 3 to take place.
  ZX_ASSERT(le_sec.level() != SecurityLevel::kNoSecurity);
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void Phase3::Start() {
  ZX_ASSERT(!has_failed());
  ZX_ASSERT(!KeyExchangeComplete());
  // TODO(fxbug.dev/49371): The spec allows both the initiator & responder to distribute distinct
  // LTKs in Legacy pairing, but our stack currently only supports a single responder LTK.
  if (role() == Role::kInitiator ? ShouldSendLtk() : ShouldReceiveLtk()) {
    bt_log(WARN, "sm", "We do not support to distributing LTK from initiator to responder");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (role() == Role::kInitiator && !RequestedKeysObtained()) {
    bt_log(DEBUG, "sm", "waiting to receive keys from the responder");
    return;
  }

  if (!LocalKeysSent() && !SendLocalKeys()) {
    bt_log(DEBUG, "sm", "unable to send local keys");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  // If there are no keys left to exchange then we're done with pairing. This cannot be an else
  // branch because the above call to SendLocalKeys could've completed pairing.
  if (KeyExchangeComplete()) {
    SignalComplete();
  }
}

void Phase3::OnEncryptionInformation(const EncryptionInformationParams& ltk) {
  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(DEBUG, "sm", "\"Encryption Information\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (!ShouldReceiveLtk()) {
    bt_log(ERROR, "sm", "received unexpected LTK");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // abort pairing if we received a second LTK from the peer.
  if (ltk_bytes_.has_value() || ltk_.has_value()) {
    bt_log(ERROR, "sm", "already received LTK! aborting");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Abort pairing if the LTK is the sample LTK from the core spec
  if (ltk == kSpecSampleLtk) {
    bt_log(ERROR, "sm", "LTK is sample from spec, not secure! aborting");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Check that the received key has 0s at all locations more significant than
  // negotiated key_size
  uint8_t key_size = features_.encryption_key_size;
  ZX_DEBUG_ASSERT(key_size <= ltk.size());
  for (auto i = key_size; i < ltk.size(); i++) {
    if (ltk[i] != 0) {
      bt_log(ERROR, "sm", "received LTK is larger than max keysize! aborting");
      Abort(ErrorCode::kInvalidParameters);
      return;
    }
  }

  ZX_DEBUG_ASSERT(!(obtained_remote_keys_ & KeyDistGen::kEncKey));
  ltk_bytes_ = ltk;

  // Wait to receive EDiv and Rand
}

void Phase3::OnMasterIdentification(const MasterIdentificationParams& params) {
  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(DEBUG, "sm", "\"Master Identification\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }
  uint16_t ediv = le16toh(params.ediv);
  uint64_t random = le64toh(params.rand);

  if (!ShouldReceiveLtk()) {
    bt_log(ERROR, "sm", "received unexpected ediv/rand");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  // We only support receiving encryption information as initiator
  ZX_ASSERT(role() == Role::kInitiator);

  // EDIV and Rand must be sent AFTER the LTK (Vol 3, Part H, 3.6.1).
  if (!ltk_bytes_.has_value()) {
    bt_log(ERROR, "sm", "received EDIV and Rand before LTK!");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (obtained_remote_keys_ & KeyDistGen::kEncKey) {
    bt_log(ERROR, "sm", "already received EDIV and Rand!");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Abort pairing if the Rand is the sample Rand from the core spec
  if (random == kSpecSampleRandom) {
    bt_log(ERROR, "sm", "random is sample from core spec, not secure! aborting");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The security properties of the LTK are determined by the current link
  // properties (i.e. the properties of the STK).
  ZX_DEBUG_ASSERT(listener());
  ltk_ = LTK(le_sec_, hci::LinkKey(*ltk_bytes_, random, ediv));
  listener()->OnNewLongTermKey(*ltk_);
  obtained_remote_keys_ |= KeyDistGen::kEncKey;

  // "EncKey" received. Complete pairing if possible.
  OnExpectedKeyReceived();
}

void Phase3::OnIdentityInformation(const IRK& irk) {
  if (!ShouldReceiveIdentity()) {
    bt_log(ERROR, "sm", "received unexpected IRK");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Abort if we receive an IRK more than once.
  if (irk_.has_value()) {
    bt_log(ERROR, "sm", "already received IRK! aborting");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  ZX_DEBUG_ASSERT(!(obtained_remote_keys_ & KeyDistGen::kIdKey));
  irk_ = irk;

  // Wait to receive identity address
}

void Phase3::OnIdentityAddressInformation(const IdentityAddressInformationParams& params) {
  if (!ShouldReceiveIdentity()) {
    bt_log(ERROR, "sm", "received unexpected identity address");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The identity address must be sent after the IRK (Vol 3, Part H, 3.6.1).
  if (!irk_.has_value()) {
    bt_log(ERROR, "sm", "received identity address before the IRK!");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (obtained_remote_keys_ & KeyDistGen::kIdKey) {
    bt_log(ERROR, "sm", "already received identity information!");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  auto type = DeviceAddress::Type::kLERandom;
  if (params.type != AddressType::kStaticRandom) {
    if (params.type != AddressType::kPublic) {
      bt_log(WARN, "sm", "received invalid bt address type: %d", static_cast<int>(params.type));
      Abort(ErrorCode::kInvalidParameters);
      return;
    }
    type = DeviceAddress::Type::kLEPublic;
  }
  auto identity_address = DeviceAddress(type, params.bd_addr);
  // Store the identity address and mark all identity info as received.
  identity_address_ = identity_address;
  obtained_remote_keys_ |= KeyDistGen::kIdKey;

  // "IdKey" received. Complete pairing if possible.
  OnExpectedKeyReceived();
}

void Phase3::OnExpectedKeyReceived() {
  if (!RequestedKeysObtained()) {
    bt_log(DEBUG, "sm", "received one expected key, more keys pending");
    return;
  }

  if (role() == Role::kInitiator && !LocalKeysSent() && !SendLocalKeys()) {
    bt_log(DEBUG, "sm", "unable to send local keys to peer");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  // If we've received all the expected keys and sent the local keys, Phase 3 is complete
  SignalComplete();
}

bool Phase3::SendLocalKeys() {
  ZX_DEBUG_ASSERT(!LocalKeysSent());

  if (ShouldSendLtk() && !SendEncryptionKey()) {
    return false;
  }

  if (ShouldSendIdentity() && !SendIdentityInfo()) {
    return false;
  }

  sent_local_keys_ = true;
  return true;
}

bool Phase3::SendEncryptionKey() {
  ZX_ASSERT(!features_.secure_connections);

  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    return false;
  }

  // Generate a completely random value for LTK.
  UInt128 ltk_bytes = Random<UInt128>();

  // Mask the ltk down to the maximum encryption key size.
  uint8_t key_size = features_.encryption_key_size;
  if (key_size < 16) {
    MutableBufferView view(ltk_bytes.data() + key_size, 16 - key_size);
    view.SetToZeros();
  }

  // Generate completely random values for EDiv and Rand, use masked Ltk.
  hci::LinkKey link_key(ltk_bytes, Random<uint64_t>(), Random<uint16_t>());

  // The LTK inherits the security properties of the STK currently encrypting the link.
  ltk_ = LTK(le_sec_, link_key);

  // Assign the link key to make it available in case the initiator starts encryption.
  ZX_DEBUG_ASSERT(listener());
  listener()->OnNewLongTermKey(*ltk_);

  // Send LTK
  sm_chan().SendMessage(kEncryptionInformation, link_key.value());
  // Send EDiv & Rand
  sm_chan().SendMessage(kMasterIdentification,
                        MasterIdentificationParams{.ediv = htole16(link_key.ediv()),
                                                   .rand = htole64(link_key.rand())});

  return true;
}

bool Phase3::SendIdentityInfo() {
  std::optional<IdentityInfo> maybe_id_info = listener()->OnIdentityRequest();
  if (!maybe_id_info.has_value()) {
    bt_log(DEBUG, "sm",
           "local identity information required but no longer "
           "available; abort pairing");
    return false;
  }
  auto id_info = *maybe_id_info;

  if (!id_info.address.IsStaticRandom() && !id_info.address.IsPublic()) {
    bt_log(DEBUG, "sm", "identity address must be public or static random!");
    return false;
  }

  // Send IRK
  sm_chan().SendMessage(kIdentityInformation, id_info.irk);
  // Send identity address
  sm_chan().SendMessage(kIdentityAddressInformation,
                        IdentityAddressInformationParams{
                            .type = (id_info.address.IsStaticRandom() ? AddressType::kStaticRandom
                                                                      : AddressType::kPublic),
                            .bd_addr = id_info.address.value()});

  return true;
}

void Phase3::SignalComplete() {
  ZX_ASSERT(KeyExchangeComplete());

  // The security properties of all keys are determined by the security properties of the link used
  // to distribute them. This is already reflected by |le_sec_|.
  PairingData pairing_data;
  pairing_data.ltk = ltk_;

  if (irk_.has_value()) {
    // If there is an IRK there must also be an identity address.
    ZX_ASSERT(identity_address_.has_value());
    pairing_data.irk = Key(le_sec_, *irk_);
    pairing_data.identity_address = identity_address_;
  }
  on_complete_(pairing_data);
}

void Phase3::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ValidPacketReader, ErrorCode> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    Abort(maybe_reader.error());
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  switch (smp_code) {
    case kPairingFailed:
      OnFailure(Status(reader.payload<ErrorCode>()));
      break;
    case kEncryptionInformation:
      OnEncryptionInformation(reader.payload<EncryptionInformationParams>());
      break;
    case kMasterIdentification:
      OnMasterIdentification(reader.payload<MasterIdentificationParams>());
      break;
    case kIdentityInformation:
      OnIdentityInformation(reader.payload<IRK>());
      break;
    case kIdentityAddressInformation:
      OnIdentityAddressInformation(reader.payload<IdentityAddressInformationParams>());
      break;
    default:
      bt_log(INFO, "sm", "received unexpected code %d when in Pairing Phase 3", smp_code);
      sm_chan().SendMessage(kPairingFailed, ErrorCode::kUnspecifiedReason);
  }
}

bool Phase3::RequestedKeysObtained() const {
  // Return true if we expect no keys from the remote.
  return !features_.remote_key_distribution ||
         (features_.remote_key_distribution == obtained_remote_keys_);
}

bool Phase3::LocalKeysSent() const {
  // Return true if we didn't agree to send any keys.
  return !features_.local_key_distribution || sent_local_keys_;
}

bool Phase3::ShouldReceiveLtk() const {
  return (features_.remote_key_distribution & KeyDistGen::kEncKey);
}

bool Phase3::ShouldReceiveIdentity() const {
  return (features_.remote_key_distribution & KeyDistGen::kIdKey);
}

bool Phase3::ShouldSendLtk() const {
  return (features_.local_key_distribution & KeyDistGen::kEncKey);
}

bool Phase3::ShouldSendIdentity() const {
  return (features_.local_key_distribution & KeyDistGen::kIdKey);
}

}  // namespace sm
}  // namespace bt
