// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <mbedtls/bignum.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>

#include "pw_assert/check.h"
#include "pw_crypto/ecdh.h"
#include "pw_status/status.h"

namespace pw::crypto::ecdh::backend {
namespace {

// Mbed-TLS uses a 0 for success for most functions.
constexpr int kMbedTlsSuccess = 0;
// First byte of an uncompressed point as defined by SEC1 §2.3.3.
constexpr std::byte kUncompressedPointHeader = std::byte(0x04);

using EcpGroup =
    Wrapper<mbedtls_ecp_group, mbedtls_ecp_group_init, mbedtls_ecp_group_free>;

EcpGroup LoadEcpGroup(mbedtls_ecp_group_id id) {
  EcpGroup group;
  PW_CHECK_INT_EQ(kMbedTlsSuccess, mbedtls_ecp_group_load(group.Get(), id));
  return group;
}

EcpGroup CloneEcpGroup(const EcpGroup& group) {
  EcpGroup cloned;
  PW_CHECK_INT_EQ(kMbedTlsSuccess,
                  mbedtls_ecp_group_copy(cloned.Get(), group.Get()));
  return cloned;
}

const EcpGroup& P256() {
  static EcpGroup p256 = LoadEcpGroup(MBEDTLS_ECP_DP_SECP256R1);
  return p256;
}

Csprng* global_csprng = nullptr;

// Adapt a Csprng to a MBED-TLS-compatible function pointer.
int AdaptCsprng(void* ptr, unsigned char* buffer, size_t size) {
  PW_CHECK_NOTNULL(ptr, "No CSPRNG set for pw_crypto MBED-TLS backend.");
  auto csprng = static_cast<Csprng*>(ptr);
  return static_cast<int>(
      csprng->Generate(span(reinterpret_cast<std::byte*>(buffer), size)));
}

template <size_t kOffset, size_t kCoordSize>
void GetCoordFromPoint(const Point& point,
                       const EcpGroup& group,
                       span<std::byte, kCoordSize> out,
                       endian endianness) {
  std::array<unsigned char, 1 + kCoordSize * 2> buffer;
  size_t written_len = 0;
  PW_CHECK_INT_EQ(kMbedTlsSuccess,
                  mbedtls_ecp_point_write_binary(group.Get(),
                                                 point.Get(),
                                                 MBEDTLS_ECP_PF_UNCOMPRESSED,
                                                 &written_len,
                                                 buffer.data(),
                                                 buffer.size()));
  PW_CHECK_INT_EQ(written_len, buffer.size());

  auto coord_bytes =
      as_bytes(span(buffer)).template subspan<kOffset, kCoordSize>();
  static_assert(coord_bytes.size() == out.size());

  switch (endianness) {
    case endian::big:
      std::copy(coord_bytes.begin(), coord_bytes.end(), out.begin());
      break;
    case endian::little:
      std::reverse_copy(coord_bytes.begin(), coord_bytes.end(), out.begin());
      break;
  }
}

void GetXFromPoint(const Point& point, P256Coordinate out, endian endianness) {
  GetCoordFromPoint<1, kP256CoordSize>(point, P256(), out, endianness);
}

void GetYFromPoint(const Point& point, P256Coordinate out, endian endianness) {
  GetCoordFromPoint<1 + kP256CoordSize, kP256CoordSize>(
      point, P256(), out, endianness);
}

Status ImportPoint(Point* point,
                   P256ConstCoordinate x,
                   P256ConstCoordinate y,
                   endian endianness) {
  std::array<std::byte, 1 + kP256CoordSize * 2> buffer{
      kUncompressedPointHeader};
  auto x_span = span(buffer).subspan<1, kP256CoordSize>();
  auto y_span = span(buffer).subspan<1 + kP256CoordSize, kP256CoordSize>();
  static_assert(x_span.size() == y_span.size() && x_span.size() == x.size());

  switch (endianness) {
    case endian::big:
      std::copy(x.begin(), x.end(), x_span.begin());
      std::copy(y.begin(), y.end(), y_span.begin());
      break;
    case endian::little:
      std::reverse_copy(x.begin(), x.end(), x_span.begin());
      std::reverse_copy(y.begin(), y.end(), y_span.begin());
      break;
  }

  if (kMbedTlsSuccess != mbedtls_ecp_point_read_binary(
                             P256().Get(),
                             point->Get(),
                             reinterpret_cast<unsigned char*>(buffer.data()),
                             buffer.size())) {
    return Status::Internal();
  }

  if (kMbedTlsSuccess != mbedtls_ecp_check_pubkey(P256().Get(), point->Get())) {
    return Status::InvalidArgument();
  }

  return OkStatus();
}

}  // namespace

Csprng::~Csprng() {}

void SetCsprng(Csprng* csprng) {
  PW_CHECK(global_csprng == nullptr);
  global_csprng = csprng;
}

void ResetCsprngForTesting() { global_csprng = nullptr; }

Status DoGetX(const NativeP256Keypair& ctx,
              P256Coordinate out,
              endian endianness) {
  GetXFromPoint(ctx.public_key, out, endianness);
  return OkStatus();
}

Status DoGetY(const NativeP256Keypair& ctx,
              P256Coordinate out,
              endian endianness) {
  GetYFromPoint(ctx.public_key, out, endianness);
  return OkStatus();
}

Status DoGetX(const NativeP256PublicKey& ctx,
              P256Coordinate out,
              endian endianness) {
  GetXFromPoint(ctx.point, out, endianness);
  return OkStatus();
}

Status DoGetY(const NativeP256PublicKey& ctx,
              P256Coordinate out,
              endian endianness) {
  GetYFromPoint(ctx.point, out, endianness);
  return OkStatus();
}

Status DoGenerate(NativeP256Keypair& ctx) {
  if (kMbedTlsSuccess != mbedtls_ecdh_gen_public(CloneEcpGroup(P256()).Get(),
                                                 ctx.private_key.Get(),
                                                 ctx.public_key.Get(),
                                                 AdaptCsprng,
                                                 global_csprng)) {
    return Status::Internal();
  }
  return OkStatus();
}

Status DoImport(NativeP256Keypair& ctx,
                P256ConstPrivateKey private_key,
                P256ConstCoordinate x,
                P256ConstCoordinate y,
                endian endianness) {
  PW_TRY(ImportPoint(&ctx.public_key, x, y, endianness));

  int (*read_binary)(mbedtls_mpi*, const unsigned char*, size_t) = nullptr;
  switch (endianness) {
    case endian::big:
      read_binary = mbedtls_mpi_read_binary;
      break;
    case endian::little:
      read_binary = mbedtls_mpi_read_binary_le;
      break;
  }

  PW_CHECK_NOTNULL(read_binary);
  if (kMbedTlsSuccess !=
      read_binary(ctx.private_key.Get(),
                  reinterpret_cast<const unsigned char*>(private_key.data()),
                  private_key.size())) {
    return Status::Internal();
  }

  if (kMbedTlsSuccess !=
      mbedtls_ecp_check_privkey(P256().Get(), ctx.private_key.Get())) {
    return Status::InvalidArgument();
  }

  return OkStatus();
}

Status DoImport(NativeP256PublicKey& ctx,
                P256ConstCoordinate x,
                P256ConstCoordinate y,
                endian endianness) {
  return ImportPoint(&ctx.point, x, y, endianness);
}

Status ComputeDiffieHellman(const NativeP256Keypair& key,
                            const NativeP256PublicKey& other_key,
                            P256DhKey out) {
  Mpi shared_key;
  if (kMbedTlsSuccess !=
      mbedtls_ecdh_compute_shared(CloneEcpGroup(P256()).Get(),
                                  shared_key.Get(),
                                  other_key.point.Get(),
                                  key.private_key.Get(),
                                  AdaptCsprng,
                                  global_csprng)) {
    return Status::Internal();
  }

  if (kMbedTlsSuccess !=
      mbedtls_mpi_write_binary(shared_key.Get(),
                               reinterpret_cast<unsigned char*>(out.data()),
                               out.size())) {
    return Status::Internal();
  }

  return OkStatus();
}

}  // namespace pw::crypto::ecdh::backend
