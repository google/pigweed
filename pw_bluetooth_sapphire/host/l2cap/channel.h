// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/socket.h>
#include <zircon/compiler.h>

#include <atomic>
#include <climits>
#include <list>
#include <memory>
#include <mutex>
#include <queue>

#include "pw_bluetooth/vendor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/tx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::l2cap {

// Represents a L2CAP channel. Each instance is owned by a service
// implementation that operates on the corresponding channel. Instances can only
// be obtained from a ChannelManager.
//
// A Channel can operate in one of 6 L2CAP Modes of Operation (see Core Spec
// v5.0, Vol 3, Part A, Section 2.4). Only Basic Mode is currently supported.
//
// USAGE:
//
// Channel is an abstract base class. There are two concrete implementations:
//
//   * internal::ChannelImpl (defined below) which implements a real L2CAP
//     channel. Instances are obtained from ChannelManager and tied to
//     internal::LogicalLink instances.
//
//   * FakeChannel, which can be used for unit testing service-layer entities
//     that operate on one or more L2CAP channel(s).
//
// Production instances are obtained from a ChannelManager. Channels are not thread safe.
//
// A Channel's owner must explicitly call Deactivate() and must not rely on
// dropping its reference to close the channel.
//
// When a LogicalLink closes, all of its active channels become deactivated
// when it closes and this is signaled by running the ClosedCallback passed to
// Activate().
class Channel {
 public:
  // Defines the state of A2DP offloading to the controller.
  enum class A2dpOffloadStatus : uint8_t {
    // The A2DP offload command was received and successfully started.
    kStarted,
    // The A2DP offload command was sent and the L2CAP channel is waiting for a response.
    kPending,
    // Either an error or an A2DP offload command stopped offloading to the controller.
    kStopped,
  };

  // Configuration received from the profile server that needs to be converted to a command packet
  // in order to send the StartA2dpOffload command
  struct A2dpOffloadConfiguration {
    hci_spec::vendor::android::A2dpCodecType codec;
    uint16_t max_latency;
    hci_spec::vendor::android::A2dpScmsTEnable scms_t_enable;
    hci_spec::vendor::android::A2dpSamplingFrequency sampling_frequency;
    hci_spec::vendor::android::A2dpBitsPerSample bits_per_sample;
    hci_spec::vendor::android::A2dpChannelMode channel_mode;
    uint32_t encoded_audio_bit_rate;
    hci_spec::vendor::android::A2dpOffloadCodecInformation codec_information;
  };

  // TODO(fxbug.dev/1022): define a preferred MTU somewhere
  Channel(ChannelId id, ChannelId remote_id, bt::LinkType link_type,
          hci_spec::ConnectionHandle link_handle, ChannelInfo info);
  virtual ~Channel() = default;

  // Identifier for this channel's endpoint on this device. It can be prior-
  // specified for fixed channels or allocated for dynamic channels per v5.0,
  // Vol 3, Part A, Section 2.1 "Channel Identifiers." Channels on a link will
  // have unique identifiers to each other.
  ChannelId id() const { return id_; }

  // Identifier for this channel's endpoint on the remote peer. Same value as
  // |id()| for fixed channels and allocated by the remote for dynamic channels.
  ChannelId remote_id() const { return remote_id_; }

  // The type of the logical link this channel operates on.
  bt::LinkType link_type() const { return link_type_; }

  // The connection handle of the underlying logical link.
  hci_spec::ConnectionHandle link_handle() const { return link_handle_; }

  // Returns a value that's unique for any channel connected to this device.
  // If two channels have different unique_ids, they represent different
  // channels even if their ids match.
  using UniqueId = uint32_t;
  UniqueId unique_id() const {
    static_assert(sizeof(UniqueId) >= sizeof(hci_spec::ConnectionHandle) + sizeof(ChannelId),
                  "UniqueId needs to be large enough to make unique IDs");
    return (link_handle() << (sizeof(ChannelId) * CHAR_BIT)) | id();
  }

  ChannelMode mode() const { return info().mode; }

  // These accessors define the concept of a Maximum Transmission Unit (MTU) as a maximum inbound
  // (rx) and outbound (tx) packet size for the L2CAP implementation (see v5.2, Vol. 3, Part A
  // 5.1). L2CAP requires that channel MTUs are at least 23 bytes for LE-U links and 48 bytes for
  // ACL-U links. A further requirement is that "[t]he minimum MTU for a channel is the larger of
  // the L2CAP minimum [...] and any MTU explicitly required by the protocols and profiles using
  // that channel." `max_rx_sdu_size` is always determined by the capabilities of the local
  // implementation. For dynamic channels, `max_tx_sdu_size` is determined through a configuration
  // procedure with the peer (v5.2 Vol. 3 Part A 7.1). For fixed channels, this is always the
  // maximum allowable L2CAP packet size, not a protocol-specific MTU.
  uint16_t max_rx_sdu_size() const { return info().max_rx_sdu_size; }
  uint16_t max_tx_sdu_size() const { return info().max_tx_sdu_size; }

  // Returns the current configuration parameters for this channel.
  ChannelInfo info() const { return info_; }

  // Returns the current link security properties of the underlying link.
  // Returns the lowest security level if the link is closed.
  virtual const sm::SecurityProperties security() = 0;

  // Callback invoked when this channel has been closed without an explicit
  // request from the owner of this instance. For example, this can happen when
  // the remote end closes a dynamically configured channel or when the
  // underlying logical link is terminated through other means.
  using ClosedCallback = fit::closure;

  // Callback invoked when a new packet is received on this channel. Any
  // previously buffered packets will be sent to |rx_cb| right away, provided
  // that |rx_cb| is not empty and the underlying logical link is active.
  using RxCallback = fit::function<void(ByteBufferPtr packet)>;

  // Activates this channel to execute |rx_callback| and |closed_callback|
  // immediately as L2CAP is notified of their underlying events.
  //
  // Any inbound data that has already been buffered for this channel will be drained by calling
  // |rx_callback| repeatedly, before this call returns.
  //
  // Execution of |rx_callback| may block L2CAP data routing, so care should be taken to avoid
  // introducing excessive latency.
  //
  // Each channel can be activated only once.
  //
  // Returns false if the channel's link has been closed.
  //
  // NOTE: Callers shouldn't assume that this method will succeed, as the underlying link can be
  // removed at any time.
  virtual bool Activate(RxCallback rx_callback, ClosedCallback closed_callback) = 0;

  // Deactivates this channel. No more packets can be sent or received after
  // this is called. |rx_callback| may still be called if it has been already
  // dispatched to its task runner.
  //
  // This method is idempotent.
  virtual void Deactivate() = 0;

  // Signals that the underlying link should be disconnected. This should be
  // called when a service layer protocol error requires the connection to be
  // severed.
  //
  // The link error callback (provided to L2CAP::Register* methods) is invoked
  // as a result of this operation. The handler is responsible for actually
  // disconnecting the link.
  //
  // This does not deactivate the channel, though the channel is expected to
  // close when the link gets removed later.
  virtual void SignalLinkError() = 0;

  // Requests to upgrade the security properties of the underlying link to the requested |level| and
  // reports the result via |callback|. Has no effect if the channel is not active.
  virtual void UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) = 0;

  // Queue the given SDU payload for transmission over this channel, taking
  // ownership of |sdu|. Returns true if the SDU was queued successfully, and
  // false otherwise.
  //
  // For reasons why queuing might fail, see the documentation for the relevant
  // TxEngine's QueueSdu() method. Note: a successfully enqueued SDU may still
  // fail to reach the receiver, due to asynchronous local errors, transmission
  // failure, or remote errors.
  virtual bool Send(ByteBufferPtr sdu) = 0;

  // Request that the ACL priority of this channel be changed to |priority|.
  // Calls |callback| with success if the request succeeded, or error otherwise.
  // Requests may fail if the controller does not support changing the ACL priority or the indicated
  // priority conflicts with another channel.
  virtual void RequestAclPriority(pw::bluetooth::AclPriority,
                                  fit::callback<void(fit::result<fit::failed>)> callback) = 0;

  // Sets an automatic flush timeout with duration |flush_timeout|. |callback| will be called with
  // the result of the operation. This is only supported if the link type is kACL (BR/EDR).
  // |flush_timeout| must be in the range [1ms - hci_spec::kMaxAutomaticFlushTimeoutDuration]. A
  // flush timeout of zx::duration::infinite() indicates an infinite flush timeout (packets will be
  // marked flushable, but there will be no automatic flush timeout).
  virtual void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                             hci::ResultCallback<> callback) = 0;

  // Attach this channel as a child node of |parent| with the given |name|.
  virtual void AttachInspect(inspect::Node& parent, std::string name) = 0;

  // Request the start of A2DP source offloading. |callback| will be called with the result of the
  // request. If offloading is already started or is pending, the request will fail and an error
  // will be reported synchronously.
  virtual void StartA2dpOffload(const A2dpOffloadConfiguration* config,
                                hci::ResultCallback<> callback) = 0;

  // The ACL priority that was both requested and accepted by the controller.
  pw::bluetooth::AclPriority requested_acl_priority() const { return requested_acl_priority_; }

  virtual fxl::WeakPtr<Channel> GetWeakPtr() = 0;

 protected:
  const ChannelId id_;
  const ChannelId remote_id_;
  const bt::LinkType link_type_;
  const hci_spec::ConnectionHandle link_handle_;
  ChannelInfo info_;
  // The ACL priority that was requested by a client and accepted by the controller.
  pw::bluetooth::AclPriority requested_acl_priority_;
  A2dpOffloadStatus a2dp_offload_status_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Channel);
};

// A socket connected to an L2cap channel and its parameters
// params will be non-empty iff socket != ZX_INVALID_HANDLE
struct ChannelSocket {
  ChannelSocket() : socket(zx::socket()), params(std::nullopt) {}
  ChannelSocket(zx::socket socket, std::optional<ChannelInfo> params)
      : socket(std::move(socket)), params(params) {
    BT_ASSERT(this->socket.is_valid() && this->params.has_value() ||
              !this->socket.is_valid() && !this->params.has_value());
  }
  ChannelSocket(ChannelSocket&&) = default;

  bool is_valid() const { return socket.is_valid(); }
  explicit operator bool() const { return is_valid(); }
  zx::socket socket;
  std::optional<const ChannelInfo> params;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelSocket);
};

namespace internal {

class LogicalLink;

// Channel implementation used in production.
class ChannelImpl : public Channel {
 public:
  // Many core-spec protocols which operate over fixed channels (e.g. v5.2 Vol. 3 Parts F (ATT) and
  // H (SMP)) define service-specific MTU values. Channels created with `CreateFixedChannel` do not
  // check against these service-specific MTUs. Thus `bt-host` local services which operate over
  // fixed channels are required to respect their MTU internally by:
  //   1.) never sending packets larger than their spec-defined MTU.
  //   2.) handling inbound PDUs which are larger than their spec-defined MTU appropriately.
  static std::unique_ptr<ChannelImpl> CreateFixedChannel(ChannelId id,
                                                         fxl::WeakPtr<internal::LogicalLink> link,
                                                         hci::CommandChannel::WeakPtr cmd_channel);

  static std::unique_ptr<ChannelImpl> CreateDynamicChannel(
      ChannelId id, ChannelId peer_id, fxl::WeakPtr<internal::LogicalLink> link, ChannelInfo info,
      hci::CommandChannel::WeakPtr cmd_channel);

  ~ChannelImpl() override = default;

  // Called by |link_| to notify us when the channel can no longer process data.
  void OnClosed();

  // Called by |link_| when a PDU targeting this channel has been received.
  // Contents of |pdu| will be moved.
  void HandleRxPdu(PDU&& pdu);

  fxl::WeakPtr<ChannelImpl> GetWeakImplPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // Channel overrides:
  const sm::SecurityProperties security() override;
  bool Activate(RxCallback rx_callback, ClosedCallback closed_callback) override;
  void Deactivate() override;
  void SignalLinkError() override;
  bool Send(ByteBufferPtr sdu) override;
  void UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) override;
  void RequestAclPriority(pw::bluetooth::AclPriority priority,
                          fit::callback<void(fit::result<fit::failed>)> callback) override;
  void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                     hci::ResultCallback<> callback) override;
  void AttachInspect(inspect::Node& parent, std::string name) override;
  void StartA2dpOffload(const A2dpOffloadConfiguration* config,
                        hci::ResultCallback<> callback) override;
  fxl::WeakPtr<Channel> GetWeakPtr() override { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  ChannelImpl(ChannelId id, ChannelId remote_id, fxl::WeakPtr<internal::LogicalLink> link,
              ChannelInfo info, hci::CommandChannel::WeakPtr cmd_channel);

  // Common channel closure logic. Called on Deactivate/OnClosed.
  void CleanUp();

  // Callback that |tx_engine_| uses to deliver a PDU to lower layers.
  void SendFrame(ByteBufferPtr pdu);

  bool active_;
  RxCallback rx_cb_;
  ClosedCallback closed_cb_;

  // The LogicalLink that this channel is associated with. A channel is always
  // created by a LogicalLink.
  //
  // |link_| is guaranteed to be valid as long as the link is active. This is
  // because when a LogicalLink is torn down, it will notify all of its
  // associated channels by calling OnLinkClosed() which sets |link_| to
  // nullptr.
  fxl::WeakPtr<internal::LogicalLink> link_;

  // Command channel used to transport A2DP offload configuration of vendor extensions.
  hci::CommandChannel::WeakPtr cmd_channel_;

  // The engine which processes received PDUs, and converts them to SDUs for
  // upper layers.
  std::unique_ptr<RxEngine> rx_engine_;

  // The engine which accepts SDUs, and converts them to PDUs for lower layers.
  std::unique_ptr<TxEngine> tx_engine_;

  // The pending SDUs on this channel. Received PDUs are buffered if |rx_cb_| is
  // currently not set.
  // TODO(armansito): We should avoid STL containers for data packets as they
  // all implicitly allocate. This is a reminder to fix this elsewhere
  // (especially in the HCI layer).
  std::queue<ByteBufferPtr, std::list<ByteBufferPtr>> pending_rx_sdus_;

  struct InspectProperties {
    inspect::Node node;
    inspect::StringProperty psm;
    inspect::StringProperty local_id;
    inspect::StringProperty remote_id;
  };
  InspectProperties inspect_;

  fxl::WeakPtrFactory<ChannelImpl> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelImpl);
};

}  // namespace internal
}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_H_
