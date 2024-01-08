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

#pragma once
#include <list>
#include <optional>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

// The ChannelConfiguration class is used for decoding and encoding channel
// configuration options (Core spec v5.1, Vol 3, Part A, Section 5) that are in
// the payloads of Configuration Requests (Section 4.4) and Configuration
// Responses (Section 4.5). To decode a configuration option payload in a
// configuration request/response signaling channel packet, instantiate a new
// ChannelConfiguration and read options into it via ReadOptions() (which may
// fail to decode invalid option payloads). Unknown options (ones we don't
// support) that are not hints (types 0x80-0xFF) are available via
// unknown_options(). Unknown options should always be handled by rejecting the
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
// Subsequent configuration payload buffers and objects can be merged into an
// existing configuration with additional calls to ReadOptions() or Merge(),
// which will overwrite current options with the new ones.
//
// A channel configuration can be iterated through and encoded with
// ChannelConfiguration::Options(), which returns a vector of non-null options.
// All options can then be encoded into a DynamicByteBuffer with *.Encode().
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
    static constexpr size_t kEncodedSize =
        sizeof(ConfigurationOption) + kPayloadLength;

    explicit MtuOption(uint16_t mtu) : mtu_(mtu) {}

    // |data_buf| must contain encoded Mtu option data. The option will be
    // initialized with the decoded fields.
    explicit MtuOption(const ByteBuffer& data_buf);

    uint16_t mtu() const { return mtu_; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    uint16_t mtu_;
  };

  // Retransmission and Flow Control option (Core Spec v5.1, Vol 3, Part A,
  // Sec 5.4). Specifies channel transmission mode and the values of related
  // parameters.
  class RetransmissionAndFlowControlOption final
      : public ConfigurationOptionInterface {
   public:
    static constexpr OptionType kType =
        OptionType::kRetransmissionAndFlowControl;
    static constexpr uint8_t kPayloadLength =
        sizeof(RetransmissionAndFlowControlOptionPayload);
    static constexpr size_t kEncodedSize =
        sizeof(ConfigurationOption) + kPayloadLength;

    static RetransmissionAndFlowControlOption MakeBasicMode();

    static RetransmissionAndFlowControlOption MakeEnhancedRetransmissionMode(
        uint8_t tx_window_size,
        uint8_t max_transmit,
        uint16_t rtx_timeout,
        uint16_t monitor_timeout,
        uint16_t mps);

    // |data_buf| must contain encoded Retransmission And Flow Control option
    // data. The option will be initialized with the decoded fields.
    explicit RetransmissionAndFlowControlOption(const ByteBuffer& data_buf);

    RetransmissionAndFlowControlMode mode() const { return mode_; }

    // TxWindow: receiver capability in request and transmit capability in
    // response (ERTM)
    uint8_t tx_window_size() const { return tx_window_size_; }

    void set_tx_window_size(uint8_t tx_window_size) {
      tx_window_size_ = tx_window_size;
    }

    // MaxTransmit: retransmissions allowed before disconnecting (ERTM: 0 means
    // infinite) in request and ignored in response
    uint8_t max_transmit() const { return max_transmit_; }

    void set_max_transmit(uint8_t max_transmit) {
      max_transmit_ = max_transmit;
    }

    // Retransmission time-out: 0 in request and error detection timeout in
    // response (ERTM)
    uint16_t rtx_timeout() const { return rtx_timeout_; }

    void set_rtx_timeout(uint16_t rtx_timeout) { rtx_timeout_ = rtx_timeout; }

    // Monitor time-out: 0 in request and link loss detection timeout in
    // response (ERTM)
    uint16_t monitor_timeout() const { return monitor_timeout_; }

    void set_monitor_timeout(uint16_t monitor_timeout) {
      monitor_timeout_ = monitor_timeout;
    }

    // Maximum PDU size
    uint16_t mps() const { return mps_; }

    void set_mps(uint16_t mps) { mps_ = mps; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    // If |mode| is kBasic, all other parameters are ignored.
    RetransmissionAndFlowControlOption(RetransmissionAndFlowControlMode mode,
                                       uint8_t tx_window_size,
                                       uint8_t max_transmit,
                                       uint16_t rtx_timeout,
                                       uint16_t monitor_timeout,
                                       uint16_t mps);

    RetransmissionAndFlowControlMode mode_;
    uint8_t tx_window_size_;
    uint8_t max_transmit_;
    uint16_t rtx_timeout_;
    uint16_t monitor_timeout_;
    uint16_t mps_;
  };

  // Frame Check Sequence (FCS) option (Core Spec v5.4, Vol 3, Part A, Sec 5.5)
  // Specifies the type of frame check sequence that will be included on S/I
  // frames. Makes it possible to disable FCS if both entities agree.
  class FrameCheckSequenceOption final : public ConfigurationOptionInterface {
   public:
    static constexpr OptionType kType = OptionType::kFCS;
    static constexpr uint8_t kPayloadLength =
        sizeof(FrameCheckSequenceOptionPayload);
    static constexpr size_t kEncodedSize =
        sizeof(ConfigurationOption) + kPayloadLength;

    explicit FrameCheckSequenceOption(FcsType fcs_type) : fcs_type_(fcs_type) {}

    // |data_buf| must contain encoded Frame Check Sequence option data. The
    // option will be initialized with the encoded FCS type field
    explicit FrameCheckSequenceOption(const ByteBuffer& data_buf);

    FcsType fcs_type() const { return fcs_type_; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    FcsType fcs_type_;
  };

  // Flush Timeout option (Core Spec v5.1, Vol 3, Part A, Sec 5.2).
  // Specifies flush timeout that sender of this option is going to use.
  class FlushTimeoutOption final : public ConfigurationOptionInterface {
   public:
    static constexpr OptionType kType = OptionType::kFlushTimeout;
    static constexpr uint8_t kPayloadLength = sizeof(FlushTimeoutOptionPayload);
    static constexpr size_t kEncodedSize =
        sizeof(ConfigurationOption) + kPayloadLength;

    explicit FlushTimeoutOption(uint16_t flush_timeout)
        : flush_timeout_(flush_timeout) {}

    // |data_buf| must contain encoded Flush Timeout option data. The option
    // will be initialized with the encoded flush timeout field.
    explicit FlushTimeoutOption(const ByteBuffer& data_buf);

    uint16_t flush_timeout() const { return flush_timeout_; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    std::string ToString() const override;

    OptionType type() const override { return kType; }

    size_t size() const override { return kEncodedSize; }

   private:
    uint16_t flush_timeout_;
  };

  // Unknown options that are not hints must be stored and sent in the
  // Configuration Response. (Core Spec v5.1, Vol 3, Sec 4.5)
  class UnknownOption final : public ConfigurationOptionInterface {
   public:
    UnknownOption(OptionType type, uint8_t length, const ByteBuffer& data);

    // Returns true if the unkown option is a hint and may be ignored/skipped.
    // Returns false if the unknown option may not be ignored (the request
    // containing this option must be refused).
    bool IsHint() const;

    const ByteBuffer& payload() const { return payload_; }

    // ConfigurationOptionInterface overrides

    DynamicByteBuffer Encode() const override;

    // Only includes the type, since options can't be decoded.
    std::string ToString() const override;

    OptionType type() const override { return type_; }

    size_t size() const override {
      return sizeof(ConfigurationOption) + payload_.size();
    }

   private:
    OptionType type_;
    // Raw option payload buffer, since we can't parse it but we must create a
    // response with it.
    DynamicByteBuffer payload_;
  };

  // Update this configuration based on other configuration's options. Used for
  // accumulating configuration options sent in a series of packets. Options
  // that are already set in this configuration will be overwritten if they are
  // set in |other|. Unknown options will be appended to the unknown options in
  // this configuration.
  void Merge(ChannelConfiguration other);

  // Read encoded list of configuration options from |buffer| and update options
  // accordingly. Returns true if all options decoded successfully.
  [[nodiscard]] bool ReadOptions(const ByteBuffer& options_payload);

  // Convenience method that returns a vector containing only the options that
  // have been set. Does not include unknown options.
  ConfigurationOptions Options() const;

  // Returns a user-friendly string representation. This is intended for debug
  // messages
  std::string ToString() const;

  void set_mtu_option(std::optional<MtuOption> option) {
    mtu_option_ = std::move(option);
  }

  void set_retransmission_flow_control_option(
      std::optional<RetransmissionAndFlowControlOption> option) {
    retransmission_flow_control_option_ = std::move(option);
  }

  void set_flush_timeout_option(std::optional<FlushTimeoutOption> option) {
    flush_timeout_option_ = std::move(option);
  }

  void set_frame_check_sequence_option(
      std::optional<FrameCheckSequenceOption> option) {
    fcs_option_ = std::move(option);
  }

  // Returns MtuOption only if it has been previously read or set.
  const std::optional<MtuOption>& mtu_option() const { return mtu_option_; }

  // Returns RetransmissionAndFlowControlOption only if it has been previously
  // read or set.
  const std::optional<RetransmissionAndFlowControlOption>&
  retransmission_flow_control_option() const {
    return retransmission_flow_control_option_;
  }

  const std::optional<FlushTimeoutOption>& flush_timeout_option() const {
    return flush_timeout_option_;
  }

  // Returns FrameCheckSequenceOption only if it has been previously set.
  const std::optional<FrameCheckSequenceOption>& frame_check_sequence_option()
      const {
    return fcs_option_;
  }

  // Returns unknown options previously decoded by |ReadOptions|. Used for
  // responding to peer with rejected options.
  const std::vector<UnknownOption>& unknown_options() const {
    return unknown_options_;
  }

 private:
  // Decoding callbacks
  void OnReadMtuOption(MtuOption option) { mtu_option_ = option; }
  void OnReadRetransmissionAndFlowControlOption(
      RetransmissionAndFlowControlOption option) {
    retransmission_flow_control_option_ = option;
  }
  void OnReadFrameCheckSequenceOption(FrameCheckSequenceOption option) {
    fcs_option_ = option;
  }
  void OnReadFlushTimeoutOption(FlushTimeoutOption option) {
    flush_timeout_option_ = option;
  }
  void OnReadUnknownOption(UnknownOption option);

  // Returns number of bytes read. A return value of 0 indicates failure to read
  // option.
  size_t ReadNextOption(const ByteBuffer& options);

  std::optional<MtuOption> mtu_option_;
  std::optional<RetransmissionAndFlowControlOption>
      retransmission_flow_control_option_;
  std::optional<FrameCheckSequenceOption> fcs_option_;
  std::optional<FlushTimeoutOption> flush_timeout_option_;
  std::vector<UnknownOption> unknown_options_;
};  // ChannelConfiguration

}  // namespace bt::l2cap::internal
