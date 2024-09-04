// Copyright 2023 The Pigweed Authors
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

#include <numeric>

#include "lib/stdcompat/utility.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

// clang-format off
// All emboss headers are listed (even if they don't have explicit tests) to
// ensure they are compiled.
#include "pw_bluetooth/att.emb.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_commands.emb.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_h4.emb.h"      // IWYU pragma: keep
#include "pw_bluetooth/hci_test.emb.h"
#include "pw_bluetooth/hci_android.emb.h"    // IWYU pragma: keep
#include "pw_bluetooth/l2cap_frames.emb.h"  // IWYU pragma: keep
// clang-format on

namespace pw::bluetooth {
namespace {

// Examples are used in docs.rst.
TEST(EmbossExamples, MakeView) {
  // DOCSTAG: [pw_bluetooth-examples-make_view]
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto view = emboss::MakeTestCommandPacketView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x03);
  // DOCSTAG: [pw_bluetooth-examples-make_view]
}

TEST(EmbossTest, MakeView) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto view = emboss::MakeTestCommandPacketView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x03);
}

// This definition has a mix of full-width values and bitfields and includes
// conditional bitfields. Let's add this to verify that the structure itself
// doesn't get changed incorrectly and that emboss' size calculation matches
// ours.
TEST(EmbossTest, CheckIsoPacketSize) {
  int max_size = 12 /* max header */ + 4095 /* max payload */;
  EXPECT_EQ(emboss::IsoDataFramePacket::MaxSizeInBytes(), max_size);
}

// Test and demonstrate various ways of reading opcodes.
TEST(EmbossTest, ReadOpcodesFromCommandHeader) {
  // First two bytes will be used as opcode.
  std::array<uint8_t, 4> buffer = {0x00, 0x00, 0x02, 0x03};
  auto view = emboss::MakeTestCommandPacketView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  auto header = view.header();

  EXPECT_EQ(header.opcode_enum().Read(), emboss::OpCode::UNSPECIFIED);
  EXPECT_EQ(header.opcode().BackingStorage().ReadUInt(), 0x0000);
  EXPECT_EQ(header.opcode_bits().ogf().Read(), 0x00);
  EXPECT_EQ(header.opcode_bits().ocf().Read(), 0x00);
  // TODO: https://pwbug.dev/338068316 - Delete these opcode type
  // OpCodeBits cases once opcode has type OpCode.
  EXPECT_EQ(header.opcode().ogf().Read(), 0x00);
  EXPECT_EQ(header.opcode().ocf().Read(), 0x00);

  // LINK_KEY_REQUEST_REPLY is OGF 0x01 and OCF 0x0B.
  header.opcode_enum().Write(emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  EXPECT_EQ(header.opcode_enum().Read(),
            emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  EXPECT_EQ(header.opcode().BackingStorage().ReadUInt(), 0x040B);
  EXPECT_EQ(header.opcode_bits().ogf().Read(), 0x01);
  EXPECT_EQ(header.opcode_bits().ocf().Read(), 0x0B);
  // TODO: https://pwbug.dev/338068316 - Delete these opcode type
  // OpCodeBits cases once opcode has type OpCode.
  EXPECT_EQ(header.opcode().ogf().Read(), 0x01);
  EXPECT_EQ(header.opcode().ocf().Read(), 0x0B);
}

// Test and demonstrate various ways of writing opcodes.
TEST(EmbossTest, WriteOpcodesFromCommandHeader) {
  std::array<uint8_t, 4> buffer = {};
  buffer.fill(0xFF);
  auto view = emboss::MakeTestCommandPacketView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  auto header = view.header();

  header.opcode_enum().Write(emboss::OpCode::UNSPECIFIED);
  EXPECT_EQ(header.opcode().BackingStorage().ReadUInt(), 0x0000);

  header.opcode().ocf().Write(0x0B);
  EXPECT_EQ(header.opcode().BackingStorage().ReadUInt(), 0x000B);

  header.opcode().ogf().Write(0x01);
  EXPECT_EQ(header.opcode().BackingStorage().ReadUInt(), 0x040B);
  // LINK_KEY_REQUEST_REPLY is OGF 0x01 and OCF 0x0B.
  EXPECT_EQ(header.opcode_enum().Read(),
            emboss::OpCode::LINK_KEY_REQUEST_REPLY);
}

// Test and demonstrate using to_underlying with OpCodes enums
TEST(EmbossTest, OPCodeEnumsWithToUnderlying) {
  EXPECT_EQ(0x0000, cpp23::to_underlying(emboss::OpCode::UNSPECIFIED));
}

TEST(EmbossTest, ReadAndWriteOpcodesInCommandResponseHeader) {
  // First two bytes will be used as opcode.
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventView::SizeInBytes()>
      buffer;
  std::iota(buffer.begin(), buffer.end(), 100);
  auto view = emboss::MakeReadBufferSizeCommandCompleteEventView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  auto header = view.command_complete();

  header.command_opcode().BackingStorage().WriteUInt(0x0000);
  EXPECT_EQ(header.command_opcode_enum().Read(), emboss::OpCode::UNSPECIFIED);
  EXPECT_EQ(header.command_opcode().BackingStorage().ReadUInt(), 0x0000);
  EXPECT_EQ(header.command_opcode_bits().ogf().Read(), 0x00);
  EXPECT_EQ(header.command_opcode_bits().ocf().Read(), 0x00);
  // TODO: https://pwbug.dev/338068316 - Delete these command_opcode type
  // OpCodeBits cases once command_opcode has type OpCode.
  EXPECT_EQ(header.command_opcode().ogf().Read(), 0x00);
  EXPECT_EQ(header.command_opcode().ocf().Read(), 0x00);

  // LINK_KEY_REQUEST_REPLY is OGF 0x01 and OCF 0x0B.
  header.command_opcode_enum().Write(emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  EXPECT_EQ(header.command_opcode_enum().Read(),
            emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  EXPECT_EQ(header.command_opcode().BackingStorage().ReadUInt(), 0x040B);
  EXPECT_EQ(header.command_opcode_bits().ogf().Read(), 0x01);
  EXPECT_EQ(header.command_opcode_bits().ocf().Read(), 0x0B);
  // TODO: https://pwbug.dev/338068316 - Delete these command_opcode type
  // OpCodeBits cases once command_opcode has type OpCode.
  EXPECT_EQ(header.command_opcode().ogf().Read(), 0x01);
  EXPECT_EQ(header.command_opcode().ocf().Read(), 0x0B);
}

TEST(EmbossTest, ReadAndWriteEventCodesInEventHeader) {
  std::array<uint8_t, emboss::EventHeaderWriter::SizeInBytes()> buffer;
  std::iota(buffer.begin(), buffer.end(), 100);
  auto header = emboss::MakeEventHeaderView(&buffer);
  EXPECT_TRUE(header.IsComplete());

  header.event_code_uint().Write(
      cpp23::to_underlying(emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS));
  EXPECT_EQ(header.event_code_enum().Read(),
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);
  EXPECT_EQ(
      header.event_code_uint().Read(),
      cpp23::to_underlying(emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS));

  // TODO: https://pwbug.dev/338068316 - Delete these event_code type
  // UInt cases once event_code has type EventCode.
  EXPECT_EQ(
      header.event_code().Read(),
      cpp23::to_underlying(emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS));

  header.event_code().Write(
      cpp23::to_underlying(emboss::EventCode::CONNECTION_REQUEST));
  EXPECT_EQ(header.event_code_uint().Read(),
            cpp23::to_underlying(emboss::EventCode::CONNECTION_REQUEST));
}

TEST(EmbossTest, ReadCommandPayloadLength) {
  std::array<uint8_t, 8> hci_buffer = {
      0x4c, 0xfc, 0x05, 0x73, 0x86, 0x30, 0x00, 0x00};
  emboss::CommandHeaderView command = emboss::MakeCommandHeaderView(
      hci_buffer.data(), emboss::CommandHeaderView::SizeInBytes());
  EXPECT_TRUE(command.IsComplete());
  EXPECT_EQ(command.parameter_total_size().Read(), 5);
}

TEST(EmbossTest, ReadEventPayloadLength) {
  std::array<uint8_t, 8> hci_buffer = {0x0e, 0x04, 0x01, 0x2e, 0xfc, 0x00};
  emboss::EventHeaderView event = emboss::MakeEventHeaderView(
      hci_buffer.data(), emboss::EventHeaderView::SizeInBytes());
  EXPECT_TRUE(event.IsComplete());
  EXPECT_EQ(event.parameter_total_size().Read(), 4);
}

TEST(EmbossTest, ReadAclPayloadLength) {
  std::array<uint8_t, 16> hci_buffer = {0x0c,
                                        0x00,
                                        0x0c,
                                        0x00,
                                        0x08,
                                        0x00,
                                        0x01,
                                        0x00,
                                        0x06,
                                        0x06,
                                        0x04,
                                        0x00,
                                        0x5b,
                                        0x00,
                                        0x41,
                                        0x00};
  emboss::AclDataFrameHeaderView acl = emboss::MakeAclDataFrameHeaderView(
      hci_buffer.data(), emboss::AclDataFrameHeaderView::SizeInBytes());
  EXPECT_TRUE(acl.IsComplete());
  EXPECT_EQ(acl.data_total_length().Read(), 12);
}

TEST(EmbossTest, ReadScoPayloadLength) {
  std::array<uint8_t, 9> hci_buffer = {
      0x02, 0x00, 0x06, 0xFF, 0xD3, 0x4A, 0x1B, 0x2C, 0x3D};
  emboss::ScoDataHeaderView sco = emboss::ScoDataHeaderView(
      hci_buffer.data(), emboss::ScoDataHeaderView::SizeInBytes());
  EXPECT_TRUE(sco.IsComplete());
  EXPECT_EQ(sco.data_total_length().Read(), 6);
}

}  // namespace
}  // namespace pw::bluetooth
