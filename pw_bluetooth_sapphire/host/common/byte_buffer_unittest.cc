// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

#include <cstddef>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace {

TEST(ByteBufferTest, StaticByteBuffer) {
  constexpr size_t kBufferSize = 5;
  StaticByteBuffer<kBufferSize> buffer;

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();
  buffer[3] = 3;

  constexpr std::array<uint8_t, kBufferSize> kExpected{{0x00, 0x00, 0x00, 0x03, 0x00}};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));

  // Moving will result in a copy.
  StaticByteBuffer<kBufferSize> buffer_copy =
      std::move(buffer);                  // NOLINT(performance-move-const-arg)
  EXPECT_EQ(kBufferSize, buffer.size());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(kBufferSize, buffer_copy.size());
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(ContainersEqual(kExpected, buffer_copy));

  // Copying should also copy.
  StaticByteBuffer<kBufferSize> buffer_copy1 = buffer;
  EXPECT_EQ(kBufferSize, buffer.size());
  EXPECT_EQ(kBufferSize, buffer_copy1.size());
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));
  EXPECT_TRUE(ContainersEqual(kExpected, buffer_copy1));
}

TEST(ByteBufferTest, StaticByteBufferVariadicConstructor) {
  constexpr size_t kBufferSize = 3;
  StaticByteBuffer<kBufferSize> buffer0;
  buffer0[0] = 0x01;
  buffer0[1] = 0x02;
  buffer0[2] = 0x03;

  StaticByteBuffer<kBufferSize> buffer1{0x01, 0x02, 0x03};
  auto buffer2 = CreateStaticByteBuffer(0x01, 0x02, 0x03);
  StaticByteBuffer buffer3{0x01, 0x02, 0x03};

  EXPECT_TRUE(ContainersEqual(buffer0, buffer1));
  EXPECT_TRUE(ContainersEqual(buffer0, buffer2));
  EXPECT_TRUE(ContainersEqual(buffer1, buffer2));
  EXPECT_TRUE(ContainersEqual(buffer1, buffer3));
}

TEST(ByteBufferTest, DynamicByteBuffer) {
  constexpr size_t kBufferSize = 5;
  DynamicByteBuffer buffer(kBufferSize);

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();
  buffer[3] = 3;

  constexpr std::array<uint8_t, kBufferSize> kExpected{{0x00, 0x00, 0x00, 0x03, 0x00}};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));

  // Moving will invalidate the source buffer.
  DynamicByteBuffer buffer_moved = std::move(buffer);
  EXPECT_EQ(0u, buffer.size());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(kBufferSize, buffer_moved.size());
  EXPECT_EQ(nullptr, buffer.data());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(ContainersEqual(kExpected, buffer_moved));
}

TEST(ByteBufferTest, DynamicByteBufferZeroSize) {
  DynamicByteBuffer buffer;

  EXPECT_EQ(0u, buffer.size());
  EXPECT_EQ(nullptr, buffer.data());

  DynamicByteBuffer zerosize(0);

  EXPECT_EQ(0u, zerosize.size());
  EXPECT_EQ(nullptr, buffer.data());
}

TEST(ByteBufferTest, DynamicByteBufferConstructFromBuffer) {
  constexpr size_t kBufferSize = 3;
  StaticByteBuffer<kBufferSize> buffer(1, 2, 3);

  DynamicByteBuffer dyn_buffer(buffer);
  EXPECT_EQ(kBufferSize, dyn_buffer.size());
  EXPECT_TRUE(ContainersEqual(buffer, dyn_buffer));
}

TEST(ByteBufferTest, DynamicByteBufferExplicitCopy) {
  DynamicByteBuffer src(1);
  src[0] = 'a';

  DynamicByteBuffer dst(src);
  EXPECT_EQ(1u, dst.size());
  EXPECT_TRUE(ContainersEqual(src, dst));
}

TEST(ByteBufferTest, DynamicByteBufferConstructFromBytes) {
  constexpr size_t kBufferSize = 3;
  std::array<uint8_t, kBufferSize> kExpected{{0, 1, 2}};

  auto bytes = std::make_unique<uint8_t[]>(kBufferSize);
  std::memcpy(bytes.get(), kExpected.data(), kBufferSize);

  DynamicByteBuffer buffer(kBufferSize, std::move(bytes));
  EXPECT_EQ(nullptr, bytes.get());
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));
}

TEST(ByteBufferTest, BufferViewTest) {
  constexpr size_t kBufferSize = 5;
  DynamicByteBuffer buffer(kBufferSize);

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();

  BufferView view(buffer);
  EXPECT_EQ(0x00, buffer[0]);
  EXPECT_EQ(0x00, view[0]);
  EXPECT_EQ(kBufferSize, buffer.size());
  EXPECT_EQ(kBufferSize, view.size());
}

TEST(ByteBufferTest, BufferViewFromVector) {
  const std::vector<uint8_t> kData{{1, 2, 3}};
  BufferView view(kData);
  EXPECT_TRUE(ContainersEqual(kData, view));
}

TEST(ByteBufferTest, MutableBufferViewTest) {
  constexpr size_t kBufferSize = 5;
  constexpr size_t kViewSize = 3;
  DynamicByteBuffer buffer(kBufferSize);

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();

  MutableBufferView view(buffer.mutable_data(), kViewSize);
  view[0] = 0x01;
  EXPECT_EQ(0x01, buffer[0]);
  EXPECT_EQ(0x01, view[0]);
  EXPECT_EQ(kBufferSize, buffer.size());
  EXPECT_EQ(kViewSize, view.size());

  MutableBufferView view2(view);
  view2[0] = 0x00;
  EXPECT_EQ(0x00, buffer[0]);
  EXPECT_EQ(0x00, view[0]);
  EXPECT_EQ(0x00, view2[0]);
  EXPECT_EQ(kBufferSize, buffer.size());
  EXPECT_EQ(kViewSize, view.size());
}

TEST(ByteBufferTest, Copy) {
  auto buffer = CreateStaticByteBuffer('T', 'e', 's', 't');
  BufferView empty_buffer;

  // Create a large enough buffer.
  StaticByteBuffer<10> target_buffer;

  // Copying an empty buffer should copy 0 bytes.
  empty_buffer.Copy(&target_buffer);

  // Copy all of |buffer|. The first |buffer.size()| octets of |target_buffer|
  // should match the contents of |buffer|.
  buffer.Copy(&target_buffer);
  EXPECT_TRUE(ContainersEqual(buffer, BufferView(target_buffer, buffer.size())));

  // Copy one octet of |buffer| starting at index 2
  target_buffer.SetToZeros();
  buffer.Copy(&target_buffer, 1, 1);
  EXPECT_EQ(buffer[1], target_buffer[0]);

  // Zero the buffer and copy its contents for later comparison.
  target_buffer.SetToZeros();
  auto target_buffer_copy = target_buffer;

  // Copy all remaining octets starting just past the end of |buffer|. This
  // should copy zero bytes and |target_buffer| should remain unchanged.
  buffer.Copy(&target_buffer, buffer.size(), 0);
  EXPECT_TRUE(ContainersEqual(target_buffer_copy, target_buffer));

  // Copied range must remain within buffer limits
  EXPECT_DEATH_IF_SUPPORTED(buffer.Copy(&target_buffer, buffer.size() + 1, 0),
                            "offset exceeds source range");
  EXPECT_DEATH_IF_SUPPORTED(buffer.Copy(&target_buffer, 0, buffer.size() + 1),
                            "end exceeds source range");
  EXPECT_DEATH_IF_SUPPORTED(
      buffer.Copy(&target_buffer, buffer.size() / 2 + 1, buffer.size() / 2 + 1),
      "end exceeds source range");

  StaticByteBuffer<1> insufficient_target_buffer;
  EXPECT_DEATH_IF_SUPPORTED(buffer.Copy(&insufficient_target_buffer),
                            "destination not large enough");

  // Range calculation overflow is fatal rather than silently erroneous
  MutableBufferView bogus_target_buffer(target_buffer.mutable_data(),
                                        std::numeric_limits<size_t>::max());
  EXPECT_DEATH_IF_SUPPORTED(
      buffer.Copy(&bogus_target_buffer, 1, std::numeric_limits<size_t>::max()),
      "end of source range overflows size_t");
}

TEST(ByteBufferTest, View) {
  auto buffer = CreateStaticByteBuffer('T', 'e', 's', 't');
  BufferView empty_buffer;

  BufferView view = empty_buffer.view();
  EXPECT_EQ(0u, view.size());

  view = empty_buffer.view(0, 200);
  EXPECT_EQ(0u, view.size());

  view = buffer.view();
  EXPECT_EQ("Test", view.AsString());

  view = buffer.view(4);
  EXPECT_EQ(0u, view.size());

  view = buffer.view(1);
  EXPECT_EQ("est", view.AsString());

  view = buffer.view(1, 1);
  EXPECT_EQ("e", view.AsString());

  view = buffer.view(1, 2);
  EXPECT_EQ("es", view.AsString());
}

TEST(ByteBufferTest, MutableView) {
  auto buffer = CreateStaticByteBuffer('T', 'e', 's', 't');
  MutableBufferView empty_buffer;

  MutableBufferView view;
  view = empty_buffer.mutable_view();
  EXPECT_EQ(0u, view.size());

  view = empty_buffer.mutable_view(0, 200);
  EXPECT_EQ(0u, view.size());

  view = buffer.mutable_view();
  EXPECT_EQ("Test", view.AsString());

  view = buffer.mutable_view(4);
  EXPECT_EQ(0u, view.size());

  view = buffer.mutable_view(1);
  EXPECT_EQ("est", view.AsString());

  view = buffer.mutable_view(1, 1);
  EXPECT_EQ("e", view.AsString());

  view = buffer.mutable_view(1, 2);
  EXPECT_EQ("es", view.AsString());

  // MutableView can modify the underlying buffer.
  view[0] = 'E';
  EXPECT_EQ("Es", view.AsString());
  EXPECT_EQ("TEst", buffer.AsString());
}

TEST(ByteBufferTest, ByteBufferEqualityFail) {
  const auto kData0 = CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = CreateStaticByteBuffer('F', 'o', 'o');
  EXPECT_FALSE(kData0 == kData1);
}

TEST(ByteBufferTest, ByteBufferEqualitySuccess) {
  const auto kData0 = CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = CreateStaticByteBuffer('T', 'e', 's', 't');
  EXPECT_TRUE(kData0 == kData1);
}

TEST(ByteBufferTest, ByteBufferAsFundamental) {
  EXPECT_EQ(191u, CreateStaticByteBuffer(191, 25).As<uint8_t>());
}

TEST(ByteBufferTest, ByteBufferAsStruct) {
  const auto data = CreateStaticByteBuffer(10, 12);
  struct point {
    uint8_t x;
    uint8_t y;
  };
  EXPECT_EQ(10, data.As<point>().x);
  EXPECT_EQ(12, data.As<point>().y);
}

TEST(ByteBufferTest, ByteBufferAsArray) {
  const auto buf = CreateStaticByteBuffer(191, 25);
  const auto array = buf.As<uint8_t[2]>();
  EXPECT_EQ(191, array[0]);
  EXPECT_EQ(25, array[1]);
}

TEST(ByteBufferTest, ByteBufferReadMember) {
  struct [[gnu::packed]] Point {
    uint8_t x;
    const uint8_t y;
    const uint8_t array[2];
    char multi[2][1];
    uint8_t flex[];
  };

  StaticByteBuffer data(0x01, 0x02, 0x03, 0x37, 0x7f, 0x02, 0x45);
  auto x = data.ReadMember<&Point::x>();
  static_assert(std::is_same_v<uint8_t, decltype(x)>);
  EXPECT_EQ(data[offsetof(Point, x)], x);

  // Top-level const qualifier is removed
  auto y = data.ReadMember<&Point::y>();
  static_assert(std::is_same_v<uint8_t, decltype(y)>);
  EXPECT_EQ(data[offsetof(Point, y)], y);

  // Returned array elements are const just like in the original struct
  auto array = data.ReadMember<&Point::array>();
  static_assert(std::is_same_v<std::array<const uint8_t, 2>, decltype(array)>);
  EXPECT_THAT(array, ::testing::ElementsAre(0x03, 0x37));

  auto multi = data.ReadMember<&Point::multi>();
  static_assert(std::is_same_v<std::array<std::array<char, 1>, 2>, decltype(multi)>);
  EXPECT_THAT(multi, ::testing::ElementsAre(std::array{char{0x7f}}, std::array{char{0x02}}));
}

TEST(ByteBufferTest, ByteBufferReadMemberOfFixedArrayType) {
  struct [[gnu::packed]] Point {
    float f;
    int8_t coordinates[3];
    char multi[2][1];
  };

  StaticByteBuffer data(
      // f
      0, 0, 0, 0,

      // coordinates[3]
      0x01, 0x02, 0x03,

      // multi[2][1]
      0x37, 0x45);
  ASSERT_LE(sizeof(Point), data.size());
  EXPECT_DEATH_IF_SUPPORTED(data.ReadMember<&Point::coordinates>(3), "index past array bounds");

  auto view = data.view(0, 6);
  ASSERT_GT(sizeof(Point), view.size());
  EXPECT_DEATH_IF_SUPPORTED(view.ReadMember<&Point::coordinates>(0), "insufficient buffer");

  EXPECT_EQ(data[offsetof(Point, coordinates)], data.ReadMember<&Point::coordinates>(0));
  EXPECT_EQ(data[offsetof(Point, coordinates) + 1], data.ReadMember<&Point::coordinates>(1));

  // Elements of a multi-dimensional C array are returned as std::arrays
  auto inner = data.ReadMember<&Point::multi>(1);
  EXPECT_EQ(data[offsetof(Point, multi) + 1], inner.at(0));
}

TEST(ByteBufferTest, ByteBufferReadMemberOfStdArrayType) {
  struct [[gnu::packed]] Point {
    float f;
    std::array<int8_t, 3> coordinates;
  };

  StaticByteBuffer data(0, 0, 0, 0, 0x01, 0x02, 0x03, 0x37);
  ASSERT_LE(sizeof(Point), data.size());
  EXPECT_DEATH_IF_SUPPORTED(data.ReadMember<&Point::coordinates>(3), "index past array bounds");

  EXPECT_EQ(data[offsetof(Point, coordinates)], data.ReadMember<&Point::coordinates>(0));
  EXPECT_EQ(data[offsetof(Point, coordinates) + 1], data.ReadMember<&Point::coordinates>(1));
}

TEST(ByteBufferTest, ByteBufferReadMemberOfFlexibleArrayType) {
  struct [[gnu::packed]] Point {
    uint16_t dimensions;
    int8_t coordinates[];
  };

  StaticByteBuffer data(0, 0, 0x01, 0x02);
  ASSERT_LE(sizeof(Point), data.size());
  EXPECT_DEATH_IF_SUPPORTED(data.ReadMember<&Point::coordinates>(2), "end exceeds source range");

  EXPECT_EQ(data[offsetof(Point, coordinates)], data.ReadMember<&Point::coordinates>(0));
  EXPECT_EQ(data[offsetof(Point, coordinates) + 1], data.ReadMember<&Point::coordinates>(1));
}

TEST(ByteBufferTest, ByteBufferReadMemberOfUnalignedArrayType) {
  struct [[gnu::packed]] Point {
    int8_t byte;
    float f[1];
  } point;

  BufferView view(&point, sizeof(point));
  static_assert(alignof(decltype(view.ReadMember<&Point::f>(0))) == alignof(float));

  // This branch (that the second field of |point| is unaligned) does get taken in manual testing
  // but there's no way to guarantee it, so make it run-time conditional.
  if (reinterpret_cast<uintptr_t>(&point.f) % alignof(float) != 0) {
    // ByteBuffer::As is essentially a pointer into the buffer contents, which will give us
    // references that are not aligned to type requirements.
    const float& through_as = view.As<Point>().f[0];
    ASSERT_NE(0U, reinterpret_cast<uintptr_t>(&through_as) % alignof(float));
  }

  // The same cref binds to a temporary new object that ByteBuffer::ReadMember creates, so the
  // alignment is correct.
  const float& through_read_member = view.ReadMember<&Point::f>(0);
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(&through_read_member) % alignof(float));
}

TEST(ByteBufferTest, MutableByteBufferAsMutableFundamental) {
  auto data = CreateStaticByteBuffer(10, 12);
  ++data.AsMutable<uint8_t>();
  EXPECT_EQ(11, data[0]);
}

TEST(ByteBufferTest, MutableByteBufferAsMutableStruct) {
  auto data = CreateStaticByteBuffer(10, 12);
  struct point {
    uint8_t x;
    uint8_t y;
  };
  ++data.AsMutable<point>().x;
  ++data.AsMutable<point>().y;

  const auto expected_data = CreateStaticByteBuffer(11, 13);
  EXPECT_EQ(expected_data, data);
}

TEST(ByteBufferTest, MutableByteBufferAsMutableArray) {
  auto buf = CreateStaticByteBuffer(10, 12);
  auto array = buf.AsMutable<uint8_t[2]>();
  ++array[0];
  ++array[1];

  EXPECT_EQ(11, buf[0]);
  EXPECT_EQ(13, buf[1]);
}

TEST(ByteBufferTest, MutableByteBufferWrite) {
  const auto kData0 = CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = CreateStaticByteBuffer('F', 'o', 'o');

  auto buffer = CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  EXPECT_EQ("XXXXXXXX", buffer.AsString());

  buffer.Write(kData0);
  EXPECT_EQ("TestXXXX", buffer.AsString());

  // Write from raw pointer.
  buffer = CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  buffer.Write(kData0.data(), kData0.size());
  EXPECT_EQ("TestXXXX", buffer.AsString());

  // Write at offset.
  buffer.Write(kData1, 1);
  EXPECT_EQ("TFooXXXX", buffer.AsString());

  // Write at offset from raw pointer
  buffer.Write(kData1.data(), kData1.size(), 3);
  EXPECT_EQ("TFoFooXX", buffer.AsString());

  // Writing zero bytes should have no effect.
  buffer = CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  buffer.Write(kData1.data(), 0u);
  buffer.Write(nullptr, 0u);  // Passing nullptr is OK when size is 0
  EXPECT_EQ("XXXXXXXX", buffer.AsString());

  // Writing zero bytes just past the buffer should be accepted (i.e. no
  // assertion) and have no effect.
  buffer.Write(kData1.data(), 0u, buffer.size());
  EXPECT_EQ("XXXXXXXX", buffer.AsString());

  // Buffer limits are strictly enforced
  EXPECT_DEATH_IF_SUPPORTED(buffer.Write(kData0.data(), buffer.size() + 1, 0),
                            "destination not large enough");
  EXPECT_DEATH_IF_SUPPORTED(buffer.Write(kData0.data(), 0, buffer.size() + 1),
                            "offset past buffer");
}

TEST(ByteBufferTest, AsString) {
  auto buffer = CreateStaticByteBuffer('T', 'e', 's', 't');
  EXPECT_EQ("Test", buffer.AsString());
}

TEST(ByteBufferTest, Fill) {
  StaticByteBuffer<5> buffer;
  buffer.Fill('A');
  EXPECT_EQ("AAAAA", buffer.AsString());
}

TEST(ByteBufferTest, ToVectorEmpty) {
  BufferView buffer;
  std::vector<uint8_t> vec = buffer.ToVector();
  EXPECT_TRUE(vec.empty());
}

TEST(ByteBufferTest, ToVector) {
  auto buffer = CreateStaticByteBuffer('h', 'e', 'l', 'l', 'o');
  std::vector<uint8_t> vec = buffer.ToVector();
  EXPECT_TRUE(ContainersEqual(vec, buffer));
}

}  // namespace
}  // namespace bt
