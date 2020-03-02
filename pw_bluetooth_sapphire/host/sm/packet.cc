// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packet.h"

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace bt {
namespace sm {

namespace {
const std::unordered_map<Code, size_t> kCodeToPayloadSize{
    {kSecurityRequest, sizeof(AuthReqField)},
    {kPairingRequest, sizeof(PairingRequestParams)},
    {kPairingResponse, sizeof(PairingResponseParams)},
    {kPairingConfirm, sizeof(PairingConfirmValue)},
    {kPairingRandom, sizeof(PairingRandomValue)},
    {kPairingFailed, sizeof(PairingFailedParams)},
    {kEncryptionInformation, sizeof(EncryptionInformationParams)},
    {kMasterIdentification, sizeof(MasterIdentificationParams)},
    {kIdentityInformation, sizeof(IRK)},
    {kIdentityAddressInformation, sizeof(IdentityAddressInformationParams)},
    {kPairingPublicKey, sizeof(PairingPublicKeyParams)},
    {kPairingDHKeyCheck, sizeof(PairingDHKeyCheckValueE)},
};
}  // namespace

PacketReader::PacketReader(const ByteBuffer* buffer)
    : PacketView<Header>(buffer, buffer->size() - sizeof(Header)) {}

ValidPacketReader::ValidPacketReader(const ByteBuffer* buffer) : PacketReader(buffer) {}

fit::result<ValidPacketReader, sm::ErrorCode> ValidPacketReader::ParseSdu(const ByteBufferPtr& sdu,
                                                                          size_t mtu) {
  ZX_DEBUG_ASSERT(sdu);
  uint8_t length = sdu->size();
  if (length < sizeof(Code)) {
    bt_log(TRACE, "sm", "PDU too short!");
    return fit::error(ErrorCode::kInvalidParameters);
  }
  if (length > mtu) {
    bt_log(TRACE, "sm", "PDU exceeds MTU!");
    return fit::error(ErrorCode::kInvalidParameters);
  }
  auto reader = PacketReader(sdu.get());
  auto expected_payload_size = kCodeToPayloadSize.find(reader.code());
  if (expected_payload_size == kCodeToPayloadSize.end()) {
    bt_log(TRACE, "sm", "smp code not recognized: %#.2X", reader.code());
    return fit::error(ErrorCode::kCommandNotSupported);
  }
  if (reader.payload_size() != expected_payload_size->second) {
    bt_log(TRACE, "sm", "malformed packet with code %#.2X", reader.code());
    return fit::error(ErrorCode::kInvalidParameters);
  }
  return fit::ok(ValidPacketReader(sdu.get()));
}

PacketWriter::PacketWriter(Code code, MutableByteBuffer* buffer)
    : MutablePacketView<Header>(buffer, buffer->size() - sizeof(Header)) {
  mutable_header()->code = code;
}

}  // namespace sm
}  // namespace bt
