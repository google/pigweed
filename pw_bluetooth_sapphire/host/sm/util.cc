// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include <zircon/assert.h>

#include <algorithm>
#include <optional>

#include <openssl/aes.h>
#include <openssl/cmac.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint256.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"

namespace bt {

namespace sm {
namespace util {
namespace {

constexpr size_t kPreqSize = 7;
constexpr uint32_t k24BitMax = 0xFFFFFF;
// F5 parameters are stored in little-endian
const auto kF5Salt = UInt128{0xBE, 0x83, 0x60, 0x5A, 0xDB, 0x0B, 0x37, 0x60,
                             0x38, 0xA5, 0xF5, 0xAA, 0x91, 0x83, 0x88, 0x6C};
const auto kF5KeyId = std::array<uint8_t, 4>{0x65, 0x6C, 0x74, 0x62};

// Swap the endianness of a 128-bit integer. |in| and |out| should not be backed
// by the same buffer.
void Swap128(const UInt128& in, UInt128* out) {
  ZX_DEBUG_ASSERT(out);
  for (size_t i = 0; i < in.size(); ++i) {
    (*out)[i] = in[in.size() - i - 1];
  }
}

// XOR two 128-bit integers and return the result in |out|. It is possible to
// pass a pointer to one of the inputs as |out|.
void Xor128(const UInt128& int1, const UInt128& int2, UInt128* out) {
  ZX_DEBUG_ASSERT(out);

  uint64_t lower1 = *reinterpret_cast<const uint64_t*>(int1.data());
  uint64_t upper1 = *reinterpret_cast<const uint64_t*>(int1.data() + 8);

  uint64_t lower2 = *reinterpret_cast<const uint64_t*>(int2.data());
  uint64_t upper2 = *reinterpret_cast<const uint64_t*>(int2.data() + 8);

  uint64_t lower_res = lower1 ^ lower2;
  uint64_t upper_res = upper1 ^ upper2;

  std::memcpy(out->data(), &lower_res, 8);
  std::memcpy(out->data() + 8, &upper_res, 8);
}

// Writes |data| to |output_data_loc| & returns a view of the remainder of |output_data_loc|.
template <typename InputType>
MutableBufferView WriteToBuffer(InputType data, MutableBufferView output_data_loc) {
  output_data_loc.WriteObj(data);
  return output_data_loc.mutable_view(sizeof(data));
}

// Converts |addr| into the 56-bit format used by F5/F6 and writes that data to a BufferView.
// Returns a buffer view pointing just past the last byte written.
MutableBufferView WriteCryptoDeviceAddr(const DeviceAddress& addr, const MutableBufferView& out) {
  std::array<uint8_t, sizeof(addr.value()) + 1> little_endian_addr_buffer;
  BufferView addr_bytes = addr.value().bytes();
  std::copy(addr_bytes.begin(), addr_bytes.end(), little_endian_addr_buffer.data());
  little_endian_addr_buffer[6] = addr.IsPublic() ? 0x00 : 0x01;
  return WriteToBuffer(little_endian_addr_buffer, out);
}

}  // namespace

std::string IOCapabilityToString(IOCapability capability) {
  switch (capability) {
    case IOCapability::kDisplayOnly:
      return "Display Only";
    case IOCapability::kDisplayYesNo:
      return "Display w/ Confirmation";
    case IOCapability::kKeyboardOnly:
      return "Keyboard";
    case IOCapability::kNoInputNoOutput:
      return "No I/O";
    case IOCapability::kKeyboardDisplay:
      return "Keyboard w/ Display";
    default:
      break;
  }
  return "(unknown)";
}

hci::IOCapability IOCapabilityForHci(IOCapability capability) {
  switch (capability) {
    case IOCapability::kDisplayOnly:
      return hci::IOCapability::kDisplayOnly;
    case IOCapability::kDisplayYesNo:
      return hci::IOCapability::kDisplayYesNo;
    case IOCapability::kKeyboardOnly:
      return hci::IOCapability::kKeyboardOnly;
    case IOCapability::kNoInputNoOutput:
      return hci::IOCapability::kNoInputNoOutput;

    // There's no dedicated HCI "Keyboard w/ Display" IO Capability. Use
    // DisplayYesNo for devices with keyboard input and numeric output. See Core
    // Spec v5.0 Vol 3, Part C, Section 5.2.2.5 (Table 5.5).
    case IOCapability::kKeyboardDisplay:
      return hci::IOCapability::kDisplayYesNo;
    default:
      break;
  }
  return hci::IOCapability::kNoInputNoOutput;
}

std::string PairingMethodToString(PairingMethod method) {
  switch (method) {
    case PairingMethod::kJustWorks:
      return "Just Works";
    case PairingMethod::kPasskeyEntryInput:
      return "Passkey Entry (input)";
    case PairingMethod::kPasskeyEntryDisplay:
      return "Passkey Entry (display)";
    case PairingMethod::kNumericComparison:
      return "Numeric Comparison";
    case PairingMethod::kOutOfBand:
      return "OOB";
    default:
      break;
  }
  return "(unknown)";
}

std::string DisplayMethodToString(Delegate::DisplayMethod method) {
  switch (method) {
    case Delegate::DisplayMethod::kComparison:
      return "Numeric Comparison";
    case Delegate::DisplayMethod::kPeerEntry:
      return "Peer Passkey Entry";
    default:
      return "(unknown)";
  }
}

MutableByteBufferPtr NewPdu(size_t param_size) {
  // TODO(fxbug.dev/1338): Remove unique_ptr->DynamicByteBuffer double indirection once sufficient
  // progress has been made on the attached bug (specifically re:l2cap::Channel::Send).
  return std::make_unique<DynamicByteBuffer>(sizeof(Header) + param_size);
}

PairingMethod SelectPairingMethod(bool sec_conn, bool local_oob, bool peer_oob, bool mitm_required,
                                  IOCapability local_ioc, IOCapability peer_ioc,
                                  bool local_initiator) {
  if ((sec_conn && (local_oob || peer_oob)) || (!sec_conn && local_oob && peer_oob)) {
    return PairingMethod::kOutOfBand;
  }

  // If neither device requires MITM protection or if the peer has not I/O
  // capable, we select Just Works.
  if (!mitm_required || peer_ioc == IOCapability::kNoInputNoOutput) {
    return PairingMethod::kJustWorks;
  }

  // Select the pairing method by comparing I/O capabilities. The switch
  // statement will return if an authenticated entry method is selected.
  // Otherwise, we'll break out and default to Just Works below.
  switch (local_ioc) {
    case IOCapability::kNoInputNoOutput:
      break;

    case IOCapability::kDisplayOnly:
      switch (peer_ioc) {
        case IOCapability::kKeyboardOnly:
        case IOCapability::kKeyboardDisplay:
          return PairingMethod::kPasskeyEntryDisplay;
        default:
          break;
      }
      break;

    case IOCapability::kDisplayYesNo:
      switch (peer_ioc) {
        case IOCapability::kDisplayYesNo:
          return sec_conn ? PairingMethod::kNumericComparison : PairingMethod::kJustWorks;
        case IOCapability::kKeyboardDisplay:
          return sec_conn ? PairingMethod::kNumericComparison : PairingMethod::kPasskeyEntryDisplay;
        case IOCapability::kKeyboardOnly:
          return PairingMethod::kPasskeyEntryDisplay;
        default:
          break;
      }
      break;

    case IOCapability::kKeyboardOnly:
      return PairingMethod::kPasskeyEntryInput;

    case IOCapability::kKeyboardDisplay:
      switch (peer_ioc) {
        case IOCapability::kKeyboardOnly:
          return PairingMethod::kPasskeyEntryDisplay;
        case IOCapability::kDisplayOnly:
          return PairingMethod::kPasskeyEntryInput;
        case IOCapability::kDisplayYesNo:
          return sec_conn ? PairingMethod::kNumericComparison : PairingMethod::kPasskeyEntryInput;
        default:
          break;
      }

      // If both devices have KeyboardDisplay then use Numeric Comparison
      // if S.C. is supported. Otherwise, the initiator always displays and the
      // responder inputs a passkey.
      if (sec_conn) {
        return PairingMethod::kNumericComparison;
      }
      return local_initiator ? PairingMethod::kPasskeyEntryDisplay
                             : PairingMethod::kPasskeyEntryInput;
  }

  return PairingMethod::kJustWorks;
}

void Encrypt(const UInt128& key, const UInt128& plaintext_data, UInt128* out_encrypted_data) {
  // Swap the bytes since "the most significant octet of key corresponds to
  // key[0], the most significant octet of plaintextData corresponds to in[0]
  // and the most significant octet of encryptedData corresponds to out[0] using
  // the notation specified in FIPS-197" for the security function "e" (Vol 3,
  // Part H, 2.2.1).
  UInt128 be_k, be_pt, be_enc;
  Swap128(key, &be_k);
  Swap128(plaintext_data, &be_pt);

  AES_KEY k;
  AES_set_encrypt_key(be_k.data(), 128, &k);
  AES_encrypt(be_pt.data(), be_enc.data(), &k);

  Swap128(be_enc, out_encrypted_data);
}

void C1(const UInt128& tk, const UInt128& rand, const ByteBuffer& preq, const ByteBuffer& pres,
        const DeviceAddress& initiator_addr, const DeviceAddress& responder_addr,
        UInt128* out_confirm_value) {
  ZX_DEBUG_ASSERT(preq.size() == kPreqSize);
  ZX_DEBUG_ASSERT(pres.size() == kPreqSize);
  ZX_DEBUG_ASSERT(out_confirm_value);

  UInt128 p1, p2;

  // Calculate p1 = pres || preq || rat’ || iat’
  hci::LEAddressType iat = hci::AddressTypeToHCI(initiator_addr.type());
  hci::LEAddressType rat = hci::AddressTypeToHCI(responder_addr.type());
  p1[0] = static_cast<uint8_t>(iat);
  p1[1] = static_cast<uint8_t>(rat);
  std::memcpy(p1.data() + 2, preq.data(), preq.size());                // Bytes [2-8]
  std::memcpy(p1.data() + 2 + preq.size(), pres.data(), pres.size());  // [9-15]

  // Calculate p2 = padding || ia || ra
  BufferView ia = initiator_addr.value().bytes();
  BufferView ra = responder_addr.value().bytes();
  std::memcpy(p2.data(), ra.data(), ra.size());              // Lowest 6 bytes
  std::memcpy(p2.data() + ra.size(), ia.data(), ia.size());  // Next 6 bytes
  std::memset(p2.data() + ra.size() + ia.size(), 0,
              p2.size() - ra.size() - ia.size());  // Pad 0s for the remainder

  // Calculate the confirm value: e(tk, e(tk, rand XOR p1) XOR p2)
  UInt128 tmp;
  Xor128(rand, p1, &p1);
  Encrypt(tk, p1, &tmp);
  Xor128(tmp, p2, &tmp);
  Encrypt(tk, tmp, out_confirm_value);
}

void S1(const UInt128& tk, const UInt128& r1, const UInt128& r2, UInt128* out_stk) {
  ZX_DEBUG_ASSERT(out_stk);

  UInt128 r_prime;

  // Take the lower 64-bits of r1 and r2 and concatanate them to produce
  // r’ = r1’ || r2’, where r2' contains the LSB and r1' the MSB.
  constexpr size_t kHalfSize = sizeof(UInt128) / 2;
  std::memcpy(r_prime.data(), r2.data(), kHalfSize);
  std::memcpy(r_prime.data() + kHalfSize, r1.data(), kHalfSize);

  // Calculate the STK: e(tk, r’)
  Encrypt(tk, r_prime, out_stk);
}

uint32_t Ah(const UInt128& k, uint32_t r) {
  ZX_DEBUG_ASSERT(r <= k24BitMax);

  // r' = padding || r.
  UInt128 r_prime;
  r_prime.fill(0);
  *reinterpret_cast<uint32_t*>(r_prime.data()) = htole32(r & k24BitMax);

  UInt128 hash128;
  Encrypt(k, r_prime, &hash128);

  return le32toh(*reinterpret_cast<uint32_t*>(hash128.data())) & k24BitMax;
}

bool IrkCanResolveRpa(const UInt128& irk, const DeviceAddress& rpa) {
  if (!rpa.IsResolvablePrivate()) {
    return false;
  }

  // The |rpa_hash| and |prand| values generated below should match the least
  // and most significant 3 bytes of |rpa|, respectively.
  BufferView rpa_bytes = rpa.value().bytes();

  // Lower 24-bits (in host order).
  uint32_t rpa_hash = le32toh(rpa_bytes.As<uint32_t>()) & k24BitMax;

  // Upper 24-bits (we avoid a cast to uint32_t to prevent an invalid access
  // since the buffer would be too short).
  BufferView prand_bytes = rpa_bytes.view(3);
  uint32_t prand = prand_bytes[0];
  prand |= static_cast<uint32_t>(prand_bytes[1]) << 8;
  prand |= static_cast<uint32_t>(prand_bytes[2]) << 16;

  return Ah(irk, prand) == rpa_hash;
}

DeviceAddress GenerateRpa(const UInt128& irk) {
  // 24-bit prand value in little-endian order.
  constexpr auto k24BitSize = 3;
  uint32_t prand_le = 0;
  static_assert(k24BitSize == sizeof(uint32_t) - 1);
  MutableBufferView prand_bytes(&prand_le, k24BitSize);

  // The specification requires that at least one bit of the address is 1 and at
  // least one bit is 0. We expect that zx_cprng_draw() satisfies these
  // requirements.
  // TODO(SEC-87): Maybe generate within a range to enforce this?
  prand_bytes.FillWithRandomBytes();

  // Make sure that the highest two bits are 0 and 1 respectively.
  prand_bytes[2] |= 0b01000000;
  prand_bytes[2] &= ~0b10000000;

  // 24-bit hash value in little-endian order.
  uint32_t hash_le = htole32(Ah(irk, le32toh(prand_le)));
  BufferView hash_bytes(&hash_le, k24BitSize);

  // The |rpa_hash| and |prand| values generated below take up the least
  // and most significant 3 bytes of |rpa|, respectively.
  StaticByteBuffer<kDeviceAddressSize> addr_bytes;
  addr_bytes.Write(hash_bytes);
  addr_bytes.Write(prand_bytes, hash_bytes.size());

  return DeviceAddress(DeviceAddress::Type::kLERandom, DeviceAddressBytes(addr_bytes));
}

DeviceAddress GenerateRandomAddress(bool is_static) {
  StaticByteBuffer<kDeviceAddressSize> addr_bytes;

  // The specification requires that at least one bit of the address is 1 and at
  // least one bit is 0. We expect that zx_cprng_draw() satisfies these
  // requirements.
  // TODO(SEC-87): Maybe generate within a range to enforce this?
  addr_bytes.FillWithRandomBytes();

  if (is_static) {
    // The highest two bits of a static random address are both 1 (see Vol 3,
    // Part B, 1.3.2.1).
    addr_bytes[kDeviceAddressSize - 1] |= 0b11000000;
  } else {
    // The highest two bits of a NRPA are both 0 (see Vol 3, Part B, 1.3.2.2).
    addr_bytes[kDeviceAddressSize - 1] &= ~0b11000000;
  }

  return DeviceAddress(DeviceAddress::Type::kLERandom, DeviceAddressBytes(addr_bytes));
}

std::optional<UInt128> AesCmac(const UInt128& hash_key, const ByteBuffer& msg) {
  // Reverse little-endian input parameters to the big-endian format expected by BoringSSL.
  UInt128 big_endian_key;
  Swap128(hash_key, &big_endian_key);
  DynamicByteBuffer big_endian_msg(msg);
  uint8_t* msg_begin = big_endian_msg.mutable_data();
  std::reverse(msg_begin, msg_begin + big_endian_msg.size());
  UInt128 big_endian_out, little_endian_out;
  // 0 is the failure error code for AES_CMAC
  if (AES_CMAC(big_endian_out.data(), big_endian_key.data(), big_endian_key.size(), msg_begin,
               big_endian_msg.size()) == 0) {
    return std::nullopt;
  }
  Swap128(big_endian_out, &little_endian_out);
  return little_endian_out;
}

std::optional<UInt128> F4(const UInt256& u, const UInt256& v, const UInt128& x, const uint8_t z) {
  constexpr size_t kDataLength = 2 * kUInt256Size + 1;
  StaticByteBuffer<kDataLength> data_to_encrypt;
  // Write to buffer in reverse of human-readable spec format as all parameters are little-endian.
  MutableBufferView current_view = WriteToBuffer(z, data_to_encrypt.mutable_view());
  current_view = WriteToBuffer(v, current_view);
  current_view = WriteToBuffer(u, current_view);

  // Ensures |current_view| is at the end of data_to_encrypt
  ZX_DEBUG_ASSERT(current_view.size() == 0);
  return AesCmac(x, data_to_encrypt);
}

std::optional<F5Results> F5(const UInt256& dhkey, const UInt128& initiator_nonce,
                            const UInt128& responder_nonce, const DeviceAddress& initiator_addr,
                            const DeviceAddress& responder_addr) {
  // Get the T key value
  StaticByteBuffer<kUInt256Size> dhkey_buffer;
  WriteToBuffer(dhkey, dhkey_buffer.mutable_view());
  std::optional<UInt128> maybe_cmac = AesCmac(kF5Salt, dhkey_buffer);
  if (!maybe_cmac.has_value()) {
    return std::nullopt;
  }
  UInt128 t_key = maybe_cmac.value();

  // Create the MacKey and LTK using the T Key value.
  uint8_t counter = 0x00;
  const std::array<uint8_t, 2> length = {0x00, 0x01};  // 256 in little-endian
  constexpr size_t kDataLength = sizeof(counter) + kF5KeyId.size() + 2 * kUInt128Size +
                                 2 * (1 + kDeviceAddressSize) + length.size();
  StaticByteBuffer<kDataLength> data_to_encrypt;

  // Write to buffer in reverse of human-readable spec format as all parameters are little-endian.
  MutableBufferView current_view = WriteToBuffer(length, data_to_encrypt.mutable_view());
  current_view = WriteCryptoDeviceAddr(responder_addr, current_view);
  current_view = WriteCryptoDeviceAddr(initiator_addr, current_view);
  current_view = WriteToBuffer(responder_nonce, current_view);
  current_view = WriteToBuffer(initiator_nonce, current_view);
  current_view = WriteToBuffer(kF5KeyId, current_view);
  current_view = WriteToBuffer(counter, current_view);

  // Ensures |current_view| is at the end of data_to_encrypt
  ZX_DEBUG_ASSERT(current_view.size() == 0);
  maybe_cmac = AesCmac(t_key, data_to_encrypt);
  if (!maybe_cmac.has_value()) {
    return std::nullopt;
  }
  F5Results results{.mac_key = *maybe_cmac};

  // Overwrite counter value only for LTK calculation.
  counter = 0x01;
  data_to_encrypt.Write(&counter, 1, kDataLength - 1);
  maybe_cmac = AesCmac(t_key, data_to_encrypt);
  if (!maybe_cmac.has_value()) {
    return std::nullopt;
  }
  results.ltk = *maybe_cmac;
  return results;
}

std::optional<UInt128> F6(const UInt128& mackey, const UInt128& n1, const UInt128& n2,
                          const UInt128& r, AuthReqField auth_req, OOBDataFlag oob,
                          IOCapability io_cap, const DeviceAddress& a1, const DeviceAddress& a2) {
  constexpr size_t kDataLength = 3 * kUInt128Size + sizeof(AuthReqField) + sizeof(OOBDataFlag) +
                                 sizeof(IOCapability) + 2 * (1 + kDeviceAddressSize);
  StaticByteBuffer<kDataLength> data_to_encrypt;
  // Write to buffer in reverse of human-readable spec format as all parameters are little-endian.
  MutableBufferView current_view = WriteCryptoDeviceAddr(a2, data_to_encrypt.mutable_view());
  current_view = WriteCryptoDeviceAddr(a1, current_view);
  current_view = WriteToBuffer(static_cast<uint8_t>(io_cap), current_view);
  current_view = WriteToBuffer(static_cast<uint8_t>(oob), current_view);
  current_view = WriteToBuffer(auth_req, current_view);
  current_view = WriteToBuffer(r, current_view);
  current_view = WriteToBuffer(n2, current_view);
  current_view = WriteToBuffer(n1, current_view);
  // Ensures |current_view| is at the end of data_to_encrypt
  ZX_DEBUG_ASSERT(current_view.size() == 0);
  return AesCmac(mackey, data_to_encrypt);
}

std::optional<uint32_t> G2(const UInt256& initiator_pubkey_x, const UInt256& responder_pubkey_x,
                           const UInt128& initiator_nonce, const UInt128& responder_nonce) {
  constexpr size_t kDataLength = 2 * kUInt256Size + kUInt128Size;
  StaticByteBuffer<kDataLength> data_to_encrypt;
  // Write to buffer in reverse of human-readable spec format as all parameters are little-endian.
  MutableBufferView current_view = WriteToBuffer(responder_nonce, data_to_encrypt.mutable_view());
  current_view = WriteToBuffer(responder_pubkey_x, current_view);
  current_view = WriteToBuffer(initiator_pubkey_x, current_view);
  ZX_DEBUG_ASSERT(current_view.size() == 0);
  std::optional<UInt128> maybe_cmac = AesCmac(initiator_nonce, data_to_encrypt);
  if (!maybe_cmac.has_value()) {
    return std::nullopt;
  }
  UInt128 cmac_output = *maybe_cmac;
  // Implements the "mod 32" part of G2 on the little-endian output of AES-CMAC.
  return (uint32_t)cmac_output[3] << 24 | (uint32_t)cmac_output[2] << 16 |
         (uint32_t)cmac_output[1] << 8 | (uint32_t)cmac_output[0];
}

}  // namespace util
}  // namespace sm
}  // namespace bt
