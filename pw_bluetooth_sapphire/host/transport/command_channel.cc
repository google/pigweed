// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_channel.h"

#include <endian.h>
#include <lib/async/default.h>
#include <lib/trace/event.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "transport.h"

namespace bt::hci {

static bool IsAsync(hci_spec::EventCode code) {
  return code != hci_spec::kCommandCompleteEventCode && code != hci_spec::kCommandStatusEventCode;
}

static std::string EventTypeToString(CommandChannel::EventType event_type) {
  switch (event_type) {
    case CommandChannel::EventType::kHciEvent:
      return "hci_event";
    case CommandChannel::EventType::kLEMetaEvent:
      return "le_meta_event";
  }
}

CommandChannel::QueuedCommand::QueuedCommand(std::unique_ptr<CommandPacket> command_packet,
                                             std::unique_ptr<TransactionData> transaction_data)
    : packet(std::move(command_packet)), data(std::move(transaction_data)) {
  ZX_DEBUG_ASSERT(data);
  ZX_DEBUG_ASSERT(packet);
}

CommandChannel::TransactionData::TransactionData(
    TransactionId id, hci_spec::OpCode opcode, hci_spec::EventCode complete_event_code,
    std::optional<hci_spec::EventCode> le_meta_subevent_code,
    std::unordered_set<hci_spec::OpCode> exclusions, CommandCallback callback)
    : id_(id),
      opcode_(opcode),
      complete_event_code_(complete_event_code),
      le_meta_subevent_code_(le_meta_subevent_code),
      exclusions_(std::move(exclusions)),
      callback_(std::move(callback)),
      handler_id_(0u) {
  ZX_DEBUG_ASSERT(id != 0u);
  exclusions_.insert(opcode_);
}

CommandChannel::TransactionData::~TransactionData() {
  if (!callback_) {
    return;
  }

  bt_log(DEBUG, "hci", "sending kUnspecifiedError for unfinished transaction %zu", id_);
  std::unique_ptr<EventPacket> event = EventPacket::New(sizeof(hci_spec::CommandStatusEventParams));
  // Init buffer to prevent stale data in buffer.
  event->mutable_view()->mutable_data().SetToZeros();

  hci_spec::EventHeader* header = event->mutable_view()->mutable_header();
  hci_spec::CommandStatusEventParams* params =
      event->mutable_view()->mutable_payload<hci_spec::CommandStatusEventParams>();

  // TODO(armansito): Instead of lying about receiving a Command Status event,
  // report this error in a different way. This can be highly misleading during
  // debugging.
  header->event_code = hci_spec::kCommandStatusEventCode;
  header->parameter_total_size = sizeof(hci_spec::CommandStatusEventParams);
  params->status = hci_spec::kUnspecifiedError;
  params->command_opcode = opcode_;

  Complete(std::move(event));
}

void CommandChannel::TransactionData::Start(fit::closure timeout_cb, zx::duration timeout) {
  // Transactions should only ever be started once.
  ZX_DEBUG_ASSERT(!timeout_task_.is_pending());

  timeout_task_.set_handler(std::move(timeout_cb));
  timeout_task_.PostDelayed(async_get_default_dispatcher(), timeout);
}

void CommandChannel::TransactionData::Complete(std::unique_ptr<EventPacket> event) {
  timeout_task_.Cancel();
  if (!callback_) {
    return;
  }
  async::PostTask(async_get_default_dispatcher(),
                  [event = std::move(event), callback = std::move(callback_),
                   transaction_id = id_]() mutable { callback(transaction_id, *event); });
  callback_ = nullptr;
}

void CommandChannel::TransactionData::Cancel() {
  timeout_task_.Cancel();
  callback_ = nullptr;
}

CommandChannel::EventCallback CommandChannel::TransactionData::MakeCallback() {
  return [id = id_, cb = callback_.share()](const EventPacket& event) {
    cb(id, event);
    return EventCallbackResult::kContinue;
  };
}

fpromise::result<std::unique_ptr<CommandChannel>> CommandChannel::Create(
    Transport* transport, zx::channel hci_command_channel) {
  std::unique_ptr<CommandChannel> channel = std::unique_ptr<CommandChannel>(
      new CommandChannel(transport, std::move(hci_command_channel)));

  if (!channel->is_initialized_) {
    return fpromise::error();
  }
  return fpromise::ok(std::move(channel));
}

CommandChannel::CommandChannel(Transport* transport, zx::channel hci_command_channel)
    : transport_(transport),
      channel_(std::move(hci_command_channel)),
      channel_wait_(this, channel_.get(), ZX_CHANNEL_READABLE),
      weak_ptr_factory_(this) {
  ZX_ASSERT(transport_);
  ZX_ASSERT(channel_.is_valid());

  zx_status_t status = channel_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "failed channel setup: %s", zx_status_get_string(status));
    channel_wait_.set_object(ZX_HANDLE_INVALID);
    return;
  }
  bt_log(INFO, "hci", "initialized");

  is_initialized_ = true;
}

void CommandChannel::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  if (!is_initialized_) {
    return;
  }

  bt_log(INFO, "hci", "shutting down");
  bt_log(DEBUG, "hci", "removing I/O handler");

  // Prevent new command packets from being queued.
  is_initialized_ = false;

  // Stop listening for HCI events.
  zx_status_t status = channel_wait_.Cancel();
  if (status != ZX_OK) {
    bt_log(WARN, "hci", "could not cancel wait on channel: %s", zx_status_get_string(status));
  }

  // Drop all queued commands and event handlers. Pending HCI commands will be resolved with an
  // "UnspecifiedError" error code upon destruction.
  send_queue_ = std::list<QueuedCommand>();
  event_handler_id_map_.clear();
  event_code_handlers_.clear();
  le_meta_subevent_code_handlers_.clear();
  pending_transactions_.clear();
}

CommandChannel::TransactionId CommandChannel::SendCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const hci_spec::EventCode complete_event_code) {
  return SendExclusiveCommand(std::move(command_packet), std::move(callback), complete_event_code);
}

CommandChannel::TransactionId CommandChannel::SendLeAsyncCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    hci_spec::EventCode le_meta_subevent_code) {
  return SendLeAsyncExclusiveCommand(std::move(command_packet), std::move(callback),
                                     le_meta_subevent_code);
}

CommandChannel::TransactionId CommandChannel::SendExclusiveCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const hci_spec::EventCode complete_event_code,
    std::unordered_set<hci_spec::OpCode> exclusions) {
  return SendExclusiveCommandInternal(std::move(command_packet), std::move(callback),
                                      complete_event_code, std::nullopt, std::move(exclusions));
}

CommandChannel::TransactionId CommandChannel::SendLeAsyncExclusiveCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    std::optional<hci_spec::EventCode> le_meta_subevent_code,
    std::unordered_set<hci_spec::OpCode> exclusions) {
  return SendExclusiveCommandInternal(std::move(command_packet), std::move(callback),
                                      hci_spec::kLEMetaEventCode, le_meta_subevent_code,
                                      std::move(exclusions));
}

CommandChannel::TransactionId CommandChannel::SendExclusiveCommandInternal(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const hci_spec::EventCode complete_event_code,
    std::optional<hci_spec::EventCode> le_meta_subevent_code,
    std::unordered_set<hci_spec::OpCode> exclusions) {
  if (!is_initialized_) {
    bt_log(DEBUG, "hci", "can't send commands while uninitialized");
    return 0u;
  }

  ZX_ASSERT(command_packet);
  ZX_ASSERT_MSG(
      (complete_event_code == hci_spec::kLEMetaEventCode) == le_meta_subevent_code.has_value(),
      "only LE Meta Event subevents are supported");

  if (IsAsync(complete_event_code)) {
    // Cannot send an asynchronous command if there's an external event handler registered for the
    // completion event.
    EventHandlerData* handler = nullptr;
    if (le_meta_subevent_code.has_value()) {
      handler = FindLEMetaEventHandler(*le_meta_subevent_code);
    } else {
      handler = FindEventHandler(complete_event_code);
    }

    if (handler && !handler->is_async()) {
      bt_log(DEBUG, "hci", "event handler already handling this event");
      return 0u;
    }
  }

  if (next_transaction_id_ == 0u) {
    next_transaction_id_++;
  }

  TransactionId id = next_transaction_id_++;
  std::unique_ptr<CommandChannel::TransactionData> data = std::make_unique<TransactionData>(
      id, command_packet->opcode(), complete_event_code, le_meta_subevent_code,
      std::move(exclusions), std::move(callback));

  QueuedCommand command(std::move(command_packet), std::move(data));

  if (IsAsync(complete_event_code)) {
    MaybeAddTransactionHandler(command.data.get());
  }

  send_queue_.push_back(std::move(command));

  async::PostTask(async_get_default_dispatcher(),
                  std::bind(&CommandChannel::TrySendQueuedCommands, this));

  return id;
}

bool CommandChannel::RemoveQueuedCommand(TransactionId id) {
  auto it = std::find_if(send_queue_.begin(), send_queue_.end(),
                         [id](const QueuedCommand& cmd) { return cmd.data->id() == id; });
  if (it == send_queue_.end()) {
    // The transaction to remove has already finished or never existed.
    bt_log(TRACE, "hci", "command to remove not found, id: %zu", id);
    return false;
  }

  bt_log(TRACE, "hci", "removing queued command id: %zu", id);
  TransactionData& data = *it->data;
  data.Cancel();

  RemoveEventHandlerInternal(data.handler_id());
  send_queue_.erase(it);
  return true;
}

CommandChannel::EventHandlerId CommandChannel::AddEventHandler(hci_spec::EventCode event_code,
                                                               EventCallback event_callback) {
  if (event_code == hci_spec::kCommandStatusEventCode ||
      event_code == hci_spec::kCommandCompleteEventCode ||
      event_code == hci_spec::kLEMetaEventCode) {
    return 0u;
  }

  EventHandlerData* handler = FindEventHandler(event_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci", "async event handler %zu already registered for event code %#.2x",
           handler->id, event_code);
    return 0u;
  }

  EventHandlerId id =
      NewEventHandler(event_code, EventType::kHciEvent, hci_spec::kNoOp, std::move(event_callback));
  event_code_handlers_.emplace(event_code, id);
  return id;
}

CommandChannel::EventHandlerId CommandChannel::AddLEMetaEventHandler(
    hci_spec::EventCode le_meta_subevent_code, EventCallback event_callback) {
  EventHandlerData* handler = FindLEMetaEventHandler(le_meta_subevent_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci",
           "async event handler %zu already registered for LE Meta Event subevent code %#.2x",
           handler->id, le_meta_subevent_code);
    return 0u;
  }

  EventHandlerId id = NewEventHandler(le_meta_subevent_code, EventType::kLEMetaEvent,
                                      hci_spec::kNoOp, std::move(event_callback));
  le_meta_subevent_code_handlers_.emplace(le_meta_subevent_code, id);
  return id;
}

void CommandChannel::RemoveEventHandler(EventHandlerId id) {
  // If the ID doesn't exist or it is internal. it can't be removed.
  auto iter = event_handler_id_map_.find(id);
  if (iter == event_handler_id_map_.end() || iter->second.is_async()) {
    return;
  }

  RemoveEventHandlerInternal(id);
}

CommandChannel::EventHandlerData* CommandChannel::FindEventHandler(hci_spec::EventCode code) {
  auto it = event_code_handlers_.find(code);
  if (it == event_code_handlers_.end()) {
    return nullptr;
  }
  return &event_handler_id_map_[it->second];
}

CommandChannel::EventHandlerData* CommandChannel::FindLEMetaEventHandler(
    hci_spec::EventCode le_meta_subevent_code) {
  auto it = le_meta_subevent_code_handlers_.find(le_meta_subevent_code);
  if (it == le_meta_subevent_code_handlers_.end()) {
    return nullptr;
  }
  return &event_handler_id_map_[it->second];
}

void CommandChannel::RemoveEventHandlerInternal(EventHandlerId id) {
  auto iter = event_handler_id_map_.find(id);
  if (iter == event_handler_id_map_.end()) {
    return;
  }

  std::unordered_multimap<hci_spec::EventCode, EventHandlerId>* handlers = nullptr;
  switch (iter->second.event_type) {
    case EventType::kHciEvent:
      handlers = &event_code_handlers_;
      break;
    case EventType::kLEMetaEvent:
      handlers = &le_meta_subevent_code_handlers_;
      break;
  }

  bt_log(TRACE, "hci", "removing handler for %s event code %#.2x",
         EventTypeToString(iter->second.event_type).c_str(), iter->second.event_code);

  auto range = handlers->equal_range(iter->second.event_code);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == id) {
      it = handlers->erase(it);
      break;
    }
  }

  event_handler_id_map_.erase(iter);
}

void CommandChannel::TrySendQueuedCommands() {
  if (!is_initialized_) {
    return;
  }

  if (allowed_command_packets_ == 0) {
    bt_log(TRACE, "hci", "controller queue full, waiting");
    return;
  }

  // Walk the waiting and see if any are sendable.
  for (auto it = send_queue_.begin(); allowed_command_packets_ > 0 && it != send_queue_.end();) {
    // Care must be taken not to dangle this reference if its owner QueuedCommand is destroyed.
    const TransactionData& data = *it->data;

    // Can't send if another is running with an opcode this can't coexist with.
    bool excluded = false;
    for (hci_spec::OpCode excluded_opcode : data.exclusions()) {
      if (pending_transactions_.count(excluded_opcode) != 0) {
        bt_log(TRACE, "hci", "pending command (%#.4x) delayed due to running opcode %#.4x",
               it->data->opcode(), excluded_opcode);
        excluded = true;
        break;
      }
    }
    if (excluded) {
      ++it;
      continue;
    }

    bool transaction_waiting_on_event = event_code_handlers_.count(data.complete_event_code());
    bool transaction_waiting_on_subevent =
        data.le_meta_subevent_code() &&
        le_meta_subevent_code_handlers_.count(*data.le_meta_subevent_code());
    bool waiting_for_other_transaction =
        transaction_waiting_on_event || transaction_waiting_on_subevent;

    // We can send this if we only expect one update, or if we aren't waiting for another
    // transaction to complete on the same event. It is unlikely but possible to have commands with
    // different opcodes wait on the same completion event.
    if (!IsAsync(data.complete_event_code()) || data.handler_id() != 0 ||
        !waiting_for_other_transaction) {
      bt_log(TRACE, "hci", "sending previously queued command id %zu", data.id());
      SendQueuedCommand(std::move(*it));
      it = send_queue_.erase(it);
      continue;
    }
    ++it;
  }
}

void CommandChannel::SendQueuedCommand(QueuedCommand&& cmd) {
  BufferView packet_bytes = cmd.packet->view().data();
  zx_status_t status = channel_.write(/*flags=*/0, packet_bytes.data(), packet_bytes.size(),
                                      /*handles=*/nullptr, /*num_handles=*/0);
  if (status < 0) {
    // TODO(armansito): We should notify the |status_callback| of the pending
    // command with a special error code in this case.
    bt_log(ERROR, "hci", "failed to send command: %s", zx_status_get_string(status));
    return;
  }
  allowed_command_packets_--;

  std::unique_ptr<TransactionData>& transaction = cmd.data;

  transaction->Start(
      [this, id = cmd.data->id()] {
        bt_log(ERROR, "hci", "command %zu timed out, notifying error", id);
        if (channel_timeout_cb_) {
          channel_timeout_cb_();
        }
      },
      hci_spec::kCommandTimeout);

  MaybeAddTransactionHandler(transaction.get());

  pending_transactions_.insert(std::make_pair(transaction->opcode(), std::move(transaction)));
}

void CommandChannel::MaybeAddTransactionHandler(TransactionData* data) {
  // We don't need to add a transaction handler for synchronous transactions.
  if (!IsAsync(data->complete_event_code())) {
    return;
  }

  EventType event_type = EventType::kHciEvent;
  std::unordered_multimap<hci_spec::EventCode, EventHandlerId>* handlers = nullptr;

  if (data->le_meta_subevent_code().has_value()) {
    event_type = EventType::kLEMetaEvent;
    handlers = &le_meta_subevent_code_handlers_;
  } else {
    event_type = EventType::kHciEvent;
    handlers = &event_code_handlers_;
  }

  const hci_spec::EventCode code =
      data->le_meta_subevent_code().value_or(data->complete_event_code());

  // We already have a handler for this transaction, or another transaction is already waiting and
  // it will be queued.
  if (handlers->count(code)) {
    bt_log(TRACE, "hci", "async command %zu: already has handler", data->id());
    return;
  }

  EventHandlerId id = NewEventHandler(code, event_type, data->opcode(), data->MakeCallback());

  ZX_ASSERT(id != 0u);
  data->set_handler_id(id);
  handlers->emplace(code, id);
  bt_log(TRACE, "hci", "async command %zu assigned handler %zu", data->id(), id);
}

CommandChannel::EventHandlerId CommandChannel::NewEventHandler(hci_spec::EventCode event_code,
                                                               EventType event_type,
                                                               hci_spec::OpCode pending_opcode,
                                                               EventCallback event_callback) {
  ZX_DEBUG_ASSERT(event_code);
  ZX_DEBUG_ASSERT(event_callback);

  size_t id = next_event_handler_id_++;
  EventHandlerData data;
  data.id = id;
  data.event_code = event_code;
  data.event_type = event_type;
  data.pending_opcode = pending_opcode;
  data.event_callback = std::move(event_callback);

  bt_log(TRACE, "hci", "adding event handler %zu for %s event code %#.2x", id,
         EventTypeToString(event_type).c_str(), event_code);
  ZX_DEBUG_ASSERT(event_handler_id_map_.find(id) == event_handler_id_map_.end());
  event_handler_id_map_[id] = std::move(data);

  return id;
}

void CommandChannel::UpdateTransaction(std::unique_ptr<EventPacket> event) {
  hci_spec::EventCode event_code = event->event_code();

  ZX_DEBUG_ASSERT(event_code == hci_spec::kCommandStatusEventCode ||
                  event_code == hci_spec::kCommandCompleteEventCode);

  hci_spec::OpCode matching_opcode;

  // The HCI Command Status event with an error status might indicate that an async command failed.
  // We use this to unregister async command handlers below.
  bool unregister_async_handler = false;

  if (event->event_code() == hci_spec::kCommandCompleteEventCode) {
    const hci_spec::CommandCompleteEventParams& params =
        event->params<hci_spec::CommandCompleteEventParams>();
    matching_opcode = le16toh(params.command_opcode);
    allowed_command_packets_ = params.num_hci_command_packets;
  } else {  //  hci_spec::kCommandStatusEventCode
    const hci_spec::CommandStatusEventParams& params =
        event->params<hci_spec::CommandStatusEventParams>();
    matching_opcode = le16toh(params.command_opcode);
    allowed_command_packets_ = params.num_hci_command_packets;
    unregister_async_handler = params.status != hci_spec::StatusCode::kSuccess;
  }
  bt_log(TRACE, "hci", "allowed packets update: %zu", allowed_command_packets_);

  if (matching_opcode == hci_spec::kNoOp) {
    return;
  }

  auto it = pending_transactions_.find(matching_opcode);
  if (it == pending_transactions_.end()) {
    bt_log(ERROR, "hci", "update for unexpected opcode: %#.4x", matching_opcode);
    return;
  }

  std::unique_ptr<TransactionData>& pending = it->second;
  ZX_DEBUG_ASSERT(pending->opcode() == matching_opcode);

  pending->Complete(std::move(event));

  // If the command is synchronous or there's no handler to cleanup, we're done.
  if (pending->handler_id() == 0u) {
    pending_transactions_.erase(it);
    return;
  }

  // TODO(fxbug.dev/1109): Do not allow asynchronous commands to finish with Command Complete.
  if (event_code == hci_spec::kCommandCompleteEventCode) {
    bt_log(WARN, "hci", "async command received CommandComplete");
    unregister_async_handler = true;
  }

  // If an asynchronous command failed, then remove its event handler.
  if (unregister_async_handler) {
    bt_log(TRACE, "hci", "async command failed; removing its handler");
    RemoveEventHandlerInternal(pending->handler_id());
    pending_transactions_.erase(it);
  }
}

void CommandChannel::NotifyEventHandler(std::unique_ptr<EventPacket> event) {
  struct PendingCallback {
    EventCallback callback;
    EventHandlerId id;
  };
  std::vector<PendingCallback> pending_callbacks;

  hci_spec::EventCode event_code;
  const std::unordered_multimap<hci_spec::EventCode, EventHandlerId>* event_handlers;

  EventType event_type;
  if (event->event_code() == hci_spec::kLEMetaEventCode) {
    event_type = EventType::kLEMetaEvent;
    event_code = event->params<hci_spec::LEMetaEventParams>().subevent_code;
    event_handlers = &le_meta_subevent_code_handlers_;
  } else {
    event_type = EventType::kHciEvent;
    event_code = event->event_code();
    event_handlers = &event_code_handlers_;
  }

  auto range = event_handlers->equal_range(event_code);
  if (range.first == range.second) {
    bt_log(DEBUG, "hci", "%s event %#.2x received with no handler",
           EventTypeToString(event_type).c_str(), event_code);
    return;
  }

  auto iter = range.first;
  while (iter != range.second) {
    EventHandlerId event_id = iter->second;
    bt_log(TRACE, "hci", "notifying handler (id %zu) for event code %#.2x", event_id, event_code);
    auto handler_iter = event_handler_id_map_.find(event_id);
    ZX_DEBUG_ASSERT(handler_iter != event_handler_id_map_.end());

    EventHandlerData& handler = handler_iter->second;
    ZX_DEBUG_ASSERT(handler.event_code == event_code);

    EventCallback callback = handler.event_callback.share();

    ++iter;  // Advance so we don't point to an invalid iterator.
    if (handler.is_async()) {
      bt_log(TRACE, "hci", "removing completed async handler (id %zu, event code: %#.2x)", event_id,
             event_code);
      pending_transactions_.erase(handler.pending_opcode);
      RemoveEventHandlerInternal(event_id);  // |handler| is now dangling.
    }

    pending_callbacks.push_back({std::move(callback), event_id});
  }

  // Process queue so callbacks can't add a handler if another queued command finishes on the same
  // event.
  TrySendQueuedCommands();

  for (auto it = pending_callbacks.begin(); it != pending_callbacks.end(); ++it) {
    std::unique_ptr<EventPacket> ev = nullptr;

    // Don't copy event for last callback.
    if (it == pending_callbacks.end() - 1) {
      ev = std::move(event);
    } else {
      ev = EventPacket::New(event->view().payload_size());
      MutableBufferView buf = ev->mutable_view()->mutable_data();
      event->view().data().Copy(&buf);
    }

    // execute the event callback
    EventCallbackResult result = it->callback(*ev);
    if (result == EventCallbackResult::kRemove) {
      RemoveEventHandler(it->id);
    }
  }
}

void CommandChannel::OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                    zx_status_t status, const zx_packet_signal_t* signal) {
  ZX_DEBUG_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  TRACE_DURATION("bluetooth", "CommandChannel::OnChannelReady", "signal->count", signal->count);

  if (status != ZX_OK) {
    bt_log(DEBUG, "hci", "channel error: %s", zx_status_get_string(status));
    return;
  }

  // Allocate a buffer for the event. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  //
  // TODO(armansito): We could first try to read into a small buffer and retry if the syscall
  // returns ZX_ERR_BUFFER_TOO_SMALL. Not sure if the second syscall would be worth it but
  // investigate.
  for (size_t count = 0; count < signal->count; count++) {
    std::unique_ptr<EventPacket> packet =
        EventPacket::New(slab_allocators::kLargeControlPayloadSize);
    if (!packet) {
      bt_log(ERROR, "hci", "failed to allocate event packet!");
      return;
    }

    zx_status_t status = ReadEventPacketFromChannel(channel_, packet);
    if (status == ZX_ERR_INVALID_ARGS) {
      continue;
    }

    if (status != ZX_OK) {
      return;
    }

    if (packet->event_code() == hci_spec::kCommandStatusEventCode ||
        packet->event_code() == hci_spec::kCommandCompleteEventCode) {
      UpdateTransaction(std::move(packet));
      TrySendQueuedCommands();
    } else {
      NotifyEventHandler(std::move(packet));
    }
  }

  status = wait->Begin(dispatcher);
  if (status != ZX_OK) {
    bt_log(DEBUG, "hci", "wait error: %s", zx_status_get_string(status));
  }
}

zx_status_t CommandChannel::ReadEventPacketFromChannel(const zx::channel& channel,
                                                       const EventPacketPtr& packet) {
  uint32_t read_size;
  MutableBufferView packet_bytes = packet->mutable_view()->mutable_data();
  zx_status_t read_status =
      channel.read(/*flags=*/0u, packet_bytes.mutable_data(), /*handles=*/nullptr,
                   packet_bytes.size(), /*num_handles=*/0, &read_size, /*actual_handles=*/nullptr);
  if (read_status != ZX_OK) {
    bt_log(DEBUG, "hci", "Failed to read event bytes: %s", zx_status_get_string(read_status));
    // Clear the handler so that we stop receiving events from it.
    // TODO(jamuraa): signal upper layers that we can't read the channel.
    return ZX_ERR_IO;
  }

  if (read_size < sizeof(hci_spec::EventHeader)) {
    bt_log(ERROR, "hci", "malformed data packet - expected at least %zu bytes, got %u",
           sizeof(hci_spec::EventHeader), read_size);
    // TODO(armansito): Should this be fatal? Ignore for now.
    return ZX_ERR_INVALID_ARGS;
  }

  // Compare the received payload size to what is in the header.
  const size_t rx_payload_size = read_size - sizeof(hci_spec::EventHeader);
  const size_t size_from_header = packet->view().header().parameter_total_size;
  if (size_from_header != rx_payload_size) {
    bt_log(ERROR, "hci",
           "malformed packet - payload size from header (%zu) does not match"
           " received payload size: %zu",
           size_from_header, rx_payload_size);
    return ZX_ERR_INVALID_ARGS;
  }

  packet->InitializeFromBuffer();
  return ZX_OK;
}

}  // namespace bt::hci
