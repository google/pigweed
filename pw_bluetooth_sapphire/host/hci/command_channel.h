// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <lib/async/cpp/task.h>
#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>
#include <zircon/compiler.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "lib/fxl/functional/cancelable_callback.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/ref_ptr.h"
#include "lib/fxl/synchronization/thread_checker.h"

namespace btlib {
namespace hci {

class Transport;

// Represents the HCI Bluetooth command channel. Manages HCI command and event
// packet control flow.
//
// TODO(armansito): I don't imagine many cases in which we will want to queue up
// HCI commands from the data thread. Consider making this class fully single
// threaded and removing the locks.
class CommandChannel final {
 public:
  // |hci_command_channel| is a Zircon channel construct that can receive
  // Bluetooth HCI command and event packets, in which the remote end is
  // implemented by the underlying Bluetooth HCI device driver.
  //
  // |transport| is the Transport instance that owns this CommandChannel.
  CommandChannel(Transport* transport, zx::channel hci_command_channel);
  ~CommandChannel();

  // Starts listening on the HCI command channel and starts handling commands
  // and events.
  void Initialize();

  // Unregisters event handlers and cleans up.
  // NOTE: Initialize() and ShutDown() MUST be called on the same thread. These
  // methods are not thread-safe.
  void ShutDown();

  // Used to identify an individual HCI command<->event transaction.
  using TransactionId = size_t;

  // Queues the given |command_packet| to be sent to the controller and returns
  // a transaction ID. The given |callback| will be posted on |dispatcher| to
  // be processed on the appropriate thread requested by the caller.
  //
  // This call takes ownership of the contents of |command_packet|.
  // |command_packet| MUST represent a valid HCI command packet.
  //
  // |callback| will be called with all events related to the transaction.
  // If the command results in a CommandStatus event, it will be sent to this
  // callback before the event with |complete_event_code| is sent.
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
  // |complete_event_code| cannot be an LE event code, the LE Meta Event code,
  // or a code that has been registered for events via AddEventHandler.
  //
  // TODO(jamuraa): Support LE Event Codes for LE Commands.
  // TODO(jamuraa): Add a way to cancel commands (NET-619)
  //
  // Returns a ID unique to the command transaction, or zero if the parameters
  // are invalid.  This ID will be supplied to |callback| in its |id| parameter
  // to identify the transaction.
  //
  // NOTE: Commands queued are not guaranteed to be finished or sent in order,
  // although commands with the same opcode will be sent in order, and
  // commands with the same |complete_code| will be sent in order.
  // If strict ordering of commands is required, use SequentialCommandRunner
  // or callbacks for sequencing.
  //
  // See Bluetooth Core Spec v5.0, Volume 2, Part E, Section 4.4 "Command Flow
  // Control" for more information about the HCI command flow control.
  using CommandCallback =
      fit::function<void(TransactionId id, const EventPacket& event)>;
  TransactionId SendCommand(
      std::unique_ptr<CommandPacket> command_packet,
      async_dispatcher_t* dispatcher, CommandCallback callback,
      const EventCode complete_event_code = kCommandCompleteEventCode);

  // Used to identify an individual HCI event handler that was registered with
  // this CommandChannel.
  using EventHandlerId = size_t;

  // Callback invoked to report generic HCI events excluding CommandComplete and
  // CommandStatus events.
  using EventCallback = fit::function<void(const EventPacket& event_packet)>;

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
  // If |dispatcher| corresponds to the I/O thread's dispatcher, then the
  // callback will be executed as soon as the event is received from the command
  // channel. No delayed task posting will occur.
  //
  // The following values for |event_code| cannot be passed to this method:
  //    - HCI_Command_Complete event code
  //    - HCI_Command_Status event code
  //    - HCI_LE_Meta event code (use AddLEMetaEventHandler instead).
  EventHandlerId AddEventHandler(EventCode event_code,
                                 EventCallback event_callback,
                                 async_dispatcher_t* dispatcher);

  // Works just like AddEventHandler but the passed in event code is only valid
  // within the LE Meta Event sub-event code namespace. |event_callback| will
  // get invoked whenever the controller sends a LE Meta Event with a matching
  // subevent code.
  //
  // |subevent_code| cannot be 0.
  EventHandlerId AddLEMetaEventHandler(EventCode subevent_code,
                                       EventCallback event_callback,
                                       async_dispatcher_t* dispatcher);

  // Removes a previously registered event handler. Does nothing if an event
  // handler with the given |id| could not be found.
  void RemoveEventHandler(EventHandlerId id);

  // Returns the underlying channel handle.
  const zx::channel& channel() const { return channel_; }

 private:
  // Represents a pending or running HCI command.
  class TransactionData {
   public:
    TransactionData(TransactionId id, OpCode opcode,
                    EventCode complete_event_code, CommandCallback callback,
                    async_dispatcher_t* dispatcher);
    ~TransactionData();

    // Starts the transaction timer, which will call timeout_cb if it's not
    // completed in time.
    void Start(fit::closure timeout_cb, zx::duration timeout);

    // Completes the transaction with |event|.
    void Complete(std::unique_ptr<EventPacket> event);

    // Makes an EventCallback that calls the callback correctly.
    EventCallback MakeCallback();

    async_dispatcher_t* dispatcher() const { return dispatcher_; }
    EventCode complete_event_code() const { return complete_event_code_; }
    OpCode opcode() const { return opcode_; }
    TransactionId id() const { return id_; }

    EventHandlerId handler_id() const { return handler_id_; }
    void set_handler_id(EventHandlerId id) { handler_id_ = id; }

   private:
    TransactionId id_;
    OpCode opcode_;
    EventCode complete_event_code_;
    CommandCallback callback_;
    async_dispatcher_t* dispatcher_;
    async::TaskClosure timeout_task_;
    EventHandlerId handler_id_;

    FXL_DISALLOW_COPY_ASSIGN_AND_MOVE(TransactionData);
  };

  // Adds an internal event handler for |data| if one does not exist yet and
  // another transaction is not waiting on the same event.
  // Used to add expiring event handlers for asynchronous commands.
  void MaybeAddTransactionHandler(TransactionData* data)
      __TA_REQUIRES(event_handler_mutex_);

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

  // Data stored for each event handler registered via AddEventHandler.
  struct EventHandlerData {
    EventHandlerId id;
    EventCode event_code;
    EventCallback event_callback;
    bool is_le_meta_subevent;
    async_dispatcher_t* dispatcher;
  };

  void ShutDownInternal()
      __TA_EXCLUDES(send_queue_mutex_, event_handler_mutex_);

  // Removes internal event handler structures for |id|.
  void RemoveEventHandlerInternal(EventHandlerId id)
      __TA_REQUIRES(event_handler_mutex_);

  // Sends any queued commands that can be processed unambiguously
  // and complete.
  void TrySendQueuedCommands()
      __TA_EXCLUDES(send_queue_mutex_, event_handler_mutex_);

  // Sends |command|, adding an internal event handler if asynchronous.
  void SendQueuedCommand(QueuedCommand&& cmd)
      __TA_REQUIRES(event_handler_mutex_);

  // Creates a new event handler entry in the event handler map and returns its
  // ID.
  EventHandlerId NewEventHandler(EventCode event_code, bool is_le_meta,
                                 EventCallback event_callback,
                                 async_dispatcher_t* dispatcher)
      __TA_REQUIRES(event_handler_mutex_);

  // Notifies any matching event handler for |event|.
  void NotifyEventHandler(std::unique_ptr<EventPacket> event);

  // Notifies handlers for Command Status and Command Complete Events.
  // This function marks opcodes that have transactions pending as complete
  // by removing them and calling their callbacks.
  void UpdateTransaction(std::unique_ptr<EventPacket> event);

  // Read ready handler for |channel_|
  void OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                      zx_status_t status, const zx_packet_signal_t* signal);

  // Opcodes of commands that we have sent to the controller but not received a
  // status update from.  New commands with these opcodes can't be sent because
  // they are indistinguishable from ones we need to get the result of.
  std::unordered_map<OpCode, std::unique_ptr<TransactionData>>
      pending_transactions_ __TA_GUARDED(event_handler_mutex_);

  // TransactionId counter.
  std::atomic_size_t next_transaction_id_ __TA_GUARDED(send_queue_mutex_);

  // EventHandlerId counter.
  std::atomic_size_t next_event_handler_id_ __TA_GUARDED(event_handler_mutex_);

  // Used to assert that certain public functions are only called on the
  // creation thread.
  fxl::ThreadChecker thread_checker_;

  // The Transport object that owns this CommandChannel.
  Transport* transport_;  // weak

  // The channel we use to send/receive HCI commands/events.
  zx::channel channel_;

  // Wait object for |channel_|
  async::WaitMethod<CommandChannel, &CommandChannel::OnChannelReady>
      channel_wait_{this};

  // True if this CommandChannel has been initialized through a call to
  // Initialize().
  std::atomic_bool is_initialized_;

  // The dispatcher used for posting tasks on the HCI transport I/O thread.
  async_dispatcher_t* io_dispatcher_;

  // Guards |send_queue_|. |send_queue_| can get accessed by threads that call
  // SendCommand() as well as from |io_thread_|.
  std::mutex send_queue_mutex_;

  // The HCI command queue.
  // This queue is not necessarily sent in order, but commands with the same
  // opcode or that wait on the same completion event code are sent first-in,
  // first-out.
  std::list<QueuedCommand> send_queue_ __TA_GUARDED(send_queue_mutex_);

  // Contains the current count of commands we ae allowed to send to the
  // controller.  This is decremented when we send a command and updated
  // when we receive a CommandStatus or CommandComplete event with the Num
  // HCI Command Packets parameter.
  //
  // Accessed only from the I/O thread and thus not guarded.
  size_t allowed_command_packets_;

  // Guards |event_handler_id_map_| and |event_code_handlers_| which can be
  // accessed by both the public EventHandler methods and |io_thread_|.
  std::mutex event_handler_mutex_;

  // Mapping from event handler IDs to handler data.
  std::unordered_map<EventHandlerId, EventHandlerData> event_handler_id_map_
      __TA_GUARDED(event_handler_mutex_);

  // Mapping from event code to the event handlers that were registered to
  // handle that event code.
  std::unordered_multimap<EventCode, EventHandlerId> event_code_handlers_
      __TA_GUARDED(event_handler_mutex_);

  // Mapping from LE Meta Event Subevent code to the event handlers that were
  // registered to handle that event code.
  std::unordered_multimap<EventCode, EventHandlerId> subevent_code_handlers_
      __TA_GUARDED(event_handler_mutex_);

  // Mapping from event code to the event handler for async command completion.
  // These are automatically removed after being called once.
  // Any event codes in this map are also in event_code_handlers_ and
  // the event handler id here must be the only one for this event code.
  //
  // The event ids in this map can not be detected or removed using
  // RemoveEventHandler (but can by RemoveEventHandlerInternal).
  std::unordered_map<EventCode, EventHandlerId> async_cmd_handlers_
      __TA_GUARDED(event_handler_mutex_);

  FXL_DISALLOW_COPY_AND_ASSIGN(CommandChannel);
};

}  // namespace hci
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_CHANNEL_H_
