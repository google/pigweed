// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"

#include <string>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace {

struct TestHeader {
  uint16_t field16;
  uint8_t field8;
} __attribute__((packed));

struct TestPayload {
  uint8_t arg0;
  uint16_t arg1;
  uint8_t arg2[2];
  uint8_t arg3[0];
} __attribute__((packed));

TEST(PacketViewTest, EmptyPayload) {
  constexpr size_t kBufferSize = sizeof(TestHeader);

  StaticByteBuffer<kBufferSize> buffer;

  // Assign some values to the header portion.
  *reinterpret_cast<uint16_t*>(buffer.mutable_data()) = 512;
  buffer[2] = 255;

  PacketView<TestHeader> packet(&buffer);
  EXPECT_EQ(kBufferSize, packet.size());
  EXPECT_EQ(0u, packet.payload_size());
  EXPECT_EQ(0u, packet.payload_data().size());

  EXPECT_EQ(512, packet.header().field16);
  EXPECT_EQ(255, packet.header().field8);

  // Verify the buffer contents.
  // TODO(armansito): This assumes that the packet is encoded in Bluetooth
  // network byte-order which is little-endian. For now we rely on the fact that
  // both ARM64 and x86-64 have little-endian encoding schemes to get away with
  // not explicitly encoding the entries. This is obviously wrong on other
  // architectures and will need to be addressed.
  constexpr std::array<uint8_t, kBufferSize> kExpected{{0x00, 0x02, 0xFF}};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));
}

TEST(PacketViewTest, NonEmptyPayload) {
  constexpr size_t kPayloadPadding = 4;
  constexpr size_t kPayloadSize = sizeof(TestPayload) + kPayloadPadding;
  constexpr size_t kBufferSize = sizeof(TestHeader) + kPayloadSize;

  StaticByteBuffer<kBufferSize> buffer;
  buffer.SetToZeros();

  MutablePacketView<TestHeader> packet(&buffer, kPayloadSize);
  EXPECT_EQ(kBufferSize, packet.size());
  EXPECT_EQ(kPayloadSize, packet.payload_size());
  EXPECT_NE(nullptr, packet.payload_data().data());

  auto payload = packet.mutable_payload<TestPayload>();
  EXPECT_NE(nullptr, payload);

  // Modify the payload.
  payload->arg0 = 127;
  payload->arg2[0] = 1;
  payload->arg2[1] = 2;
  memcpy(payload->arg3, "Test", 4);

  constexpr std::array<uint8_t, kBufferSize> kExpected{{
      0x00, 0x00, 0x00,   // header
      0x7F,               // arg0
      0x00, 0x00,         // arg1
      0x01, 0x02,         // arg2
      'T', 'e', 's', 't'  // arg3
  }};
  EXPECT_TRUE(ContainersEqual(kExpected, buffer));
}

}  // namespace
}  // namespace bt
