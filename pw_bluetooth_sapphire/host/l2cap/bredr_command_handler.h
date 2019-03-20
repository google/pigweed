// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt {
namespace l2cap {
namespace internal {

// Wrapper for a BR/EDR signaling channel that sends and receives command
// transactions. It does not hold state. Rather, it:
// - constructs outbound request payloads and decodes/dispatches the received
//   response payloads
// - constructs request handlers that decode inbound payloads, registers the
//   handlers with SignalingChannel, and creates Responder objects that bind
//   response parameters and can be used to send appropriate response commands
// BrEdrCommandHandler can be constructed for each command to be sent or each
// kind of request to register, and even ephemerally as a temporary around a
// SignalingChannel.
//
// For outbound requests, use the BrEdrCommandHandler::Send*Req methods. They
// take parameters to be encoded into the request payload (with endian
// conversion and bounds checking) and a *ResponseCallback callback. When a
// matching response or rejection is received, the callback will be passed a
// *Response object containing the decoded command's parameters. Its |status()|
// shall be checked first to determine whether it's a rejection or response
// command. Return true in the *ResponseCallback if additional responses are
// expected from the peer for the request sent.
//
// Example:
//   DisconnectionResponseCallback rsp_cb =
//       [](const DisconnectionResponse& rsp) {
//         if (rsp.status() == Status::kReject) {
//           // Do something with rsp.reject_reason()
//         } else {
//           // Do something with rsp.local_cid() and rsp.remote_cid()
//         }
//         return false;  // No further responses expected for this transaction
//       };
//   cmd_handler.SendDisconnectionRequest(id0, id1, std::move(rsp_cb));
//
// For inbound requests, use the BrEdrCommandHandler::Serve*Req methods. They
// each take a request-handling delegate that will be called with decoded
// parameters from the received request, as well as a *Responder object. The
// Responder can be used to send a rejection (|RejectNotUnderstood()| or
// |RejectInvalidChannelId()|) or a matching response (|Send*()|). The channel
// IDs to encode into the response will be bound to the Responder. The Responder
// is only valid during the invocation of the request handler. Its sending
// methods can be called multiple times but it does not check that a malformed
// permutation of commands are sent (e.g. multiple rejections, a rejection
// followed by a response, etc.).
//
// Example:
//  DisconnectionRequestCallback req_cb =
//      [](ChannelId local_cid, ChannelId remote_cid,
//         DisconnectionResponder* responder) {
//        // Do something with local_cid and remote_cid
//        responder->Send();  // Request's IDs already bound to responder, omit
//        // OR:
//        responder->RejectInvalidChannelId();  // Idem.
//      };
//  cmd_handler.ServeDisconnectionRequest(std::move(req_cb));
//
// For both inbound requests and responses, if the received payload data is
// insufficient in size or otherwise malformed, it will be replied to with a
// Reject Not Understood and the corresponding callback will not be invoked.
class BrEdrCommandHandler final {
 public:
  using Status = SignalingChannel::Status;

  // Base for all responses received, including Command Reject. If |status()|
  // evaluates as |Status::kReject|, then this holds a Command Reject; then
  // |reject_reason()| should be read and the data in the derived Response
  // object should not be accessed.
  class Response {
   public:
    explicit Response(Status status) : status_(status) {}

    Status status() const { return status_; }

    // These are valid for reading if the response format contains them;
    // otherwise, they read as kInvalidChannelId.
    ChannelId local_cid() const { return local_cid_; }
    ChannelId remote_cid() const { return remote_cid_; }

    // This is valid for reading if |status| is kReject. If its value is
    // kInvalidCID, then |local_cid| and |remote_cid| are valid for reading.
    RejectReason reject_reason() const { return reject_reason_; }

   protected:
    friend class BrEdrCommandHandler;

    // Fills the reject fields of |rsp|. Returns true if successful.
    bool ParseReject(const common::ByteBuffer& rej_payload_buf);

    Status status_;
    ChannelId local_cid_ = kInvalidChannelId;
    ChannelId remote_cid_ = kInvalidChannelId;
    RejectReason reject_reason_;
  };

  class ConnectionResponse : public Response {
   public:
    using PayloadT = ConnectionResponsePayload;
    static constexpr const char* kName = "Connection Response";

    using Response::Response;  // Inherit ctor
    void Decode(const common::ByteBuffer& payload_buf);

    ConnectionResult result() const { return result_; }
    ConnectionStatus conn_status() const { return conn_status_; }

   private:
    ConnectionResult result_;
    ConnectionStatus conn_status_;
  };

  class ConfigurationResponse : public Response {
   public:
    using PayloadT = ConfigurationResponsePayload;
    static constexpr const char* kName = "Configuration Response";

    using Response::Response;  // Inherit ctor
    void Decode(const common::ByteBuffer& payload_buf);

    uint16_t flags() const { return flags_; }
    ConfigurationResult result() const { return result_; }
    const common::ByteBuffer& options() const { return options_; }

    // TODO(BT-466): Replace raw option access with abstraction over parsed
    // options.

   private:
    friend class BrEdrCommandHandler;

    uint16_t flags_;
    ConfigurationResult result_;

    // View into the raw options received from the peer. It is only valid for
    // the duration of the ConfigurationResponseCallback invocation.
    common::BufferView options_;
  };

  class DisconnectionResponse : public Response {
   public:
    using PayloadT = DisconnectionResponsePayload;
    static constexpr const char* kName = "Disconnection Response";

    using Response::Response;  // Inherit ctor
    void Decode(const common::ByteBuffer& payload_buf);
  };

  class InformationResponse : public Response {
   public:
    using PayloadT = InformationResponsePayload;
    static constexpr const char* kName = "Information Response";

    using Response::Response;  // Inherit ctor
    void Decode(const common::ByteBuffer& payload_buf);

    InformationType type() const { return type_; }
    InformationResult result() const { return result_; }

    uint16_t connectionless_mtu() const {
      ZX_DEBUG_ASSERT(type() == InformationType::kConnectionlessMTU);
      return data_.As<uint16_t>();
    }

    uint32_t extended_features() const {
      ZX_DEBUG_ASSERT(type() == InformationType::kExtendedFeaturesSupported);
      return data_.As<uint32_t>();
    }

    uint64_t fixed_channels() const {
      ZX_DEBUG_ASSERT(type() == InformationType::kFixedChannelsSupported);
      return data_.As<uint64_t>();
    }

   private:
    friend class BrEdrCommandHandler;

    InformationType type_;
    InformationResult result_;

    // View into the payload received from the peer in host endianness. It is
    // only valid for the duration of the InformationResponseCallback
    // invocation.
    common::BufferView data_;
  };

  using ConnectionResponseCallback =
      fit::function<bool(const ConnectionResponse& rsp)>;
  using ConfigurationResponseCallback =
      fit::function<bool(const ConfigurationResponse& rsp)>;
  using DisconnectionResponseCallback =
      fit::function<bool(const DisconnectionResponse& rsp)>;
  using InformationResponseCallback =
      fit::function<bool(const InformationResponse& rsp)>;

  // Base of response-sending objects passed to request delegates that they can
  // use to reply with a corresponding response or a rejection. This base
  // includes rejection methods because they can always be sent (but are not
  // always a reasonable reply to a given request). This also binds channel IDs
  // from the request received and uses them for the outbound response payload,
  // so that the delegate can not omit or send incorrect channel IDs.
  class Responder {
   public:
    void RejectNotUnderstood();
    void RejectInvalidChannelId();

   protected:
    explicit Responder(SignalingChannel::Responder* sig_responder,
                       ChannelId local_cid = kInvalidChannelId,
                       ChannelId remote_cid = kInvalidChannelId);
    virtual ~Responder() = default;
    FXL_DISALLOW_COPY_AND_ASSIGN(Responder);

    ChannelId local_cid() const { return local_cid_; }
    ChannelId remote_cid() const { return remote_cid_; }

    SignalingChannel::Responder* const sig_responder_;

   private:
    ChannelId local_cid_;
    ChannelId remote_cid_;
  };

  class ConnectionResponder : public Responder {
   public:
    ConnectionResponder(SignalingChannel::Responder* sig_responder,
                        ChannelId remote_cid);

    void Send(ChannelId local_cid, ConnectionResult result,
              ConnectionStatus status);
  };

  class ConfigurationResponder : public Responder {
   public:
    ConfigurationResponder(SignalingChannel::Responder* sig_responder,
                           ChannelId local_cid);

    void Send(ChannelId remote_cid, uint16_t flags, ConfigurationResult result,
              const common::ByteBuffer& data);

    // TODO(NET-1084): Add builder abstraction for configuration options
  };

  class DisconnectionResponder : public Responder {
   public:
    DisconnectionResponder(SignalingChannel::Responder* sig_responder,
                           ChannelId local_cid, ChannelId remote_cid);

    void Send();
  };

  class InformationResponder : public Responder {
   public:
    InformationResponder(SignalingChannel::Responder* sig_responder,
                         InformationType type);

    void SendNotSupported();

    void SendConnectionlessMtu(uint16_t mtu);

    void SendExtendedFeaturesSupported(ExtendedFeatures extended_features);

    void SendFixedChannelsSupported(FixedChannelsSupported channels_supported);

   private:
    void Send(InformationResult result, const common::ByteBuffer& data);
    InformationType type_;
  };

  using ConnectionRequestCallback = fit::function<void(
      PSM psm, ChannelId remote_cid, ConnectionResponder* responder)>;
  using ConfigurationRequestCallback = fit::function<void(
      ChannelId local_cid, uint16_t flags, const common::ByteBuffer& options,
      ConfigurationResponder* responder)>;
  using DisconnectionRequestCallback =
      fit::function<void(ChannelId local_cid, ChannelId remote_cid,
                         DisconnectionResponder* responder)>;
  using InformationRequestCallback = fit::function<void(
      InformationType type, InformationResponder* responder)>;

  // |sig| must be valid for the lifetime of this object
  explicit BrEdrCommandHandler(SignalingChannelInterface* sig);
  ~BrEdrCommandHandler() = default;

  // Disallow copy even though there's no state because having multiple
  // BrEdrCommandHandlers in the same scope is likely due to a bug or is at
  // least redundant.
  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrCommandHandler);

  // Outbound request sending methods. Response callbacks are required to be
  // non-empty. The callbacks are wrapped and moved into the SignalingChannel
  // and may outlive BrEdrCommandHandler.
  bool SendConnectionRequest(uint16_t psm, ChannelId local_cid,
                             ConnectionResponseCallback cb);
  bool SendConfigurationRequest(ChannelId remote_cid, uint16_t flags,
                                const common::ByteBuffer& options,
                                ConfigurationResponseCallback cb);
  bool SendDisconnectionRequest(ChannelId remote_cid, ChannelId local_cid,
                                DisconnectionResponseCallback cb);
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
  void ServeDisconnectionRequest(DisconnectionRequestCallback cb);
  void ServeInformationRequest(InformationRequestCallback cb);

 private:
  // Returns a function that decodes a response status and payload into a
  // |ResponseT| object and invokes |cb| with it.
  // |ResponseT| needs to have
  //  - |Decode| function that accepts a buffer of at least
  //    |sizeof(ResponseT::PayloadT)| bytes
  //  - |kName| string literal
  template <class ResponseT, typename CallbackT>
  SignalingChannel::ResponseHandler BuildResponseHandler(CallbackT rsp_cb);

  SignalingChannelInterface* const sig_;  // weak
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_COMMAND_HANDLER_H_
