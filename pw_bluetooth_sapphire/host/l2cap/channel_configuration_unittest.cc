// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"

#include "fbl/macros.h"
#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

using MtuOption = ChannelConfiguration::MtuOption;
using RetransmissionAndFlowControlOption = ChannelConfiguration::RetransmissionAndFlowControlOption;
using UnknownOption = ChannelConfiguration::UnknownOption;

constexpr auto kUnknownOptionType = static_cast<OptionType>(0x10);

const ChannelConfiguration::MtuOption kMtuOption(48);
const ChannelConfiguration::RetransmissionAndFlowControlOption kRetransmissionAndFlowControlOption(
    ChannelMode::kBasic, 1, 1, 1, 1, 1);

class L2CAP_ChannelConfigurationTest : public ::testing::Test {
 public:
  L2CAP_ChannelConfigurationTest() = default;
  ~L2CAP_ChannelConfigurationTest() override = default;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(L2CAP_ChannelConfigurationTest);
};

TEST_F(L2CAP_ChannelConfigurationTest, ReadAllOptionTypes) {
  const auto kOptionBuffer = CreateStaticByteBuffer(
      // MTU Option
      static_cast<uint8_t>(OptionType::kMTU), MtuOption::kPayloadLength, LowerBits(kMinACLMTU),
      UpperBits(kMinACLMTU),
      // Rtx Option
      static_cast<uint8_t>(OptionType::kRetransmissionAndFlowControl),
      RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Unknown option, Type = 0x70, Length = 1, payload = 0x03
      0x70, 0x01, 0x03);

  ChannelConfiguration config;

  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));

  EXPECT_TRUE(config.mtu_option());
  EXPECT_EQ(kMinACLMTU, config.mtu_option()->mtu());

  EXPECT_TRUE(config.retransmission_flow_control_option());
  EXPECT_EQ(ChannelMode::kRetransmission, config.retransmission_flow_control_option()->mode());

  EXPECT_EQ(1u, config.unknown_options().size());
  auto& option = config.unknown_options().front();
  EXPECT_EQ(0x70, static_cast<uint8_t>(option.type()));
  EXPECT_EQ(1u, option.payload().size());
  EXPECT_EQ(0x03, option.payload().data()[0]);
}

TEST_F(L2CAP_ChannelConfigurationTest, ReadTooShortOption) {
  // clang-format off
  // missing required Length field
  auto kEncodedOption = CreateStaticByteBuffer(
      // Type = QoS
      0x03);
  //clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedOption));
}

TEST_F(L2CAP_ChannelConfigurationTest, ReadInvalidOptionField) {
  // clang-format off
  auto kEncodedOption = CreateStaticByteBuffer(
      // Length = 255
      static_cast<uint8_t>(kUnknownOptionType), 0xFF);
  // clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedOption));
}

TEST_F(L2CAP_ChannelConfigurationTest, ReadIncorrectOptionLength) {
  // clang-format off
  auto kEncodedMtuOption = CreateStaticByteBuffer(
      // Type = MTU, Length = 3 (spec length is 2)
      OptionType::kMTU, 0x03, 0x00, 0x00, 0x00);
  //clang-format on

  ChannelConfiguration config;
  EXPECT_FALSE(config.ReadOptions(kEncodedMtuOption));

  // clang-format off
  auto kEncodedRetransmissionOption = CreateStaticByteBuffer(
      // Type, Length = 1 (spec length is 9)
    OptionType::kRetransmissionAndFlowControl, 0x01, 0x00);
  //clang-format on

  EXPECT_FALSE(config.ReadOptions(kEncodedRetransmissionOption));
}

TEST_F(L2CAP_ChannelConfigurationTest, MtuOptionDecodeEncode) {
  constexpr uint16_t kMTU = 48;

  // clang-format off
  auto kExpectedEncodedMtuOption = CreateStaticByteBuffer(
      // Type = MTU, Length = 2
      0x01, 0x02,
      // MTU = 48
      LowerBits(kMTU), UpperBits(kMTU));
  //clang-format on
  ChannelConfiguration::MtuOption mtu_option(kExpectedEncodedMtuOption.view(sizeof(ConfigurationOption)));

  EXPECT_EQ(mtu_option.mtu(), kMTU);

  EXPECT_TRUE(ContainersEqual(mtu_option.Encode(), kExpectedEncodedMtuOption));
}

TEST_F(L2CAP_ChannelConfigurationTest, RetransmissionAndFlowControlOptionDecodeEncode) {
  const auto kMode = ChannelMode::kRetransmission;
  const uint8_t kTxWindow = 32;
  const uint8_t kMaxTransmit = 1;
  const uint16_t kRtxTimeout = 512;
  const uint16_t kMonitorTimeout = 528;
  const uint16_t kMaxPDUPayloadSize = 256;

  // clang-format off
  auto kExpectedEncodedRetransmissionAndFlowControlOption = CreateStaticByteBuffer(
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

TEST_F(L2CAP_ChannelConfigurationTest, UnknownOptionDecodeEncode) {
  // clang-format off
  auto kExpectedEncodedUnknownOption = CreateStaticByteBuffer(
      kUnknownOptionType, 0x02, // Length = 2
      0x01, 0x02); // random data
  // clang-format on

  ChannelConfiguration::UnknownOption unknown_option(
      kUnknownOptionType, 2, kExpectedEncodedUnknownOption.view(sizeof(ConfigurationOption)));
  EXPECT_TRUE(ContainersEqual(unknown_option.Encode(), kExpectedEncodedUnknownOption));
}

TEST_F(L2CAP_ChannelConfigurationTest, UnknownOptionHints) {
  constexpr auto kHintOptionType = static_cast<OptionType>(0x80);
  constexpr auto kNotHintOptionType = static_cast<OptionType>(0x70);
  auto data = CreateStaticByteBuffer(0x01);
  ChannelConfiguration::UnknownOption unknown_option_hint(kHintOptionType, 1, data);
  EXPECT_TRUE(unknown_option_hint.IsHint());
  ChannelConfiguration::UnknownOption unknown_option(kNotHintOptionType, 1, data);
  EXPECT_FALSE(unknown_option.IsHint());
}

TEST_F(L2CAP_ChannelConfigurationTest, OptionsReturnsKnownOptions) {
  // Empty configuration
  EXPECT_EQ(0u, ChannelConfiguration().Options().size());

  ChannelConfiguration config;
  constexpr uint8_t kMtu = 50;
  config.set_mtu_option(MtuOption(kMtu));
  auto options0 = config.Options();
  // |options0| should not include all options
  EXPECT_EQ(1u, options0.size());
  EXPECT_EQ(OptionType::kMTU, options0[0]->type());

  // Set all options
  config.set_retransmission_flow_control_option(kRetransmissionAndFlowControlOption);
  auto options1 = config.Options();
  EXPECT_EQ(2u, options1.size());
  EXPECT_TRUE(std::find_if(options1.begin(), options1.end(), [](auto& option) {
                return option->type() == OptionType::kRetransmissionAndFlowControl;
              }) != options1.end());
  EXPECT_TRUE(std::find_if(options1.begin(), options1.end(), [](auto& option) {
                return option->type() == OptionType::kMTU;
              }) != options1.end());

  // Remove mtu option
  config.set_mtu_option(std::nullopt);
  auto options2 = config.Options();
  EXPECT_EQ(1u, options2.size());
  EXPECT_EQ(OptionType::kRetransmissionAndFlowControl, options2[0]->type());
}

TEST_F(L2CAP_ChannelConfigurationTest, MergingConfigurations) {
  ChannelConfiguration config0;
  config0.set_mtu_option(kMtuOption);

  ChannelConfiguration config1;
  config1.set_retransmission_flow_control_option(kRetransmissionAndFlowControlOption);

  EXPECT_FALSE(config0.retransmission_flow_control_option().has_value());
  config0.Merge(config1);
  EXPECT_TRUE(config0.mtu_option().has_value());
  EXPECT_TRUE(config0.retransmission_flow_control_option().has_value());

  // Test that |config2|'s MTU option overwrites |config0|'s MTU option.
  ChannelConfiguration config2;
  config2.set_mtu_option(MtuOption(96));

  config0.Merge(config2);
  EXPECT_EQ(96u, config0.mtu_option()->mtu());
}

TEST_F(L2CAP_ChannelConfigurationTest, ReadOptions) {
  const auto kOptionBuffer = CreateStaticByteBuffer(
      // MTU Option
      OptionType::kMTU, MtuOption::kPayloadLength, LowerBits(kMinACLMTU), UpperBits(kMinACLMTU),
      // Rtx Option
      OptionType::kRetransmissionAndFlowControl, RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Unknown option, Type = 0x70, Length = 0
      0x70, 0x00,
      // Unknown option (hint), Type = 0x80, Length = 0
      0x80, 0x00);
  ChannelConfiguration config;
  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));

  EXPECT_TRUE(config.mtu_option().has_value());
  EXPECT_EQ(kMinACLMTU, config.mtu_option()->mtu());

  EXPECT_TRUE(config.retransmission_flow_control_option().has_value());
  EXPECT_EQ(ChannelMode::kRetransmission, config.retransmission_flow_control_option()->mode());

  // Hint should have been dropped
  EXPECT_EQ(1u, config.unknown_options().size());
  EXPECT_EQ(0x70, static_cast<uint8_t>(config.unknown_options()[0].type()));
}

TEST_F(L2CAP_ChannelConfigurationTest, ConfigToString) {
  const auto kOptionBuffer = CreateStaticByteBuffer(
      // MTU Option
      static_cast<uint8_t>(OptionType::kMTU), MtuOption::kPayloadLength, LowerBits(kMinACLMTU),
      UpperBits(kMinACLMTU),
      // Rtx Option
      static_cast<uint8_t>(OptionType::kRetransmissionAndFlowControl),
      RetransmissionAndFlowControlOption::kPayloadLength,
      static_cast<uint8_t>(ChannelMode::kRetransmission), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // Unknown option, Type = 0x70, Length = 1, payload = 0x03
      0x70, 0x01, 0x03);

  ChannelConfiguration config;
  EXPECT_TRUE(config.ReadOptions(kOptionBuffer));
  const std::string kExpected =
      "{[type: MTU, mtu: 48], "
      "[type: RtxFlowControl, mode: 1, tx window size: 0, max transmit: 0, rtx timeout: 0, monitor "
      "timeout: 0, max pdu payload size: 0], "
      "[type: 0x70, length: 1]}";
  EXPECT_EQ(kExpected, config.ToString());
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
