// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_

#include <lib/async/cpp/task.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/fit/result.h>
#include <lib/fit/thread_checker.h>
#include <lib/trace/event.h>
#include <lib/zx/channel.h>
#include <zircon/compiler.h>

#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/memory/ref_ptr.h"

namespace bt::hci {

class Transport;

// Represents the HCI Bluetooth command channel. Manages HCI command and event
// packet control flow.
class CommandChannel final {
 public:
  // Starts listening on the HCI command channel and starts handling commands
  // and events.
  //
  // |hci_command_channel| is a Zircon channel construct that can receive
  // Bluetooth HCI command and event packets, in which the remote end is
  // implemented by the underlying Bluetooth HCI device driver.
  //
  // |transport| is the Transport instance that owns this CommandChannel.
  static fit::result<std::unique_ptr<CommandChannel>> Create(Transport* transport,
                                                             zx::channel hci_command_channel);

  ~CommandChannel();

  // Unregisters event handlers and cleans up.
  // TODO(fxbug.dev/667): Remove ShutDown and move logic to destructor. Let Transport destroy
  // CommandChannel to initiate shut down.
  void ShutDown();

  // Used to identify an individual HCI command<->event transaction.
  using TransactionId = size_t;

  // Queues the given |command_packet| to be sent to the controller and returns
  // a transaction ID.
  //
  // This call takes ownership of the contents of |command_packet|.
  // |command_packet| MUST represent a valid HCI command packet.
  //
  // |callback| will be called with all events related to the transaction,
  // unless the transaction is removed with RemoveQueuedCommand. If the command
  // results in a CommandStatus event, it will be sent to this callback before
  // the event with |complete_event_code| is sent.
  //
  // Synchronous transactions complete with a CommandComplete HCI event.
  // This function is the only way to receive a CommandComplete event.
  //
  // Most asynchronous transactions return the CommandStatus event and
  // another event to indicate completion, which should be indicated in
  // |complete_event_code|.
  //
  // If |complete_event_code| is set to kCommandStatus, the transaction
  // is considered complete when the CommandStatus event is received.
  //
  // |complete_event_code| cannot be a code that has been registered for events via AddEventHandler.
  //
  // Returns a ID unique to the command transaction, or zero if the parameters
  // are invalid.  This ID will be supplied to |callback| in its |id| parameter
  // to identify the transaction.
  //
  // NOTE: Commands queued are not guaranteed to be finished or sent in order,
  // although commands with the same opcode will be sent in order, and
  // commands with the same |complete_event_code| and |subevent_code| will be sent in order.
  // If strict ordering of commands is required, use SequentialCommandRunner
  // or callbacks for sequencing.
  //
  // See Bluetooth Core Spec v5.0, Volume 2, Part E, Section 4.4 "Command Flow
  // Control" for more information about the HCI command flow control.
  using CommandCallback = fit::function<void(TransactionId id, const EventPacket& event)>;
  TransactionId SendCommand(std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
                            const EventCode complete_event_code = kCommandCompleteEventCode);

  // As SendCommand, but the transaction completes on the LE Meta Event.
  // |le_meta_subevent_code| is a LE Meta Event subevent code as described in Core Spec v5.2, Vol 4,
  // Part E, Sec 7.7.65.
  //
  // |le_meta_subevent_code| cannot be a code that has been registered for events via
  // AddLEMetaEventHandler.
  TransactionId SendLeAsyncCommand(std::unique_ptr<CommandPacket> command_packet,
                                   CommandCallback callback, EventCode le_meta_subevent_code);

  // As SendCommand, but will wait to run this command until there are no
  // commands with with opcodes specified in |exclude| from executing. This
  // is useful to prevent running different commands that cannot run
  // concurrently (i.e. Inquiry and Connect).
  // Two commands with the same opcode will never run simultaneously.
  TransactionId SendExclusiveCommand(
      std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
      const EventCode complete_event_code = kCommandCompleteEventCode,
      std::unordered_set<OpCode> exclusions = {});

  // As SendExclusiveCommand, but the transaction completes on the LE Meta Event with subevent code
  // |le_meta_subevent_code|.
  TransactionId SendLeAsyncExclusiveCommand(std::unique_ptr<CommandPacket> command_packet,
                                            CommandCallback callback,
                                            std::optional<EventCode> le_meta_subevent_code,
                                            std::unordered_set<OpCode> exclusions = {});

  // If the command identified by |id| has not been sent to the controller, remove it from the send
  // queue and return true. In this case, its CommandCallback will not be notified. If the command
  // identified by |id| has already been sent to the controller or if it does not exist, this has no
  // effect and returns false.
  [[nodiscard]] bool RemoveQueuedCommand(TransactionId id);

  // Used to identify an individual HCI event handler that was registered with
  // this CommandChannel.
  using EventHandlerId = size_t;

  // Return values for EventCallbacks.
  enum class EventCallbackResult {
    // Continue handling this event.
    kContinue,

    // Remove this event handler.
    // NOTE: The event callback may still be called again after it has been removed
    // if the handler has already been posted to the dispatcher for subsequent events.
    kRemove
  };

  // Callback invoked to report generic HCI events excluding CommandComplete and
  // CommandStatus events.
  using EventCallback = fit::function<EventCallbackResult(const EventPacket& event_packet)>;

  // Registers an event handler for HCI events that match |event_code|. Incoming
  // HCI event packets that are not associated with a pending command sequence
  // will be posted on the given |dispatcher| via the given |event_callback|.
  // The returned ID can be used to unregister a previously registered event
  // handler.
  //
  // |event_callback| will be invoked for all HCI event packets that match
  // |event_code|, except for:
  //   - HCI_CommandStatus events;
  //   - HCI_CommandComplete events;
  //   - The completion event of the currently pending command packet, if any;
  //
  // Returns an ID if the handler was successfully registered. Returns
  // zero in case of an error.
  //
  // Multiple handlers can be registered for a given |event_code| at a time.
  // All handlers that are registered will be called with a reference to the
  // event.
  //
  // If an asynchronous command is queued which completes on |event_code|, this
  // method returns zero. It is good practice to avoid using asynchronous
  // commands and event handlers for the same event code.  SendCommand allows
  // for queueing multiple asynchronous commands with the same callback.
  // Alternately a long-lived event handler can be registered with Commands
  // completing on CommandStatus.
  //
  // The following values for |event_code| cannot be passed to this method:
  //    - HCI_Command_Complete event code
  //    - HCI_Command_Status event code
  //    - HCI_LE_Meta event code (use AddLEMetaEventHandler instead).
  EventHandlerId AddEventHandler(EventCode event_code, EventCallback event_callback);

  // Works just like AddEventHandler but the passed in event code is only valid
  // within the LE Meta Event sub-event code namespace. |event_callback| will
  // get invoked whenever the controller sends a LE Meta Event with a matching
  // subevent code.
  //
  // |subevent_code| cannot be 0.
  EventHandlerId AddLEMetaEventHandler(EventCode subevent_code, EventCallback event_callback);

  // Removes a previously registered event handler. Does nothing if an event
  // handler with the given |id| could not be found.
  void RemoveEventHandler(EventHandlerId id);

  // Returns the underlying channel handle.
  const zx::channel& channel() const { return channel_; }

  // Set callback that will be called when a command times out after kCommandTimeout. This is
  // distinct from channel closure.
  void set_channel_timeout_cb(fit::closure timeout_cb) {
    channel_timeout_cb_ = std::move(timeout_cb);
  }

  // Reads bytes from the channel and try to parse them as EventPacket.
  // ZX_ERR_IO means error happens while reading from the channel.
  // ZX_ERR_INVALID_ARGS means the packet is malformed.
  // Otherwise, ZX_OK is returned.
  static zx_status_t ReadEventPacketFromChannel(const zx::channel& channel,
                                                const EventPacketPtr& packet);

  fxl::WeakPtr<CommandChannel> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  CommandChannel(Transport* transport, zx::channel hci_command_channel);

  TransactionId SendExclusiveCommandInternal(std::unique_ptr<CommandPacket> command_packet,
                                             CommandCallback callback,
                                             const EventCode complete_event_code,
                                             std::optional<EventCode> subevent_code = std::nullopt,
                                             std::unordered_set<OpCode> exclusions = {});

  // Data related to a queued or running command.
  //
  // When a command is sent to the controller, it is placed in the
  // pending_transactions_ map.  It remains in the pending_transactions_ map
  // until it is completed - asynchronous commands remain until their
  // complete_event_code_ is received.
  //
  // There are a number of reasons a command may be queued before sending:
  //  - An asynchronous command is waiting on the same event code
  //  - A command with an opcode that is in the exclusions set for this
  //    transaction is pending.
  //  - We cannot send any commands because of the limit of outstanding command
  //    packets from the controller
  // Queued commands are held in the send_queue_ and are sent when possible,
  // FIFO (but skipping commands that cannot be sent)
  class TransactionData {
   public:
    TransactionData(TransactionId id, OpCode opcode, EventCode complete_event_code,
                    std::optional<EventCode> subevent_code, std::unordered_set<OpCode> exclusions,
                    CommandCallback callback);
    ~TransactionData();

    // Starts the transaction timer, which will call timeout_cb if it's not
    // completed in time.
    void Start(fit::closure timeout_cb, zx::duration timeout);

    // Completes the transaction with |event|.
    void Complete(std::unique_ptr<EventPacket> event);

    // Cancels the transaction timeout and erases the callback so it isn't called upon destruction.
    void Cancel();

    // Makes an EventCallback that calls |callback_| correctly.
    EventCallback MakeCallback();

    EventCode complete_event_code() const { return complete_event_code_; }
    std::optional<EventCode> subevent_code() const { return subevent_code_; }
    OpCode opcode() const { return opcode_; }
    TransactionId id() const { return id_; }
    // The set of opcodes in progress that will hold this transaction in queue.
    const std::unordered_set<OpCode>& exclusions() const { return exclusions_; };

    EventHandlerId handler_id() const { return handler_id_; }
    void set_handler_id(EventHandlerId id) { handler_id_ = id; }

   private:
    TransactionId id_;
    OpCode opcode_;
    EventCode complete_event_code_;
    std::optional<EventCode> subevent_code_;
    std::unordered_set<OpCode> exclusions_;
    CommandCallback callback_;
    async::TaskClosure timeout_task_;
    // If non-zero, the id of the handler registered for this transaction.
    // Always zero if this transaction is synchronous.
    EventHandlerId handler_id_;

    DISALLOW_COPY_ASSIGN_AND_MOVE(TransactionData);
  };

  // Adds an internal event handler for |data| if one does not exist yet and
  // another transaction is not waiting on the same event.
  // Used to add expiring event handlers for asynchronous commands.
  void MaybeAddTransactionHandler(TransactionData* data);

  // Represents a queued command packet.
  struct QueuedCommand {
    QueuedCommand(std::unique_ptr<CommandPacket> command_packet,
                  std::unique_ptr<TransactionData> data);
    QueuedCommand() = default;

    QueuedCommand(QueuedCommand&& other) = default;
    QueuedCommand& operator=(QueuedCommand&& other) = default;

    std::unique_ptr<CommandPacket> packet;
    std::unique_ptr<TransactionData> data;
  };

  // Data stored for each event handler registered.
  struct EventHandlerData {
    EventHandlerId id;
    // If |is_le_meta_subevent|, then |event_code| is the LE Meta Event subevent code.
    EventCode event_code;
    // For asynchronous transaction event handlers, the pending command opcode.
    // kNoOp if this is a static event handler.
    OpCode pending_opcode;
    EventCallback event_callback;
    bool is_le_meta_subevent;

    // Returns true if handler is for async command transaction, or false if handler is a static
    // event handler.
    bool is_async() const { return pending_opcode != kNoOp; }
  };

  void ShutDownInternal();

  // Finds the event handler for |code|.  Returns nullptr if one doesn't exist.
  EventHandlerData* FindEventHandler(EventCode code);

  // Finds the LE Meta Event handler for |subevent_code|.  Returns nullptr if one doesn't exist.
  EventHandlerData* FindLEMetaEventHandler(EventCode subevent_code);

  // Removes internal event handler structures for |id|.
  void RemoveEventHandlerInternal(EventHandlerId id);

  // Sends any queued commands that can be processed unambiguously
  // and complete.
  void TrySendQueuedCommands();

  // Sends |command|, adding an internal event handler if asynchronous.
  void SendQueuedCommand(QueuedCommand&& cmd);

  // Creates a new event handler entry in the event handler map and returns its
  // ID. If |is_le_meta|, then |event_code| should be the LE Meta Event subevent code.
  EventHandlerId NewEventHandler(EventCode event_code, bool is_le_meta, OpCode pending_opcode,
                                 EventCallback event_callback);

  // Notifies any matching event handler for |event|.
  void NotifyEventHandler(std::unique_ptr<EventPacket> event);

  // Run callback and handle callback result.
  void ExecuteEventCallback(EventCallback cb, EventHandlerId id,
                            std::unique_ptr<EventPacket> event);

  // Notifies handlers for Command Status and Command Complete Events.
  // This function marks opcodes that have transactions pending as complete
  // by removing them and calling their callbacks.
  void UpdateTransaction(std::unique_ptr<EventPacket> event);

  // Read ready handler for |channel_|
  void OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                      const zx_packet_signal_t* signal);

  // Opcodes of commands that we have sent to the controller but not received a
  // status update from.  New commands with these opcodes can't be sent because
  // they are indistinguishable from ones we need to get the result of.
  std::unordered_map<OpCode, std::unique_ptr<TransactionData>> pending_transactions_;

  // TransactionId counter.
  size_t next_transaction_id_;

  // EventHandlerId counter.
  size_t next_event_handler_id_;

  // Used to assert that certain public functions are only called on the
  // creation thread.
  fit::thread_checker thread_checker_;

  // The Transport object that owns this CommandChannel.
  Transport* transport_;  // weak

  // The channel we use to send/receive HCI commands/events.
  zx::channel channel_;

  // Callback called when a command times out.
  fit::closure channel_timeout_cb_;

  // Wait object for |channel_|
  async::WaitMethod<CommandChannel, &CommandChannel::OnChannelReady> channel_wait_{this};

  // True if channel initialization succeeded and ShutDown() has not been called yet.
  bool is_initialized_;

  // The HCI command queue.
  // This queue is not necessarily sent in order, but commands with the same
  // opcode or that wait on the same completion event code are sent first-in,
  // first-out.
  std::list<QueuedCommand> send_queue_;

  // Contains the current count of commands we ae allowed to send to the
  // controller.  This is decremented when we send a command and updated
  // when we receive a CommandStatus or CommandComplete event with the Num
  // HCI Command Packets parameter.
  //
  // Accessed only from the I/O thread and thus not guarded.
  size_t allowed_command_packets_;

  // Mapping from event handler IDs to handler data.
  std::unordered_map<EventHandlerId, EventHandlerData> event_handler_id_map_;

  // Mapping from event code to the event handlers that were registered to
  // handle that event code.
  std::unordered_multimap<EventCode, EventHandlerId> event_code_handlers_;

  // Mapping from LE Meta Event Subevent code to the event handlers that were
  // registered to handle that event code.
  std::unordered_multimap<EventCode, EventHandlerId> subevent_code_handlers_;

  fxl::WeakPtrFactory<CommandChannel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(CommandChannel);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_
