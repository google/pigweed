// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_channel.h"

#include <endian.h>
#include <lib/async/default.h>
#include <lib/fit/defer.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

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
    case CommandChannel::EventType::kVendorEvent:
      return "vendor_event";
  }
}

CommandChannel::QueuedCommand::QueuedCommand(std::unique_ptr<CommandPacket> command_packet,
                                             std::unique_ptr<TransactionData> transaction_data)
    : packet(std::move(command_packet)), data(std::move(transaction_data)) {
  ZX_DEBUG_ASSERT(data);
  ZX_DEBUG_ASSERT(packet);
}

CommandChannel::TransactionData::TransactionData(
    TransactionId transaction_id, hci_spec::OpCode opcode, hci_spec::EventCode complete_event_code,
    std::optional<hci_spec::EventCode> le_meta_subevent_code,
    std::unordered_set<hci_spec::OpCode> exclusions, CommandCallback callback)
    : transaction_id_(transaction_id),
      opcode_(opcode),
      complete_event_code_(complete_event_code),
      le_meta_subevent_code_(le_meta_subevent_code),
      exclusions_(std::move(exclusions)),
      callback_(std::move(callback)),
      handler_id_(0u) {
  ZX_DEBUG_ASSERT(transaction_id != 0u);
  exclusions_.insert(opcode_);
}

CommandChannel::TransactionData::~TransactionData() {
  if (callback_) {
    bt_log(DEBUG, "hci", "destroying unfinished transaction: %zu", transaction_id_);
  }
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

  // Call callback_ synchronously to ensure that asynchronous status & complete events are not
  // handled out of order if they are dispatched from the HCI API simultaneously.
  callback_(transaction_id_, *event);

  // Asynchronous commands will have an additional reference to callback_ in the event
  // map. Clear this reference to ensure that destruction or unexpected command complete events or
  // status events do not call this reference to callback_ twice.
  callback_ = nullptr;
}

void CommandChannel::TransactionData::Cancel() {
  timeout_task_.Cancel();
  callback_ = nullptr;
}

CommandChannel::EventCallback CommandChannel::TransactionData::MakeCallback() {
  return [transaction_id = transaction_id_, cb = callback_.share()](const EventPacket& event) {
    cb(transaction_id, event);
    return EventCallbackResult::kContinue;
  };
}

CommandChannel::CommandChannel(HciWrapper* hci)
    : next_transaction_id_(1u),
      next_event_handler_id_(1u),
      hci_(hci),
      allowed_command_packets_(1u),
      weak_ptr_factory_(this) {
  hci_->SetEventCallback(fit::bind_member<&CommandChannel::OnEvent>(this));

  bt_log(INFO, "hci", "CommandChannel initialized");
}

CommandChannel::~CommandChannel() { bt_log(INFO, "hci", "CommandChannel destroyed"); }

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

  if (next_transaction_id_.value() == 0u) {
    next_transaction_id_.Set(1);
  }

  TransactionId transaction_id = next_transaction_id_.value();
  next_transaction_id_.Set(transaction_id + 1);
  std::unique_ptr<CommandChannel::TransactionData> data = std::make_unique<TransactionData>(
      transaction_id, command_packet->opcode(), complete_event_code, le_meta_subevent_code,
      std::move(exclusions), std::move(callback));

  QueuedCommand command(std::move(command_packet), std::move(data));

  if (IsAsync(complete_event_code)) {
    MaybeAddTransactionHandler(command.data.get());
  }

  send_queue_.push_back(std::move(command));
  TrySendQueuedCommands();

  return transaction_id;
}

bool CommandChannel::RemoveQueuedCommand(TransactionId transaction_id) {
  auto it = std::find_if(
      send_queue_.begin(), send_queue_.end(),
      [transaction_id](const QueuedCommand& cmd) { return cmd.data->id() == transaction_id; });
  if (it == send_queue_.end()) {
    // The transaction to remove has already finished or never existed.
    bt_log(TRACE, "hci", "command to remove not found, id: %zu", transaction_id);
    return false;
  }

  bt_log(TRACE, "hci", "removing queued command id: %zu", transaction_id);
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
           handler->handler_id, event_code);
    return 0u;
  }

  EventHandlerId handler_id =
      NewEventHandler(event_code, EventType::kHciEvent, hci_spec::kNoOp, std::move(event_callback));
  event_code_handlers_.emplace(event_code, handler_id);
  return handler_id;
}

CommandChannel::EventHandlerId CommandChannel::AddLEMetaEventHandler(
    hci_spec::EventCode le_meta_subevent_code, EventCallback event_callback) {
  EventHandlerData* handler = FindLEMetaEventHandler(le_meta_subevent_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci",
           "async event handler %zu already registered for LE Meta Event subevent code %#.2x",
           handler->handler_id, le_meta_subevent_code);
    return 0u;
  }

  EventHandlerId handler_id = NewEventHandler(le_meta_subevent_code, EventType::kLEMetaEvent,
                                              hci_spec::kNoOp, std::move(event_callback));
  le_meta_subevent_code_handlers_.emplace(le_meta_subevent_code, handler_id);
  return handler_id;
}

CommandChannel::EventHandlerId CommandChannel::AddVendorEventHandler(
    hci_spec::EventCode vendor_subevent_code, EventCallback event_callback) {
  CommandChannel::EventHandlerData* handler = FindVendorEventHandler(vendor_subevent_code);
  if (handler && handler->is_async()) {
    bt_log(ERROR, "hci",
           "async event handler %zu already registered for Vendor Event subevent code %#.2x",
           handler->handler_id, vendor_subevent_code);
    return 0u;
  }

  EventHandlerId handler_id = NewEventHandler(vendor_subevent_code, EventType::kVendorEvent,
                                              hci_spec::kNoOp, std::move(event_callback));
  vendor_subevent_code_handlers_.emplace(vendor_subevent_code, handler_id);
  return handler_id;
}

void CommandChannel::RemoveEventHandler(EventHandlerId handler_id) {
  // If the ID doesn't exist or it is internal. it can't be removed.
  auto iter = event_handler_id_map_.find(handler_id);
  if (iter == event_handler_id_map_.end() || iter->second.is_async()) {
    return;
  }

  RemoveEventHandlerInternal(handler_id);
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

CommandChannel::EventHandlerData* CommandChannel::FindVendorEventHandler(
    hci_spec::EventCode vendor_subevent_code) {
  auto it = vendor_subevent_code_handlers_.find(vendor_subevent_code);
  if (it == vendor_subevent_code_handlers_.end()) {
    return nullptr;
  }

  return &event_handler_id_map_[it->second];
}

void CommandChannel::RemoveEventHandlerInternal(EventHandlerId handler_id) {
  auto iter = event_handler_id_map_.find(handler_id);
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
    case EventType::kVendorEvent:
      handlers = &vendor_subevent_code_handlers_;
      break;
  }

  bt_log(TRACE, "hci", "removing handler for %s event code %#.2x",
         EventTypeToString(iter->second.event_type).c_str(), iter->second.event_code);

  auto range = handlers->equal_range(iter->second.event_code);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == handler_id) {
      it = handlers->erase(it);
      break;
    }
  }

  event_handler_id_map_.erase(iter);
}

void CommandChannel::TrySendQueuedCommands() {
  if (allowed_command_packets_.value() == 0) {
    bt_log(TRACE, "hci", "controller queue full, waiting");
    return;
  }

  // Walk the waiting and see if any are sendable.
  for (auto it = send_queue_.begin();
       allowed_command_packets_.value() > 0 && it != send_queue_.end();) {
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
  zx_status_t status = hci_->SendCommand(std::move(cmd.packet));

  if (status < 0) {
    // TODO(armansito): We should notify the |status_callback| of the pending
    // command with a special error code in this case.
    bt_log(ERROR, "hci", "failed to send command: %s", zx_status_get_string(status));
    return;
  }
  allowed_command_packets_.Set(allowed_command_packets_.value() - 1);

  std::unique_ptr<TransactionData>& transaction = cmd.data;

  transaction->Start(
      [this, transaction_id = cmd.data->id()] {
        bt_log(ERROR, "hci", "command %zu timed out, notifying error", transaction_id);
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

  EventHandlerId handler_id =
      NewEventHandler(code, event_type, data->opcode(), data->MakeCallback());

  ZX_ASSERT(handler_id != 0u);
  data->set_handler_id(handler_id);
  handlers->emplace(code, handler_id);
  bt_log(TRACE, "hci", "async command %zu assigned handler %zu", data->id(), handler_id);
}

CommandChannel::EventHandlerId CommandChannel::NewEventHandler(hci_spec::EventCode event_code,
                                                               EventType event_type,
                                                               hci_spec::OpCode pending_opcode,
                                                               EventCallback event_callback) {
  ZX_DEBUG_ASSERT(event_code);
  ZX_DEBUG_ASSERT(event_callback);

  auto handler_id = next_event_handler_id_.value();
  next_event_handler_id_.Set(handler_id + 1);
  EventHandlerData data;
  data.handler_id = handler_id;
  data.event_code = event_code;
  data.event_type = event_type;
  data.pending_opcode = pending_opcode;
  data.event_callback = std::move(event_callback);

  bt_log(TRACE, "hci", "adding event handler %zu for %s event code %#.2x", handler_id,
         EventTypeToString(event_type).c_str(), event_code);
  ZX_DEBUG_ASSERT(event_handler_id_map_.find(handler_id) == event_handler_id_map_.end());
  event_handler_id_map_[handler_id] = std::move(data);

  return handler_id;
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
    allowed_command_packets_.Set(params.num_hci_command_packets);
  } else {  //  hci_spec::kCommandStatusEventCode
    const hci_spec::CommandStatusEventParams& params =
        event->params<hci_spec::CommandStatusEventParams>();
    matching_opcode = le16toh(params.command_opcode);
    allowed_command_packets_.Set(params.num_hci_command_packets);
    unregister_async_handler = params.status != hci_spec::StatusCode::kSuccess;
  }
  bt_log(TRACE, "hci", "allowed packets update: %zu", allowed_command_packets_.value());

  if (matching_opcode == hci_spec::kNoOp) {
    return;
  }

  auto it = pending_transactions_.find(matching_opcode);
  if (it == pending_transactions_.end()) {
    bt_log(ERROR, "hci", "update for unexpected opcode: %#.4x", matching_opcode);
    return;
  }

  std::unique_ptr<TransactionData>& transaction_ref = it->second;
  ZX_DEBUG_ASSERT(transaction_ref->opcode() == matching_opcode);

  // If the command is synchronous or there's no handler to cleanup, we're done.
  if (transaction_ref->handler_id() == 0u) {
    std::unique_ptr<TransactionData> transaction = std::move(it->second);
    pending_transactions_.erase(it);
    transaction->Complete(std::move(event));
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
    RemoveEventHandlerInternal(transaction_ref->handler_id());
    std::unique_ptr<TransactionData> transaction = std::move(it->second);
    pending_transactions_.erase(it);
    transaction->Complete(std::move(event));
  } else {
    // Send the status event to the async transaction.
    transaction_ref->Complete(std::move(event));
  }
}

void CommandChannel::NotifyEventHandler(std::unique_ptr<EventPacket> event) {
  struct PendingCallback {
    EventCallback callback;
    EventHandlerId handler_id;
  };
  std::vector<PendingCallback> pending_callbacks;

  hci_spec::EventCode event_code;
  const std::unordered_multimap<hci_spec::EventCode, EventHandlerId>* event_handlers;

  EventType event_type;
  switch (event->event_code()) {
    case hci_spec::kLEMetaEventCode:
      event_type = EventType::kLEMetaEvent;
      event_code = event->params<hci_spec::LEMetaEventParams>().subevent_code;
      event_handlers = &le_meta_subevent_code_handlers_;
      break;
    case hci_spec::kVendorDebugEventCode:
      event_type = EventType::kVendorEvent;
      event_code = event->params<hci_spec::VendorEventParams>().subevent_code;
      event_handlers = &vendor_subevent_code_handlers_;
      break;
    default:
      event_type = EventType::kHciEvent;
      event_code = event->event_code();
      event_handlers = &event_code_handlers_;
      break;
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
      RemoveEventHandler(it->handler_id);
    }
  }
}

void CommandChannel::OnEvent(std::unique_ptr<EventPacket> event) {
  if (event->event_code() == hci_spec::kCommandStatusEventCode ||
      event->event_code() == hci_spec::kCommandCompleteEventCode) {
    UpdateTransaction(std::move(event));
    TrySendQueuedCommands();
  } else {
    NotifyEventHandler(std::move(event));
  }
}

void CommandChannel::AttachInspect(inspect::Node& parent, const std::string& name) {
  command_channel_node_ = parent.CreateChild(name);
  next_transaction_id_.AttachInspect(command_channel_node_, "next_transaction_id");
  next_event_handler_id_.AttachInspect(command_channel_node_, "next_event_handler_id");
  allowed_command_packets_.AttachInspect(command_channel_node_, "allowed_command_packets");
}

}  // namespace bt::hci
