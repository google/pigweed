
// Copyright 2022 The Pigweed Authors
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

#include "pw_bytes/packed_ptr.h"

#include <cstdint>

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace {

template <size_t kAlign>
struct alignas(kAlign) Aligned {
  uint16_t val_ = 0;
};

TEST(PackedPtrTest, NumBits) {
  EXPECT_EQ(pw::PackedPtr<Aligned<2>>::NumBits(), 1U);
  EXPECT_EQ(pw::PackedPtr<Aligned<4>>::NumBits(), 2U);
  EXPECT_EQ(pw::PackedPtr<Aligned<8>>::NumBits(), 3U);
  EXPECT_EQ(pw::PackedPtr<Aligned<16>>::NumBits(), 4U);
  EXPECT_EQ(pw::PackedPtr<Aligned<32>>::NumBits(), 5U);
  EXPECT_EQ(pw::PackedPtr<Aligned<64>>::NumBits(), 6U);
  EXPECT_EQ(pw::PackedPtr<Aligned<128>>::NumBits(), 7U);
  EXPECT_EQ(pw::PackedPtr<Aligned<256>>::NumBits(), 8U);
}

#if PW_NC_TEST(NumBits_AlignmentIsOne)
PW_NC_EXPECT("Alignment must be more than one to pack any bits");
TEST(PackedPtrTest, NumBits_Char) {
  EXPECT_EQ(pw::PackedPtr<char>::NumBits(), 0U);
}
#elif PW_NC_TEST(Construct_AlignmentIsOne)
PW_NC_EXPECT("Alignment must be more than one to pack any bits");
[[maybe_unused]] pw::PackedPtr<std::byte> bad;
#endif  // PW_NC_TEST

TEST(PackedPtrTest, Construct_Default) {
  pw::PackedPtr<Aligned<4>> ptr;
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_EQ(ptr.packed_value(), 0U);
}

TEST(PackedPtrTest, Construct_FromArgs) {
  Aligned<16> obj;
  pw::PackedPtr ptr(&obj, 1);
  EXPECT_EQ(ptr.get(), &obj);
  EXPECT_EQ(ptr.packed_value(), 1U);
}

TEST(PackedPtrTest, Construct_Copy) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 2);
  pw::PackedPtr ptr2(ptr1);
  EXPECT_EQ(ptr1.get(), &obj);
  EXPECT_EQ(ptr1.packed_value(), 2U);
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 2U);
}

TEST(PackedPtrTest, Construct_Copy_AddConst) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 2);
  pw::PackedPtr<const Aligned<16>> ptr2(ptr1);
  EXPECT_EQ(ptr1.get(), &obj);
  EXPECT_EQ(ptr1.packed_value(), 2U);
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 2U);
}

TEST(PackedPtrTest, Construct_Move) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 3);
  pw::PackedPtr ptr2(std::move(ptr1));
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_EQ(ptr1.packed_value(), 0U);
  // NOLINTEND(bugprone-use-after-move)
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 3U);
}

TEST(PackedPtrTest, Construct_Move_AddConst) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 3);
  pw::PackedPtr<const Aligned<16>> ptr2(std::move(ptr1));
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_EQ(ptr1.packed_value(), 0U);
  // NOLINTEND(bugprone-use-after-move)
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 3U);
}

// Ensure we can create PackedPtrs to types that include PackedPtrs to
// themsevles.
struct Recursive {
  size_t field_ = 0;
  pw::PackedPtr<Recursive> ptr_;
};

TEST(PackedPtrTest, Construct_Recursive) {
  pw::PackedPtr<Recursive> ptr;
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_EQ(ptr.packed_value(), 0U);
}

TEST(PackedPtrTest, Copy) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 4);
  pw::PackedPtr ptr2 = ptr1;
  EXPECT_EQ(ptr1.get(), &obj);
  EXPECT_EQ(ptr1.packed_value(), 4U);
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 4U);
}

TEST(PackedPtrTest, Copy_AddConst) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 4);
  pw::PackedPtr<const Aligned<16>> ptr2 = ptr1;
  EXPECT_EQ(ptr1.get(), &obj);
  EXPECT_EQ(ptr1.packed_value(), 4U);
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 4U);
}

TEST(PackedPtrTest, Move) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 6);
  pw::PackedPtr ptr2 = std::move(ptr1);
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_EQ(ptr1.packed_value(), 0U);
  // NOLINTEND(bugprone-use-after-move)
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 6U);
}

TEST(PackedPtrTest, Move_AddConst) {
  Aligned<16> obj;
  pw::PackedPtr ptr1(&obj, 6);
  pw::PackedPtr<const Aligned<16>> ptr2 = std::move(ptr1);
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_EQ(ptr1.packed_value(), 0U);
  // NOLINTEND(bugprone-use-after-move)
  EXPECT_EQ(ptr2.get(), &obj);
  EXPECT_EQ(ptr2.packed_value(), 6U);
}

TEST(PackedPtrTest, Dereference) {
  Aligned<4> obj{1u};
  pw::PackedPtr ptr(&obj, 0);
  EXPECT_EQ((*ptr).val_, 1u);
}

TEST(PackedPtrTest, Dereference_Const) {
  Aligned<4> obj{22u};
  const pw::PackedPtr ptr(&obj, 0);
  EXPECT_EQ((*ptr).val_, 22u);
}

TEST(PackedPtrTest, StructureDereference) {
  Aligned<4> obj{333u};
  pw::PackedPtr ptr(&obj, 0);
  EXPECT_EQ(ptr->val_, 333u);
}

TEST(PackedPtrTest, StructureDereference_Const) {
  Aligned<4> obj{4444u};
  const pw::PackedPtr ptr(&obj, 0);
  EXPECT_EQ(ptr->val_, 4444u);
}

}  // namespace
