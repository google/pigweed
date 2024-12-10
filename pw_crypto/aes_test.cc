// Copyright 2024 The Pigweed Authors
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

#include "pw_crypto/aes.h"

#include <algorithm>
#include <iterator>

#include "pw_assert/assert.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

#define EXPECT_OK(expr) EXPECT_EQ(pw::OkStatus(), (expr))
#define STR_TO_BYTES(str) (as_bytes(span(str).subspan<0, sizeof(str) - 1>()))

namespace pw::crypto::aes {
namespace {

using backend::AesOperation;
using backend::SupportedKeySize;
using internal::BackendSupports;
using unsafe::aes::EncryptBlock;

template <typename T>
void ZeroOut(T& container) {
  std::fill(std::begin(container), std::end(container), std::byte{0});
}

// Create a view (`span<const T>`, as opposed to a `span<T>`) of a `pw::Vector`.
template <typename T>
span<const T> View(pw::Vector<T>& vector) {
  return span(vector.begin(), vector.end());
}

template <typename T, size_t N>
std::array<typename std::remove_cv<T>::type, N> SpanToArray(
    const span<T, N>& span) {
  static_assert(N != dynamic_extent, "Must have statically-known size.");
  static_assert(std::is_trivially_copyable<T>::value, "Must be memcpy-able.");

  std::array<typename std::remove_cv<T>::type, N> result;
  std::memcpy(result.data(), span.data(), sizeof(result));

  return result;
}

template <typename T>
auto ToArray(const T& container) {
  return SpanToArray(span(container));
}

// Intentionally chosen to not be a valid AES key size, but larger than the
// largest AES key size.
constexpr size_t kMaxVectorSize = 503;

TEST(Aes, UnsafeEncryptApi) {
  constexpr auto kRawEncryptBlockOp = AesOperation::kUnsafeEncryptBlock;
  ConstBlockSpan message_block = STR_TO_BYTES("hello, world!\0\0\0");

  // Ensure various output types work correctly.
  std::byte encrypted_carr[16];
  std::array<std::byte, 16> encrypted_arr;
  Block encrypted_block;

  // Ensure dynamically-sized keys will work.
  Vector<std::byte, kMaxVectorSize> dynamic_key;

  auto reset = [&] {
    ZeroOut(dynamic_key);
    ZeroOut(encrypted_carr);
    ZeroOut(encrypted_arr);
    ZeroOut(encrypted_block);

    dynamic_key.clear();
  };

  if constexpr (BackendSupports<kRawEncryptBlockOp>(SupportedKeySize::k128)) {
    span<const std::byte, 16> key = STR_TO_BYTES(
        "\x13\xA2\x27\x93\x8D\x1D\x89\x46\x07\x4C\xA0\x71\xF2\xF7\x54\xC5");
    Block expected = SpanToArray(STR_TO_BYTES(
        "\xC0\x9A\x54\x34\xFD\xB8\xB4\x37\xAD\x84\x67\x60\x79\x8D\xCE\x40"));

    reset();
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_carr));
    EXPECT_EQ(ToArray(encrypted_carr), expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_arr));
    EXPECT_EQ(encrypted_arr, expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);

    reset();
    std::copy(key.begin(), key.end(), std::back_inserter(dynamic_key));
    EXPECT_OK(EncryptBlock(View(dynamic_key), message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);
  }

  if constexpr (BackendSupports<kRawEncryptBlockOp>(SupportedKeySize::k192)) {
    span<const std::byte, 24> key = STR_TO_BYTES(
        "\x2B\x43\x70\x51\xBF\x91\xF0\xFD\x4E\x9B\x89\xB7\x35\x40\xD4\x1B"
        "\x15\xBC\xD7\xC2\x22\xBC\x03\x76");
    Block expected = SpanToArray(STR_TO_BYTES(
        "\x35\x45\xC1\xA5\x89\x73\x1F\x28\x2E\x92\xAC\x24\x37\x85\xFC\xCA"));

    reset();
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_carr));
    EXPECT_EQ(ToArray(encrypted_carr), expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_arr));
    EXPECT_EQ(encrypted_arr, expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);

    reset();
    std::copy(key.begin(), key.end(), std::back_inserter(dynamic_key));
    EXPECT_OK(EncryptBlock(View(dynamic_key), message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);
  }

  if constexpr (BackendSupports<kRawEncryptBlockOp>(SupportedKeySize::k256)) {
    span<const std::byte, 32> key = STR_TO_BYTES(
        "\xA4\xB9\x15\x76\xF2\x16\x67\xB0\x33\x5E\xA6\x8D\xBD\x23\xDF\x29"
        "\x84\xBF\x8D\xBE\x56\x77\x13\x28\x14\x55\xD9\x75\xDD\xEE\x4E\x0B");
    Block expected = SpanToArray(STR_TO_BYTES(
        "\x9B\xC4\x12\x39\xB7\x2A\xA1\x14\xB3\x6E\x6C\xAE\x2C\x7f\xDD\xE7"));

    reset();
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_carr));
    EXPECT_EQ(ToArray(encrypted_carr), expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_arr));
    EXPECT_EQ(encrypted_arr, expected);
    EXPECT_OK(EncryptBlock(key, message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);

    reset();
    std::copy(key.begin(), key.end(), std::back_inserter(dynamic_key));
    EXPECT_OK(EncryptBlock(View(dynamic_key), message_block, encrypted_block));
    EXPECT_EQ(encrypted_block, expected);
  }
}

}  // namespace
}  // namespace pw::crypto::aes
