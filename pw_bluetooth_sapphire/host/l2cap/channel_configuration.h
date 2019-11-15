// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_CONFIGURATION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_CONFIGURATION_H_

#include <list>
#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace l2cap {
namespace internal {

// The ChannelConfiguration class is used for decoding and encoding channel configuration options
// (Core spec v5.1, Vol 3, Part A, Section 5) that are in the payloads of Configuration Requests
// (Section 4.4) and Configuration Responses (Section 4.5). To decode a configuration option payload
// in a configuration request/response signaling channel packet, instantiate a new
// ChannelConfiguration and read options into it via ReadOptions() (which may fail to decode invalid
// option payloads). Unknown options (ones we don't support) that are not hints (types 0x80-0xFF)
// are available via unknown_options(). Unknown options should always be handled by rejecting the
// packet containing them (Section 5).
//
// Decoding a configuration option payload example:
//   ChannelConfiguration config;
//   if (!config.ReadOptions(payload_buffer)) {
//     // Handle decoding failure
//   }
//   if (config.unknown_options.size() != 0) {
//     // Reject packet
//   }
//   if (config.mtu_option()) {
//     // Handle mtu option
//   }
//
// Subsequent configuration payload buffers and objects can be merged into an existing configuration
// with additional calls to ReadOptions() or Merge(), which will overwrite current options with the
// new ones.
//
// A channel configuration can be iterated through and encoded with ChannelConfiguration::Options(),
// which returns a vector of non-null options. All options can then be encoded into a
// DynamicByteBuffer with *.Encode().
//
// Encoding channel configuration options example:
//   ChannelConfiguration config;
//   config.set_mtu_option(MtuOption(mtu));
//   ConfigurationOption options = config.Options();
//   DynamicByteBuffer encoded_mtu = options[0].Encode();
//   payload_byte_buffer.Write(encoded_mtu);
class ChannelConfiguration final {
 public:
  class ConfigurationOptionInterface {
   public:
    virtual ~ConfigurationOptionInterface() = default;

    virtual DynamicByteBuffer Encode() const = 0;
    virtual std::string ToString() const = 0;
    virtual OptionType type() const = 0;
    virtual size_t size() const = 0;
  };

  using ConfigurationOptionPtr = std::unique_ptr<ConfigurationOptionInterface>;
  using ConfigurationOptions = std::vector<ConfigurationOptionPtr>;

  // Maximum Transmission Unit option (Core Spec v5.1, Vol 3, Part A, Sec 5.1).
  // Specifies the max SDU size the sender is capable of accepting on a channel.
  class MtuOption final : public ConfigurationOptionInterface {
   public:
    static constexpr OptionType kType = OptionType::kMTU;
    static constexpr uint8_t kPayloadLength = sizeof(MtuOptionPayload);
    static constexpr size_t kEncodedSize = sizeof(ConfigurationOption) + kPayloadLength;

    explicit MtuOption(uint16_t mtu) : mtu_(mtu) {}

    // |data_buf| must contain encoded Mtu option data. The option will be initialized with the
    // decoded fields.
    explicit MtuOption(const ByteBuffer& data_buf);

    uint16_t mtu() const { return mtu_; };

    // ConfigurationOption overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    uint16_t mtu_;
  };

  // Retransmission and Flow Control option (Core Spec v5.1, Vol 3, Part A, Sec 5.4).
  // Specifies channel transmission mode and the values of related parameters.
  class RetransmissionAndFlowControlOption final : public ConfigurationOptionInterface {
   public:
    static constexpr OptionType kType = OptionType::kRetransmissionAndFlowControl;
    static constexpr uint8_t kPayloadLength = sizeof(RetransmissionAndFlowControlOptionPayload);
    static constexpr size_t kEncodedSize = sizeof(ConfigurationOption) + kPayloadLength;

    RetransmissionAndFlowControlOption(ChannelMode mode, uint8_t tx_window_size,
                                       uint8_t max_transmit, uint16_t rtx_timeout,
                                       uint16_t monitor_timeout, uint16_t mps);

    // |data_buf| must contain encoded Retransmission And Flow Control option data. The option will
    // be initialized with the decoded fields.
    explicit RetransmissionAndFlowControlOption(const ByteBuffer& data_buf);

    ChannelMode mode() const { return mode_; }

    uint8_t tx_window_size() const { return tx_window_size_; }

    uint8_t max_transmit() const { return max_transmit_; }

    uint16_t rtx_timeout() const { return rtx_timeout_; }

    uint16_t monitor_timeout() const { return monitor_timeout_; }

    // Maximum PDU size
    uint16_t mps() const { return mps_; }

    // ConfigurationOption overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    ChannelMode mode_;
    uint8_t tx_window_size_;
    uint8_t max_transmit_;
    uint16_t rtx_timeout_;
    uint16_t monitor_timeout_;
    uint16_t mps_;
  };

  // Unknown options that are not hints must be stored and sent in the Configuration Response.
  // (Core Spec v5.1, Vol 3, Sec 4.5)
  class UnknownOption final : public ConfigurationOptionInterface {
   public:
    UnknownOption(OptionType type, uint8_t length, const ByteBuffer& data);

    // Returns true if the unkown option is a hint and may be ignored/skipped. Returns false if
    // the unknown option may not be ignored (the request containing this option must be
    // refused).
    bool IsHint() const;

    // uint8_t payload_length() const { return payload_length_; }

    const ByteBuffer& payload() const { return payload_; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    // Only includes the type, since options can't be decoded.
    std::string ToString() const override;

    OptionType type() const override { return type_; }

    size_t size() const override { return sizeof(ConfigurationOption) + payload_.size(); }

   private:
    OptionType type_;
    // Raw option payload buffer, since we can't parse it but we must create a response with it.
    DynamicByteBuffer payload_;
  };

  // Update this configuration based on other configuration's options. Used for accumulating
  // configuration options sent in a series of packets. Options that are already set in this
  // configuration will be overwritten if they are set in |other|. Does not merge unknown options.
  void Merge(const ChannelConfiguration& other);

  // Read encoded list of configuration options from |buffer| and update options accordingly.
  // Returns true if all options decoded successfully.
  [[nodiscard]] bool ReadOptions(const ByteBuffer& options_payload);

  // Convenience method that returns a vector containing only the options that have been set. Does
  // not include unknown options.
  ConfigurationOptions Options() const;

  // Returns a user-friendly string representation. This is intended for debug messages
  std::string ToString() const;

  void set_mtu_option(std::optional<MtuOption> option) { mtu_option_ = std::move(option); }

  void set_retransmission_flow_control_option(
      std::optional<RetransmissionAndFlowControlOption> option) {
    retransmission_flow_control_option_ = std::move(option);
  }

  // Returns MtuOption only if it has been previously read or set.
  const std::optional<MtuOption>& mtu_option() const { return mtu_option_; }

  // Returns RetransmissionAndFlowControlOption only if it has been previously read or set.
  const std::optional<RetransmissionAndFlowControlOption>& retransmission_flow_control_option()
      const {
    return retransmission_flow_control_option_;
  };

  // Returns unknown options previously decoded by |ReadOptions|. Used for responding to peer with
  // rejected options.
  const std::vector<UnknownOption>& unknown_options() const { return unknown_options_; }

 private:
  // Decoding callbacks
  void OnReadMtuOption(MtuOption option) { mtu_option_ = option; }
  void OnReadRetransmissionAndFlowControlOption(RetransmissionAndFlowControlOption option) {
    retransmission_flow_control_option_ = option;
  }
  void OnReadUnknownOption(UnknownOption option);

  // Returns number of bytes read. A return value of 0 indicates failure to read option.
  size_t ReadNextOption(const ByteBuffer& options);

  std::optional<MtuOption> mtu_option_;
  std::optional<RetransmissionAndFlowControlOption> retransmission_flow_control_option_;
  std::vector<UnknownOption> unknown_options_;
};  // ChannelConfiguration

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_CONFIGURATION_H_
