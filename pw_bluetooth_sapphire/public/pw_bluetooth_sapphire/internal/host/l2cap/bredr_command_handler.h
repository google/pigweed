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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_

#include <lib/fit/function.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel_configuration.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/command_handler.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Wrapper for a BR/EDR signaling channel that sends and receives command
// transactions.
class BrEdrCommandHandler final : public CommandHandler {
 public:
  class ConnectionResponse final : public Response {
   public:
    using PayloadT = ConnectionResponsePayload;
    static constexpr const char* kName = "Connection Response";

    using Response::Response;  // Inherit ctor
    bool Decode(const ByteBuffer& payload_buf);

    ConnectionResult result() const { return result_; }
    ConnectionStatus conn_status() const { return conn_status_; }

   private:
    ConnectionResult result_;
    ConnectionStatus conn_status_;
  };

  class ConfigurationResponse final : public Response {
   public:
    using PayloadT = ConfigurationResponsePayload;
    static constexpr const char* kName = "Configuration Response";

    using Response::Response;  // Inherit ctor
    bool Decode(const ByteBuffer& payload_buf);

    uint16_t flags() const { return flags_; }
    ConfigurationResult result() const { return result_; }
    const ChannelConfiguration& config() const { return config_; }

   private:
    friend class BrEdrCommandHandler;

    uint16_t flags_;
    ConfigurationResult result_;
    ChannelConfiguration config_;
  };

  class InformationResponse final : public Response {
   public:
    using PayloadT = InformationResponsePayload;
    static constexpr const char* kName = "Information Response";

    using Response::Response;  // Inherit ctor
    bool Decode(const ByteBuffer& payload_buf);

    InformationType type() const { return type_; }
    InformationResult result() const { return result_; }

    uint16_t connectionless_mtu() const {
      BT_ASSERT(result() == InformationResult::kSuccess);
      BT_ASSERT(type() == InformationType::kConnectionlessMTU);
      return data_.To<uint16_t>();
    }

    ExtendedFeatures extended_features() const {
      BT_ASSERT(result() == InformationResult::kSuccess);
      BT_ASSERT(type() == InformationType::kExtendedFeaturesSupported);
      return data_.To<ExtendedFeatures>();
    }

    FixedChannelsSupported fixed_channels() const {
      BT_ASSERT(result() == InformationResult::kSuccess);
      BT_ASSERT(type() == InformationType::kFixedChannelsSupported);
      return data_.To<FixedChannelsSupported>();
    }

   private:
    friend class BrEdrCommandHandler;

    InformationType type_;
    InformationResult result_;

    // View into the payload received from the peer in host endianness. It is
    // only valid for the duration of the InformationResponseCallback
    // invocation.
    BufferView data_;
  };

  using ConnectionResponseCallback =
      fit::function<SignalingChannelInterface::ResponseHandlerAction(
          const ConnectionResponse& rsp)>;
  using ConfigurationResponseCallback =
      fit::function<ResponseHandlerAction(const ConfigurationResponse& rsp)>;
  // Information Responses never have additional responses.
  using InformationResponseCallback =
      fit::function<void(const InformationResponse& rsp)>;

  class ConnectionResponder final : public Responder {
   public:
    ConnectionResponder(SignalingChannel::Responder* sig_responder,
                        ChannelId remote_cid);

    void Send(ChannelId local_cid,
              ConnectionResult result,
              ConnectionStatus status);
  };

  class ConfigurationResponder final : public Responder {
   public:
    ConfigurationResponder(SignalingChannel::Responder* sig_responder,
                           ChannelId local_cid);

    void Send(ChannelId remote_cid,
              uint16_t flags,
              ConfigurationResult result,
              ChannelConfiguration::ConfigurationOptions options);
  };

  class InformationResponder final : public Responder {
   public:
    InformationResponder(SignalingChannel::Responder* sig_responder,
                         InformationType type);

    void SendNotSupported();

    void SendConnectionlessMtu(uint16_t mtu);

    void SendExtendedFeaturesSupported(ExtendedFeatures extended_features);

    void SendFixedChannelsSupported(FixedChannelsSupported channels_supported);

   private:
    void Send(InformationResult result, const ByteBuffer& data);
    InformationType type_;
  };

  using ConnectionRequestCallback = fit::function<void(
      Psm psm, ChannelId remote_cid, ConnectionResponder* responder)>;
  using ConfigurationRequestCallback =
      fit::function<void(ChannelId local_cid,
                         uint16_t flags,
                         ChannelConfiguration config,
                         ConfigurationResponder* responder)>;
  using InformationRequestCallback = fit::function<void(
      InformationType type, InformationResponder* responder)>;

  // |sig| must be valid for the lifetime of this object.
  // |command_failed_callback| is called if an outbound request timed out with
  // RTX or ERTX timers after retransmission (if configured). The call may come
  // after the lifetime of this object.
  explicit BrEdrCommandHandler(SignalingChannelInterface* sig,
                               fit::closure request_fail_callback = nullptr);
  ~BrEdrCommandHandler() = default;

  // Disallow copy even though there's no state because having multiple
  // BrEdrCommandHandlers in the same scope is likely due to a bug or is at
  // least redundant.
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrCommandHandler);

  // Outbound request sending methods. Response callbacks are required to be
  // non-empty. The callbacks are wrapped and moved into the SignalingChannel
  // and may outlive BrEdrCommandHandler.
  bool SendConnectionRequest(uint16_t psm,
                             ChannelId local_cid,
                             ConnectionResponseCallback cb);
  bool SendConfigurationRequest(
      ChannelId remote_cid,
      uint16_t flags,
      ChannelConfiguration::ConfigurationOptions options,
      ConfigurationResponseCallback cb);
  bool SendInformationRequest(InformationType type,
                              InformationResponseCallback cb);

  // Inbound request delegate registration methods. The callbacks are wrapped
  // and moved into the SignalingChannel and may outlive BrEdrCommandHandler. It
  // is expected that any request delegates registered will span the lifetime of
  // its signaling channel and hence link, so no unregistration is provided.
  // However each call to register will replace any currently registered request
  // delegate.
  void ServeConnectionRequest(ConnectionRequestCallback cb);
  void ServeConfigurationRequest(ConfigurationRequestCallback cb);
  void ServeInformationRequest(InformationRequestCallback cb);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_
