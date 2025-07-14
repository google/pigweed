// Copyright 2020 The Pigweed Authors
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

#include "pw_bytes/endian.h"

#include <array>
#include <cstddef>

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace pw::bytes {
namespace {

constexpr endian kNonNative =
    (endian::native == endian::little) ? endian::big : endian::little;

PW_CONSTEXPR_TEST(ConvertOrder, ToNativeUnsigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, uint8_t{0x12}),
                    uint8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, uint16_t{0x0011}),
                    uint16_t{0x0011});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, uint32_t{0x33221100}),
                    uint32_t{0x33221100});
  PW_TEST_EXPECT_EQ(
      ConvertOrderTo(endian::native, uint64_t{0x0011223344556677}),
      uint64_t{0x0011223344556677});
});

PW_CONSTEXPR_TEST(ConvertOrder, ToNativeSigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, int8_t{0x12}), int8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, int16_t{0x0011}),
                    int16_t{0x0011});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, int32_t{0x33221100}),
                    int32_t{0x33221100});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(endian::native, int64_t{0x0011223344556677}),
                    int64_t{0x0011223344556677});
});

PW_CONSTEXPR_TEST(ConvertOrder, FromNativeUnsigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, uint8_t{0x12}),
                    uint8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, uint16_t{0x0011}),
                    uint16_t{0x0011});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, uint32_t{0x33221100}),
                    uint32_t{0x33221100});
  PW_TEST_EXPECT_EQ(
      ConvertOrderFrom(endian::native, uint64_t{0x0011223344556677}),
      uint64_t{0x0011223344556677});
});

PW_CONSTEXPR_TEST(ConvertOrder, FromNativeSigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, int8_t{0x12}),
                    int8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, int16_t{0x0011}),
                    int16_t{0x0011});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(endian::native, int32_t{0x33221100}),
                    int32_t{0x33221100});
  PW_TEST_EXPECT_EQ(
      ConvertOrderFrom(endian::native, int64_t{0x0011223344556677}),
      int64_t{0x0011223344556677});
});

PW_CONSTEXPR_TEST(ConvertOrder, ToNonNativeUnsigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, uint8_t{0x12}), uint8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, uint16_t{0x0011}),
                    uint16_t{0x1100});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, uint32_t{0x33221100}),
                    uint32_t{0x00112233});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, uint64_t{0x0011223344556677}),
                    uint64_t{0x7766554433221100});
});

PW_CONSTEXPR_TEST(ConvertOrder, ToNonNativeSigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, int8_t{0x12}), int8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, int16_t{0x0011}),
                    int16_t{0x1100});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, int32_t{0x33221100}),
                    int32_t{0x00112233});
  PW_TEST_EXPECT_EQ(ConvertOrderTo(kNonNative, int64_t{0x0011223344556677}),
                    int64_t{0x7766554433221100});
});

PW_CONSTEXPR_TEST(ConvertOrder, FromNonNativeUnsigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, uint8_t{0x12}), uint8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, uint16_t{0x0011}),
                    uint16_t{0x1100});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, uint32_t{0x33221100}),
                    uint32_t{0x00112233});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, uint64_t{0x0011223344556677}),
                    uint64_t{0x7766554433221100});
});

PW_CONSTEXPR_TEST(ConvertOrder, FromNonNativeSigned, {
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, int8_t{0x12}), int8_t{0x12});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, int16_t{0x0011}),
                    int16_t{0x1100});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, int32_t{0x33221100}),
                    int32_t{0x00112233});
  PW_TEST_EXPECT_EQ(ConvertOrderFrom(kNonNative, int64_t{0x0011223344556677}),
                    int64_t{0x7766554433221100});
});

template <typename T, typename U>
constexpr bool Equal(const T& lhs, const U& rhs) {
  if (sizeof(lhs) != sizeof(rhs) || std::size(lhs) != std::size(rhs)) {
    return false;
  }

  for (size_t i = 0; i < std::size(lhs); ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }

  return true;
}

PW_CONSTEXPR_TEST(CopyInOrder, 8bitLittle, {
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::little, '?'), Array<'?'>()));
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::little, uint8_t{0x10}), Array<0x10>()));
  PW_TEST_EXPECT_TRUE(Equal(
      CopyInOrder(endian::little, static_cast<int8_t>(0x10)), Array<0x10>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 8bitBig, {
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::big, '?'), Array<'?'>()));
  PW_TEST_EXPECT_TRUE(Equal(
      CopyInOrder(endian::big, static_cast<uint8_t>(0x10)), Array<0x10>()));
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::big, static_cast<int8_t>(0x10)),
                            Array<0x10>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 16bitLittle, {
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::little, uint16_t{0xAB12}),
                            Array<0x12, 0xAB>()));
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::little, static_cast<int16_t>(0xAB12)),
            Array<0x12, 0xAB>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 16bitBig, {
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::big, uint16_t{0xAB12}), Array<0xAB, 0x12>()));
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::big, static_cast<int16_t>(0xAB12)),
            Array<0xAB, 0x12>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 32bitLittle, {
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::little, uint32_t{0xAABBCCDD}),
                            Array<0xDD, 0xCC, 0xBB, 0xAA>()));
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::little, static_cast<int32_t>(0xAABBCCDD)),
            Array<0xDD, 0xCC, 0xBB, 0xAA>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 32bitBig, {
  PW_TEST_EXPECT_TRUE(Equal(CopyInOrder(endian::big, uint32_t{0xAABBCCDD}),
                            Array<0xAA, 0xBB, 0xCC, 0xDD>()));
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::big, static_cast<int32_t>(0xAABBCCDD)),
            Array<0xAA, 0xBB, 0xCC, 0xDD>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 64bitLittle, {
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::little, uint64_t{0xAABBCCDD11223344}),
            Array<0x44, 0x33, 0x22, 0x11, 0xDD, 0xCC, 0xBB, 0xAA>()));
  PW_TEST_EXPECT_TRUE(Equal(
      CopyInOrder(endian::little, static_cast<int64_t>(0xAABBCCDD11223344ull)),
      Array<0x44, 0x33, 0x22, 0x11, 0xDD, 0xCC, 0xBB, 0xAA>()));
});

PW_CONSTEXPR_TEST(CopyInOrder, 64bitBig, {
  PW_TEST_EXPECT_TRUE(
      Equal(CopyInOrder(endian::big, uint64_t{0xAABBCCDD11223344}),
            Array<0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44>()));
  PW_TEST_EXPECT_TRUE(Equal(
      CopyInOrder(endian::big, static_cast<int64_t>(0xAABBCCDD11223344ull)),
      Array<0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44>()));
});

constexpr const char* kNumber = "\x11\x22\x33\x44\xaa\xbb\xcc\xdd";

TEST(ReadInOrder, 8Bit_Big) {
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::big, "\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::big, "\x80"), 0x80u);
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::big, kNumber), 0x11u);

  EXPECT_EQ(ReadInOrder<int8_t>(endian::big, "\0"), 0);
  EXPECT_EQ(ReadInOrder<int8_t>(endian::big, "\x80"), -128);
  EXPECT_EQ(ReadInOrder<int8_t>(endian::big, kNumber), 0x11);
}

TEST(ReadInOrder, 8Bit_Little) {
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::little, "\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::little, "\x80"), 0x80u);
  EXPECT_EQ(ReadInOrder<uint8_t>(endian::little, kNumber), 0x11u);

  EXPECT_EQ(ReadInOrder<int8_t>(endian::little, "\0"), 0);
  EXPECT_EQ(ReadInOrder<int8_t>(endian::little, "\x80"), -128);
  EXPECT_EQ(ReadInOrder<int8_t>(endian::little, kNumber), 0x11);
}

TEST(ReadInOrder, 16Bit_Big) {
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::big, "\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::big, "\x80\0"), 0x8000u);
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::big, kNumber), 0x1122u);

  EXPECT_EQ(ReadInOrder<int16_t>(endian::big, "\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int16_t>(endian::big, "\x80\0"), -32768);
  EXPECT_EQ(ReadInOrder<int16_t>(endian::big, kNumber), 0x1122);
}

TEST(ReadInOrder, 16Bit_Little) {
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::little, "\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::little, "\x80\0"), 0x80u);
  EXPECT_EQ(ReadInOrder<uint16_t>(endian::little, kNumber), 0x2211u);

  EXPECT_EQ(ReadInOrder<int16_t>(endian::little, "\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int16_t>(endian::little, "\x80\0"), 0x80);
  EXPECT_EQ(ReadInOrder<int16_t>(endian::little, kNumber), 0x2211);
}

TEST(ReadInOrder, 32Bit_Big) {
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::big, "\0\0\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::big, "\x80\0\0\0"), 0x80000000u);
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::big, kNumber), 0x11223344u);

  EXPECT_EQ(ReadInOrder<int32_t>(endian::big, "\0\0\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int32_t>(endian::big, "\x80\0\0\0"), -2147483648);
  EXPECT_EQ(ReadInOrder<int32_t>(endian::big, kNumber), 0x11223344);
}

TEST(ReadInOrder, 32Bit_Little) {
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::little, "\0\0\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::little, "\x80\0\0\0"), 0x80u);
  EXPECT_EQ(ReadInOrder<uint32_t>(endian::little, kNumber), 0x44332211u);

  EXPECT_EQ(ReadInOrder<int32_t>(endian::little, "\0\0\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int32_t>(endian::little, "\x80\0\0\0"), 0x80);
  EXPECT_EQ(ReadInOrder<int32_t>(endian::little, kNumber), 0x44332211);
}

TEST(ReadInOrder, 64Bit_Big) {
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::big, "\0\0\0\0\0\0\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::big, "\x80\0\0\0\0\0\0\0"),
            0x80000000'00000000llu);
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::big, kNumber), 0x11223344AABBCCDDu);

  EXPECT_EQ(ReadInOrder<int64_t>(endian::big, "\0\0\0\0\0\0\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int64_t>(endian::big, "\x80\0\0\0\0\0\0\0"),
            static_cast<int64_t>(1llu << 63));
  EXPECT_EQ(ReadInOrder<int64_t>(endian::big, kNumber), 0x11223344AABBCCDD);
}

TEST(ReadInOrder, 64Bit_Little) {
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::little, "\0\0\0\0\0\0\0\0"), 0u);
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::little, "\x80\0\0\0\0\0\0\0"), 0x80u);
  EXPECT_EQ(ReadInOrder<uint64_t>(endian::little, kNumber),
            0xDDCCBBAA44332211u);

  EXPECT_EQ(ReadInOrder<int64_t>(endian::little, "\0\0\0\0\0\0\0\0"), 0);
  EXPECT_EQ(ReadInOrder<int64_t>(endian::little, "\x80\0\0\0\0\0\0\0"), 0x80);
  EXPECT_EQ(ReadInOrder<int64_t>(endian::little, kNumber),
            static_cast<int64_t>(0xDDCCBBAA44332211));
}

TEST(ReadInOrder, StdArray) {
  std::array<std::byte, 4> buffer = Array<1, 2, 3, 4>();
  EXPECT_EQ(0x04030201, ReadInOrder<int32_t>(endian::little, buffer));
  EXPECT_EQ(0x01020304, ReadInOrder<int32_t>(endian::big, buffer));
}

TEST(ReadInOrder, CArray) {
  char buffer[5] = {1, 2, 3, 4, 99};
  EXPECT_EQ(0x04030201, ReadInOrder<int32_t>(endian::little, buffer));
  EXPECT_EQ(0x01020304, ReadInOrder<int32_t>(endian::big, buffer));
}

TEST(ReadInOrder, BoundsChecking_Ok) {
  constexpr auto buffer = Array<1, 2, 3, 4>();
  uint16_t value = 0;
  EXPECT_TRUE(ReadInOrder(endian::little, buffer, value));
  EXPECT_EQ(0x0201, value);
}

TEST(ReadInOrder, BoundsChecking_TooSmall) {
  constexpr auto buffer = Array<1, 2, 3>();
  int32_t value = 0;
  EXPECT_FALSE(ReadInOrder(endian::little, buffer, value));
  EXPECT_EQ(0, value);
}

TEST(ReadInOrder, PartialLittleEndian) {
  constexpr auto buffer = Array<1, 2, 3, 4>();

  EXPECT_EQ(0x00000000, ReadInOrder<int32_t>(endian::little, buffer.data(), 0));
  EXPECT_EQ(0x00000001, ReadInOrder<int32_t>(endian::little, buffer.data(), 1));
  EXPECT_EQ(0x00000201, ReadInOrder<int32_t>(endian::little, buffer.data(), 2));
  EXPECT_EQ(0x00030201, ReadInOrder<int32_t>(endian::little, buffer.data(), 3));
  EXPECT_EQ(0x04030201, ReadInOrder<int32_t>(endian::little, buffer.data(), 4));
  EXPECT_EQ(0x04030201, ReadInOrder<int32_t>(endian::little, buffer.data(), 5));
  EXPECT_EQ(0x04030201,
            ReadInOrder<int32_t>(endian::little, buffer.data(), 100));
}

TEST(ReadInOrder, PartialBigEndian) {
  constexpr auto buffer = Array<1, 2, 3, 4>();

  EXPECT_EQ(0x00000000, ReadInOrder<int32_t>(endian::big, buffer.data(), 0));
  EXPECT_EQ(0x01000000, ReadInOrder<int32_t>(endian::big, buffer.data(), 1));
  EXPECT_EQ(0x01020000, ReadInOrder<int32_t>(endian::big, buffer.data(), 2));
  EXPECT_EQ(0x01020300, ReadInOrder<int32_t>(endian::big, buffer.data(), 3));
  EXPECT_EQ(0x01020304, ReadInOrder<int32_t>(endian::big, buffer.data(), 4));
  EXPECT_EQ(0x01020304, ReadInOrder<int32_t>(endian::big, buffer.data(), 5));
  EXPECT_EQ(0x01020304, ReadInOrder<int32_t>(endian::big, buffer.data(), 100));
}

}  // namespace
}  // namespace pw::bytes
