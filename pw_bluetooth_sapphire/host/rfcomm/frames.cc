// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "frames.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

#include "rfcomm.h"

namespace btlib {
namespace rfcomm {

namespace {

// FCS utilites. These come from GSM Annex B.
// CRC table for calculating FCS. GSM B.3.5.
const uint8_t crctable[] = {
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C,
    0x09, 0x98, 0xEA, 0x7B, 0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,
    0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67, 0x38, 0xA9, 0xDB, 0x4A,
    0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58,
    0x2D, 0xBC, 0xCE, 0x5F, 0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,
    0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B, 0x6C, 0xFD, 0x8F, 0x1E,
    0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34,
    0x41, 0xD0, 0xA2, 0x33, 0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,
    0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F, 0xE0, 0x71, 0x03, 0x92,
    0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80,
    0xF5, 0x64, 0x16, 0x87, 0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,
    0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3, 0xC4, 0x55, 0x27, 0xB6,
    0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC,
    0x99, 0x08, 0x7A, 0xEB, 0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,
    0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7, 0xA8, 0x39, 0x4B, 0xDA,
    0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8,
    0xBD, 0x2C, 0x5E, 0xCF};

// FCS calculation function from GSM B.3.3.
uint8_t CalculateFCS(const common::ByteBuffer& p) {
  uint8_t fcs = 0xFF;
  size_t offset = 0;
  size_t len = p.size();
  while (len--) {
    fcs = crctable[fcs ^ p[offset++]];
  }
  /* Ones compliment */
  return (0xFF - fcs);
}

// FCS checking function from GSM B.3.4.
bool CheckFCS(uint8_t received_fcs, const common::ByteBuffer& p) {
  uint8_t fcs = 0xFF;
  size_t offset = 0;
  size_t len = p.size();
  while (len--) {
    fcs = crctable[fcs ^ p[offset++]];
  }
  fcs = crctable[fcs ^ received_fcs];
  return fcs == 0xCF;
}

constexpr uint8_t kControlMask = 0b11101111;
constexpr size_t kCRShift = 1;
constexpr size_t kPFShift = 4;
constexpr uint8_t kCRMask = 1 << kCRShift;
constexpr uint8_t kPFMask = 1 << kPFShift;
constexpr uint8_t kEAMask = 0b00000001;
constexpr size_t kAddressIndex = 0;
constexpr size_t kControlIndex = 1;
constexpr size_t kLengthIndex = 2;
constexpr size_t kLengthFirstOctetShift = 1;
constexpr size_t kLengthSecondOctetShift = 7;
constexpr size_t kDLCIShift = 2;
// Address field, control field, length field, FCS field. See RFCOMM 5.2.6.
constexpr size_t kMinimumFrameSize = 4;

// The length field in RFCOMM frames and multiplexer control commands can be
// extended from one octet to two. This function checks whether two octets are
// needed.
bool TwoOctetLength(size_t length) { return length > kMaxSingleOctetLength; }

// Logic for determining whether a frame will have a credit octet based on
// characteristics of the frame and of the Session forming the frame. See RFCOMM
// 6.5.2.
//
// |credit_based_flow| indicates whether credit-based flow control is turned on
// in this RFCOMM session.
inline bool FrameHasCreditOctet(bool credit_based_flow, FrameType frame_type,
                                bool poll_final) {
  return credit_based_flow &&
         (frame_type == FrameType::kUnnumberedInfoHeaderCheck) &&
         poll_final == true;
}

}  // namespace

Frame::Frame(Role role, CommandResponse command_response, DLCI dlci,
             uint8_t control, bool poll_final)
    : role_(role),
      command_response_(command_response),
      dlci_(dlci),
      control_(control),
      poll_final_(poll_final) {}

// To parse an RFCOMM frame, we must parse all the way until the end to check
// the FCS octet. This means there wouldn't be much benefit in adding Parse
// functions for each Frame subclass, as Frame::Parse will already parse all of
// the needed information.
std::unique_ptr<Frame> Frame::Parse(bool credit_based_flow, Role role,
                                    const common::ByteBuffer& buffer) {
  if (buffer.size() < kMinimumFrameSize) {
    bt_log(TRACE, "rfcomm",
           "buffer size of %zu is less than minimum frame size (%zu)",
           buffer.size(), kMinimumFrameSize);
    return nullptr;
  }

  // Parse the frame type and DLCI first and then determine if we can parse the
  // frame. We can parse the frame if the multiplexer is started, or if the
  // multiplexer isn't started and it's a valid pre-mux-startup frame.
  DLCI dlci = buffer[kAddressIndex] >> kDLCIShift;
  FrameType frame_type = (FrameType)(buffer[kControlIndex] & kControlMask);

  if (!IsMultiplexerStarted(role) && !IsMuxStartupFrame(frame_type, dlci)) {
    bt_log(TRACE, "rfcomm", "frame type %u before mux start",
           static_cast<uint8_t>(frame_type));
    return nullptr;
  }

  bool cr_bit = buffer[kAddressIndex] & kCRMask;
  // See table 1 in 5.2.1.2, which describes exactly how the C/R bit is
  // interpreted.
  CommandResponse command_response;
  if (IsMultiplexerStarted(role)) {
    // See table 1 in 5.2.1.2, which describes exactly how the C/R bit is
    // interpreted.
    command_response = (role == Role::kInitiator && cr_bit) ||
                               (role == Role::kResponder && !cr_bit)
                           ? CommandResponse::kCommand
                           : CommandResponse::kResponse;
  } else {
    // This is not defined explicitly in the spec. If the multiplexer isn't
    // started, we assume the frame has the role the sender would take if
    // multiplexer startup succeeds.
    switch (frame_type) {
      case FrameType::kSetAsynchronousBalancedMode:
        command_response =
            cr_bit ? CommandResponse::kCommand : CommandResponse::kResponse;
        break;
      case FrameType::kDisconnectedMode:
      case FrameType::kUnnumberedAcknowledgement:
        command_response =
            cr_bit ? CommandResponse::kResponse : CommandResponse::kCommand;
        break;
      default:
        // TODO(armansito): Add a unit test for this case.
        bt_log(ERROR, "rfcomm", "malformed frame type: %u",
               static_cast<unsigned int>(frame_type));
        return nullptr;
    }
  }
  bool poll_final = buffer[kControlIndex] & kPFMask;

  InformationLength length = buffer[kLengthIndex] >> kLengthFirstOctetShift;
  bool two_octet_length = !(buffer[kLengthIndex] & kEAMask);
  if (two_octet_length) {
    if (buffer.size() <= kLengthIndex + 1) {
      bt_log(WARN, "rfcomm", "buffer ended unexpectedly while parsing length");
      return nullptr;
    }
    length |= buffer[kLengthIndex + 1] << kLengthSecondOctetShift;
  }

  uint8_t credits = 0;
  bool has_credit_octet =
      FrameHasCreditOctet(credit_based_flow, frame_type, poll_final);
  if (has_credit_octet) {
    size_t credit_index = kLengthIndex + (two_octet_length ? 2 : 1);
    if (buffer.size() <= credit_index) {
      bt_log(WARN, "rfcomm", "buffer ended unexpectedly while parsing credits");
      return nullptr;
    }
    credits = buffer[credit_index];
  }

  // Address and control, one or two length octets, plus a possible credits
  // octet.
  size_t header_size =
      2 + (two_octet_length ? 2 : 1) + (has_credit_octet ? 1 : 0);

  // Check the FCS before we do any allocation or copying.
  size_t fcs_index = header_size + length;
  if (buffer.size() <= fcs_index) {
    bt_log(WARN, "rfcomm", "buffer ended unexpectedly while parsing FCS");
    return nullptr;
  }
  uint8_t fcs = buffer[fcs_index];
  size_t num_octets =
      frame_type == FrameType::kUnnumberedInfoHeaderCheck ? 2 : 3;
  if (!CheckFCS(fcs, buffer.view(0, num_octets))) {
    bt_log(WARN, "rfcomm", "FCS check failed");
    return nullptr;
  }

  if (frame_type == FrameType::kUnnumberedInfoHeaderCheck) {
    // Determine whether to parse a MuxControlFrame or a UserDataFrame based on
    // the DLCI.
    if (dlci == kMuxControlDLCI) {
      std::unique_ptr<MuxCommand> mux_command =
          MuxCommand::Parse(buffer.view(header_size, length));
      if (!mux_command) {
        bt_log(WARN, "rfcomm", "unable to parse mux command");
        return nullptr;
      }
      auto mux_command_frame = std::make_unique<MuxCommandFrame>(
          role, credit_based_flow, std::move(mux_command));
      mux_command_frame->set_credits(credits);
      return mux_command_frame;
    } else if (IsUserDLCI(dlci)) {
      common::MutableByteBufferPtr information;
      if (length > 0) {
        information = common::NewSlabBuffer(length);
        buffer.Copy(information.get(), header_size, length);
      }
      auto user_data_frame = std::make_unique<UserDataFrame>(
          role, credit_based_flow, dlci, std::move(information));
      user_data_frame->set_credits(credits);
      return user_data_frame;
    } else {
      bt_log(WARN, "rfcomm",
             "Parsed DLCI %u is not a valid multiplexer or user data channel",
             dlci);
      return nullptr;
    }
  } else {
    return std::make_unique<Frame>(role, command_response, dlci,
                                   uint8_t(frame_type), poll_final);
  }
}

// Write this RFCOMM frame into a buffer.
void Frame::Write(common::MutableBufferView buffer) const {
  ZX_DEBUG_ASSERT(buffer.size() >= header_size());

  // Writes address, control, and length octets.
  WriteHeader(buffer);

  // Begin writing after header.
  size_t offset = header_size();
  ZX_DEBUG_ASSERT(buffer.size() > offset);

  // FCS is calculated on address and control fields for UIH frames, and
  // calculated on address, control, and length fields for all other frames.
  size_t num_octets =
      (FrameType)control_ == FrameType::kUnnumberedInfoHeaderCheck ? 2 : 3;
  uint8_t fcs = CalculateFCS(buffer.view(0, num_octets));
  buffer[offset] = fcs;
}

void Frame::WriteHeader(common::MutableBufferView buffer) const {
  ZX_DEBUG_ASSERT(buffer.size() >= header_size());

  size_t offset = 0;

  uint8_t address = dlci_ << kDLCIShift;
  // Set EA bit (DLCI/address is always 1 octet in length)
  address |= kEAMask;
  // Set C/R bit
  uint8_t command_response_bit;
  if (role_ == Role::kInitiator || role_ == Role::kResponder) {
    // GSM Table 1.
    command_response_bit = (role_ == Role::kInitiator &&
                            command_response_ == CommandResponse::kCommand) ||
                           (role_ == Role::kResponder &&
                            command_response_ == CommandResponse::kResponse);
  } else if (role_ == Role::kUnassigned || role_ == Role::kNegotiating) {
    // There are only specific frames which can be encoded when the multiplexer
    // is not yet started.
    ZX_DEBUG_ASSERT(IsMuxStartupFrame(FrameType(control_), dlci_));

    // TODO(gusss): the spec does not say how we encode the C/R bit when the
    // multiplexer is not yet started.
    // Set the C/R bit as if startup succeeds (see GSM table 1)
    //  - SABM (command) frames become initiator, C/R = 1
    //  - UA (response) frames become responder, C/R = 1
    //  - DM (response) frames come from would-be responder, C/R = 1
    command_response_bit = 1;
  } else {
    ZX_PANIC("unexpected role while writing frame: %u",
             static_cast<unsigned int>(role_));
    return;
  }

  address &= ~kCRMask;
  address |= command_response_bit << kCRShift;
  buffer[offset] = address;
  ++offset;

  uint8_t control = control_;
  control &= ~kPFMask;
  control |= (poll_final_ ? 1 : 0) << kPFShift;
  buffer[offset] = control;
  ++offset;

  uint8_t length_octet = 0;
  const InformationLength length_val = length();
  bool two_octet_length = TwoOctetLength(length_val);
  // Set E/A bit.
  length_octet |= two_octet_length ? 0 : 1;
  length_octet |= length_val << kLengthFirstOctetShift;
  buffer[offset] = length_octet;
  ++offset;
  if (two_octet_length) {
    length_octet = length_val >> kLengthSecondOctetShift;
    buffer[offset] = length_octet;
    ++offset;
  }
}

size_t Frame::header_size() const {
  // Address, control, and one or two length octets.
  return sizeof(uint8_t) + sizeof(uint8_t) +
         sizeof(uint8_t) * (TwoOctetLength(length()) ? 2 : 1);
}

MuxCommandFrame* Frame::AsMuxCommandFrame() {
  ZX_DEBUG_ASSERT(dlci() == 0);
  return static_cast<MuxCommandFrame*>(this);
}

UserDataFrame* Frame::AsUserDataFrame() {
  ZX_DEBUG_ASSERT(IsUserDLCI(dlci()));
  return static_cast<UserDataFrame*>(this);
}

UnnumberedInfoHeaderCheckFrame* Frame::AsUnnumberedInfoHeaderCheckFrame() {
  FrameType type = static_cast<FrameType>(control());
  ZX_DEBUG_ASSERT(type == FrameType::kUnnumberedInfoHeaderCheck);
  return static_cast<UnnumberedInfoHeaderCheckFrame*>(this);
}

DisconnectCommand::DisconnectCommand(Role role, DLCI dlci)
    : Frame(role,
            // DISC frames are always commands.
            CommandResponse::kCommand, dlci, uint8_t(FrameType::kDisconnect),
            // Poll bit is always 1. See GSM 5.4.4.1.
            true) {}

SetAsynchronousBalancedModeCommand::SetAsynchronousBalancedModeCommand(
    Role role, DLCI dlci)
    : Frame(role,
            // SABM frames are always commands.
            CommandResponse::kCommand, dlci,
            uint8_t(FrameType::kSetAsynchronousBalancedMode),
            // Poll bit is always 1. See GSM 5.4.4.1.
            true) {}

UnnumberedAcknowledgementResponse::UnnumberedAcknowledgementResponse(Role role,
                                                                     DLCI dlci)
    : Frame(role,
            // UA frames are always responses.
            CommandResponse::kResponse, dlci,
            uint8_t(FrameType::kUnnumberedAcknowledgement),
            // Final bit is always 1. See GSM 5.4.4.2.
            true) {}

DisconnectedModeResponse::DisconnectedModeResponse(Role role, DLCI dlci)
    : Frame(role,
            // DM frames are always responses.
            CommandResponse::kResponse, dlci,
            uint8_t(FrameType::kDisconnectedMode),
            // Our implementation will always set the final bit to 1. The final
            // bit in a DM response must be 1 when responding negatively to a
            // SABM command (GSM 5.4.1). In all other cases, the P/F bit setting
            // is unspecified and does not matter for DM frames. See, for
            // example, GSM 5.4.4.2: "If an unsolicited DM response is received
            // then the frame shall be processed irrespective of the P/F
            // setting."
            true) {}

UnnumberedInfoHeaderCheckFrame::UnnumberedInfoHeaderCheckFrame(
    Role role, bool credit_based_flow, DLCI dlci)
    : Frame(role,
            // UIH frames are always commands.
            CommandResponse::kCommand, dlci,
            uint8_t(FrameType::kUnnumberedInfoHeaderCheck),
            // P/F bit initially 0, as there should be no credits field (credits
            // initialized to 0).
            false),
      credit_based_flow_(credit_based_flow),
      // Initially no credits.
      credits_(0) {}

void UnnumberedInfoHeaderCheckFrame::set_credits(uint8_t credits) {
  // Can't set credits on a frame if credit-based flow is off.
  if (!credit_based_flow_)
    return;

  credits_ = credits;
  // P/F is used to indicate the presence of a credits octet. We turn on the
  // credits octet if there are credits.
  poll_final_ = credits != 0;
}

size_t UnnumberedInfoHeaderCheckFrame::header_size() const {
  return Frame::header_size()  // Address, control, length
         + sizeof(uint8_t) * (has_credit_octet() ? 1 : 0);  // Credits
}

void UnnumberedInfoHeaderCheckFrame::WriteHeader(
    common::MutableBufferView buffer) const {
  ZX_DEBUG_ASSERT(buffer.size() >= header_size());

  // Write address, control, length
  Frame::WriteHeader(buffer);

  // Write credit octet if it exists.
  if (has_credit_octet()) {
    size_t offset = Frame::header_size();
    buffer[offset] = credits_;
  }
}

UserDataFrame::UserDataFrame(Role role, bool credit_based_flow, DLCI dlci,
                             common::ByteBufferPtr information)
    : UnnumberedInfoHeaderCheckFrame(role, credit_based_flow, dlci),
      information_(std::move(information)) {}

void UserDataFrame::Write(common::MutableBufferView buffer) const {
  ZX_DEBUG_ASSERT(buffer.size() >= written_size());

  WriteHeader(buffer);
  size_t offset = header_size();

  if (information_) {
    buffer.Write(*information_, offset);
    offset += information_->size();
  }

  // FCS is always calculated over first two octets for UIH frames.
  buffer[offset] = CalculateFCS(buffer.view(0, 2));
}

size_t UserDataFrame::written_size() const {
  return UnnumberedInfoHeaderCheckFrame::header_size() +
         length()            // Information
         + sizeof(uint8_t);  // FCS
}

common::ByteBufferPtr UserDataFrame::TakeInformation() {
  return std::move(information_);
}

MuxCommandFrame::MuxCommandFrame(Role role, bool credit_based_flow,
                                 std::unique_ptr<MuxCommand> mux_command)
    : UnnumberedInfoHeaderCheckFrame(role, credit_based_flow, kMuxControlDLCI),
      mux_command_(std::move(mux_command)) {}

void MuxCommandFrame::Write(common::MutableBufferView buffer) const {
  ZX_DEBUG_ASSERT(buffer.size() >= written_size());

  WriteHeader(buffer);

  size_t offset = header_size();
  mux_command_->Write(buffer.mutable_view(offset));
  offset += mux_command_->written_size();

  // FCS is always calculated over first two octets for UIH frames.
  buffer[offset] = CalculateFCS(buffer.view(0, 2));
}

size_t MuxCommandFrame::written_size() const {
  return UnnumberedInfoHeaderCheckFrame::header_size() +
         +mux_command_->written_size()  // MuxCommand
         + sizeof(uint8_t);             // FCS
}

std::unique_ptr<MuxCommand> MuxCommandFrame::TakeMuxCommand() {
  return std::move(mux_command_);
}

}  // namespace rfcomm
}  // namespace btlib
