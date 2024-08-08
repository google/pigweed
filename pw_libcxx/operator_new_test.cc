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

#include <new>

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

void CheckNonNull(void* ptr) {
  EXPECT_NE(ptr, nullptr);
  ::operator delete(ptr);
}

void CheckNonNullArray(void* ptr) {
  EXPECT_NE(ptr, nullptr);
  ::operator delete[](ptr);
}

void CheckNonNullWithAlignment(void* ptr, std::align_val_t alignment) {
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % static_cast<size_t>(alignment),
            size_t{0});
  ::operator delete(ptr, alignment);
}

void CheckNonNullArrayWithAlignment(void* ptr, std::align_val_t alignment) {
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % static_cast<size_t>(alignment),
            size_t{0});
  ::operator delete[](ptr, alignment);
}

TEST(OperatorNew, CallAllNews) {
  constexpr std::align_val_t kAlignment{16};
  constexpr size_t kSize{16};
  char kBuff[kSize];

  // Replaceable allocation functions
  CheckNonNull(::operator new(kSize));
  CheckNonNullArray(::operator new[](kSize));
  CheckNonNullWithAlignment(::operator new(kSize, kAlignment), kAlignment);
  CheckNonNullArrayWithAlignment(::operator new[](kSize, kAlignment),
                                 kAlignment);

  // Replaceable non-throwing allocation functions
  CheckNonNull(::operator new(kSize, std::nothrow));
  CheckNonNullArray(::operator new[](kSize, std::nothrow));
  CheckNonNullWithAlignment(::operator new(kSize, kAlignment, std::nothrow),
                            kAlignment);
  CheckNonNullArrayWithAlignment(
      ::operator new[](kSize, kAlignment, std::nothrow), kAlignment);

  // Non-allocating placement allocation functions
  EXPECT_EQ(::operator new(kSize, kBuff), kBuff);
  EXPECT_EQ(::operator new[](kSize, kBuff), kBuff);
}

}  // namespace
}  // namespace pw
