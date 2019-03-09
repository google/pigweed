// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_MUX_COMMANDS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_MUX_COMMANDS_H_

#include <cstddef>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "rfcomm.h"

namespace btlib {
namespace rfcomm {

// Defined in GSM 5.4.6.3.*.
// clang-format off
enum class MuxCommandType : uint8_t {
  kDLCParameterNegotiation      = 0b10000000,
  kTestCommand                  = 0b00100000,
  kFlowControlOnCommand         = 0b10100000,
  kFlowControlOffCommand        = 0b01100000,
  kModemStatusCommand           = 0b11100000,
  kNonSupportedCommandResponse  = 0b00010000,
  kRemotePortNegotiationCommand = 0b10010000,
  kRemoteLineStatusCommand      = 0b01010000
};
// clang-format on

// A MuxCommand represents an RFCOMM multiplexer command which is ready to be
// sent or which has been parsed from a ByteBuffer. Specifically, each
// MuxCommand can be seen as a collection of fields and their accessors, plus a
// function Write which knows how to write these fields into a buffer
// in the format defined by the RFCOMM/GSM specs.
//
// MuxCommands are used in two ways:
//  1. Parsing multiplexer commands out of a received buffer and reading their
//      fields
//  2. Constructing multiplexer commands and writing them into a buffer to send
//
// To use MuxCommands in the first way, use MuxCommand::Parse() on a received
// buffer. Check the type using command_type(), and cast the returned pointer
// to the appropriate subclass.
//
// TODO(gusss): this downcasting mechanism is non-ideal -- it is potentially
// very bug-prone. NET-1224 tracks the improvement of this mechanism.
//
// To use MuxCommands in the second way, construct the specific MuxCommand
// subtype which you are trying to send, allocate a buffer (using written_size()
// to calculate the needed size), and write into the buffer using Write().
//
// Supported commands are listed in RFCOMM 4.3.
class MuxCommand {
 public:
  // Parses a buffer and constructs the appropriate subclass of MuxCommand.
  // Users of Parse should read the resulting command type and cast the pointer
  // to the appropriate MuxCommand subclass.
  static std::unique_ptr<MuxCommand> Parse(const common::ByteBuffer& buffer);

  virtual ~MuxCommand() = default;

  // Writes this MuxCommand into |buffer|. |buffer| must be at least the size
  // indicated by written_size().
  virtual void Write(common::MutableBufferView buffer) const = 0;

  // The amount of space this command takes up when written. This is used when
  // allocating space for an RFCOMM frame which will hold a multiplexer command.
  virtual size_t written_size() const = 0;

  inline MuxCommandType command_type() const { return command_type_; }

  // The multiplexer-command-level C/R setting. This setting is described in GSM
  // 5.4.6.2.
  inline CommandResponse command_response() const { return command_response_; }

 protected:
  MuxCommand(MuxCommandType command_type, CommandResponse command_response);

  // Forms the type field octet for this multiplexer command by ORing:
  //  - the EA bit (which is always 1 for GSM/RFCOMM -- see GSM 5.4.6.1)
  //  - the C/R bit (whose value is defined in 5.4.6.2), and
  //  - the type octet for the command in question (see 5.4.6.3.*).
  inline uint8_t type_field_octet() const {
    return 1 | ((command_response_ == CommandResponse::kCommand ? 1 : 0) << 1) |
           (uint8_t)command_type_;
  }

  MuxCommandType command_type_;
  CommandResponse command_response_;
};

// This command is used to test the connection between two peers. The command
// contains an attached test pattern, which the peer must repeat back in full.
// See GSM 5.4.6.3.4.
//
// Multiplexer frames can encode an arbitrary number of length octets. In our
// implementation, the length of the pattern contained within the test command
// is limited by the system's max value of size_t.
class TestCommand : public MuxCommand {
 public:
  TestCommand(CommandResponse command_response,
              const common::ByteBuffer& test_pattern);

  // Returns nullptr if parse fails. |command_response| and |length| are
  // parameters which have already been parsed from |buffer|.
  static std::unique_ptr<TestCommand> Parse(CommandResponse command_response,
                                            size_t length,
                                            const common::ByteBuffer& buffer);

  inline common::BufferView test_pattern() const {
    return test_pattern_.view();
  }

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;

 private:
  common::DynamicByteBuffer test_pattern_;
};

// This command is sent when our device is able to begin receiving messages. See
// GSM 5.4.6.3.5.
class FlowControlOnCommand : public MuxCommand {
 public:
  explicit FlowControlOnCommand(CommandResponse command_response);

  // Returns nullptr if parse fails. |command_response| is a parameter which has
  // already been parsed from |buffer|.
  static std::unique_ptr<FlowControlOnCommand> Parse(
      CommandResponse command_response);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;
};

// This command is sent when our device is not able to receive messages. See
// GSM 5.4.6.3.6.
class FlowControlOffCommand : public MuxCommand {
 public:
  explicit FlowControlOffCommand(CommandResponse command_response);

  // Returns nullptr if parse fails. |command_response| is a parameter which has
  // already been parsed from |buffer|.
  static std::unique_ptr<FlowControlOffCommand> Parse(
      CommandResponse command_response);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;
};

// These signals map to various V.24 signals as described in GSM tables 6 and 7.
// TODO(armansito): Remove this and use the bitfield directly. There is no
// benefit to unpacking a bitfield as a struct of bools and it is inefficient.
struct ModemStatusCommandSignals {
  bool flow_control;
  bool ready_to_communicate;
  bool ready_to_receive;
  bool incoming_call;
  bool data_valid;
};

using BreakValue = uint8_t;
constexpr BreakValue kMinBreakValue = 0b0000;
constexpr BreakValue kMaxBreakValue = 0b1111;
constexpr BreakValue kDefaultInvalidBreakValue = 0xFF;

// This command conveys the virtual V.24 signals. See GSM 5.4.6.3.7.
class ModemStatusCommand : public MuxCommand {
 public:
  ModemStatusCommand(CommandResponse command_response, DLCI dlci,
                     ModemStatusCommandSignals signals,
                     BreakValue break_value = kDefaultInvalidBreakValue);

  // Returns nullptr if parse fails. |command_response| and |length| are
  // parameters which have already been parsed from |buffer|.
  static std::unique_ptr<ModemStatusCommand> Parse(
      CommandResponse command_response, size_t length,
      const common::ByteBuffer& buffer);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;

  size_t written_size() const override;

  inline DLCI dlci() const { return dlci_; }

  inline ModemStatusCommandSignals signals() const { return signals_; }

  // We only include a break signal if the break value is valid.
  bool has_break_signal() const {
    return break_value_ >= kMinBreakValue && break_value_ <= kMaxBreakValue;
  }

  inline BreakValue break_value() const { return break_value_; }

 private:
  DLCI dlci_;
  ModemStatusCommandSignals signals_;
  BreakValue break_value_;
};

// GSM table 12
// clang-format off
enum class Baud : uint8_t {
  k2400   = 0,
  k4800   = 1,
  k7200   = 2,
  k9600   = 3,
  k19200  = 4,
  k38400  = 5,
  k57600  = 6,
  k115200 = 7,
  k230400 = 8
};
// clang-format on
constexpr Baud kDefaultBaud = Baud::k9600;

// GSM 5.4.6.3.9 (below table 12)
enum class DataBits : uint8_t {
  k5Bits = 0,
  k6Bits = 2,
  k7Bits = 1,
  k8Bits = 3
};
constexpr DataBits kDefaultDataBits = DataBits::k8Bits;

// GSM 5.4.6.3.9 (below table 12)
// clang-format off
enum class StopBits : bool {
  k1Bit         = 0,
  k1AndHalfBits = 1
};
// clang-format on
constexpr StopBits kDefaultStopBits = StopBits::k1Bit;

// GSM 5.4.6.3.9 (below table 12)
// clang-format off
enum class ParityType : uint8_t {
  kOdd    = 0,
  kMark   = 1,
  kEven   = 2,
  kSpace  = 3
};
// clang-format on
// No default is defined in the spec, so we choose even arbitrarily.
constexpr ParityType kDefaultParityType = ParityType::kEven;
// Whether parity is on or off by default. This is not defined in the spec.
constexpr bool kDefaultParity = true;

// GSM 5.4.6.3.9 (below table 12)
// clang-format off
enum FlowControlFlags : uint8_t {
  kXonXoffInputFlag   = 1 << 0,
  kXonXoffOutputFlag  = 1 << 1,
  kRTRInputFlag       = 1 << 2,
  kRTROutputFlag      = 1 << 3,
  kRTCInputFlag       = 1 << 4,
  kRTCOutputFlag      = 1 << 5
};
// clang-format on
using FlowControlFlagsBitfield = uint8_t;
// GSM 5.4.6.3.9 (below table 12)
constexpr FlowControlFlagsBitfield kDefaultFlowControlFlags = 0;

struct RemotePortNegotiationParams {
  Baud baud;
  DataBits data_bits;
  StopBits stop_bits;
  bool parity;
  ParityType parity_type;
  FlowControlFlagsBitfield flow_control;
  uint8_t xon_character;
  uint8_t xoff_character;
};
// GSM 5.4.6.3.9 (below table 12)
constexpr uint8_t kDefaultXONCharacter = 0x01;   // DC1
constexpr uint8_t kDefaultXOFFCharacter = 0x03;  // DC3
constexpr RemotePortNegotiationParams kDefaultRemotePortNegotiationParams = {
    kDefaultBaud,         kDefaultDataBits,     kDefaultStopBits,
    kDefaultParity,       kDefaultParityType,   kDefaultFlowControlFlags,
    kDefaultXONCharacter, kDefaultXOFFCharacter};

// GSM 5.4.6.3.9 (below table 12)
// clang-format off
enum RemotePortNegotiationMask : uint16_t {
  kXonXoffInput   = 1 << 0,
  kXonXoffOutput  = 1 << 1,
  kRTRInput       = 1 << 2,
  kRTROutput      = 1 << 3,
  kRTCInput       = 1 << 4,
  kRTCOutput      = 1 << 5,
  kBitRate        = 1 << 8,
  kDataBits       = 1 << 9,
  kStopBits       = 1 << 10,
  kParity         = 1 << 11,
  kParityType     = 1 << 12,
  kXONCharacter   = 1 << 13,
  kXOFFCharacter  = 1 << 14
};
// clang-format on
using RemotePortNegotiationMaskBitfield = uint16_t;
// This default is not defined in the spec.
constexpr RemotePortNegotiationMaskBitfield
    kDefaultRemotePortNegotiationMaskBitfield = 0;

// This command is used to negotiate port settings such as baud rate. See GSM
// 5.4.6.3.9.
class RemotePortNegotiationCommand : public MuxCommand {
 public:
  // Creates an RPN command with one value octet. This type of RPN command is
  // used to request the remote port settings.
  RemotePortNegotiationCommand(CommandResponse command_response, DLCI dlci);

  // Creates an RPN command with eight value octets. This type of RPN command is
  // used to negotiate port settings.
  RemotePortNegotiationCommand(CommandResponse command_response, DLCI dlci,
                               RemotePortNegotiationParams params,
                               RemotePortNegotiationMaskBitfield mask);

  // Returns nullptr if parse fails. |command_response| and |length| are
  // parameters which have already been parsed from |buffer|.
  static std::unique_ptr<RemotePortNegotiationCommand> Parse(
      CommandResponse command_response, size_t length,
      const common::ByteBuffer& buffer);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;

  // The DLCI which this RPN command is negotiating over.
  inline DLCI dlci() const { return dlci_; }

  // The Remote Port Negotiation parameters. These are described in detail in
  // GSM 5.4.6.3.9.
  inline RemotePortNegotiationParams params() const { return params_; }

  // The mask used to indicate which parameters are being negotiated.
  inline RemotePortNegotiationMaskBitfield mask() const { return mask_; }

 private:
  // Indicates whether this is the short (1 value octet) or long (8 value
  // octets) version of the RPN command.
  bool short_RPN_command_;

  DLCI dlci_;

  RemotePortNegotiationParams params_;
  RemotePortNegotiationMaskBitfield mask_;
};

// GSM 5.4.6.3.10 (below table 15)
// clang-format off
enum class LineError : uint8_t {
  kOverrunError = (1 << 0),
  kParityError  = (1 << 1),
  kFramingError = (1 << 2)
};
// clang-format on

// This command is used to convey changes in the status of the line. See GSM
// 5.4.6.3.10.
class RemoteLineStatusCommand : public MuxCommand {
 public:
  RemoteLineStatusCommand(CommandResponse command_response, DLCI dlci,
                          bool error_occurred, LineError error);

  // Returns nullptr if parse fails. |command_response| is a parameter which has
  // already been parsed from |buffer|.
  static std::unique_ptr<RemoteLineStatusCommand> Parse(
      CommandResponse command_response, const common::ByteBuffer& buffer);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;

  // The DLCI which this command pertains to.
  inline DLCI dlci() const { return dlci_; }

  // Whether or not this frame contains an error.
  inline bool error_occurred() const { return error_occurred_; }

  // The type of error which occurred. See GSM 5.4.6.3.10 for an explanation of
  // the errors.
  inline LineError error() const { return error_; }

 private:
  DLCI dlci_;
  bool error_occurred_;
  LineError error_;
};

// RFCOMM table 5.3.
enum class CreditBasedFlowHandshake : uint8_t {
  kUnsupported = 0x0,
  kSupportedRequest = 0xF,
  kSupportedResponse = 0xE
};

using Priority = uint8_t;
constexpr Priority kMinPriority = 0;
constexpr Priority kMaxPriority = 63;

// See GSM 5.4.6.3.1 and the modifications presented in RFCOMM 5.5.3.
struct ParameterNegotiationParams {
  // 6 bits wide.
  DLCI dlci;
  CreditBasedFlowHandshake credit_based_flow_handshake;
  Priority priority;
  uint16_t maximum_frame_size;
  InitialCredits initial_credits;
};

// This command is used prior to opening a DLC. It is used to set up the
// parameters of the DLC. See GSM 5.4.6.3.1 and the modifications described in
// RFCOMM 5.5.3.
class DLCParameterNegotiationCommand : public MuxCommand {
 public:
  DLCParameterNegotiationCommand(CommandResponse command_response,
                                 ParameterNegotiationParams params);

  // Returns nullptr if parse fails. |command_response| is a parameter which has
  // already been parsed from |buffer|.
  static std::unique_ptr<DLCParameterNegotiationCommand> Parse(
      CommandResponse command_response, const common::ByteBuffer& buffer);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;

  inline ParameterNegotiationParams params() const { return params_; }

 private:
  ParameterNegotiationParams params_;
};

// This response is sent when we receive a command which we do not recognize or
// support. See GSM 5.4.6.3.8.
class NonSupportedCommandResponse : public MuxCommand {
 public:
  // Note that NSC is always a response.
  // |incoming_non_supported_command| is 6 bits.
  NonSupportedCommandResponse(CommandResponse incoming_command_response,
                              uint8_t incoming_non_supported_command);

  // Returns nullptr if parse fails. |command_response| is a parameter which has
  // already been parsed from |buffer|.
  static std::unique_ptr<NonSupportedCommandResponse> Parse(
      CommandResponse command_response, const common::ByteBuffer& buffer);

  // MuxCommand overrides
  void Write(common::MutableBufferView buffer) const override;
  size_t written_size() const override;

  inline CommandResponse incoming_command_response() const {
    return incoming_command_response_;
  }
  inline uint8_t incoming_non_supported_command() const {
    return incoming_non_supported_command_;
  }

 private:
  CommandResponse incoming_command_response_;
  uint8_t incoming_non_supported_command_;
};

}  // namespace rfcomm
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_MUX_COMMANDS_H_
