// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SIGNALING_CHANNEL_H_

#include <fbl/macros.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace l2cap {

class Channel;

namespace internal {

using SignalingPacket = PacketView<CommandHeader>;
using MutableSignalingPacket = MutablePacketView<CommandHeader>;

using DataCallback = fit::function<void(const ByteBuffer& data)>;
using SignalingPacketHandler = fit::function<void(const SignalingPacket& packet)>;

// SignalingChannelInterface contains the procedures that command flows use to
// send and receive signaling channel transactions.
class SignalingChannelInterface {
 public:
  // Action in response to a request-type packet.
  enum class Status {
    kSuccess,  // Remote response received
    kReject,   // Remote rejection received
    kTimeOut,  // Timed out waiting for matching remote command
  };

  // Callback invoked to handle a response received from the remote. If |status|
  // is kSuccess or kReject, then |rsp_payload| will contain any payload
  // received. Return true if an additional response is expected.
  using ResponseHandler = fit::function<bool(Status status, const ByteBuffer& rsp_payload)>;

  // Initiate an outbound transaction. The signaling channel will send a request
  // then expect reception of one or more responses with a code one greater than
  // the request. Each response or rejection received invokes |cb|. When |cb|
  // returns false, it will be removed. Returns false if the request failed to
  // send.
  virtual bool SendRequest(CommandCode req_code, const ByteBuffer& payload, ResponseHandler cb) = 0;

  // Send a command packet in response to an incoming request.
  class Responder {
   public:
    // Send a response that corresponds to the request received
    virtual void Send(const ByteBuffer& rsp_payload) = 0;

    // Reject invalid, malformed, or unhandled request
    virtual void RejectNotUnderstood() = 0;

    // Reject request non-existent or otherwise invalid channel ID(s)
    virtual void RejectInvalidChannelId(ChannelId local_cid, ChannelId remote_cid) = 0;

   protected:
    virtual ~Responder() = default;
  };

  // Callback invoked to handle a request received from the remote.
  // |req_payload| contains any payload received, without the command header.
  // The callee can use |responder| to respond or reject. Parameters passed to
  // this handler are only guaranteed to be valid while the handler is running.
  using RequestDelegate = fit::function<void(const ByteBuffer& req_payload, Responder* responder)>;

  // Register a handler for all inbound transactions matching |req_code|, which
  // should be the code of a request. |cb| will be called with request payloads
  // received, and is expected to respond to, reject, or ignore the requests.
  // Calls to this function with a previously registered |req_code| will replace
  // the current delegate.
  virtual void ServeRequest(CommandCode req_code, RequestDelegate cb) = 0;

 protected:
  virtual ~SignalingChannelInterface() = default;
};

// SignalingChannel is an abstract class that handles the common operations
// involved in LE and BR/EDR signaling channels.
//
// TODO(armansito): Implement flow control (RTX/ERTX timers).
class SignalingChannel : public SignalingChannelInterface {
 public:
  SignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role);
  ~SignalingChannel() override;

  bool is_open() const { return is_open_; }

  // Local signaling MTU (i.e. MTU_sig, per spec)
  uint16_t mtu() const { return mtu_; }
  void set_mtu(uint16_t mtu) { mtu_ = mtu; }

 protected:
  // Implementation for responding to a request that binds the request's
  // identifier and the response's code so that the client's |Send| invocation
  // does not need to supply them nor even know them.
  class ResponderImpl : public Responder {
   public:
    ResponderImpl(SignalingChannel* sig, CommandCode code, CommandId id);
    void Send(const ByteBuffer& rsp_payload) override;
    void RejectNotUnderstood() override;
    void RejectInvalidChannelId(ChannelId local_cid, ChannelId remote_cid) override;

   private:
    SignalingChannel* sig() const { return sig_; }

    SignalingChannel* const sig_;
    const CommandCode code_;
    const CommandId id_;
  };

  // Sends out a single signaling packet using the given parameters.
  bool SendPacket(CommandCode code, uint8_t identifier, const ByteBuffer& data);

  // Called when a frame is received to decode into L2CAP signaling command
  // packets. The derived implementation should invoke |cb| for each packet with
  // a valid payload length, send a Command Reject packet for each packet with
  // an intact ID in its header but invalid payload length, and drop any other
  // incoming data.
  virtual void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) = 0;

  // Called when a new signaling packet has been received. Returns false if
  // |packet| is rejected. Otherwise returns true and sends a response packet.
  //
  // This method is thread-safe in that a SignalingChannel cannot be deleted
  // while this is running. SendPacket() can be called safely from this method.
  virtual bool HandlePacket(const SignalingPacket& packet) = 0;

  // Sends out a command reject packet with the given parameters.
  bool SendCommandReject(uint8_t identifier, RejectReason reason, const ByteBuffer& data);

  // Returns true if called on this SignalingChannel's creation thread. Mainly
  // intended for debug assertions.
  bool IsCreationThreadCurrent() const { return thread_checker_.IsCreationThreadCurrent(); }

  // Returns the logical link that signaling channel is operating on.
  hci::Connection::Role role() const { return role_; }

  // Generates a command identifier in sequential order that is never
  // kInvalidId. The caller is responsible for bookkeeping when reusing command
  // IDs to prevent collisions with pending commands.
  CommandId GetNextCommandId();

 private:
  // Sends out the given signaling packet directly via |chan_| after running
  // debug-mode assertions for validity. Packet must correspond to exactly one
  // C-frame payload.
  //
  // This method is not thread-safe (i.e. requires external locking).
  //
  // TODO(armansito): This should be generalized for ACL-U to allow multiple
  // signaling commands in a single C-frame.
  bool Send(ByteBufferPtr packet);

  // Builds a signaling packet with the given parameters and payload. The
  // backing buffer is slab allocated.
  ByteBufferPtr BuildPacket(CommandCode code, uint8_t identifier, const ByteBuffer& data);

  // Channel callbacks:
  void OnChannelClosed();
  void OnRxBFrame(ByteBufferPtr sdu);

  // Invoke the abstract packet handler |HandlePacket| for well-formed command
  // packets and send responses for command packets that exceed this host's MTU
  // or can't be handled by this host.
  void CheckAndDispatchPacket(const SignalingPacket& packet);

  // Destroy all other members prior to this so they can use this for checking.
  fxl::ThreadChecker thread_checker_;

  bool is_open_;
  l2cap::ScopedChannel chan_;
  hci::Connection::Role role_;
  uint16_t mtu_;
  uint8_t next_cmd_id_;

  fxl::WeakPtrFactory<SignalingChannel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SignalingChannel);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_SIGNALING_CHANNEL_H_
