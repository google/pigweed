// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"

namespace bt::l2cap::internal {
namespace {

using MtuOption = ChannelConfiguration::MtuOption;
using RetransmissionAndFlowControlOption = ChannelConfiguration::RetransmissionAndFlowControlOption;
using FlushTimeoutOption = ChannelConfiguration::FlushTimeoutOption;
using UnknownOption = ChannelConfiguration::UnknownOption;

constexpr auto kUnknownOptionType = static_cast<OptionType>(0x10);

const MtuOption kMtuOption(48);
const RetransmissionAndFlowControlOption kRetransmissionAndFlowControlOption =
    RetransmissionAndFlowControlOption::MakeBasicMode();
const FlushTimeoutOption kFlushTimeoutOption(200);

class ChannelConfigurationTest : public ::testing::Test {
 public:
  ChannelConfigurationTest() = default;
  ~ChannelConfigurationTest() override = default;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelConfigurationTest);
};

TEST_F(ChannelConfigurationTest, ReadAllOptionTypes) {
  const StaticByteBuffer kOptionBuffer(
      // MTU Option
      static_cast<uint8_t>(OptionType::kMTU), MtuOption::kPayloadLength, LowerBits(kMinACLMTU),
      UpperBits(kMinACLMTU),
      // Rtx Option
      static_cast<uint8_t>(OptionType::kRetransmissionAndFlowControl),
      RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Flush Timeout option (timeout = 200)
      static_cast<uint8_t>(OptionType::kFlushTimeout), FlushTimeoutOption::kPayloadLength, 0xc8,
      0x00,
      // Unknown option, Type = 0x70, Length = 1, payload = 0x03
      0x70, 0x01, 0x03);

  ChannelConfiguration config;

  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));

  EXPECT_TRUE(config.mtu_option());
  EXPECT_EQ(kMinACLMTU, config.mtu_option()->mtu());

  EXPECT_TRUE(config.retransmission_flow_control_option());
  EXPECT_EQ(ChannelMode::kRetransmission, config.retransmission_flow_control_option()->mode());

  EXPECT_TRUE(config.flush_timeout_option());
  EXPECT_EQ(200u, config.flush_timeout_option()->flush_timeout());

  EXPECT_EQ(1u, config.unknown_options().size());
  auto& option = config.unknown_options().front();
  EXPECT_EQ(0x70, static_cast<uint8_t>(option.type()));
  EXPECT_EQ(1u, option.payload().size());
  EXPECT_EQ(0x03, option.payload().data()[0]);
}

TEST_F(ChannelConfigurationTest, ReadTooShortOption) {
  // clang-format off
  // missing required Length field
  StaticByteBuffer kEncodedOption(
      // Type = QoS
      0x03);
  //clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedOption));
}

TEST_F(ChannelConfigurationTest, ReadInvalidOptionField) {
  // clang-format off
  StaticByteBuffer kEncodedOption(
      // Length = 255
      static_cast<uint8_t>(kUnknownOptionType), 0xFF);
  // clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedOption));
}

TEST_F(ChannelConfigurationTest, ReadIncorrectOptionLength) {
  // clang-format off
  StaticByteBuffer kEncodedMtuOption(
      // Type = MTU, Length = 3 (spec length is 2)
      OptionType::kMTU, 0x03, 0x00, 0x00, 0x00);
  //clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedMtuOption));

  // clang-format off
  StaticByteBuffer kEncodedRetransmissionOption(
      // Type, Length = 1 (spec length is 9)
    OptionType::kRetransmissionAndFlowControl, 0x01, 0x00);
  //clang-format on

  EXPECT_FALSE(config.ReadOptions(kEncodedRetransmissionOption));

  StaticByteBuffer kEncodedFlushTimeoutOption(
    // Type, Length = 1 (spec length is 2)
    OptionType::kFlushTimeout, 0x01, 0x00);

  EXPECT_FALSE(config.ReadOptions(kEncodedFlushTimeoutOption));
}

TEST_F(ChannelConfigurationTest, MtuOptionDecodeEncode) {
  constexpr uint16_t kMTU = 48;

  // clang-format off
  StaticByteBuffer kExpectedEncodedMtuOption(
      // Type = MTU, Length = 2
      0x01, 0x02,
      // MTU = 48
      LowerBits(kMTU), UpperBits(kMTU));
  //clang-format on
  ChannelConfiguration::MtuOption mtu_option(kExpectedEncodedMtuOption.view(sizeof(ConfigurationOption)));

  EXPECT_EQ(mtu_option.mtu(), kMTU);

  EXPECT_TRUE(ContainersEqual(mtu_option.Encode(), kExpectedEncodedMtuOption));
}

TEST_F(ChannelConfigurationTest, RetransmissionAndFlowControlOptionDecodeEncode) {
  const auto kMode = ChannelMode::kRetransmission;
  const uint8_t kTxWindow = 32;
  const uint8_t kMaxTransmit = 1;
  const uint16_t kRtxTimeout = 512;
  const uint16_t kMonitorTimeout = 528;
  const uint16_t kMaxPDUPayloadSize = 256;

  // clang-format off
  StaticByteBuffer kExpectedEncodedRetransmissionAndFlowControlOption(
      // Type = rtx and flow control, Length = 9
      0x04, 0x09,
      static_cast<uint8_t>(kMode), kTxWindow, kMaxTransmit,
      LowerBits(kRtxTimeout), UpperBits(kRtxTimeout),
      LowerBits(kMonitorTimeout), UpperBits(kMonitorTimeout),
      LowerBits(kMaxPDUPayloadSize), UpperBits(kMaxPDUPayloadSize)
      );
  // clang-format on

  ChannelConfiguration::RetransmissionAndFlowControlOption rtx_option(
      kExpectedEncodedRetransmissionAndFlowControlOption.view(sizeof(ConfigurationOption)));
  EXPECT_EQ(kTxWindow, rtx_option.tx_window_size());
  EXPECT_EQ(kMaxTransmit, rtx_option.max_transmit());
  EXPECT_EQ(kRtxTimeout, rtx_option.rtx_timeout());
  EXPECT_EQ(kMonitorTimeout, rtx_option.monitor_timeout());
  EXPECT_EQ(kMaxPDUPayloadSize, rtx_option.mps());
  EXPECT_TRUE(
      ContainersEqual(rtx_option.Encode(), kExpectedEncodedRetransmissionAndFlowControlOption));
}

TEST_F(ChannelConfigurationTest, FlushTimeoutOptionDecodeEncode) {
  const uint16_t kFlushTimeout = 200;

  StaticByteBuffer kExpectedEncodedFlushTimeoutOption(
      // flush timeout type = 0x02, length = 2
      0x02, 0x02, LowerBits(kFlushTimeout), UpperBits(kFlushTimeout));

  FlushTimeoutOption option(kExpectedEncodedFlushTimeoutOption.view(sizeof(ConfigurationOption)));

  EXPECT_EQ(kFlushTimeout, option.flush_timeout());
  EXPECT_EQ(kExpectedEncodedFlushTimeoutOption, option.Encode());
}

TEST_F(ChannelConfigurationTest, UnknownOptionDecodeEncode) {
  // clang-format off
  StaticByteBuffer kExpectedEncodedUnknownOption(
      kUnknownOptionType, 0x02, // Length = 2
      0x01, 0x02); // random data
  // clang-format on

  ChannelConfiguration::UnknownOption unknown_option(
      kUnknownOptionType, 2, kExpectedEncodedUnknownOption.view(sizeof(ConfigurationOption)));
  EXPECT_TRUE(ContainersEqual(unknown_option.Encode(), kExpectedEncodedUnknownOption));
}

TEST_F(ChannelConfigurationTest, UnknownOptionHints) {
  constexpr auto kHintOptionType = static_cast<OptionType>(0x80);
  constexpr auto kNotHintOptionType = static_cast<OptionType>(0x70);
  StaticByteBuffer data(0x01);
  ChannelConfiguration::UnknownOption unknown_option_hint(kHintOptionType, 1, data);
  EXPECT_TRUE(unknown_option_hint.IsHint());
  ChannelConfiguration::UnknownOption unknown_option(kNotHintOptionType, 1, data);
  EXPECT_FALSE(unknown_option.IsHint());
}

TEST_F(ChannelConfigurationTest, OptionsReturnsKnownOptions) {
  // Empty configuration
  EXPECT_EQ(0u, ChannelConfiguration().Options().size());

  // Single option
  ChannelConfiguration config;
  config.set_mtu_option(kMtuOption);
  auto options0 = config.Options();
  // |options0| should not include all options
  EXPECT_EQ(1u, options0.size());
  EXPECT_EQ(OptionType::kMTU, options0[0]->type());

  // All options
  config.set_retransmission_flow_control_option(kRetransmissionAndFlowControlOption);
  config.set_flush_timeout_option(kFlushTimeoutOption);
  auto options1 = config.Options();
  EXPECT_EQ(3u, options1.size());

  const auto OptionsContainsOptionOfType = [](ChannelConfiguration::ConfigurationOptions& options,
                                              OptionType type) {
    return std::find_if(options.begin(), options.end(),
                        [&](auto& option) { return option->type() == type; }) != options.end();
  };
  constexpr std::array<OptionType, 3> AllOptionTypes = {
      OptionType::kRetransmissionAndFlowControl, OptionType::kMTU, OptionType::kFlushTimeout};
  for (auto& type : AllOptionTypes) {
    EXPECT_TRUE(OptionsContainsOptionOfType(options1, type));
  }

  // Remove mtu option
  config.set_mtu_option(std::nullopt);
  auto options2 = config.Options();
  EXPECT_EQ(2u, options2.size());
  EXPECT_FALSE(OptionsContainsOptionOfType(options2, OptionType::kMTU));
}

TEST_F(ChannelConfigurationTest, MergingConfigurations) {
  ChannelConfiguration config0;
  config0.set_mtu_option(kMtuOption);
  config0.set_retransmission_flow_control_option(kRetransmissionAndFlowControlOption);
  config0.set_flush_timeout_option(kFlushTimeoutOption);

  // Test merging options to empty config
  ChannelConfiguration config1;
  config1.Merge(config0);

  EXPECT_EQ(kMtuOption.mtu(), config1.mtu_option()->mtu());
  EXPECT_EQ(kRetransmissionAndFlowControlOption.mode(),
            config1.retransmission_flow_control_option()->mode());
  EXPECT_EQ(kFlushTimeoutOption.flush_timeout(), config1.flush_timeout_option()->flush_timeout());

  // Test overwriting options
  constexpr uint16_t kMtu = 96;
  config0.set_mtu_option(MtuOption(kMtu));
  constexpr auto kMode = ChannelMode::kEnhancedRetransmission;
  config0.set_retransmission_flow_control_option(
      RetransmissionAndFlowControlOption::MakeEnhancedRetransmissionMode(0, 0, 0, 0, 0));
  constexpr uint16_t kTimeout = 150;
  config0.set_flush_timeout_option(FlushTimeoutOption(kTimeout));

  config1.Merge(config0);

  EXPECT_EQ(kMtu, config1.mtu_option()->mtu());
  EXPECT_EQ(kMode, config1.retransmission_flow_control_option()->mode());
  EXPECT_EQ(kTimeout, config1.flush_timeout_option()->flush_timeout());
}

TEST_F(ChannelConfigurationTest, MergingUnknownOptionsAppendsThem) {
  const StaticByteBuffer kUnknownOption0(
      // code, length, payload
      0x70, 0x01, 0x01);
  const StaticByteBuffer kUnknownOption1(
      // code, length, payload
      0x71, 0x01, 0x01);
  ChannelConfiguration config0;
  EXPECT_TRUE(config0.ReadOptions(kUnknownOption0));
  EXPECT_EQ(1u, config0.unknown_options().size());

  ChannelConfiguration config1;
  EXPECT_TRUE(config1.ReadOptions(kUnknownOption1));
  EXPECT_EQ(1u, config1.unknown_options().size());

  config0.Merge(std::move(config1));
  EXPECT_EQ(2u, config0.unknown_options().size());
  EXPECT_EQ(kUnknownOption0.data()[0], static_cast<uint8_t>(config0.unknown_options()[0].type()));
  EXPECT_EQ(kUnknownOption1.data()[0], static_cast<uint8_t>(config0.unknown_options()[1].type()));

  ChannelConfiguration config2;
  EXPECT_TRUE(config2.ReadOptions(kUnknownOption0));
  EXPECT_EQ(1u, config2.unknown_options().size());

  // Merge duplicate of kUnknownOption0. Duplicates should be appended.
  config0.Merge(std::move(config2));
  EXPECT_EQ(3u, config0.unknown_options().size());
  EXPECT_EQ(kUnknownOption0.data()[0], static_cast<uint8_t>(config0.unknown_options()[0].type()));
  EXPECT_EQ(kUnknownOption0.data()[0], static_cast<uint8_t>(config0.unknown_options()[2].type()));
}

TEST_F(ChannelConfigurationTest, ReadOptions) {
  const StaticByteBuffer kOptionBuffer(
      // MTU Option
      OptionType::kMTU, MtuOption::kPayloadLength, LowerBits(kMinACLMTU), UpperBits(kMinACLMTU),
      // Rtx Option
      OptionType::kRetransmissionAndFlowControl, RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Flush Timeout option (timeout = 200)
      static_cast<uint8_t>(OptionType::kFlushTimeout), FlushTimeoutOption::kPayloadLength, 0xc8,
      0x00,
      // Unknown option, Type = 0x70, Length = 1
      0x70, 0x01, 0x03,
      // Unknown option (hint), Type = 0x80, Length = 0
      0x80, 0x00);
  ChannelConfiguration config;
  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));

  EXPECT_TRUE(config.mtu_option().has_value());
  EXPECT_EQ(kMinACLMTU, config.mtu_option()->mtu());

  EXPECT_TRUE(config.retransmission_flow_control_option().has_value());
  EXPECT_EQ(ChannelMode::kRetransmission, config.retransmission_flow_control_option()->mode());

  EXPECT_TRUE(config.flush_timeout_option().has_value());
  EXPECT_EQ(200u, config.flush_timeout_option()->flush_timeout());

  // Hint should have been dropped
  EXPECT_EQ(1u, config.unknown_options().size());
  auto& option = config.unknown_options().front();
  EXPECT_EQ(0x70, static_cast<uint8_t>(option.type()));
  EXPECT_EQ(1u, option.payload().size());
  EXPECT_EQ(0x03, option.payload().data()[0]);
}

TEST_F(ChannelConfigurationTest, ConfigToString) {
  const StaticByteBuffer kOptionBuffer(
      // MTU Option
      static_cast<uint8_t>(OptionType::kMTU), MtuOption::kPayloadLength, LowerBits(kMinACLMTU),
      UpperBits(kMinACLMTU),
      // Rtx Option
      static_cast<uint8_t>(OptionType::kRetransmissionAndFlowControl),
      RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Flush Timeout option (timeout = 200)
      static_cast<uint8_t>(OptionType::kFlushTimeout), FlushTimeoutOption::kPayloadLength, 0xc8,
      0x00,
      // Unknown option, Type = 0x70, Length = 1, payload = 0x03
      0x70, 0x01, 0x03);

  ChannelConfiguration config;
  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));
  const std::string kExpected =
      "{[type: MTU, mtu: 48], "
      "[type: RtxFlowControl, mode: 1, tx window size: 0, max transmit: 0, rtx timeout: 0, monitor "
      "timeout: 0, max pdu payload size: 0], "
      "[type: FlushTimeout, flush timeout: 200], "
      "[type: 0x70, length: 1]}";
  EXPECT_EQ(kExpected, config.ToString());
}

}  // namespace
}  // namespace bt::l2cap::internal
