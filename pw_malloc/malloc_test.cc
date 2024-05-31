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

#include "pw_unit_test/framework.h"

namespace {

class MallocTest : public ::testing::Test {
 protected:
  void SetUp() override { pw::malloc::InitSystemAllocator(heap_); }

 private:
  std::array<std::byte, 1024> heap_;
};

TEST_F(MallocTest, MallocFree) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);

  void* ptr = malloc(kSize);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kSize);

  free(ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, NewDelete) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);

  auto* ptr = new std::array<std::byte, kSize>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_GE(system_metrics.allocated_bytes.value(), kSize);

  delete (ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, CallocFree) {
  constexpr size_t kNum = 4;
  constexpr size_t kSize = 64;
  auto& system_metrics = pw::malloc::GetSystemMetrics();
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);

  void* ptr = calloc(kNum, kSize);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(system_metrics.requested_bytes.value(), kNum * kSize);

  free(ptr);
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);
}

TEST_F(MallocTest, ReallocFree) {
  constexpr size_t kSize = 256;
  auto& system_metrics = pw::malloc::GetSystemMetrics();
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);

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
  EXPECT_EQ(system_metrics.allocated_bytes.value(), 0U);

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

}  // namespace
