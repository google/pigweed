// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"

#include <endian.h>
#include <lib/fit/function.h>

#include <iterator>
#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::l2cap::internal {

template <typename OptionT, typename PayloadT>
DynamicByteBuffer EncodeOption(PayloadT payload) {
  DynamicByteBuffer buffer(OptionT::kEncodedSize);
  MutablePacketView<ConfigurationOption> option(&buffer, OptionT::kPayloadLength);
  option.mutable_header()->type = OptionT::kType;
  option.mutable_header()->length = OptionT::kPayloadLength;
  option.mutable_payload_data().WriteObj(payload);
  return buffer;
}

// Compares length field in option header with expected option payload length for that option type.
// Returns true if lengths match, false otherwise.
template <typename OptionT>
bool CheckHeaderLengthField(PacketView<ConfigurationOption> option) {
  if (option.header().length != OptionT::kPayloadLength) {
    bt_log(WARN, "l2cap",
           "received channel configuration option with incorrect length (type: %#.2x, "
           "length: %hhu, expected length: %hhu)",
           static_cast<uint8_t>(option.header().type), option.header().length,
           OptionT::kPayloadLength);
    return false;
  }
  return true;
}

// ChannelConfiguration::Reader implementation
bool ChannelConfiguration::ReadOptions(const ByteBuffer& options_payload) {
  auto remaining_view = options_payload.view();
  while (remaining_view.size() != 0) {
    size_t bytes_read = ReadNextOption(remaining_view);

    // Check for read failure
    if (bytes_read == 0) {
      return false;
    }

    remaining_view = remaining_view.view(bytes_read);
  }
  return true;
}

size_t ChannelConfiguration::ReadNextOption(const ByteBuffer& options) {
  if (options.size() < sizeof(ConfigurationOption)) {
    bt_log(WARN, "l2cap",
           "tried to decode channel configuration option from buffer with invalid size (size: %lu)",
           options.size());
    return 0;
  }

  size_t remaining_size = options.size() - sizeof(ConfigurationOption);
  PacketView<ConfigurationOption> option(&options, remaining_size);

  // Check length against buffer bounds.
  if (option.header().length > remaining_size) {
    bt_log(WARN, "l2cap",
           "decoded channel configuration option with length greater than remaining buffer size "
           "(length: %hhu, remaining: %zu)",
           option.header().length, remaining_size);
    return 0;
  }

  switch (option.header().type) {
    case OptionType::kMTU:
      if (!CheckHeaderLengthField<MtuOption>(option)) {
        return 0;
      }
      OnReadMtuOption(MtuOption(option.payload_data()));
      return MtuOption::kEncodedSize;
    case OptionType::kRetransmissionAndFlowControl:
      if (!CheckHeaderLengthField<RetransmissionAndFlowControlOption>(option)) {
        return 0;
      }
      OnReadRetransmissionAndFlowControlOption(
          RetransmissionAndFlowControlOption(option.payload_data()));
      return RetransmissionAndFlowControlOption::kEncodedSize;
    case OptionType::kFlushTimeout:
      if (!CheckHeaderLengthField<FlushTimeoutOption>(option)) {
        return 0;
      }
      OnReadFlushTimeoutOption(FlushTimeoutOption(option.payload_data()));
      return FlushTimeoutOption::kEncodedSize;
    default:
      bt_log(DEBUG, "l2cap", "decoded unsupported channel configuration option (type: %#.2x)",
             static_cast<uint8_t>(option.header().type));

      UnknownOption unknown_option(option.header().type, option.header().length,
                                   option.payload_data());
      size_t option_size = unknown_option.size();

      OnReadUnknownOption(std::move(unknown_option));

      return option_size;
  }
}

// MtuOption implementation

ChannelConfiguration::MtuOption::MtuOption(const ByteBuffer& data_buf) {
  mtu_ = letoh16(data_buf.ReadMember<&MtuOptionPayload::mtu>());
}

DynamicByteBuffer ChannelConfiguration::MtuOption::Encode() const {
  return EncodeOption<MtuOption>(MtuOptionPayload{htole16(mtu_)});
}

std::string ChannelConfiguration::MtuOption::ToString() const {
  return bt_lib_cpp_string::StringPrintf("[type: MTU, mtu: %hu]", mtu_);
}

// RetransmissionAndFlowControlOption implementation

ChannelConfiguration::RetransmissionAndFlowControlOption
ChannelConfiguration::RetransmissionAndFlowControlOption::MakeBasicMode() {
  return RetransmissionAndFlowControlOption(ChannelMode::kBasic, 0, 0, 0, 0, 0);
}

ChannelConfiguration::RetransmissionAndFlowControlOption
ChannelConfiguration::RetransmissionAndFlowControlOption::MakeEnhancedRetransmissionMode(
    uint8_t tx_window_size, uint8_t max_transmit, uint16_t rtx_timeout, uint16_t monitor_timeout,
    uint16_t mps) {
  return RetransmissionAndFlowControlOption(ChannelMode::kEnhancedRetransmission, tx_window_size,
                                            max_transmit, rtx_timeout, monitor_timeout, mps);
}
ChannelConfiguration::RetransmissionAndFlowControlOption::RetransmissionAndFlowControlOption(
    ChannelMode mode, uint8_t tx_window_size, uint8_t max_transmit, uint16_t rtx_timeout,
    uint16_t monitor_timeout, uint16_t mps)
    : mode_(mode),
      tx_window_size_(tx_window_size),
      max_transmit_(max_transmit),
      rtx_timeout_(rtx_timeout),
      monitor_timeout_(monitor_timeout),
      mps_(mps) {}

ChannelConfiguration::RetransmissionAndFlowControlOption::RetransmissionAndFlowControlOption(
    const ByteBuffer& data_buf) {
  const auto option_payload = data_buf.To<RetransmissionAndFlowControlOptionPayload>();
  mode_ = option_payload.mode;
  tx_window_size_ = option_payload.tx_window_size;
  max_transmit_ = option_payload.max_transmit;
  rtx_timeout_ = letoh16(option_payload.rtx_timeout);
  monitor_timeout_ = letoh16(option_payload.monitor_timeout);
  mps_ = letoh16(option_payload.mps);
}

DynamicByteBuffer ChannelConfiguration::RetransmissionAndFlowControlOption::Encode() const {
  RetransmissionAndFlowControlOptionPayload payload;
  payload.mode = mode_;
  payload.tx_window_size = tx_window_size_;
  payload.max_transmit = max_transmit_;
  payload.rtx_timeout = htole16(rtx_timeout_);
  payload.monitor_timeout = htole16(monitor_timeout_);
  payload.mps = mps_;
  return EncodeOption<RetransmissionAndFlowControlOption>(payload);
}

std::string ChannelConfiguration::RetransmissionAndFlowControlOption::ToString() const {
  return bt_lib_cpp_string::StringPrintf(
      "[type: RtxFlowControl, mode: %hhu, tx window size: %hhu, max transmit: %hhu, rtx timeout: "
      "%hu, monitor timeout: %hu, max pdu payload size: %hu]",
      static_cast<uint8_t>(mode_), tx_window_size_, max_transmit_, rtx_timeout_, monitor_timeout_,
      mps_);
}

// FlushTimeoutOption implementation

ChannelConfiguration::FlushTimeoutOption::FlushTimeoutOption(const ByteBuffer& data_buf) {
  flush_timeout_ = letoh16(data_buf.ReadMember<&FlushTimeoutOptionPayload::flush_timeout>());
}

DynamicByteBuffer ChannelConfiguration::FlushTimeoutOption::Encode() const {
  FlushTimeoutOptionPayload payload;
  payload.flush_timeout = htole16(flush_timeout_);
  return EncodeOption<FlushTimeoutOption>(payload);
}

std::string ChannelConfiguration::FlushTimeoutOption::ToString() const {
  return bt_lib_cpp_string::StringPrintf("[type: FlushTimeout, flush timeout: %hu]",
                                         flush_timeout_);
}

// UnknownOption implementation

ChannelConfiguration::UnknownOption::UnknownOption(OptionType type, uint8_t length,
                                                   const ByteBuffer& data)
    : type_(type), payload_(BufferView(data, length)) {}

DynamicByteBuffer ChannelConfiguration::UnknownOption::Encode() const {
  DynamicByteBuffer buffer(size());
  MutablePacketView<ConfigurationOption> option(&buffer, payload_.size());
  option.mutable_header()->type = type_;
  option.mutable_header()->length = static_cast<uint8_t>(payload_.size());

  // Raw data is already in little endian
  option.mutable_payload_data().Write(payload_);

  return buffer;
}

bool ChannelConfiguration::UnknownOption::IsHint() const {
  // An option is a hint if its MSB is 1.
  const uint8_t kMSBMask = 0x80;
  return static_cast<uint8_t>(type_) & kMSBMask;
}

std::string ChannelConfiguration::UnknownOption::ToString() const {
  return bt_lib_cpp_string::StringPrintf("[type: %#.2hhx, length: %zu]",
                                         static_cast<unsigned char>(type_), payload_.size());
}

// ChannelConfiguration implementation

ChannelConfiguration::ConfigurationOptions ChannelConfiguration::Options() const {
  ConfigurationOptions options;
  if (mtu_option_) {
    options.push_back(ConfigurationOptionPtr(new MtuOption(*mtu_option_)));
  }

  if (retransmission_flow_control_option_) {
    options.push_back(ConfigurationOptionPtr(
        new RetransmissionAndFlowControlOption(*retransmission_flow_control_option_)));
  }

  if (flush_timeout_option_) {
    options.push_back(ConfigurationOptionPtr(new FlushTimeoutOption(*flush_timeout_option_)));
  }

  return options;
}

std::string ChannelConfiguration::ToString() const {
  std::string str("{");

  std::vector<std::string> options;
  if (mtu_option_) {
    options.push_back(mtu_option_->ToString());
  }
  if (retransmission_flow_control_option_) {
    options.push_back(retransmission_flow_control_option_->ToString());
  }
  if (flush_timeout_option_) {
    options.push_back(flush_timeout_option_->ToString());
  }
  for (auto& option : unknown_options_) {
    options.push_back(option.ToString());
  }

  for (auto it = options.begin(); it != options.end(); it++) {
    str += *it;
    if (it != options.end() - 1) {
      str += ", ";
    }
  }
  str += "}";
  return str;
}

void ChannelConfiguration::Merge(ChannelConfiguration other) {
  if (other.mtu_option_) {
    mtu_option_ = other.mtu_option_;
  }

  if (other.retransmission_flow_control_option_) {
    retransmission_flow_control_option_ = other.retransmission_flow_control_option_;
  }

  if (other.flush_timeout_option_) {
    flush_timeout_option_ = other.flush_timeout_option_;
  }

  unknown_options_.insert(unknown_options_.end(),
                          std::make_move_iterator(other.unknown_options_.begin()),
                          std::make_move_iterator(other.unknown_options_.end()));
}

void ChannelConfiguration::OnReadUnknownOption(UnknownOption option) {
  // Drop unknown hint options
  if (!option.IsHint()) {
    unknown_options_.push_back(std::move(option));
  }
}

}  // namespace bt::l2cap::internal
