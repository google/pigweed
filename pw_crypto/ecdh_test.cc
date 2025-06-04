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

#include "pw_unit_test/framework.h"
#include "pw_unit_test/status_macros.h"

namespace pw::crypto::ecdh {
namespace {

TEST(EcdhP256, Build) {
  constexpr static std::array<std::byte, kP256CoordSize> kXValue{};
  constexpr static std::array<std::byte, kP256CoordSize> kYValue{};

  std::array<std::byte, kP256CoordSize> x{};
  std::array<std::byte, kP256CoordSize> y{};

  std::array<std::byte, kP256DiffieHellmanKeySize> dh_key;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto public_key, P256PublicKey::Import(kXValue, kYValue, endian::big));
  PW_TEST_EXPECT_OK(public_key.GetX(x, endian::big));
  PW_TEST_EXPECT_OK(public_key.GetY(y, endian::big));

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto keypair, P256Keypair::Generate());
  PW_TEST_EXPECT_OK(keypair.GetX(x, endian::big));
  PW_TEST_EXPECT_OK(keypair.GetY(y, endian::big));
  PW_TEST_EXPECT_OK(keypair.ComputeDiffieHellman(public_key, dh_key));
}

}  // namespace
}  // namespace pw::crypto::ecdh
