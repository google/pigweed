// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/rfcomm/mux_commands.h"
#include "gtest/gtest.h"

namespace bt {
namespace rfcomm {
namespace {

class RFCOMM_MuxCommandTest : public ::testing::Test {};

// Manual contruction of multiplexer command for this test:
// Please see GSM 5.4.6.3.4.
// Our frame will have the following characteristics:
//  - Test command
//  - Command/response: command
//  - Pattern: "fuchsia" (length 7)
//
// We now form the packet as follows.
// All octets are written LSB...MSB as in the GSM spec.
//
// Type octet is EA ++ C/R ++ type bits. EA is always 1 for RFCOMM/GSM; there
// are no commands which take more than one type octet. C/R bit is 1 for
// commands. Type bits are 000100. Thus, type octet is 11000100.
//
// Length field is one octet long; the octet is EA ++ length, or 1 ++ 1110000.
TEST_F(RFCOMM_MuxCommandTest, TestCommand) {
  auto test_pattern =
      common::CreateStaticByteBuffer('f', 'u', 'c', 'h', 's', 'i', 'a');
  TestCommand command(CommandResponse::kCommand, test_pattern);
  ASSERT_EQ(command.written_size(), 1ul                          // Type
                                        + 1ul                    // Length
                                        + test_pattern.size());  // Payload

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer,
            common::CreateStaticByteBuffer(0b00100011, 0b00001111, 'f', 'u',
                                           'c', 'h', 's', 'i', 'a'));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  ASSERT_EQ(read_command->command_type(), MuxCommandType::kTestCommand);
  auto test_command = std::unique_ptr<TestCommand>(
      static_cast<TestCommand*>(read_command.release()));
  EXPECT_EQ(test_command->test_pattern(), test_pattern);
}

// Manual contruction of multiplexer command for this test:
// Please see GSM 5.4.6.3.5.
// Our frame will have the following characteristics:
//  - Flow Control On Command
//  - Command/response: response
//
// We now form the packet as follows.
// All octets are written LSB...MSB as in the GSM spec.
//
// Type octet is EA ++ C/R ++ type bits. EA is always 1 for RFCOMM/GSM; there
// are no commands which take more than one type octet. C/R bit is 0 for
// response. Type bits are 000101. Thus, type octet is 10000101.
//
// Length field is one octet long; the octet is EA ++ length, or 1 ++ 0000000.
TEST_F(RFCOMM_MuxCommandTest, FconCommand) {
  FlowControlOnCommand command(CommandResponse::kResponse);
  ASSERT_EQ(command.written_size(), 1ul          // Type
                                        + 1ul);  // Length

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer, common::CreateStaticByteBuffer(0b10100001, 0b00000001));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kResponse);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kFlowControlOnCommand);
}

// Manual contruction of multiplexer command for this test:
// Please see GSM 5.4.6.3.6.
// Our frame will have the following characteristics:
//  - Flow Control Off Command
//  - Command/response: command
//
// We now form the packet as follows.
// All octets are written LSB...MSB as in the GSM spec.
//
// Type octet is EA ++ C/R ++ type bits. EA is always 1 for RFCOMM/GSM; there
// are no commands which take more than one type octet. C/R bit is 1 for
// command. Type bits are 000110. Thus, type octet is 11000110.
//
// Length field is one octet long; the octet is EA ++ length, or 1 ++ 0000000.
TEST_F(RFCOMM_MuxCommandTest, FcoffCommand) {
  FlowControlOffCommand command(CommandResponse::kCommand);
  ASSERT_EQ(command.written_size(), 1ul          // Type
                                        + 1ul);  // Length

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer, common::CreateStaticByteBuffer(0b01100011, 0b00000001));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kFlowControlOffCommand);
}

// Manual contruction of multiplexer command for this test:
// Please see GSM 5.4.6.3.7.
// Our frame will have the following characteristics:
//  - Modem Status command
//  - Command/response: command
//  - DLCI: 0x23
//  - Signals: FC=1, RTC=0, RTR=1, IC=0, DV=1
//  - Break value: 0b1010
//
// We now form the packet as follows.
// All octets are written LSB...MSB as in the GSM spec.
//
// Type octet is EA ++ C/R ++ type bits. EA is always 1 for RFCOMM/GSM; there
// are no commands which take more than one type octet. C/R bit is 1 for
// command. Type bits are 000111. Thus, type octet is 11000111.
//
// Length field is one octet long; the octet is EA ++ length, or 1 ++ 1100000.
//
// DLCI octet is EA ++ 1 ++ DLCI, or 1 ++ 1 ++ 010011.
//
// Signal octet is EA ++ FC ++ RTC ++ RTR ++ 00 ++ IC ++ DV, or 0 ++ 1 ++ 0 ++ 1
// ++ 00 ++ 0 ++ 1.
//
// Break octet is EA ++ B1 ++ 00 ++ break value, or 1 ++ 1 ++ 00 ++ 0101.
TEST_F(RFCOMM_MuxCommandTest, ModemStatusCommand) {
  ModemStatusCommandSignals signals = {true, false, true, false, true};
  BreakValue break_value = 0b1010;
  ModemStatusCommand command(CommandResponse::kCommand, 0x23, signals,
                             break_value);
  ASSERT_EQ(command.written_size(), 5ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer,
            common::CreateStaticByteBuffer(0b11100011, 0b00000111, 0b10001111,
                                           0b10001010, 0b10100011));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  ASSERT_EQ(read_command->command_type(), MuxCommandType::kModemStatusCommand);
  auto msc = std::unique_ptr<ModemStatusCommand>(
      static_cast<ModemStatusCommand*>(read_command.release()));
  EXPECT_EQ(msc->dlci(), 0x23);
  EXPECT_EQ(msc->signals().flow_control, signals.flow_control);
  EXPECT_EQ(msc->signals().ready_to_communicate, signals.ready_to_communicate);
  EXPECT_EQ(msc->signals().ready_to_receive, signals.ready_to_receive);
  EXPECT_EQ(msc->signals().incoming_call, signals.incoming_call);
  EXPECT_EQ(msc->signals().data_valid, signals.data_valid);
  EXPECT_TRUE(msc->has_break_signal());
  EXPECT_EQ(msc->break_value(), break_value);
}

TEST_F(RFCOMM_MuxCommandTest, RemotePortNegotiationCommand8Octets) {
  FlowControlFlagsBitfield flow_control = FlowControlFlags::kXonXoffInputFlag |
                                          FlowControlFlags::kRTRInputFlag |
                                          FlowControlFlags::kRTCInputFlag;

  RemotePortNegotiationParams params;
  params.baud = Baud::k115200;
  params.data_bits = DataBits::k7Bits;
  params.stop_bits = StopBits::k1AndHalfBits;
  params.parity = true;
  params.parity_type = ParityType::kOdd;
  params.flow_control = flow_control;
  params.xon_character = 0x23;
  params.xoff_character = 0xDA;

  RemotePortNegotiationMaskBitfield mask;
  mask = RemotePortNegotiationMask::kBitRate |
         RemotePortNegotiationMask::kStopBits |
         RemotePortNegotiationMask::kParityType |
         RemotePortNegotiationMask::kXOFFCharacter |
         RemotePortNegotiationMask::kXonXoffInput |
         RemotePortNegotiationMask::kXonXoffOutput |
         RemotePortNegotiationMask::kRTROutput |
         RemotePortNegotiationMask::kRTCInput;

  RemotePortNegotiationCommand command(CommandResponse::kCommand, 61, params,
                                       mask);
  EXPECT_EQ(command.written_size(), 10ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  EXPECT_EQ(buffer,
            common::CreateStaticByteBuffer(0b10010011, 0b00010001, 0b11110111,
                                           0b00000111, 0b00001101, 0b00010101,
                                           0x23, 0xDA, 0b01010101, 0b00011011));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  EXPECT_EQ(read_command->command_type(),
            MuxCommandType::kRemotePortNegotiationCommand);
  auto rpn_command = std::unique_ptr<RemotePortNegotiationCommand>(
      static_cast<RemotePortNegotiationCommand*>(read_command.release()));
  EXPECT_EQ(rpn_command->dlci(), 61);
  EXPECT_EQ(rpn_command->mask(), mask);
  EXPECT_EQ(rpn_command->params().baud, params.baud);
  EXPECT_EQ(rpn_command->params().data_bits, params.data_bits);
  EXPECT_EQ(rpn_command->params().stop_bits, params.stop_bits);
  EXPECT_EQ(rpn_command->params().parity, params.parity);
  EXPECT_EQ(rpn_command->params().parity_type, params.parity_type);
  EXPECT_EQ(rpn_command->params().xon_character, params.xon_character);
  EXPECT_EQ(rpn_command->params().xoff_character, params.xoff_character);
  EXPECT_EQ(rpn_command->params().flow_control, flow_control);
}
TEST_F(RFCOMM_MuxCommandTest, RemotePortNegotiationCommand1Octet) {
  RemotePortNegotiationCommand command(CommandResponse::kCommand, 61);
  ASSERT_EQ(command.written_size(), 3ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer,
            common::CreateStaticByteBuffer(0b10010011, 0b00000011, 0b11110111));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kRemotePortNegotiationCommand);
  auto rpn_command = std::unique_ptr<RemotePortNegotiationCommand>(
      static_cast<RemotePortNegotiationCommand*>(read_command.release()));
  EXPECT_EQ(rpn_command->dlci(), 61);
}

TEST_F(RFCOMM_MuxCommandTest, RemoteLineStatusCommand) {
  RemoteLineStatusCommand command(CommandResponse::kCommand, 61, true,
                                  LineError::kFramingError);
  ASSERT_EQ(command.written_size(), 4ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer, common::CreateStaticByteBuffer(0b01010011, 0b00000101,
                                                   0b11110111, 0b00001001));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kCommand);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kRemoteLineStatusCommand);
  auto rpn_command = std::unique_ptr<RemoteLineStatusCommand>(
      static_cast<RemoteLineStatusCommand*>(read_command.release()));
  EXPECT_EQ(rpn_command->dlci(), 61);
  EXPECT_EQ(rpn_command->error_occurred(), true);
  EXPECT_EQ(rpn_command->error(), LineError::kFramingError);
}

TEST_F(RFCOMM_MuxCommandTest, NonSupportedCommandResponse) {
  NonSupportedCommandResponse command(CommandResponse::kResponse, 0b00101001);
  ASSERT_EQ(command.written_size(), 3ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer,
            common::CreateStaticByteBuffer(0b00010001, 0b00000011, 0b10100101));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kResponse);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kNonSupportedCommandResponse);
  auto nsc_response = std::unique_ptr<NonSupportedCommandResponse>(
      static_cast<NonSupportedCommandResponse*>(read_command.release()));
  EXPECT_EQ(nsc_response->incoming_command_response(),
            CommandResponse::kResponse);
  EXPECT_EQ(nsc_response->incoming_non_supported_command(), 0b00101001);
}

TEST_F(RFCOMM_MuxCommandTest, DLCParameterNegotiationCommand) {
  ParameterNegotiationParams params;
  params.dlci = 61;
  params.credit_based_flow_handshake =
      CreditBasedFlowHandshake::kSupportedResponse;
  params.priority = kMaxPriority;
  params.maximum_frame_size = 0x1234;
  params.initial_credits = kMaxInitialCredits;

  DLCParameterNegotiationCommand command(CommandResponse::kResponse, params);
  ASSERT_EQ(command.written_size(), 10ul);

  common::DynamicByteBuffer buffer(command.written_size());
  command.Write(buffer.mutable_view());
  ASSERT_EQ(buffer, common::CreateStaticByteBuffer(
                        0b10000001, 0b00010001, 0b00111101, 0xE0, kMaxPriority,
                        0, 0x34, 0x12, 0, kMaxInitialCredits));

  auto read_command = MuxCommand::Parse(buffer);
  EXPECT_EQ(read_command->command_response(), CommandResponse::kResponse);
  ASSERT_EQ(read_command->command_type(),
            MuxCommandType::kDLCParameterNegotiation);
  auto pn_command = std::unique_ptr<DLCParameterNegotiationCommand>(
      static_cast<DLCParameterNegotiationCommand*>(read_command.release()));
  EXPECT_EQ(pn_command->params().dlci, 61);
  EXPECT_EQ(pn_command->params().credit_based_flow_handshake,
            CreditBasedFlowHandshake::kSupportedResponse);
  EXPECT_EQ(pn_command->params().priority, kMaxPriority);
  EXPECT_EQ(pn_command->params().maximum_frame_size, 0x1234);
  EXPECT_EQ(pn_command->params().initial_credits, kMaxInitialCredits);
}

}  // namespace
}  // namespace rfcomm
}  // namespace bt
