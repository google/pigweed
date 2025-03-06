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

#include "pw_malloc/malloc.h"

#include <cstdint>

#include "pw_tokenizer/detokenize.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::string_view_literals;

std::array<std::byte, 8192> system_heap;

class MallocTest : public ::pw::unit_test::internal::Test {
 public:
  // NOTE! This tests should only be run WITHOUT a pw_malloc backend set.
  // Otherwise, `InitSystemAllocator` may have already been called and may
  // crash.
  static void SetUpTestSuite() { pw::malloc::InitSystemAllocator(system_heap); }

 protected:
  // NOTE!! This tests ONLY run on host with the "light" framework. Other
  // test frameworks may attempt and fail to de/allocate outside the test method
  // outside the test body.
  void SetUp() override {
    auto& system_metrics = pw::malloc::GetSystemMetrics();
    ASSERT_EQ(system_metrics.allocated_bytes.value(), 0U);
  }

  void TearDown() override {
    auto& system_metrics = pw::malloc::GetSystemMetrics();
    ASSERT_EQ(system_metrics.allocated_bytes.value(), 0U);
  }
};

TEST_F(MallocTest, MallocFree) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();

  void* ptr = malloc(kSize);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kSize);

  free(ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, NewDelete) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();

  auto* ptr = new std::array<std::byte, kSize>();
  // Prevent elision of the allocation.
  pw::test::DoNotOptimize(ptr);
  ASSERT_NE(ptr, nullptr);
  EXPECT_GE(system_metrics.allocated_bytes.value(), kSize);

  delete (ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, CallocFree) {
  constexpr size_t kNum = 4;
  constexpr size_t kSize = 64;
  auto& system_metrics = pw::malloc::GetSystemMetrics();

  void* ptr = calloc(kNum, kSize);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kNum * kSize);

  free(ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, ReallocFree) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();

  void* ptr = realloc(nullptr, kSize);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kSize);

  free(ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, MallocReallocFree) {
  constexpr size_t kSize1 = 256;
  constexpr size_t kSize2 = 512;
  auto& system_metrics = pw::malloc::GetSystemMetrics();

  void* ptr = malloc(kSize1);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kSize1);
  std::memset(ptr, 1, kSize1);

  void* new_ptr = realloc(ptr, kSize2);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kSize2);

  // Using `new_ptr` prevents the call to `realloc from being optimized away.
  auto* bytes = std::launder(reinterpret_cast<uint8_t*>(new_ptr));
  for (size_t i = 0; i < kSize1; ++i) {
    EXPECT_EQ(bytes[i], 1U);
  }

  free(new_ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

// This test mimics pw_tokenizer//detokenize_test.cc in order to perform memory
// allocations as a result of manipulating a std::unordered_map.
// See also b/345526413 for the failure that motivated this test.
TEST_F(MallocTest, Detokenize) {
  static constexpr char kTestDatabase[] =
      "TOKENS\0\0"
      "\x06\x00\x00\x00"  // Number of tokens in this database.
      "\0\0\0\0"
      "\x01\x00\x00\x00----"
      "\x05\x00\x00\x00----"
      "\xFF\x00\x00\x00----"
      "\xFF\xEE\xEE\xDD----"
      "\xEE\xEE\xEE\xEE----"
      "\x9D\xA7\x97\xF8----"
      "One\0"
      "TWO\0"
      "333\0"
      "FOUR\0"
      "$AQAAAA==\0"
      "■msg♦This is $AQAAAA== message■module♦■file♦file.txt";
  pw::tokenizer::Detokenizer detok(
      pw::tokenizer::TokenDatabase::Create<kTestDatabase>());
  EXPECT_EQ(detok.Detokenize("\1\0\0\0"sv).BestString(), "One");
  EXPECT_EQ(detok.Detokenize("\5\0\0\0"sv).BestString(), "TWO");
  EXPECT_EQ(detok.Detokenize("\xff\x00\x00\x00"sv).BestString(), "333");
  EXPECT_EQ(detok.Detokenize("\xff\xee\xee\xdd"sv).BestString(), "FOUR");
}

}  // namespace
