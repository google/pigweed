// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "frames.h"

#include "gtest/gtest.h"
#include "mux_commands.h"
#include "rfcomm.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

namespace bt {
namespace rfcomm {
namespace {

// Contruction of "empty" RFCOMM frame:
// Please see GSM 5.2.1 and RFCOMM 5.1.
// Our frame will have the following characteristics:
//  - Sent from the RFCOMM initiator
//  - SABM frame
//  - Sent to DLCI 0x02
//  - Command frame
//  - P/F bit = 1
constexpr Role kEmptyFrameRole = Role::kInitiator;
constexpr CommandResponse kEmptyFrameCR = CommandResponse::kCommand;
constexpr FrameType kEmptyFrameType = FrameType::kSetAsynchronousBalancedMode;
constexpr DLCI kEmptyFrameDLCI = 0x02;
constexpr bool kEmptyFramePF = true;
constexpr bool kEmptyFrameCreditBasedFlow = false;
const auto kEmptyFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. C/R bit is 1 in the case of a command being sent
    // from the initiator role. DLCI is 0x02. Thus: 1 (E/A) ++ 1 (C/R) ++ 010000
    // (DLCI) = 11010000
    0b00001011,
    // Control octet:
    // SABM is 1111p100, P/F bit (p) is 1 --> 11111100
    0b00111111,
    // Length octet:
    // Length is 0; E/A bit is thus 1 --> 10000000
    0b00000001,
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b01011001);

// Contruction of "helloworld" RFCOMM frame:
//  - Sent from the RFCOMM responder
//  - UIH frame
//  - Sent to DLCI 0x23
//  - Command frame
//  - P/F bit = 0
constexpr Role kHelloFrameRole = Role::kResponder;
constexpr CommandResponse kHelloFrameCR = CommandResponse::kCommand;
constexpr FrameType kHelloFrameType = FrameType::kUnnumberedInfoHeaderCheck;
constexpr DLCI kHelloFrameDLCI = 0x23;
constexpr bool kHelloFramePF = false;
constexpr bool kHelloFrameCreditBasedFlow = false;
const auto kHelloFrameInformation =
    CreateStaticByteBuffer('h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd');
const auto kHelloFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. C/R bit is 0 in the case of a command being sent
    // from the responder role. DLCI is 0x23. Thus: 1 (E/A) ++ 0 (C/R) ++ 110001
    // (DLCI) = 10110001
    0b10001101,
    // Control octet:
    // UIH is 1111p111, P/F bit (p) is 0 --> 11110111
    0b11101111,
    // Length octet:
    // Length is 10; E/A bit is thus 1 --> 10101000
    0b00010101,
    // Information
    'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd',
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b10011101);

// Contruction of "hellofuchsia" RFCOMM frame:
//  - Sent from the RFCOMM responder
//  - UIH frame
//  - Sent to DLCI 0x23
//  - Command frame
//  - P/F bit = 1 (credit-based flow on)
constexpr Role kFuchsiaFrameRole = Role::kResponder;
constexpr CommandResponse kFuchsiaFrameCR = CommandResponse::kCommand;
constexpr FrameType kFuchsiaFrameType = FrameType::kUnnumberedInfoHeaderCheck;
constexpr DLCI kFuchsiaFrameDLCI = 0x23;
constexpr bool kFuchsiaFramePF = true;
constexpr bool kFuchsiaFrameCreditBasedFlow = true;
constexpr uint8_t kFuchsiaFrameCredits = 5;
const auto kFuchsiaFrameInformation = CreateStaticByteBuffer(
    'h', 'e', 'l', 'l', 'o', 'f', 'u', 'c', 'h', 's', 'i', 'a');
const auto kFuchsiaFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. C/R bit is 0 in the case of a command being sent
    // from the responder role. DLCI is 0x23. Thus: 1 (E/A) ++ 0 (C/R) ++ 110001
    // (DLCI) = 10110001
    0b10001101,
    // Control octet:
    // UIH is 1111p111, P/F bit (p) is 1 --> 11111111
    0b11111111,
    // Length octet:
    // Length is 12; E/A bit is thus 1 --> 10011000
    0b00011001,
    // Credit octet:
    // Credits = 5
    0b00000101,
    // Information
    'h', 'e', 'l', 'l', 'o', 'f', 'u', 'c', 'h', 's', 'i', 'a',
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b10000001);

constexpr Role kTestCommandFrameRole = Role::kInitiator;
constexpr CommandResponse kTestCommandFrameCR = CommandResponse::kCommand;
constexpr FrameType kTestCommandFrameType =
    FrameType::kUnnumberedInfoHeaderCheck;
constexpr bool kTestCommandCreditBasedFlow = true;
constexpr uint8_t kTestCommandFrameCredits = 63;
constexpr MuxCommandType kTestCommandFrameMuxCommandType =
    MuxCommandType::kTestCommand;
const auto kTestCommandFrameMuxCommandPattern =
    CreateStaticByteBuffer(0, 1, 2, 3);
constexpr CommandResponse kTestCommandFrameMuxCommandCR =
    CommandResponse::kCommand;
const auto kTestCommandFrame = CreateStaticByteBuffer(
    // Address: E/A = 1, C/R is 1 for a command from the initiator, DLCI = 0.
    0b00000011,
    // Control: UIH is 1111p11, P/F is 1 due to presence of a credits field.
    0b11111111,
    // Length: E/A = 1, length = 6
    0b00001101,
    // Credits
    kTestCommandFrameCredits,
    // Mux command type field octet: E/A = 1, C/R = 1, Test Command type
    0b00100011,
    // Mux command length field: E/A = 1, length = 4
    0b00001001,
    // Mux command test pattern
    0, 1, 2, 3,
    // FCS
    0b01101100);

// Contruction of pre-multiplexer-startup SABM frame:
//  - Role is unset
//  - Sent to DLCI 0
const auto kPreMuxSABMFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. Our implementation sets C/R bit to 1 for
    // pre-mux-startup SABM frames. DLCI is 0.
    0b00000011,
    // Control octet:
    // P/F bit (p) is 1 for SABM.
    0b00111111,
    // Length octet:
    // Length is 0.
    0b00000001,
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b00011100);

// Contruction of pre-multiplexer-startup SABM frame:
//  - Role is unset
//  - Sent to DLCI 0
const auto kPreMuxUAFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. Our implementation sets C/R bit to 1 for
    // pre-mux-startup UA frames. DLCI is 0.
    0b00000011,
    // Control octet:
    // P/F bit (Final bit) is 1 for UA.
    0b01110011,
    // Length octet:
    // Length is 0.
    0b00000001,
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b11010111);

// Contruction of pre-multiplexer-startup DM frame:
//  - Role is unset
//  - Sent to DLCI 0
const auto kPreMuxDMFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. Our implementation sets C/R bit to 1 for
    // pre-mux-startup frames. DLCI is 0.
    0b00000011,
    // Control octet:
    // P/F bit (Final bit) is 1 for DM.
    0b00011111,
    // Length octet:
    // Length is 0.
    0b00000001,
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b00110110);

// Contruction of empty user data frame
//  - Sent from the RFCOMM responder
//  - UIH frame
//  - Sent to DLCI 0x23
//  - Command frame
//  - Credits: 11
constexpr Role kEmptyUserDataFrameRole = Role::kResponder;
constexpr DLCI kEmptyUserDataFrameDLCI = 0x23;
constexpr bool kEmptyUserDataFrameCreditBasedFlow = true;
constexpr FrameCredits kEmptyUserDataFrameCredits = 11;
const auto kEmptyUserDataFrame = CreateStaticByteBuffer(
    // Address octet:
    // E/A bit is always 1. C/R bit is 0 in the case of a command being sent
    // from the responder role. DLCI is 0x23. Thus: 1 (E/A) ++ 0 (C/R) ++ 110001
    // (DLCI) = 10110001
    0b10001101,
    // Control octet:
    // UIH is 1111p111, P/F bit (p) is 1 (credit-based fc) --> 11111111
    0b11111111,
    // Length octet: length is 0
    0b00000001,
    // Credits: 11
    0b00001011,
    // FCS octet:
    // Please see GSM 5.2.1.6, GSM Annex B, and RFCOMM 5.1.1.
    0b10000001);

const auto kInvalidLengthFrame = CreateStaticByteBuffer(0, 1, 2);

// Same as the hellofuchsia frame, but information field is too short.
const auto kInvalidLengthFrame2 = CreateStaticByteBuffer(
    0b10001101, 0b11111111, 0b00011001, 0b00000101, 'h', 'e', 'l', 'l', 'o');

// Same as the hellofuchsia frame, but with an invalid FCS.
const auto kInvalidFCSFrame = CreateStaticByteBuffer(
    0b10001101, 0b11111111, 0b00011001, 0b00000101, 'h', 'e', 'l', 'l', 'o',
    'f', 'u', 'c', 'h', 's', 'i', 'a', 0b10000001 + 1);

// Same as the hellofuchsia frame, but with an invalid DLCI (1)
const auto kInvalidDLCIFrame = CreateStaticByteBuffer(
    0b00000101, 0b11111111, 0b00011001, 0b00000101, 'h', 'e', 'l', 'l', 'o',
    'f', 'u', 'c', 'h', 's', 'i', 'a', 0b11000011);

// Same as the hellofuchsia frame, but with an invalid DLCI (62)
const auto kInvalidDLCIFrame2 = CreateStaticByteBuffer(
    0b11111001, 0b11111111, 0b00011001, 0b00000101, 'h', 'e', 'l', 'l', 'o',
    'f', 'u', 'c', 'h', 's', 'i', 'a', 0b10011111);

using RFCOMM_FrameTest = ::testing::Test;

TEST_F(RFCOMM_FrameTest, WriteFrame) {
  SetAsynchronousBalancedModeCommand frame(kEmptyFrameRole, kEmptyFrameDLCI);
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(4ul, frame.written_size());
  EXPECT_EQ(kEmptyFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WriteFrameWithData) {
  auto information = NewSlabBuffer(kHelloFrameInformation.size());
  kHelloFrameInformation.Copy(information.get());
  UserDataFrame frame(kHelloFrameRole, kHelloFrameCreditBasedFlow,
                      kHelloFrameDLCI, std::move(information));
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(14ul, frame.written_size());
  EXPECT_EQ(kHelloFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WriteFrameWithDataAndCredits) {
  auto information = NewSlabBuffer(kFuchsiaFrameInformation.size());
  kFuchsiaFrameInformation.Copy(information.get());
  UserDataFrame frame(kFuchsiaFrameRole, kFuchsiaFrameCreditBasedFlow,
                      kFuchsiaFrameDLCI, std::move(information));
  frame.set_credits(kFuchsiaFrameCredits);
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(17ul, frame.written_size());
  EXPECT_EQ(kFuchsiaFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WriteFrameWithMuxCommandAndCredits) {
  auto mux_command = std::make_unique<TestCommand>(
      kTestCommandFrameMuxCommandCR, kTestCommandFrameMuxCommandPattern);
  MuxCommandFrame frame(kTestCommandFrameRole, kTestCommandCreditBasedFlow,
                        std::move(mux_command));
  frame.set_credits(kTestCommandFrameCredits);
  EXPECT_EQ(kTestCommandFrame.size(), frame.written_size());
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(kTestCommandFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WritePreMuxStartupSABM) {
  SetAsynchronousBalancedModeCommand frame(Role::kUnassigned, kMuxControlDLCI);
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(kPreMuxSABMFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WritePreMuxStartupUA) {
  UnnumberedAcknowledgementResponse frame(Role::kUnassigned, kMuxControlDLCI);
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(kPreMuxUAFrame, buffer);
}
TEST_F(RFCOMM_FrameTest, WritePreMuxStartupDM) {
  DisconnectedModeResponse frame(Role::kUnassigned, kMuxControlDLCI);
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(kPreMuxDMFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, WriteEmptyUserDataFrameWithCredits) {
  UserDataFrame frame(kEmptyUserDataFrameRole,
                      kEmptyUserDataFrameCreditBasedFlow,
                      kEmptyUserDataFrameDLCI, nullptr);
  frame.set_credits(kEmptyUserDataFrameCredits);
  EXPECT_EQ(kEmptyUserDataFrame.size(), frame.written_size());
  DynamicByteBuffer buffer(frame.written_size());
  frame.Write(buffer.mutable_view());
  EXPECT_EQ(kEmptyUserDataFrame, buffer);
}

TEST_F(RFCOMM_FrameTest, ReadFrame) {
  auto frame =
      Frame::Parse(kEmptyFrameCreditBasedFlow, kEmptyFrameRole, kEmptyFrame);
  EXPECT_EQ(kEmptyFrameCR, frame->command_response());
  EXPECT_EQ(kEmptyFrameDLCI, frame->dlci());
  EXPECT_EQ((uint8_t)kEmptyFrameType, frame->control());
  EXPECT_EQ(kEmptyFramePF, frame->poll_final());
  EXPECT_EQ(0, frame->length());
}

TEST_F(RFCOMM_FrameTest, ReadFrameWithData) {
  auto frame =
      Frame::Parse(kHelloFrameCreditBasedFlow, kHelloFrameRole, kHelloFrame);
  EXPECT_EQ((uint8_t)kHelloFrameType, frame->control());
  auto user_data_frame = Frame::DowncastFrame<UserDataFrame>(std::move(frame));
  EXPECT_EQ(nullptr, frame);
  EXPECT_EQ(kHelloFrameCR, user_data_frame->command_response());
  EXPECT_EQ(kHelloFrameDLCI, user_data_frame->dlci());
  EXPECT_EQ(kHelloFramePF, user_data_frame->poll_final());
  EXPECT_EQ(kHelloFrameInformation.size(), user_data_frame->length());
  EXPECT_EQ(0, user_data_frame->credits());
  EXPECT_EQ(kHelloFrameInformation, *user_data_frame->TakeInformation());
  EXPECT_EQ(nullptr, user_data_frame->TakeInformation());
}

TEST_F(RFCOMM_FrameTest, ReadFrameWithDataAndCredits) {
  auto frame = Frame::Parse(kFuchsiaFrameCreditBasedFlow, kFuchsiaFrameRole,
                            kFuchsiaFrame);
  EXPECT_EQ((uint8_t)kFuchsiaFrameType, frame->control());
  auto user_data_frame = Frame::DowncastFrame<UserDataFrame>(std::move(frame));
  EXPECT_EQ(nullptr, frame);
  EXPECT_EQ(kFuchsiaFrameCR, user_data_frame->command_response());
  EXPECT_EQ(kFuchsiaFrameDLCI, user_data_frame->dlci());
  EXPECT_EQ(kFuchsiaFramePF, user_data_frame->poll_final());
  EXPECT_EQ(kFuchsiaFrameInformation.size(), user_data_frame->length());
  EXPECT_EQ(kFuchsiaFrameCredits, user_data_frame->credits());
  EXPECT_EQ(kFuchsiaFrameInformation, *user_data_frame->TakeInformation());
  EXPECT_EQ(nullptr, user_data_frame->TakeInformation());
}

TEST_F(RFCOMM_FrameTest, ReadFrameWithMuxCommandAndCredits) {
  auto frame = Frame::Parse(kTestCommandCreditBasedFlow, kTestCommandFrameRole,
                            kTestCommandFrame);
  EXPECT_EQ(kTestCommandFrameCR, frame->command_response());
  EXPECT_EQ(kTestCommandFrameType, static_cast<FrameType>(frame->control()));
  EXPECT_EQ(kMuxControlDLCI, frame->dlci());
  auto mux_command_frame =
      Frame::DowncastFrame<MuxCommandFrame>(std::move(frame));
  EXPECT_EQ(nullptr, frame);
  auto mux_command = mux_command_frame->TakeMuxCommand();
  EXPECT_EQ(kTestCommandFrameMuxCommandCR, mux_command->command_response());
  EXPECT_EQ(kTestCommandFrameMuxCommandType, mux_command->command_type());
  EXPECT_EQ(nullptr, mux_command_frame->TakeMuxCommand());
  auto test_command = std::unique_ptr<TestCommand>(
      static_cast<TestCommand*>(mux_command.release()));
  EXPECT_EQ(kTestCommandFrameMuxCommandPattern, test_command->test_pattern());
}

TEST_F(RFCOMM_FrameTest, ReadFramesPreMuxStartup) {
  auto frame = Frame::Parse(true, Role::kUnassigned, kPreMuxSABMFrame);
  EXPECT_TRUE(frame);
  EXPECT_EQ(CommandResponse::kCommand, frame->command_response());

  frame = Frame::Parse(true, Role::kUnassigned, kPreMuxUAFrame);
  EXPECT_TRUE(frame);
  EXPECT_EQ(CommandResponse::kResponse, frame->command_response());

  frame = Frame::Parse(true, Role::kUnassigned, kPreMuxDMFrame);
  EXPECT_TRUE(frame);
  EXPECT_EQ(CommandResponse::kResponse, frame->command_response());

  EXPECT_FALSE(Frame::Parse(true, Role::kUnassigned, kHelloFrame));
  EXPECT_FALSE(Frame::Parse(true, Role::kUnassigned, kFuchsiaFrame));
  EXPECT_FALSE(Frame::Parse(true, Role::kUnassigned, kTestCommandFrame));
}

TEST_F(RFCOMM_FrameTest, ReadEmptyUserDataFrameWithCredits) {
  auto frame = Frame::Parse(kEmptyUserDataFrameCreditBasedFlow,
                            kEmptyUserDataFrameRole, kEmptyUserDataFrame);
  EXPECT_EQ(FrameType::kUnnumberedInfoHeaderCheck,
            static_cast<FrameType>(frame->control()));
  EXPECT_EQ(kEmptyUserDataFrameDLCI, frame->dlci());
  auto user_data_frame = Frame::DowncastFrame<UserDataFrame>(std::move(frame));
  EXPECT_EQ(0, user_data_frame->length());
  EXPECT_EQ(nullptr, user_data_frame->TakeInformation());
}

TEST_F(RFCOMM_FrameTest, ReadInvalidFrame_TooShort) {
  auto frame = Frame::Parse(true, Role::kInitiator, kInvalidLengthFrame);
  EXPECT_EQ(nullptr, frame);
}

TEST_F(RFCOMM_FrameTest, ReadInvalidFrame_EndsUnexpectedly) {
  auto frame = Frame::Parse(true, Role::kInitiator, kInvalidLengthFrame2);
  EXPECT_EQ(nullptr, frame);
}

TEST_F(RFCOMM_FrameTest, ReadInvalidFrame_InvalidFCS) {
  auto frame = Frame::Parse(true, Role::kInitiator, kInvalidFCSFrame);
  EXPECT_EQ(nullptr, frame);
}

TEST_F(RFCOMM_FrameTest, ReadInvalidFrame_InvalidDLCI) {
  EXPECT_EQ(nullptr, Frame::Parse(true, Role::kInitiator, kInvalidDLCIFrame));
  EXPECT_EQ(nullptr, Frame::Parse(true, Role::kInitiator, kInvalidDLCIFrame2));
}

}  // namespace
}  // namespace rfcomm
}  // namespace bt
