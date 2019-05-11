// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mux_commands.h"

#include <cstring>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace rfcomm {

namespace {

// Used to mask different parts of the type and length fields. See GSM 5.4.6.1.
constexpr uint8_t kEAMask = 0b00000001;
constexpr uint8_t kCRMask = 0b00000010;
constexpr uint8_t kTypeMask = 0b11111100;

constexpr size_t kTypeIndex = 0;
constexpr size_t kLengthIndex = 1;

constexpr size_t kLengthShift = 1;

// The information lengths for different types of mux commands. These are the
// only values that should appear in the length field for these commands.
constexpr size_t kPNLength = 8;
constexpr size_t kMSCWithoutBreakLength = 2;
constexpr size_t kMSCWithBreakLength = 3;
constexpr size_t kNSCLength = 1;
constexpr size_t kFConLength = 0;
constexpr size_t kFCoffLength = 0;
constexpr size_t kRPNShortLength = 1;
constexpr size_t kRPNLongLength = 8;
constexpr size_t kRLSLength = 2;

constexpr size_t kPNCreditBasedFlowHandshakeShift = 4;

constexpr uint8_t kMSCDLCIShift = 2;
constexpr size_t kMSCFlowControlShift = 1;
constexpr uint8_t kMSCFlowControlMask = 1 << kMSCFlowControlShift;
constexpr size_t kMSCReadyToCommunicateShift = 2;
constexpr uint8_t kMSCReadyToCommunicateMask = 1 << kMSCReadyToCommunicateShift;
constexpr size_t kMSCReadyToReceiveShift = 3;
constexpr uint8_t kMSCReadyToReceiveMask = 1 << kMSCReadyToReceiveShift;
constexpr size_t kMSCIncomingCallShift = 6;
constexpr uint8_t kMSCIncomingCallMask = 1 << kMSCIncomingCallShift;
constexpr size_t kMSCDataValidShift = 7;
constexpr uint8_t kMSCDataValidMask = 1 << kMSCDataValidShift;
constexpr size_t kMSCBreakSignalShift = 1;
constexpr uint8_t kMSCBreakSignalMask = 1 << kMSCBreakSignalShift;
constexpr size_t kMSCBreakValueShift = 4;

constexpr size_t kNSCNotSupportedCommandShift = 2;
constexpr size_t kNSCCRShift = 1;

constexpr size_t kRPNDLCIShift = 2;
constexpr uint8_t kRPNDataBitsMask = 0b11;
constexpr size_t kRPNStopBitsShift = 2;
constexpr uint8_t kRPNStopBitsMask = 1;
constexpr size_t kRPNParityShift = 3;
constexpr uint8_t kRPNParityMask = 1;
constexpr size_t kRPNParityTypeShift = 4;
constexpr uint8_t kRPNParityTypeMask = 0b11;

constexpr size_t kRLSDLCIShift = 2;
constexpr size_t kRLSErrorOccurredShift = 0;
constexpr uint8_t kRLSErrorOccurredMask = 1 << kRLSErrorOccurredShift;
constexpr size_t kRLSErrorShift = 1;
constexpr uint8_t kRLSErrorMask = 0b111;

constexpr uint8_t kPNDLCIMask = 0b00111111;
constexpr uint8_t kPNPriorityMask = 0b00111111;
constexpr uint8_t kPNInitialCreditsMask = 0b00000111;

// Length = 0, EA bit = 1.
constexpr uint8_t kFlowcontrolOnLength = 0b00000001;
constexpr uint8_t kFlowcontrolOffLength = 0b00000001;

// The number of octets which form the header when the length field fits in one
// octet. This is thus the header size for all mux commands with the possible
// exception of the Test command.
constexpr size_t kMinHeaderSize = 2;

// For a given payload |length|, calculates the number of octets needed to
// encode |length|.
// Note that this is only needed by the Test multiplexer command.
size_t NumLengthOctetsNeeded(size_t length) {
  size_t l = length;
  size_t counter = 0;
  while (l) {
    l >>= 1;
    ++counter;
  }
  // Counter now contains the number of least significant bits which are
  // occupied by meaningful data. Subtracting 1, counter becomes the index of
  // the most significant bit containing a 1.

  // If counter is 0, no bits in |length| are set. We still need to return a
  // single octet indicating a value of 0, though.
  size_t num_octets = 1;
  if (counter != 0)
    num_octets = (counter - 1) / 7 + 1;

  return num_octets;
}

// Turns a size_t into a buffer of length field octets as described in GSM
// 5.4.6.1. In this case, we are talking about the length field within the
// multiplexer control commands.
// GSM allows the length field to be specified with a variable number
// of octets.
//
// TODO(gusss): I can't actually find any bounds within the GSM or RFCOMM specs
// on how long this length field is allowed to be. Most of the multiplexer
// commands which we support have fixed-sized payloads (no larger than 8 octets,
// most of the time), so this isn't a problem. However, the Test command takes a
// user-supplied pattern of octets. There is no restriction in the spec on how
// long this pattern can be.
DynamicByteBuffer CreateLengthFieldOctets(size_t length) {
  DynamicByteBuffer octets(NumLengthOctetsNeeded(length));

  for (size_t i = 0; i < octets.size(); i++) {
    // There should still be meaningful data left in length.
    ZX_DEBUG_ASSERT(length);
    // We set the EA bit to 0.
    octets[i] = ~kEAMask & (length << kLengthShift);
    length >>= 7;
  }
  // If we calculated the number of octets correctly above, then there should be
  // nothing remaining in length.
  ZX_DEBUG_ASSERT(length == 0);

  // Set the EA bit of the last octet to 1, to indicate it's the last octet.
  octets[octets.size() - 1] |= kEAMask;

  return octets;
}

// Compares |length| with the possible lengths for the information field of a
// multiplexer command of type |type|.
bool CommandLengthValid(MuxCommandType type, size_t length) {
  switch (type) {
    case MuxCommandType::kDLCParameterNegotiation:
      return length == kPNLength;
    case MuxCommandType::kTestCommand:
      // Any length is valid for a Test command.
      return true;
    case MuxCommandType::kFlowControlOnCommand:
      return length == kFConLength;
    case MuxCommandType::kFlowControlOffCommand:
      return length == kFCoffLength;
    case MuxCommandType::kModemStatusCommand:
      return length == kMSCWithBreakLength || length == kMSCWithoutBreakLength;
    case MuxCommandType::kNonSupportedCommandResponse:
      return length == kNSCLength;
    case MuxCommandType::kRemoteLineStatusCommand:
      return length == kRLSLength;
    case MuxCommandType::kRemotePortNegotiationCommand:
      return length == kRPNShortLength || length == kRPNLongLength;
  }
  return false;
}

}  // namespace

MuxCommand::MuxCommand(MuxCommandType command_type,
                       CommandResponse command_response)
    : command_type_(command_type), command_response_(command_response) {}

std::unique_ptr<MuxCommand> MuxCommand::Parse(const ByteBuffer& buffer) {
  ZX_DEBUG_ASSERT_MSG(buffer.size() >= kMinHeaderSize,
                      "buffer must contain at least a type and length octet");

  CommandResponse command_response = (buffer[kTypeIndex] & kCRMask)
                                         ? CommandResponse::kCommand
                                         : CommandResponse::kResponse;
  MuxCommandType type = (MuxCommandType)(buffer[kTypeIndex] & kTypeMask);

  // Read the (potentially numerous) length octets.
  size_t length = 0, length_idx = kLengthIndex, num_length_octets = 0;
  do {
    // Shift right to shift out the EA bit, and then shift into place.
    length |= (buffer[length_idx] >> kLengthShift) << (7 * num_length_octets);
    ++num_length_octets;
  } while ((buffer[length_idx++] & kEAMask) == 0);
  // 7*num_length_octets is the number of bits encoded by length.
  // Here, we're limiting the size of the length field. In theory, the spec
  // allows the length field to be of any size, but here we're limiting it to
  // fitting in size_t.
  if (7 * num_length_octets > 8 * sizeof(size_t)) {
    bt_log(WARN, "rfcomm", "encoded length is larger than allowed");
    return nullptr;
  }

  // Check that the buffer is actually at least as big as the command which it
  // contains. Command is 1 control octet, multiple length octets, and payload.
  if (buffer.size() < 1 + num_length_octets + length) {
    bt_log(WARN, "rfcomm", "buffer is shorter than the command it contains");
    return nullptr;
  }

  if (!CommandLengthValid(MuxCommandType(type), length)) {
    bt_log(ERROR, "rfcomm",
           "unexpected length %zu for multiplexer command of type %u", length,
           static_cast<unsigned>(type));
    return nullptr;
  }

  switch (type) {
    case MuxCommandType::kDLCParameterNegotiation:
      return DLCParameterNegotiationCommand::Parse(command_response, buffer);

    case MuxCommandType::kTestCommand:
      return TestCommand::Parse(command_response, length, buffer);

    case MuxCommandType::kFlowControlOnCommand:
      return FlowControlOnCommand::Parse(command_response);

    case MuxCommandType::kFlowControlOffCommand:
      return FlowControlOffCommand::Parse(command_response);

    case MuxCommandType::kModemStatusCommand:
      return ModemStatusCommand::Parse(command_response, length, buffer);

    case MuxCommandType::kNonSupportedCommandResponse:
      return NonSupportedCommandResponse::Parse(command_response, buffer);

    case MuxCommandType::kRemotePortNegotiationCommand:
      return RemotePortNegotiationCommand::Parse(command_response, length,
                                                 buffer);

    case MuxCommandType::kRemoteLineStatusCommand:
      return RemoteLineStatusCommand::Parse(command_response, buffer);

    default:
      bt_log(WARN, "rfcomm", "unrecognized multiplexer command type: %u",
             static_cast<unsigned>(type));
  }

  return std::unique_ptr<MuxCommand>(nullptr);
}

TestCommand::TestCommand(CommandResponse command_response,
                         const ByteBuffer& test_pattern)
    : MuxCommand(MuxCommandType::kTestCommand, command_response) {
  test_pattern_ = DynamicByteBuffer(test_pattern.size());
  test_pattern.Copy(&test_pattern_, 0, test_pattern.size());
}

std::unique_ptr<TestCommand> TestCommand::Parse(
    CommandResponse command_response, size_t length, const ByteBuffer& buffer) {
  return std::make_unique<TestCommand>(command_response,
                                       buffer.view(2, length));
}

void TestCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());

  size_t idx = 0;

  buffer[idx] = type_field_octet();
  ++idx;

  // Write the length field octet(s). If the length fits in one byte (accounting
  // for the EA bit), we write it immediately. This should be the common case.
  if (test_pattern_.size() <= kMaxSingleOctetLength) {
    buffer[idx] = kEAMask | (test_pattern_.size() << kLengthShift);
    ++idx;
  } else {
    auto length_field_octets = CreateLengthFieldOctets(test_pattern_.size());
    buffer.Write(length_field_octets, idx);
    idx += length_field_octets.size();
  }

  buffer.Write(test_pattern_, idx);
}

size_t TestCommand::written_size() const {
  return 1                                              // Type
         + NumLengthOctetsNeeded(test_pattern_.size())  // Length
         + test_pattern_.size();                        // Payload
}

FlowControlOnCommand::FlowControlOnCommand(CommandResponse command_response)
    : MuxCommand(MuxCommandType::kFlowControlOnCommand, command_response) {}

std::unique_ptr<FlowControlOnCommand> FlowControlOnCommand::Parse(
    CommandResponse command_response) {
  return std::make_unique<FlowControlOnCommand>(command_response);
}

void FlowControlOnCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // Length = 0, EA bit = 1.
  buffer[kLengthIndex] = kFlowcontrolOnLength;
}

size_t FlowControlOnCommand::written_size() const { return 2ul + kFConLength; }

std::unique_ptr<FlowControlOffCommand> FlowControlOffCommand::Parse(
    CommandResponse command_response) {
  return std::make_unique<FlowControlOffCommand>(command_response);
}

void FlowControlOffCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // Length = 0, EA bit = 1.
  buffer[kLengthIndex] = kFlowcontrolOffLength;
}

FlowControlOffCommand::FlowControlOffCommand(CommandResponse command_response)
    : MuxCommand(MuxCommandType::kFlowControlOffCommand, command_response) {}

size_t FlowControlOffCommand::written_size() const { return 2ul + kFConLength; }

ModemStatusCommand::ModemStatusCommand(CommandResponse command_response,
                                       DLCI dlci,
                                       ModemStatusCommandSignals signals,
                                       BreakValue break_value)
    : MuxCommand(MuxCommandType::kModemStatusCommand, command_response),
      dlci_(dlci),
      signals_(signals),
      break_value_(break_value) {}

std::unique_ptr<ModemStatusCommand> ModemStatusCommand::Parse(
    CommandResponse command_response, size_t length, const ByteBuffer& buffer) {
  DLCI dlci = buffer[2] >> kMSCDLCIShift;
  ModemStatusCommandSignals signals;
  BreakValue break_value = kDefaultInvalidBreakValue;

  // The first bit of |buffer[4]| encodes whether the octet encodes a break
  // signal. If it does not, then we let break_value default to an invalid
  // value.
  if (length == kMSCWithBreakLength && buffer[4] & kMSCBreakSignalMask) {
    break_value = buffer[4] >> kMSCBreakValueShift;
  }

  // clang-format off
  signals.flow_control          = buffer[3] & kMSCFlowControlMask;
  signals.ready_to_communicate  = buffer[3] & kMSCReadyToCommunicateMask;
  signals.ready_to_receive      = buffer[3] & kMSCReadyToReceiveMask;
  signals.incoming_call         = buffer[3] & kMSCIncomingCallMask;
  signals.data_valid            = buffer[3] & kMSCDataValidMask;
  // clang-format on

  return std::make_unique<ModemStatusCommand>(command_response, dlci, signals,
                                              break_value);
}

void ModemStatusCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // EA bit = 1.
  buffer[kLengthIndex] =
      ((has_break_signal() ? kMSCWithBreakLength : kMSCWithoutBreakLength)
       << kLengthShift) |
      kEAMask;
  // EA bit = 1, bit 2 = 1.
  buffer[2] = kEAMask | (1 << 1) | (dlci_ << kMSCDLCIShift);
  // clang-format off
  buffer[3] = !has_break_signal()
              | signals_.flow_control             << kMSCFlowControlShift
              | signals_.ready_to_communicate     << kMSCReadyToCommunicateShift
              | signals_.ready_to_receive         << kMSCReadyToReceiveShift
              | signals_.incoming_call            << kMSCIncomingCallShift
              | signals_.data_valid               << kMSCDataValidShift;
  if (has_break_signal()) {
    buffer[4] = kEAMask
                | has_break_signal()  << kMSCBreakSignalShift
                | break_value_        << kMSCBreakValueShift;
    // clang-format on
  }
}

size_t ModemStatusCommand::written_size() const {
  return 2ul +
         (has_break_signal() ? kMSCWithBreakLength : kMSCWithoutBreakLength);
}

RemotePortNegotiationCommand::RemotePortNegotiationCommand(
    CommandResponse command_response, DLCI dlci)
    : MuxCommand(MuxCommandType::kRemotePortNegotiationCommand,
                 command_response),
      short_RPN_command_(true),
      dlci_(dlci),
      params_(kDefaultRemotePortNegotiationParams),
      mask_(kDefaultRemotePortNegotiationMaskBitfield) {}

RemotePortNegotiationCommand::RemotePortNegotiationCommand(
    CommandResponse command_response, DLCI dlci,
    RemotePortNegotiationParams params, RemotePortNegotiationMaskBitfield mask)
    : MuxCommand(MuxCommandType::kRemotePortNegotiationCommand,
                 command_response),
      short_RPN_command_(false),
      dlci_(dlci),
      params_(params),
      mask_(mask) {}

std::unique_ptr<RemotePortNegotiationCommand>
RemotePortNegotiationCommand::Parse(CommandResponse command_response,
                                    size_t length, const ByteBuffer& buffer) {
  DLCI dlci = buffer[2] >> kRPNDLCIShift;

  if (length == kRPNShortLength) {
    return std::make_unique<RemotePortNegotiationCommand>(command_response,
                                                          dlci);
  }

  RemotePortNegotiationParams params;
  RemotePortNegotiationMaskBitfield mask;

  // TODO(gusss): again, this kind of casting is probably not a good idea.
  params.baud = static_cast<Baud>(buffer[3]);
  params.data_bits = static_cast<DataBits>(buffer[4] & kRPNDataBitsMask);
  params.stop_bits =
      static_cast<StopBits>(buffer[4] >> kRPNStopBitsShift & kRPNStopBitsMask);
  params.parity = buffer[4] & kRPNParityMask << kRPNParityShift;
  params.parity_type = static_cast<ParityType>(
      buffer[4] >> kRPNParityTypeShift & kRPNParityTypeMask);
  params.flow_control = buffer[5];
  params.xon_character = buffer[6];
  params.xoff_character = buffer[7];
  mask = buffer[8] << 8 | buffer[9];

  return std::make_unique<RemotePortNegotiationCommand>(
      command_response, dlci, std::move(params), std::move(mask));
}

void RemotePortNegotiationCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // EA bit = 1.
  buffer[kLengthIndex] =
      ((short_RPN_command_ ? kRPNShortLength : kRPNLongLength)
       << kLengthShift) |
      kEAMask;
  // EA bit = 1, bit 2 = 1.
  buffer[2] = kEAMask | (1 << 1) | (dlci_ << kRPNDLCIShift);

  if (short_RPN_command_)
    return;

  // See GSM table 11.
  buffer[3] = static_cast<uint8_t>(params_.baud);
  buffer[4] = static_cast<uint8_t>(params_.data_bits);
  buffer[4] |= (static_cast<bool>(params_.stop_bits) << kRPNStopBitsShift);
  buffer[4] |= (params_.parity << kRPNParityShift);
  buffer[4] |=
      (static_cast<uint8_t>(params_.parity_type) << kRPNParityTypeShift);
  buffer[5] = params_.flow_control;
  buffer[6] = params_.xon_character;
  buffer[7] = params_.xoff_character;
  buffer[8] = static_cast<uint8_t>(mask_ >> 8);
  buffer[9] = static_cast<uint8_t>(mask_);
}

size_t RemotePortNegotiationCommand::written_size() const {
  return 2ul + (short_RPN_command_ ? kRPNShortLength : kRPNLongLength);
}

RemoteLineStatusCommand::RemoteLineStatusCommand(
    CommandResponse command_response, DLCI dlci, bool error_occurred,
    LineError error)
    : MuxCommand(MuxCommandType::kRemoteLineStatusCommand, command_response),
      dlci_(dlci),
      error_occurred_(error_occurred),
      error_(error) {}

std::unique_ptr<RemoteLineStatusCommand> RemoteLineStatusCommand::Parse(
    CommandResponse command_response, const ByteBuffer& buffer) {
  DLCI dlci = buffer[2] >> kRLSDLCIShift;
  bool error_occurred = buffer[3] & kRLSErrorOccurredMask;
  // TODO(gusss)
  LineError error =
      static_cast<LineError>(buffer[3] >> kRLSErrorShift & kRLSErrorMask);

  return std::make_unique<RemoteLineStatusCommand>(command_response, dlci,
                                                   error_occurred, error);
}

void RemoteLineStatusCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // EA bit = 1.
  buffer[kLengthIndex] = (kRLSLength << kLengthShift) | kEAMask;
  // EA bit = 1, bit 2 = 1.
  buffer[2] = kEAMask | (1 << 1) | (dlci_ << kRLSDLCIShift);
  buffer[3] =
      error_occurred_ | (static_cast<uint8_t>(error_) << kRLSErrorShift);
}

size_t RemoteLineStatusCommand::written_size() const {
  return 2ul + kRLSLength;
}

NonSupportedCommandResponse::NonSupportedCommandResponse(
    CommandResponse incoming_command_response,
    uint8_t incoming_non_supported_command)
    : MuxCommand(MuxCommandType::kNonSupportedCommandResponse,
                 CommandResponse::kResponse),
      incoming_command_response_(incoming_command_response),
      incoming_non_supported_command_(incoming_non_supported_command) {}

std::unique_ptr<NonSupportedCommandResponse> NonSupportedCommandResponse::Parse(
    CommandResponse command_response, const ByteBuffer& buffer) {
  CommandResponse incoming_command_response = buffer[2] & kCRMask
                                                  ? CommandResponse::kCommand
                                                  : CommandResponse::kResponse;
  uint8_t incoming_non_supported_command =
      buffer[2] >> kNSCNotSupportedCommandShift;

  return std::make_unique<NonSupportedCommandResponse>(
      incoming_command_response, incoming_non_supported_command);
}

void NonSupportedCommandResponse::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // EA bit = 1.
  buffer[kLengthIndex] = (kNSCLength << kLengthShift) | kEAMask;
  buffer[2] = kEAMask |
              (incoming_command_response_ == CommandResponse::kCommand ? 1 : 0)
                  << kNSCCRShift |
              incoming_non_supported_command_ << kNSCNotSupportedCommandShift;
}

size_t NonSupportedCommandResponse::written_size() const {
  return 2ul + kNSCLength;
}

DLCParameterNegotiationCommand::DLCParameterNegotiationCommand(
    CommandResponse command_response, ParameterNegotiationParams params)
    : MuxCommand(MuxCommandType::kDLCParameterNegotiation, command_response),
      params_(params) {}

std::unique_ptr<DLCParameterNegotiationCommand>
DLCParameterNegotiationCommand::Parse(CommandResponse command_response,
                                      const ByteBuffer& buffer) {
  ParameterNegotiationParams params;

  params.dlci = buffer[2];
  params.credit_based_flow_handshake = static_cast<CreditBasedFlowHandshake>(
      buffer[3] >> kPNCreditBasedFlowHandshakeShift);
  params.priority = buffer[4];
  params.maximum_frame_size = buffer[6] | buffer[7] << 8;
  params.initial_credits = buffer[9];

  return std::make_unique<DLCParameterNegotiationCommand>(command_response,
                                                          params);
}

void DLCParameterNegotiationCommand::Write(MutableBufferView buffer) const {
  ZX_ASSERT(buffer.size() >= written_size());
  buffer[kTypeIndex] = type_field_octet();
  // EA bit = 1.
  buffer[kLengthIndex] = (kPNLength << kLengthShift) | kEAMask;

  buffer[2] = params_.dlci & kPNDLCIMask;
  buffer[3] = static_cast<uint8_t>(params_.credit_based_flow_handshake)
              << kPNCreditBasedFlowHandshakeShift;
  buffer[4] = params_.priority & kPNPriorityMask;
  buffer[5] = 0;
  buffer[6] = static_cast<uint8_t>(params_.maximum_frame_size);
  buffer[7] = static_cast<uint8_t>(params_.maximum_frame_size >> 8);
  buffer[8] = 0;
  buffer[9] = params_.initial_credits & kPNInitialCreditsMask;
}

size_t DLCParameterNegotiationCommand::written_size() const {
  return 2ul + kPNLength;
}

}  // namespace rfcomm
}  // namespace bt
