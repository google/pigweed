// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bearer.h"

#include <lib/async/default.h>
#include <zircon/status.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

#include "lib/fxl/strings/string_printf.h"

#include "util.h"

namespace btlib {

using common::ByteBuffer;
using common::DeviceAddress;
using common::HostError;

namespace sm {
namespace {

common::MutableByteBufferPtr NewPDU(size_t param_size) {
  auto pdu = common::NewSlabBuffer(sizeof(Header) + param_size);
  if (!pdu) {
    bt_log(TRACE, "sm", "out of memory");
  }

  return pdu;
}

}  // namespace

Bearer::Bearer(fbl::RefPtr<l2cap::Channel> chan, hci::Connection::Role role,
               bool sc_supported, IOCapability io_capability,
               fxl::WeakPtr<Listener> listener)
    : chan_(std::move(chan)),
      role_(role),
      oob_available_(false),
      mitm_required_(false),
      sc_supported_(sc_supported),
      io_capability_(io_capability),
      listener_(listener),
      feature_exchange_pending_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(chan_);
  ZX_DEBUG_ASSERT(listener_);
  ZX_DEBUG_ASSERT_MSG(async_get_default_dispatcher(),
                      "default dispatcher required!");

  if (chan_->link_type() == hci::Connection::LinkType::kLE) {
    ZX_DEBUG_ASSERT(chan_->id() == l2cap::kLESMPChannelId);
    mtu_ = kLEMTU;
  } else if (chan_->link_type() == hci::Connection::LinkType::kACL) {
    ZX_DEBUG_ASSERT(chan_->id() == l2cap::kSMPChannelId);
    mtu_ = kBREDRMTU;
  } else {
    ZX_PANIC("unsupported link type!");
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  chan_->Activate(
      [self](auto sdu) {
        if (self) {
          self->OnRxBFrame(std::move(sdu));
        }
      },
      [self] {
        if (self) {
          self->OnChannelClosed();
        }
      },
      async_get_default_dispatcher());
}

bool Bearer::InitiateFeatureExchange() {
  // TODO(armansito): It should be possible to re-initiate pairing with
  // different parameters even when it's in progress.
  if (pairing_started() || feature_exchange_pending_) {
    bt_log(TRACE, "sm", "feature exchange already pending!");
    return false;
  }

  if (role_ == hci::Connection::Role::kSlave) {
    bt_log(TRACE, "sm", "slave cannot initiate a feature exchange!");
    return false;
  }

  auto pdu = NewPDU(sizeof(PairingRequestParams));
  if (!pdu) {
    return false;
  }

  PacketWriter writer(kPairingRequest, pdu.get());
  auto* params = writer.mutable_payload<PairingRequestParams>();
  BuildPairingParameters(params, &params->initiator_key_dist_gen,
                         &params->responder_key_dist_gen);

  // Cache the pairing request. This will be used as the |preq| parameter for
  // crypto functions later (e.g. during confirm value generation in legacy
  // pairing).
  pdu->Copy(&pairing_payload_buffer_);

  // Start pairing timer.
  ZX_DEBUG_ASSERT(!timeout_task_.is_pending());
  timeout_task_.PostDelayed(async_get_default_dispatcher(), kPairingTimeout);

  feature_exchange_pending_ = true;
  chan_->Send(std::move(pdu));

  return true;
}

bool Bearer::SendConfirmValue(const common::UInt128& confirm) {
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "not pairing!");
    return false;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    return false;
  }

  auto pdu = NewPDU(sizeof(PairingConfirmValue));
  if (!pdu) {
    bt_log(ERROR, "sm", "out of memory!");
    Abort(ErrorCode::kUnspecifiedReason);
    return false;
  }

  PacketWriter writer(kPairingConfirm, pdu.get());
  *writer.mutable_payload<PairingConfirmValue>() = confirm;
  chan_->Send(std::move(pdu));

  return true;
}

bool Bearer::SendRandomValue(const common::UInt128& random) {
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "not pairing!");
    return false;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    return false;
  }

  auto pdu = NewPDU(sizeof(PairingRandomValue));
  if (!pdu) {
    bt_log(ERROR, "sm", "out of memory!");
    Abort(ErrorCode::kUnspecifiedReason);
    return false;
  }

  PacketWriter writer(kPairingRandom, pdu.get());
  *writer.mutable_payload<PairingRandomValue>() = random;
  chan_->Send(std::move(pdu));

  return true;
}

bool Bearer::SendEncryptionKey(const hci::LinkKey& link_key) {
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "not pairing!");
    return false;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    return false;
  }

  auto enc_info_pdu = NewPDU(sizeof(EncryptionInformationParams));
  auto master_id_pdu = NewPDU(sizeof(MasterIdentificationParams));
  if (!enc_info_pdu || !master_id_pdu) {
    bt_log(ERROR, "sm", "out of memory!");
    Abort(ErrorCode::kUnspecifiedReason);
    return false;
  }

  // Send LTK
  {
    PacketWriter writer(kEncryptionInformation, enc_info_pdu.get());
    *writer.mutable_payload<EncryptionInformationParams>() = link_key.value();
    chan_->Send(std::move(enc_info_pdu));
  }

  // Send EDiv & Rand
  {
    PacketWriter writer(kMasterIdentification, master_id_pdu.get());
    auto* params = writer.mutable_payload<MasterIdentificationParams>();
    params->ediv = htole16(link_key.ediv());
    params->rand = htole64(link_key.rand());
    chan_->Send(std::move(master_id_pdu));
  }

  return true;
}

void Bearer::StopTimer() {
  if (timeout_task_.is_pending()) {
    zx_status_t status = timeout_task_.Cancel();
    if (status != ZX_OK) {
      bt_log(SPEW, "sm", "smp: failed to stop timer: %s",
             zx_status_get_string(status));
    }
  }
}

void Bearer::Abort(ErrorCode ecode) {
  // TODO(armansito): Check the states of other procedures once we have them.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "pairing not started! nothing to abort");
    return;
  }

  bt_log(ERROR, "sm", "abort pairing");

  StopTimer();
  SendPairingFailed(ecode);
  OnFailure(Status(ecode));
}

void Bearer::OnFailure(Status status) {
  bt_log(ERROR, "sm", "pairing failed: %s", status.ToString().c_str());

  // TODO(armansito): Clear other procedure states here.
  feature_exchange_pending_ = false;
  ZX_DEBUG_ASSERT(listener_);
  listener_->OnPairingFailed(status);
}

void Bearer::OnPairingTimeout() {
  // Pairing is no longer allowed on this bearer. Disconnect the link.
  bt_log(ERROR, "sm", "pairing timed out! disconnecting link");
  chan_->SignalLinkError();

  OnFailure(Status(HostError::kTimedOut));
}

ErrorCode Bearer::ResolveFeatures(bool local_initiator,
                                  const PairingRequestParams& preq,
                                  const PairingResponseParams& pres,
                                  PairingFeatures* out_features) {
  ZX_DEBUG_ASSERT(pairing_started());
  ZX_DEBUG_ASSERT(feature_exchange_pending_);

  // Select the smaller of the initiator and responder max. encryption key size
  // values (Vol 3, Part H, 2.3.4).
  uint8_t enc_key_size =
      std::min(preq.max_encryption_key_size, pres.max_encryption_key_size);
  if (enc_key_size < kMinEncryptionKeySize) {
    bt_log(TRACE, "sm", "encryption key size too small! (%u)", enc_key_size);
    return ErrorCode::kEncryptionKeySize;
  }

  bool sc = (preq.auth_req & AuthReq::kSC) && (pres.auth_req & AuthReq::kSC);
  bool mitm =
      (preq.auth_req & AuthReq::kMITM) || (pres.auth_req & AuthReq::kMITM);
  bool init_oob = preq.oob_data_flag == OOBDataFlag::kPresent;
  bool rsp_oob = pres.oob_data_flag == OOBDataFlag::kPresent;

  IOCapability local_ioc, peer_ioc;
  if (local_initiator) {
    local_ioc = preq.io_capability;
    peer_ioc = pres.io_capability;
  } else {
    local_ioc = pres.io_capability;
    peer_ioc = preq.io_capability;
  }

  PairingMethod method = util::SelectPairingMethod(
      sc, init_oob, rsp_oob, mitm, local_ioc, peer_ioc, local_initiator);

  // If MITM protection is required but the pairing method cannot provide MITM,
  // then reject the pairing.
  if (mitm && method == PairingMethod::kJustWorks) {
    return ErrorCode::kAuthenticationRequirements;
  }

  // The "Pairing Response" command (i.e. |pres|) determines the keys that shall
  // be distributed. The keys that will be distributed by us and the peer
  // depends on whichever one initiated the feature exchange by sending a
  // "Pairing Request" command.
  KeyDistGenField local_keys, remote_keys;
  if (local_initiator) {
    local_keys = pres.initiator_key_dist_gen;
    remote_keys = pres.responder_key_dist_gen;

    // v5.1, Vol 3, Part H requires that the responder shall not set to one
    // any flag in the key dist gen fields that the initiator has set to zero.
    // Hence we reject the pairing if the responder requests keys that we don't
    // support.
    if ((preq.initiator_key_dist_gen & local_keys) != local_keys ||
        (preq.responder_key_dist_gen & remote_keys) != remote_keys) {
      return ErrorCode::kInvalidParameters;
    }
  } else {
    local_keys = pres.responder_key_dist_gen;
    remote_keys = pres.initiator_key_dist_gen;

    // When we are the responder we always respect the initiator's wishes.
    ZX_DEBUG_ASSERT((preq.initiator_key_dist_gen & remote_keys) == remote_keys);
    ZX_DEBUG_ASSERT((preq.responder_key_dist_gen & local_keys) == local_keys);
  }

  *out_features = PairingFeatures(local_initiator, sc, method, enc_key_size,
                                  local_keys, remote_keys);

  return ErrorCode::kNoError;
}

void Bearer::BuildPairingParameters(PairingRequestParams* params,
                                    KeyDistGenField* out_local_keys,
                                    KeyDistGenField* out_remote_keys) {
  ZX_DEBUG_ASSERT(params);
  ZX_DEBUG_ASSERT(out_local_keys);
  ZX_DEBUG_ASSERT(out_remote_keys);

  // We always request bonding.
  AuthReqField auth_req = AuthReq::kBondingFlag;
  if (sc_supported_) {
    auth_req |= AuthReq::kSC;
  }
  if (mitm_required_) {
    auth_req |= AuthReq::kMITM;
  }

  params->io_capability = io_capability_;
  params->auth_req = auth_req;
  params->max_encryption_key_size = kMaxEncryptionKeySize;
  params->oob_data_flag =
      oob_available_ ? OOBDataFlag::kPresent : OOBDataFlag::kNotPresent;

  // We always request identity information from the remote.
  // TODO(armansito): Support sending local identity info when we support local
  // RPAs.
  *out_local_keys = 0;
  *out_remote_keys = KeyDistGen::kIdKey;

  // When we are the master, we request that the slave send us encryption
  // information as it is required to do so (Vol 3, Part H, 2.4.2.3). Otherwise
  // we always request to distribute it.
  if (role_ == hci::Connection::Role::kMaster) {
    *out_remote_keys |= KeyDistGen::kEncKey;
  } else {
    *out_local_keys |= KeyDistGen::kEncKey;
  }
}

void Bearer::OnPairingFailed(const PacketReader& reader) {
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "received \"Pairing Failed\" while not pairing!");
    return;
  }

  Status status(HostError::kFailed);

  if (reader.payload_size() == sizeof(ErrorCode)) {
    status = Status(reader.payload<ErrorCode>());
  } else {
    bt_log(TRACE, "sm", "malformed \"Pairing Failed\" payload");
  }

  StopTimer();
  OnFailure(status);
}

void Bearer::OnPairingRequest(const PacketReader& reader) {
  if (reader.payload_size() != sizeof(PairingRequestParams)) {
    bt_log(TRACE, "sm", "malformed \"Pairing Request\" payload");
    SendPairingFailed(ErrorCode::kInvalidParameters);
    return;
  }

  // Reject the command if we are the master.
  if (role_ == hci::Connection::Role::kMaster) {
    bt_log(TRACE, "sm", "rejecting \"Pairing Request\" from slave");
    SendPairingFailed(ErrorCode::kCommandNotSupported);
    return;
  }

  // We shouldn't be in this state when pairing is initiated by the remote.
  ZX_DEBUG_ASSERT(!feature_exchange_pending_);
  feature_exchange_pending_ = true;

  const auto& req_params = reader.payload<PairingRequestParams>();
  auto pdu = NewPDU(sizeof(PairingResponseParams));
  if (!pdu) {
    bt_log(ERROR, "sm", "out of memory!");
    SendPairingFailed(ErrorCode::kUnspecifiedReason);
    return;
  }

  // "Upon reception of the Pairing Request command, the Security Manager Timer
  // shall be reset and started" (Vol 3, Part H, 3.4).
  if (pairing_started()) {
    StopTimer();
  }

  // Start pairing timer.
  ZX_DEBUG_ASSERT(!timeout_task_.is_pending());
  timeout_task_.PostDelayed(async_get_default_dispatcher(), kPairingTimeout);

  PacketWriter writer(kPairingResponse, pdu.get());
  KeyDistGenField local_keys, remote_keys;
  auto* rsp_params = writer.mutable_payload<PairingResponseParams>();
  BuildPairingParameters(rsp_params, &local_keys, &remote_keys);

  // The keys that will be exchanged correspond to the intersection of what the
  // initiator requests and we support.
  rsp_params->initiator_key_dist_gen =
      remote_keys & req_params.initiator_key_dist_gen;
  rsp_params->responder_key_dist_gen =
      local_keys & req_params.responder_key_dist_gen;

  PairingFeatures features;
  ErrorCode ecode = ResolveFeatures(false /* local_initiator */, req_params,
                                    *rsp_params, &features);
  feature_exchange_pending_ = false;
  if (ecode != ErrorCode::kNoError) {
    bt_log(TRACE, "sm", "rejecting pairing features");
    Abort(ecode);
    return;
  }

  // Copy the pairing response so that it's available after moving |pdu|. (We
  // want to make sure that we send the pairing response before calling
  // Listener::OnFeatureExchange which may trigger other SMP transactions.
  //
  // This will be used as the |pres| parameter for crypto functions later (e.g.
  // during confirm value generation in legacy pairing).
  pdu->Copy(&pairing_payload_buffer_);
  chan_->Send(std::move(pdu));

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnFeatureExchange(features, reader.data(),
                               pairing_payload_buffer_);
}

void Bearer::OnPairingResponse(const PacketReader& reader) {
  if (reader.payload_size() != sizeof(PairingResponseParams)) {
    bt_log(TRACE, "sm", "malformed \"Pairing Response\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  // Reject the command if we are the slave.
  if (role_ == hci::Connection::Role::kSlave) {
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (!feature_exchange_pending_) {
    bt_log(TRACE, "sm", "ignoring unexpected \"Pairing Response\" packet");
    return;
  }

  PairingFeatures features;
  ErrorCode ecode = ResolveFeatures(
      true /* local_initiator */,
      pairing_payload_buffer_.view(sizeof(Code)).As<PairingRequestParams>(),
      reader.payload<PairingResponseParams>(), &features);
  feature_exchange_pending_ = false;

  if (ecode != ErrorCode::kNoError) {
    Abort(ecode);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnFeatureExchange(features, pairing_payload_buffer_,
                               reader.data());
}

void Bearer::OnPairingConfirm(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"confirm value\"");
    return;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "\"Confirm value\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (reader.payload_size() != sizeof(PairingConfirmValue)) {
    bt_log(TRACE, "sm", "malformed \"Pairing Confirm\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnPairingConfirm(reader.payload<PairingConfirmValue>());
}

void Bearer::OnPairingRandom(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"random value\"");
    return;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "\"Random value\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (reader.payload_size() != sizeof(PairingRandomValue)) {
    bt_log(TRACE, "sm", "malformed \"Pairing Random\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnPairingRandom(reader.payload<PairingRandomValue>());
}

void Bearer::OnEncryptionInformation(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"Encryption Information\"");
    return;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm",
           "\"Encryption Information\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (reader.payload_size() != sizeof(EncryptionInformationParams)) {
    bt_log(TRACE, "sm", "malformed \"Encryption Information\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnLongTermKey(reader.payload<EncryptionInformationParams>());
}

void Bearer::OnMasterIdentification(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"Master Identification\"");
    return;
  }

  // Only allowed on the LE transport.
  if (chan_->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "\"Master Identification\" over BR/EDR not supported!");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (reader.payload_size() != sizeof(MasterIdentificationParams)) {
    bt_log(TRACE, "sm", "malformed \"Master Identification\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  const auto& params = reader.payload<MasterIdentificationParams>();
  ZX_DEBUG_ASSERT(listener_);
  listener_->OnMasterIdentification(le16toh(params.ediv), le64toh(params.rand));
}

void Bearer::OnIdentityInformation(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"Identity Information\"");
    return;
  }

  if (reader.payload_size() != sizeof(IRK)) {
    bt_log(TRACE, "sm", "malformed \"Identity Information\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnIdentityResolvingKey(reader.payload<IRK>());
}

void Bearer::OnIdentityAddressInformation(const PacketReader& reader) {
  // Ignore the command if not pairing.
  if (!pairing_started()) {
    bt_log(TRACE, "sm", "dropped unexpected \"Identity Address Information\"");
    return;
  }

  if (reader.payload_size() != sizeof(IdentityAddressInformationParams)) {
    bt_log(TRACE, "sm", "malformed \"Identity Address Information\" payload");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  const auto& params = reader.payload<IdentityAddressInformationParams>();
  ZX_DEBUG_ASSERT(listener_);
  listener_->OnIdentityAddress(
      DeviceAddress(params.type == AddressType::kStaticRandom
                        ? DeviceAddress::Type::kLERandom
                        : DeviceAddress::Type::kLEPublic,
                    params.bd_addr));
}

void Bearer::OnSecurityRequest(const PacketReader& reader) {
  // Ignore the security request if pairing is in progress.
  if (pairing_started() || feature_exchange_pending_) {
    bt_log(TRACE, "sm", "ignoring \"Security Request\" while already pairing");
    return;
  }

  if (reader.payload_size() != sizeof(AuthReqField)) {
    bt_log(TRACE, "sm", "malformed \"Security Request\" payload");
    SendPairingFailed(ErrorCode::kInvalidParameters);
    return;
  }

  // Reject the command if we are not the master.
  if (role_ != hci::Connection::Role::kMaster) {
    bt_log(TRACE, "sm", "rejecting \"Security Request\" as master");
    SendPairingFailed(ErrorCode::kCommandNotSupported);
    return;
  }

  ZX_DEBUG_ASSERT(listener_);
  listener_->OnSecurityRequest(reader.payload<AuthReqField>());
}

void Bearer::SendPairingFailed(ErrorCode ecode) {
  auto pdu = NewPDU(sizeof(ErrorCode));
  PacketWriter writer(kPairingFailed, pdu.get());
  *writer.mutable_payload<PairingFailedParams>() = ecode;
  chan_->Send(std::move(pdu));
}

void Bearer::OnChannelClosed() {
  bt_log(TRACE, "sm", "channel closed");

  if (pairing_started()) {
    OnFailure(Status(HostError::kLinkDisconnected));
  }
}

void Bearer::OnRxBFrame(common::ByteBufferPtr sdu) {
  ZX_DEBUG_ASSERT(sdu);
  uint8_t length = sdu->size();
  if (length < sizeof(Code)) {
    bt_log(TRACE, "sm", "PDU too short!");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  if (length > mtu_) {
    bt_log(TRACE, "sm", "PDU exceeds MTU!");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  PacketReader reader(sdu.get());

  switch (reader.code()) {
    case kPairingFailed:
      OnPairingFailed(reader);
      break;
    case kPairingRequest:
      OnPairingRequest(reader);
      break;
    case kPairingResponse:
      OnPairingResponse(reader);
      break;
    case kPairingConfirm:
      OnPairingConfirm(reader);
      break;
    case kPairingRandom:
      OnPairingRandom(reader);
      break;
    case kEncryptionInformation:
      OnEncryptionInformation(reader);
      break;
    case kMasterIdentification:
      OnMasterIdentification(reader);
      break;
    case kIdentityInformation:
      OnIdentityInformation(reader);
      break;
    case kIdentityAddressInformation:
      OnIdentityAddressInformation(reader);
      break;
    case kSecurityRequest:
      OnSecurityRequest(reader);
      break;
    default:
      bt_log(SPEW, "sm", "unsupported command: %#.2x", reader.code());
      auto ecode = ErrorCode::kCommandNotSupported;
      if (pairing_started()) {
        Abort(ecode);
      } else {
        SendPairingFailed(ecode);
      }
      break;
  }
}

}  // namespace sm
}  // namespace btlib
