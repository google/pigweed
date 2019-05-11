// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_SESSION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_SESSION_H_

#include <fbl/ref_ptr.h>
#include <lib/async/cpp/task.h>
#include <lib/fit/function.h>
#include <src/lib/fxl/memory/weak_ptr.h>

#include <map>
#include <queue>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/frames.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/rfcomm.h"

namespace bt {
namespace rfcomm {

// Represents a single RFCOMM session from this device to another remote device,
// mulitplexed over a single L2CAP channel. We assume the underlying L2CAP
// channel is reliable. All data-sending functions are asynchronous, and do not
// provide notifications of delivery.
//
// THREAD SAFETY
//
// Session is not thread safe, and should only be accessed from the thread it is
// created on. Session will dispatch all tasks onto the thread it is created on.
class Session {
 public:
  // User should first use GetMaximumUserDataLength() to determine the maximum
  // amount of data they can send.
  void SendUserData(DLCI dlci, ByteBufferPtr data);

  // Get the maximum length of data which can be sent in a single RFCOMM frame.
  // This should only be called after initial parameter negotiation is complete.
  // Once initial parameter negotiation is complete, the value returned will not
  // change.
  size_t GetMaximumUserDataLength() const;

 private:
  friend class ChannelManager;

  // Returns nullptr if creation fails -- for example, if activating the L2CAP
  // channel fails. |channel_opened_cb| will be called whenever a new channel is
  // opened on this session. The callback will be dispatched onto the thread on
  // which Session was created.
  using ChannelOpenedCallback =
      fit::function<void(fbl::RefPtr<Channel>, ServerChannel)>;
  static std::unique_ptr<Session> Create(
      fbl::RefPtr<l2cap::Channel> l2cap_channel,
      ChannelOpenedCallback channel_opened_cb);

  // Should only be called from Create().
  Session(ChannelOpenedCallback channel_opened_cb);

  // Sets |l2cap_channel| as the Session's underlying L2CAP channel.
  // |l2cap_channel| should not be activated. This function activates
  // |l2cap_channel|; returns true iff channel activation succeeds. Should only
  // be called from Create() during Session creation.
  bool SetL2CAPChannel(fbl::RefPtr<l2cap::Channel> l2cap_channel);

  // Opens a remote channel and delivers it via |channel_opened_cb|.
  void OpenRemoteChannel(ServerChannel server_channel,
                         ChannelOpenedCallback channel_opened_cb);

  // l2cap::Channel callbacks.
  void RxCallback(ByteBufferPtr sdu);
  void ClosedCallback();

  // Send a SABM or a DISC command. When a response (UA or DM) is received,
  // it is passed to |command_response_cb|. If an error occurs, the callback
  // is not called. Otherwise, if the callback is given, it will be called with
  // a valid UA or DM frame.
  using CommandResponseCallback = fit::function<void(std::unique_ptr<Frame>)>;
  void SendCommand(FrameType frame_type, DLCI dlci,
                   CommandResponseCallback command_response_cb = nullptr);

  // Send a UA or DM response.
  void SendResponse(FrameType frame_type, DLCI dlci);

  // The raw frame-sending function. This function should only be called by the
  // other frame-sending functions. |sent_cb| is called synchronously once the
  // frame is actually sent. Returns true if the frame was sent or queued;
  // returns false on send error.
  bool SendFrame(std::unique_ptr<Frame> frame, fit::closure sent_cb = nullptr);

  // Send a multiplexer command along the multiplexer control channel.
  //
  // All multiplexer commands come in command/response pairs (RFCOMM 5.4.6.2).
  // This function takes an optional callback which will be called when the
  // response is received, and will be passed a |nullptr| if a DM response was
  // sent in response to the multiplexer command (meaning the command was
  // declined). A DM response can only occur for specific commands, e.g.
  // parameter negotiation (RFCOMM 5.5.3).
  using MuxResponseCallback = fit::function<void(std::unique_ptr<MuxCommand>)>;
  void SendMuxCommand(std::unique_ptr<MuxCommand> mux_command,
                      MuxResponseCallback callback = nullptr);

  // Handle an incoming SABM request.
  void HandleSABM(DLCI dlci);

  // Handle incoming multiplexer command.
  void HandleMuxCommand(std::unique_ptr<MuxCommand> mux_command);

  // Begin the multiplexer start-up routine described in RFCOMM 5.2.1. This
  // function implements the "initiator" side of the multiplexer startup
  // protocol. For the "responder" side, see HandleSABM().
  void StartupMultiplexer();

  // Sets |role_|, and runs any Tasks that were pending multiplexer startup.
  void SetMultiplexerStarted(Role role);

  inline bool multiplexer_started() { return IsMultiplexerStarted(role_); }

  void Closedown();

  // Begin this session's initial parameter negotiation. Our RFCOMM
  // implementation will initiate parameter negotiation at most once, before the
  // first DLC is opened. This initial parameter negotiation is required by the
  // spec. Any other PN which we participate in will be initiated by the remote.
  //
  // TODO(gusss): what happens when we set parameters and then receive a DISC/DM
  // for that channel? Do we need to undo, somehow? For something like credits,
  // we can probably just unset that entry in the credits map.
  //
  // TODO(gusss): does the spec say what happens (in terms of initial parameter
  // negotiation) if we do initial parameter negotiation, it finishes, and the
  // remote sends a DISC? Do we have to re-do initial parameter negotiation?
  void RunInitialParameterNegotiation(DLCI dlci);

  // Get the ideal parameters for this session. This is used in both PN commands
  // and PN responses to determine what our parameter set would be in the ideal
  // case. The final parameters may be different after negotiation.
  ParameterNegotiationParams GetIdealParameters(DLCI dlci) const;

  // Set initial parameter negotiation as complete and run any pending tasks.
  void InitialParameterNegotiationComplete();

  // Queue a frame for sending later, when the flow control situation changes
  // (e.g. when more credits are available).
  void QueueFrame(std::unique_ptr<Frame> frame, fit::closure sent_cb);

  // Attempt to send any queued frames.
  void TrySendQueued();
  // Attempt to send any queued frames for |dlci|.
  void TrySendQueued(DLCI dlci);

  // Finds or iniitalizes a new Channel object for |dlci|
  // Returns a pair with the channel and a boolean indicating if it was created.
  std::pair<fbl::RefPtr<Channel>, bool> FindOrCreateChannel(DLCI dicl);

  // Gets the Channel for |dlci|, or a null pointer if it doesn't exist.
  fbl::RefPtr<Channel> GetChannel(DLCI dlci);

  // Called when an incoming frame has credits attached. Adds |credits| to this
  // Session's store of outgoing credits.
  void HandleReceivedCredits(DLCI dlci, FrameCredits credits);

  l2cap::ScopedChannel l2cap_channel_;

  // The RFCOMM role of this device for this particular Session. This is
  // determined not when the object is created, but when the multiplexer control
  // channel is set up.
  Role role_;

  // Whether or not this Session is using credit-based flow, as described in the
  // RFCOMM spec. Whether credit-based flow is being used is determined in the
  // first Parameter Negotiation interaction.
  bool credit_based_flow_;

  // Keeps track of opened channels.
  std::map<DLCI, fbl::RefPtr<Channel>> channels_;

  // Called when the remote peer opens a new incoming channel. The session
  // object constructs a new channel and then passes ownership of the channel
  // via this callback.
  ChannelOpenedCallback channel_opened_cb_;

  // This dispatcher is used for all tasks, including the ChannelOpenCallback
  // passed in to Create().
  async_dispatcher_t* dispatcher_;

  // Tasks which are to be run once the multiplexer starts.
  std::queue<fit::closure> tasks_pending_mux_startup_;

  // Called when a command frame or a multiplexer command doesn't receive a
  // response.
  using TimeoutCallback = async::TaskClosure;

  // Outstanding SABM and DISC commands awaiting responses. GSM 5.4.4.1 states
  // that there can be at most one command with the P bit set to 1 outstanding
  // on a given DLC at any time. Thus, we can identify outstanding frames by
  // their DLCI.
  using CommandResponseCallbacks =
      std::pair<CommandResponseCallback, std::unique_ptr<TimeoutCallback>>;
  std::unordered_map<DLCI, CommandResponseCallbacks> outstanding_frames_;

  // Outstanding multiplexer commands awaiting responses. We identify
  // outstanding commands as a tuple of the command type and the DLCI referenced
  // in the command, or kNoDLCI. Some multiplexer commands do not identify a
  // DLCI (e.g. the Test command); these commands use kNoDLCI as their DLCI.
  //
  // The TimeoutCallback is called when the timeout elapses before a response is
  // received.
  using OutstandingMuxCommand = std::pair<MuxCommandType, DLCI>;
  using MuxResponseCallbacks =
      std::pair<MuxResponseCallback, std::unique_ptr<TimeoutCallback>>;
  struct outstanding_mux_commands_hash {
    inline size_t operator()(OutstandingMuxCommand key) const {
      return ((uint8_t)std::get<0>(key) << 8) | std::get<1>(key);
    }
  };
  std::unordered_map<OutstandingMuxCommand, MuxResponseCallbacks,
                     outstanding_mux_commands_hash>
      outstanding_mux_commands_;

  // Tracks whether the initial parameter negotiation has completed. RFCOMM
  // requires that parameter negotiation run at least once, before any DLCs are
  // opened. The first request to open a DLC to the remote peer will trigger
  // initial parameter negotiation, which will delay all other channel opening
  // requests until it completes.
  ParameterNegotiationState initial_param_negotiation_state_;

  // Tasks which are to be run once parameter negotiation completes.
  std::queue<fit::closure> tasks_pending_parameter_negotiation_;

  // The RX and TX MTU for this Session. This is determined during initial
  // parameter negotiation, and is based on the MTU of the underlying L2CAP
  // link. Our RFCOMM implementation refuses to change the maximum frame size
  // once it is set the first time.
  uint16_t maximum_frame_size_;

  fxl::WeakPtrFactory<Session> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Session);
};

}  // namespace rfcomm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_SESSION_H_
