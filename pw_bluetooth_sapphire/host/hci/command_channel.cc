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
#include "src/connectivity/bluetooth/core/bt-host/common/run_or_post.h"
#include "src/connectivity/bluetooth/core/bt-host/common/run_task_sync.h"
#include "src/lib/fxl/strings/string_printf.h"
#include "transport.h"

namespace bt {
namespace hci {

namespace {

bool IsAsync(EventCode code) {
  return code != kCommandCompleteEventCode && code != kCommandStatusEventCode;
}

}  //  namespace

CommandChannel::QueuedCommand::QueuedCommand(std::unique_ptr<CommandPacket> command_packet,
                                             std::unique_ptr<TransactionData> transaction_data)
    : packet(std::move(command_packet)), data(std::move(transaction_data)) {
  ZX_DEBUG_ASSERT(data);
  ZX_DEBUG_ASSERT(packet);
}

CommandChannel::TransactionData::TransactionData(TransactionId id, OpCode opcode,
                                                 EventCode complete_event_code,
                                                 std::optional<EventCode> subevent_code,
                                                 std::unordered_set<OpCode> exclusions,
                                                 CommandCallback callback)
    : id_(id),
      opcode_(opcode),
      complete_event_code_(complete_event_code),
      subevent_code_(subevent_code),
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
  auto event = EventPacket::New(sizeof(CommandStatusEventParams));
  auto* header = event->mutable_view()->mutable_header();
  auto* params = event->mutable_view()->mutable_payload<CommandStatusEventParams>();

  // TODO(armansito): Instead of lying about receiving a Command Status event,
  // report this error in a different way. This can be highly misleading during
  // debugging.
  header->event_code = kCommandStatusEventCode;
  header->parameter_total_size = sizeof(CommandStatusEventParams);
  params->status = kUnspecifiedError;
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
    return CommandChannel::EventCallbackResult::kContinue;
  };
}

fit::result<std::unique_ptr<CommandChannel>> CommandChannel::Create(
    Transport* transport, zx::channel hci_command_channel) {
  auto channel = std::unique_ptr<CommandChannel>(
      new CommandChannel(transport, std::move(hci_command_channel)));

  if (!channel->is_initialized_) {
    return fit::error();
  }
  return fit::ok(std::move(channel));
}

CommandChannel::CommandChannel(Transport* transport, zx::channel hci_command_channel)
    : next_transaction_id_(1u),
      next_event_handler_id_(1u),
      transport_(transport),
      channel_(std::move(hci_command_channel)),
      channel_wait_(this, channel_.get(), ZX_CHANNEL_READABLE),
      is_initialized_(false),
      allowed_command_packets_(1u) {
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

CommandChannel::~CommandChannel() {
  // Do nothing. Since Transport is shared across threads, this can be called
  // from any thread and calling ShutDown() would be unsafe.
}

void CommandChannel::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  if (!is_initialized_)
    return;

  bt_log(INFO, "hci", "shutting down");

  ShutDownInternal();
}

void CommandChannel::ShutDownInternal() {
  bt_log(DEBUG, "hci", "removing I/O handler");

  // Prevent new command packets from being queued.
  is_initialized_ = false;

  // Stop listening for HCI events.
  zx_status_t status = channel_wait_.Cancel();
  if (status != ZX_OK) {
    bt_log(WARN, "hci", "could not cancel wait on channel: %s", zx_status_get_string(status));
  }

  // Drop all queued commands and event handlers. Pending HCI commands will be
  // resolved with an "UnspecifiedError" error code upon destruction.
  send_queue_ = std::list<QueuedCommand>();
  event_handler_id_map_.clear();
  event_code_handlers_.clear();
  subevent_code_handlers_.clear();
  pending_transactions_.clear();
}

CommandChannel::TransactionId CommandChannel::SendCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const EventCode complete_event_code) {
  return SendExclusiveCommand(std::move(command_packet), std::move(callback), complete_event_code);
}

CommandChannel::TransactionId CommandChannel::SendLeAsyncCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    EventCode le_meta_subevent_code) {
  return SendLeAsyncExclusiveCommand(std::move(command_packet), std::move(callback),
                                     le_meta_subevent_code);
}

CommandChannel::TransactionId CommandChannel::SendExclusiveCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const EventCode complete_event_code, std::unordered_set<OpCode> exclusions) {
  return SendExclusiveCommandInternal(std::move(command_packet), std::move(callback),
                                      complete_event_code, std::nullopt, std::move(exclusions));
}

CommandChannel::TransactionId CommandChannel::SendLeAsyncExclusiveCommand(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    std::optional<EventCode> le_meta_subevent_code, std::unordered_set<OpCode> exclusions) {
  return SendExclusiveCommandInternal(std::move(command_packet), std::move(callback),
                                      hci::kLEMetaEventCode, le_meta_subevent_code,
                                      std::move(exclusions));
}

CommandChannel::TransactionId CommandChannel::SendExclusiveCommandInternal(
    std::unique_ptr<CommandPacket> command_packet, CommandCallback callback,
    const EventCode complete_event_code, std::optional<EventCode> subevent_code,
    std::unordered_set<OpCode> exclusions) {
  if (!is_initialized_) {
    bt_log(DEBUG, "hci", "can't send commands while uninitialized");
    return 0u;
  }

  ZX_ASSERT(command_packet);
  ZX_ASSERT_MSG((complete_event_code == hci::kLEMetaEventCode) == subevent_code.has_value(),
                "only LE Meta Event subevents are supported");

  if (IsAsync(complete_event_code)) {
    // Cannot send an asynchronous command if there's an external event handler
    // registered for the completion event.
    EventHandlerData* handler = nullptr;
    if (subevent_code.has_value()) {
      handler = FindLEMetaEventHandler(*subevent_code);
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
  auto data =
      std::make_unique<TransactionData>(id, command_packet->opcode(), complete_event_code,
                                        subevent_code, std::move(exclusions), std::move(callback));

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
  ZX_ASSERT(id != 0u);

  auto it = std::find_if(send_queue_.begin(), send_queue_.end(),
                         [id](const auto& cmd) { return cmd.data->id() == id; });
  if (it != send_queue_.end()) {
    bt_log(TRACE, "hci", "removing queued command id: %zu", id);
    TransactionData& data = *it->data;
    data.Cancel();
    if (data.handler_id() != 0u) {
      RemoveEventHandlerInternal(data.handler_id());
    }
    send_queue_.erase(it);
    return true;
  }

  // The transaction to remove has already finished or never existed.
  bt_log(TRACE, "hci", "command to remove not found, id: %zu", id);
  return false;
}

CommandChannel::EventHandlerId CommandChannel::AddEventHandler(EventCode event_code,
                                                               EventCallback event_callback) {
  if (event_code == kCommandStatusEventCode || event_code == kCommandCompleteEventCode ||
      event_code == kLEMetaEventCode) {
    return 0u;
  }

  auto* handler = FindEventHandler(event_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci", "async event handler %zu already registered for event code %#.2x",
           handler->id, event_code);
    return 0u;
  }

  auto id = NewEventHandler(event_code, false /* is_le_meta */, kNoOp, std::move(event_callback));
  event_code_handlers_.emplace(event_code, id);
  return id;
}

CommandChannel::EventHandlerId CommandChannel::AddLEMetaEventHandler(EventCode subevent_code,
                                                                     EventCallback event_callback) {
  auto* handler = FindLEMetaEventHandler(subevent_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci",
           "async event handler %zu already registered for LE Meta Event subevent code %#.2x",
           handler->id, subevent_code);
    return 0u;
  }

  auto id = NewEventHandler(subevent_code, true /* is_le_meta */, kNoOp, std::move(event_callback));
  subevent_code_handlers_.emplace(subevent_code, id);
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

CommandChannel::EventHandlerData* CommandChannel::FindEventHandler(EventCode code) {
  auto it = event_code_handlers_.find(code);
  if (it == event_code_handlers_.end()) {
    return nullptr;
  }
  return &event_handler_id_map_[it->second];
}

CommandChannel::EventHandlerData* CommandChannel::FindLEMetaEventHandler(EventCode subevent_code) {
  auto it = subevent_code_handlers_.find(subevent_code);
  if (it == subevent_code_handlers_.end()) {
    return nullptr;
  }
  return &event_handler_id_map_[it->second];
}

void CommandChannel::RemoveEventHandlerInternal(EventHandlerId id) {
  auto iter = event_handler_id_map_.find(id);
  if (iter == event_handler_id_map_.end()) {
    return;
  }

  if (iter->second.event_code != 0) {
    auto* event_handlers =
        iter->second.is_le_meta_subevent ? &subevent_code_handlers_ : &event_code_handlers_;

    bt_log(TRACE, "hci", "removing handler for %sevent code %#.2x",
           (iter->second.is_le_meta_subevent ? "LE " : ""), iter->second.event_code);

    auto range = event_handlers->equal_range(iter->second.event_code);
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second == id) {
        event_handlers->erase(it);
        break;
      }
    }
  }
  event_handler_id_map_.erase(iter);
}

void CommandChannel::TrySendQueuedCommands() {
  if (!is_initialized_)
    return;

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
    for (const auto& excluded_opcode : data.exclusions()) {
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
        data.subevent_code() && subevent_code_handlers_.count(*data.subevent_code());
    bool waiting_for_other_transaction =
        transaction_waiting_on_event || transaction_waiting_on_subevent;

    // We can send this if we only expect one update, or if we aren't
    // waiting for another transaction to complete on the same event.
    // It is unlikely but possible to have commands with different opcodes
    // wait on the same completion event.
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
  auto packet_bytes = cmd.packet->view().data();
  zx_status_t status = channel_.write(0, packet_bytes.data(), packet_bytes.size(), nullptr, 0);
  if (status < 0) {
    // TODO(armansito): We should notify the |status_callback| of the pending
    // command with a special error code in this case.
    bt_log(ERROR, "hci", "failed to send command: %s", zx_status_get_string(status));
    return;
  }
  allowed_command_packets_--;

  auto& transaction = cmd.data;

  transaction->Start(
      [this, id = cmd.data->id()] {
        bt_log(ERROR, "hci", "command %zu timed out, notifying error", id);
        if (channel_timeout_cb_) {
          channel_timeout_cb_();
        }
      },
      kCommandTimeout);

  MaybeAddTransactionHandler(transaction.get());

  pending_transactions_.insert(std::make_pair(transaction->opcode(), std::move(transaction)));
}

void CommandChannel::MaybeAddTransactionHandler(TransactionData* data) {
  // We don't need to add a transaction handler for synchronous transactions.
  if (!IsAsync(data->complete_event_code())) {
    return;
  }

  const bool is_le_meta = data->subevent_code().has_value();
  auto* const code_handlers = is_le_meta ? &subevent_code_handlers_ : &event_code_handlers_;
  const EventCode code = data->subevent_code().value_or(data->complete_event_code());

  // We already have a handler for this transaction, or another transaction
  // is already waiting and it will be queued.
  if (code_handlers->count(code)) {
    bt_log(TRACE, "hci", "async command %zu: already has handler", data->id());
    return;
  }

  // The handler hasn't been added yet.
  EventHandlerId id = NewEventHandler(code, is_le_meta, data->opcode(), data->MakeCallback());

  ZX_ASSERT(id != 0u);
  data->set_handler_id(id);
  code_handlers->emplace(code, id);
  bt_log(TRACE, "hci", "async command %zu assigned handler %zu", data->id(), id);
}

CommandChannel::EventHandlerId CommandChannel::NewEventHandler(EventCode event_code,
                                                               bool is_le_meta,
                                                               OpCode pending_opcode,
                                                               EventCallback event_callback) {
  ZX_DEBUG_ASSERT(event_code);
  ZX_DEBUG_ASSERT(event_callback);

  auto id = next_event_handler_id_++;
  EventHandlerData data;
  data.id = id;
  data.event_code = event_code;
  data.pending_opcode = pending_opcode;
  data.event_callback = std::move(event_callback);
  data.is_le_meta_subevent = is_le_meta;

  bt_log(TRACE, "hci", "adding event handler %zu for %sevent code %#.2x", id,
         (is_le_meta ? "LE sub" : ""), event_code);
  ZX_DEBUG_ASSERT(event_handler_id_map_.find(id) == event_handler_id_map_.end());
  event_handler_id_map_[id] = std::move(data);

  return id;
}

void CommandChannel::UpdateTransaction(std::unique_ptr<EventPacket> event) {
  hci::EventCode event_code = event->event_code();

  ZX_DEBUG_ASSERT(event_code == kCommandStatusEventCode || event_code == kCommandCompleteEventCode);

  OpCode matching_opcode;

  // The HCI Command Status event with an error status might indicate that an
  // async command failed. We use this to unregister async command handlers
  // below.
  bool unregister_async_handler = false;

  if (event->event_code() == kCommandCompleteEventCode) {
    const auto& params = event->params<CommandCompleteEventParams>();
    matching_opcode = le16toh(params.command_opcode);
    allowed_command_packets_ = params.num_hci_command_packets;
  } else {  // kCommandStatusEventCode
    const auto& params = event->params<CommandStatusEventParams>();
    matching_opcode = le16toh(params.command_opcode);
    allowed_command_packets_ = params.num_hci_command_packets;
    unregister_async_handler = params.status != StatusCode::kSuccess;
  }
  bt_log(TRACE, "hci", "allowed packets update: %zu", allowed_command_packets_);

  if (matching_opcode == kNoOp) {
    return;
  }

  auto it = pending_transactions_.find(matching_opcode);
  if (it == pending_transactions_.end()) {
    bt_log(ERROR, "hci", "update for unexpected opcode: %#.4x", matching_opcode);
    return;
  }

  auto& pending = it->second;
  ZX_DEBUG_ASSERT(pending->opcode() == matching_opcode);

  pending->Complete(std::move(event));

  // If the command is synchronous or there's no handler to cleanup, we're done.
  if (pending->handler_id() == 0u) {
    pending_transactions_.erase(it);
    return;
  }

  // TODO(fxbug.dev/1109): Do not allow asynchronous commands to finish with Command
  // Complete.
  if (event_code == kCommandCompleteEventCode) {
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

  EventCode event_code;
  const std::unordered_multimap<EventCode, EventHandlerId>* event_handlers;

  bool is_le_event = false;
  if (event->event_code() == kLEMetaEventCode) {
    is_le_event = true;
    event_code = event->params<LEMetaEventParams>().subevent_code;
    event_handlers = &subevent_code_handlers_;
  } else {
    event_code = event->event_code();
    event_handlers = &event_code_handlers_;
  }

  auto range = event_handlers->equal_range(event_code);
  if (range.first == range.second) {
    bt_log(DEBUG, "hci", "%sevent %#.2x received with no handler", (is_le_event ? "LE " : ""),
           event_code);
    return;
  }

  auto iter = range.first;
  while (iter != range.second) {
    EventHandlerId event_id = iter->second;
    bt_log(TRACE, "hci", "notifying handler (id %zu) for event code %#.2x", event_id, event_code);
    auto handler_iter = event_handler_id_map_.find(event_id);
    ZX_DEBUG_ASSERT(handler_iter != event_handler_id_map_.end());

    auto& handler = handler_iter->second;
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

  // Process queue so callbacks can't add a handler if another queued command
  // finishes on the same event.
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

    ExecuteEventCallback(std::move(it->callback), it->id, std::move(ev));
  }
}

void CommandChannel::ExecuteEventCallback(EventCallback cb, EventHandlerId id,
                                          std::unique_ptr<EventPacket> event) {
  auto result = cb(*event);
  if (result == EventCallbackResult::kRemove) {
    RemoveEventHandler(id);
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

  // Allocate a buffer for the event. Since we don't know the size beforehand we
  // allocate the largest possible buffer.
  // TODO(armansito): We could first try to read into a small buffer and retry
  // if the syscall returns ZX_ERR_BUFFER_TOO_SMALL. Not sure if the second
  // syscall would be worth it but investigate.

  for (size_t count = 0; count < signal->count; count++) {
    auto packet = EventPacket::New(slab_allocators::kLargeControlPayloadSize);
    if (!packet) {
      bt_log(ERROR, "hci", "failed to allocate event packet!");
      return;
    }
    zx_status_t status = ReadEventPacketFromChannel(channel_, packet);
    if (status == ZX_ERR_INVALID_ARGS) {
      continue;
    } else if (status != ZX_OK) {
      return;
    }
    if (packet->event_code() == kCommandStatusEventCode ||
        packet->event_code() == kCommandCompleteEventCode) {
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
  auto packet_bytes = packet->mutable_view()->mutable_data();
  zx_status_t read_status = channel.read(0u, packet_bytes.mutable_data(), nullptr,
                                         packet_bytes.size(), 0, &read_size, nullptr);
  if (read_status < 0) {
    bt_log(DEBUG, "hci", "Failed to read event bytes: %s", zx_status_get_string(read_status));
    // Clear the handler so that we stop receiving events from it.
    // TODO(jamuraa): signal upper layers that we can't read the channel.
    return ZX_ERR_IO;
  }

  if (read_size < sizeof(EventHeader)) {
    bt_log(ERROR, "hci", "malformed data packet - expected at least %zu bytes, got %u",
           sizeof(EventHeader), read_size);
    // TODO(armansito): Should this be fatal? Ignore for now.
    return ZX_ERR_INVALID_ARGS;
  }

  // Compare the received payload size to what is in the header.
  const size_t rx_payload_size = read_size - sizeof(EventHeader);
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

}  // namespace hci
}  // namespace bt
