// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

#include "gtest/gtest.h"

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace common {
namespace {

TEST(ByteBufferTest, StaticByteBuffer) {
  constexpr size_t kBufferSize = 5;
  StaticByteBuffer<kBufferSize> buffer;

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();
  buffer[3] = 3;

  constexpr std::array<uint8_t, kBufferSize> kExpected{
      {0x00, 0x00, 0x00, 0x03, 0x00}};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));

  // Moving will result in a copy.
  StaticByteBuffer<kBufferSize> buffer_copy = std::move(buffer);
  EXPECT_EQ(kBufferSize, buffer.size());
  EXPECT_EQ(kBufferSize, buffer_copy.size());
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));
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

  EXPECT_TRUE(ContainersEqual(buffer0, buffer1));
  EXPECT_TRUE(ContainersEqual(buffer0, buffer2));
  EXPECT_TRUE(ContainersEqual(buffer1, buffer2));
}

TEST(ByteBufferTest, DynamicByteBuffer) {
  constexpr size_t kBufferSize = 5;
  DynamicByteBuffer buffer(kBufferSize);

  EXPECT_EQ(kBufferSize, buffer.size());
  buffer.SetToZeros();
  buffer[3] = 3;

  constexpr std::array<uint8_t, kBufferSize> kExpected{
      {0x00, 0x00, 0x00, 0x03, 0x00}};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));

  // Moving will invalidate the source buffer.
  DynamicByteBuffer buffer_moved = std::move(buffer);
  EXPECT_EQ(0u, buffer.size());
  EXPECT_EQ(kBufferSize, buffer_moved.size());
  EXPECT_EQ(nullptr, buffer.data());
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
  StaticByteBuffer<kBufferSize> buffer({1, 2, 3});

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
  auto buffer = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  BufferView empty_buffer;

  // Create a large enough buffer.
  StaticByteBuffer<10> target_buffer;

  // Copying an empty buffer should copy 0 bytes.
  EXPECT_EQ(0u, empty_buffer.Copy(&target_buffer));

  // Copy all of |buffer|. The first |buffer.size()| octets of |target_buffer|
  // should match the contents of |buffer|.
  size_t expected_write_size = buffer.size();
  ASSERT_EQ(expected_write_size, buffer.Copy(&target_buffer));
  EXPECT_TRUE(
      ContainersEqual(buffer, BufferView(target_buffer, expected_write_size)));

  // Copy all of |buffer| starting at index 1.
  target_buffer.SetToZeros();
  expected_write_size = buffer.size() - 1;
  ASSERT_EQ(expected_write_size, buffer.Copy(&target_buffer, 1));
  BufferView sub = buffer.view(1);
  EXPECT_TRUE(
      ContainersEqual(sub, BufferView(target_buffer, expected_write_size)));

  // Copy one octet of |buffer| starting at index 2
  target_buffer.SetToZeros();
  expected_write_size = 1;
  ASSERT_EQ(expected_write_size, buffer.Copy(&target_buffer, 1, 1));
  EXPECT_EQ(buffer[1], target_buffer[0]);

  // Zero the buffer and copy its contents for later comparison.
  target_buffer.SetToZeros();
  auto target_buffer_copy = target_buffer;

  // Copy all remaining octets starting just past the end of |buffer|. This
  // should copy zero bytes and |target_buffer| should remain unchanged.
  ASSERT_EQ(0u, buffer.Copy(&target_buffer, buffer.size()));
  EXPECT_TRUE(ContainersEqual(target_buffer_copy, target_buffer));
}

TEST(ByteBufferTest, View) {
  auto buffer = common::CreateStaticByteBuffer('T', 'e', 's', 't');
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
  auto buffer = common::CreateStaticByteBuffer('T', 'e', 's', 't');
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
  const auto kData0 = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = common::CreateStaticByteBuffer('F', 'o', 'o');
  EXPECT_FALSE(kData0 == kData1);
}

TEST(ByteBufferTest, ByteBufferEqualitySuccess) {
  const auto kData0 = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  EXPECT_TRUE(kData0 == kData1);
}

TEST(ByteBufferTest, MutableByteBufferWrite) {
  const auto kData0 = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  const auto kData1 = common::CreateStaticByteBuffer('F', 'o', 'o');

  auto buffer =
      common::CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  EXPECT_EQ("XXXXXXXX", buffer.AsString());

  buffer.Write(kData0);
  EXPECT_EQ("TestXXXX", buffer.AsString());

  // Write from raw pointer.
  buffer =
      common::CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  buffer.Write(kData0.data(), kData0.size());
  EXPECT_EQ("TestXXXX", buffer.AsString());

  // Write at offset.
  buffer.Write(kData1, 1);
  EXPECT_EQ("TFooXXXX", buffer.AsString());

  // Write at offset from raw pointer
  buffer.Write(kData1.data(), kData1.size(), 3);
  EXPECT_EQ("TFoFooXX", buffer.AsString());

  // Writing zero bytes should have no effect.
  buffer =
      common::CreateStaticByteBuffer('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X');
  buffer.Write(kData1.data(), 0u);
  buffer.Write(nullptr, 0u);  // Passing nullptr is OK when size is 0
  EXPECT_EQ("XXXXXXXX", buffer.AsString());

  // Writing zero bytes just past the buffer should be accepted (i.e. no
  // assertion) and have no effect.
  buffer.Write(kData1.data(), 0u, buffer.size());
  EXPECT_EQ("XXXXXXXX", buffer.AsString());
}

TEST(ByteBufferTest, AsString) {
  auto buffer = common::CreateStaticByteBuffer('T', 'e', 's', 't');
  EXPECT_EQ("Test", buffer.AsString());
}

TEST(ByteBufferTest, Fill) {
  StaticByteBuffer<5> buffer;
  buffer.Fill('A');
  EXPECT_EQ("AAAAA", buffer.AsString());
}

}  // namespace
}  // namespace common
}  // namespace bt
