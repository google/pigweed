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

#include "pw_crypto/ecdh.h"
#include "pw_status/status.h"

// This is a backend that is only provided to ensure the facade builds correctly
// and will be removed once real backends are implemented.
namespace pw::crypto::ecdh::backend {

Status DoGetX(const NativeP256Keypair&, P256Coordinate, endian) {
  return OkStatus();
}

Status DoGetX(const NativeP256PublicKey&, P256Coordinate, endian) {
  return OkStatus();
}

Status DoGetY(const NativeP256Keypair&, P256Coordinate, endian) {
  return OkStatus();
}

Status DoGetY(const NativeP256PublicKey&, P256Coordinate, endian) {
  return OkStatus();
}

Status DoGenerate(NativeP256Keypair&) { return OkStatus(); }

Status DoImport(NativeP256Keypair&,
                P256PrivateKey,
                P256ConstCoordinate,
                P256ConstCoordinate,
                endian) {
  return OkStatus();
}

Status DoImport(NativeP256PublicKey&,
                P256ConstCoordinate,
                P256ConstCoordinate,
                endian) {
  return OkStatus();
}

Status ComputeDiffieHellman(const NativeP256Keypair&,
                            const NativeP256PublicKey&,
                            P256DhKey) {
  return OkStatus();
}

}  // namespace pw::crypto::ecdh::backend
