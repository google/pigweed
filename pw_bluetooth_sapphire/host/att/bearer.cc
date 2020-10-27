// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bearer.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt::att {

// static
constexpr Bearer::HandlerId Bearer::kInvalidHandlerId;
constexpr Bearer::TransactionId Bearer::kInvalidTransactionId;

namespace {

// Returns the security level that is required to resolve the given ATT error
// code and the current security properties of the link, according to the table
// in v5.0, Vol 3, Part C, 10.3.2 (table 10.2). A security upgrade is not
// required if the returned value equals sm::SecurityLevel::kNoSecurity.
// TODO(armansito): Supporting requesting Secure Connections in addition to the
// encrypted/MITM dimensions.
sm::SecurityLevel CheckSecurity(ErrorCode ecode, const sm::SecurityProperties& security) {
  bool encrypted = (security.level() != sm::SecurityLevel::kNoSecurity);

  switch (ecode) {
    // "Insufficient Encryption" error code is specified for cases when the peer
    // is paired (i.e. a LTK or STK exists for it) but the link is not
    // encrypted. We treat this as equivalent to "Insufficient Authentication"
    // sent on an unencrypted link.
    case ErrorCode::kInsufficientEncryption:
      encrypted = false;
      [[fallthrough]];
    // We achieve authorization by pairing which requires a confirmation from
    // the host's pairing delegate.
    // TODO(armansito): Allow for this to be satisfied with a simple user
    // confirmation if we're not paired?
    case ErrorCode::kInsufficientAuthorization:
    case ErrorCode::kInsufficientAuthentication:
      // If the link is already authenticated we cannot request a further
      // upgrade.
      // TODO(armansito): Take into account "secure connections" once it's
      // supported.
      if (security.authenticated()) {
        return sm::SecurityLevel::kNoSecurity;
      }
      return encrypted ? sm::SecurityLevel::kAuthenticated : sm::SecurityLevel::kEncrypted;

    // Our SMP implementation always claims to support the maximum encryption
    // key size. If the key size is too small then the peer must support a
    // smaller size and we cannot upgrade the key.
    case ErrorCode::kInsufficientEncryptionKeySize:
      break;
    default:
      break;
  }

  return sm::SecurityLevel::kNoSecurity;
}

MethodType GetMethodType(OpCode opcode) {
  // We treat all packets as a command if the command bit was set. An
  // unrecognized command will always be ignored (so it is OK to return kCommand
  // here if, for example, |opcode| is a response with the command-bit set).
  if (opcode & kCommandFlag)
    return MethodType::kCommand;

  switch (opcode) {
    case kInvalidOpCode:
      return MethodType::kInvalid;

    case kExchangeMTURequest:
    case kFindInformationRequest:
    case kFindByTypeValueRequest:
    case kReadByTypeRequest:
    case kReadRequest:
    case kReadBlobRequest:
    case kReadMultipleRequest:
    case kReadByGroupTypeRequest:
    case kWriteRequest:
    case kPrepareWriteRequest:
    case kExecuteWriteRequest:
      return MethodType::kRequest;

    case kErrorResponse:
    case kExchangeMTUResponse:
    case kFindInformationResponse:
    case kFindByTypeValueResponse:
    case kReadByTypeResponse:
    case kReadResponse:
    case kReadBlobResponse:
    case kReadMultipleResponse:
    case kReadByGroupTypeResponse:
    case kWriteResponse:
    case kPrepareWriteResponse:
    case kExecuteWriteResponse:
      return MethodType::kResponse;

    case kNotification:
      return MethodType::kNotification;
    case kIndication:
      return MethodType::kIndication;
    case kConfirmation:
      return MethodType::kConfirmation;

    // These are redundant with the check above but are included for
    // completeness.
    case kWriteCommand:
    case kSignedWriteCommand:
      return MethodType::kCommand;

    default:
      break;
  }

  // Everything else will be treated as an incoming request.
  return MethodType::kRequest;
}

// Returns the corresponding originating transaction opcode for
// |transaction_end_code|, where the latter must correspond to a response or
// confirmation.
OpCode MatchingTransactionCode(OpCode transaction_end_code) {
  switch (transaction_end_code) {
    case kExchangeMTUResponse:
      return kExchangeMTURequest;
    case kFindInformationResponse:
      return kFindInformationRequest;
    case kFindByTypeValueResponse:
      return kFindByTypeValueRequest;
    case kReadByTypeResponse:
      return kReadByTypeRequest;
    case kReadResponse:
      return kReadRequest;
    case kReadBlobResponse:
      return kReadBlobRequest;
    case kReadMultipleResponse:
      return kReadMultipleRequest;
    case kReadByGroupTypeResponse:
      return kReadByGroupTypeRequest;
    case kWriteResponse:
      return kWriteRequest;
    case kPrepareWriteResponse:
      return kPrepareWriteRequest;
    case kExecuteWriteResponse:
      return kExecuteWriteRequest;
    case kConfirmation:
      return kIndication;
    default:
      break;
  }

  return kInvalidOpCode;
}

}  // namespace

// static
fxl::RefPtr<Bearer> Bearer::Create(fbl::RefPtr<l2cap::Channel> chan) {
  auto bearer = fxl::AdoptRef(new Bearer(std::move(chan)));
  return bearer->Activate() ? bearer : nullptr;
}

Bearer::PendingTransaction::PendingTransaction(OpCode opcode, TransactionCallback callback,
                                               ErrorCallback error_callback, ByteBufferPtr pdu)
    : opcode(opcode),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)),
      pdu(std::move(pdu)),
      security_retry_level(sm::SecurityLevel::kNoSecurity) {
  ZX_DEBUG_ASSERT(this->callback);
  ZX_DEBUG_ASSERT(this->error_callback);
  ZX_DEBUG_ASSERT(this->pdu);
}

Bearer::PendingRemoteTransaction::PendingRemoteTransaction(TransactionId id, OpCode opcode)
    : id(id), opcode(opcode) {}

Bearer::TransactionQueue::TransactionQueue(TransactionQueue&& other)
    : queue_(std::move(other.queue_)), current_(std::move(other.current_)) {
  // The move constructor is only used during shut down below. So we simply
  // cancel the task and not worry about moving it.
  other.timeout_task_.Cancel();
}

Bearer::PendingTransactionPtr Bearer::TransactionQueue::ClearCurrent() {
  ZX_DEBUG_ASSERT(current_);
  ZX_DEBUG_ASSERT(timeout_task_.is_pending());

  timeout_task_.Cancel();

  return std::move(current_);
}

void Bearer::TransactionQueue::Enqueue(PendingTransactionPtr transaction) {
  queue_.push_back(std::move(transaction));
}

void Bearer::TransactionQueue::TrySendNext(l2cap::Channel* chan, async::Task::Handler timeout_cb,
                                           zx::duration timeout) {
  ZX_DEBUG_ASSERT(chan);

  // Abort if a transaction is currently pending.
  if (current())
    return;

  // Advance to the next transaction.
  current_ = queue_.pop_front();
  while (current()) {
    ZX_DEBUG_ASSERT(!timeout_task_.is_pending());
    ZX_DEBUG_ASSERT(current()->pdu);

    // We copy the PDU payload in case it needs to be retried following a
    // security upgrade.
    auto pdu = NewSlabBuffer(current()->pdu->size());
    if (pdu) {
      current()->pdu->Copy(pdu.get());
      timeout_task_.set_handler(std::move(timeout_cb));
      timeout_task_.PostDelayed(async_get_default_dispatcher(), timeout);
      chan->Send(std::move(pdu));
      break;
    }

    bt_log(TRACE, "att", "Failed to start transaction: out of memory!");
    auto t = std::move(current_);
    t->error_callback(Status(HostError::kOutOfMemory), kInvalidHandle);

    // Process the next command until we can send OR we have drained the queue.
    current_ = queue_.pop_front();
  }
}

void Bearer::TransactionQueue::Reset() {
  timeout_task_.Cancel();
  queue_.clear();
  current_ = nullptr;
}

void Bearer::TransactionQueue::InvokeErrorAll(Status status) {
  if (current_) {
    current_->error_callback(status, kInvalidHandle);
  }

  for (const auto& t : queue_) {
    if (t.error_callback)
      t.error_callback(status, kInvalidHandle);
  }
}

Bearer::Bearer(fbl::RefPtr<l2cap::Channel> chan)
    : chan_(std::move(chan)),
      next_remote_transaction_id_(1u),
      next_handler_id_(1u),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(chan_);

  if (chan_->link_type() == hci::Connection::LinkType::kLE) {
    min_mtu_ = kLEMinMTU;
  } else {
    min_mtu_ = kBREDRMinMTU;
  }

  mtu_ = min_mtu();
  // TODO (fxbug.dev/1447): Dynamically configure preferred MTU value.
  preferred_mtu_ = kLEMaxMTU;
}

Bearer::~Bearer() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  rx_task_.Cancel();
  chan_ = nullptr;

  request_queue_.Reset();
  indication_queue_.Reset();
}

bool Bearer::Activate() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  rx_task_.Reset(fit::bind_member(this, &Bearer::OnRxBFrame));
  chan_closed_cb_.Reset(fit::bind_member(this, &Bearer::OnChannelClosed));

  return chan_->Activate(rx_task_.callback(), chan_closed_cb_.callback());
}

void Bearer::ShutDown() {
  if (is_open())
    ShutDownInternal(false /* due_to_timeout */);
}

void Bearer::ShutDownInternal(bool due_to_timeout) {
  ZX_DEBUG_ASSERT(is_open());
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  bt_log(DEBUG, "att", "bearer shutting down");

  rx_task_.Cancel();
  chan_closed_cb_.Cancel();

  // This will have no effect if the channel is already closed (e.g. if
  // ShutDown() was called by OnChannelClosed()).
  chan_->SignalLinkError();
  chan_ = nullptr;

  // Move the contents to temporaries. This prevents a potential memory
  // corruption in InvokeErrorAll if the Bearer gets deleted by one of the
  // invoked error callbacks.
  TransactionQueue req_queue(std::move(request_queue_));
  TransactionQueue ind_queue(std::move(indication_queue_));

  if (closed_cb_)
    closed_cb_();

  // Terminate all remaining procedures with an error. This is safe even if
  // the bearer got deleted by |closed_cb_|.
  Status status(due_to_timeout ? HostError::kTimedOut : HostError::kFailed);
  req_queue.InvokeErrorAll(status);
  ind_queue.InvokeErrorAll(status);
}

bool Bearer::StartTransaction(ByteBufferPtr pdu, TransactionCallback callback,
                              ErrorCallback error_callback) {
  ZX_DEBUG_ASSERT(pdu);
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(error_callback);

  return SendInternal(std::move(pdu), std::move(callback), std::move(error_callback));
}

bool Bearer::SendWithoutResponse(ByteBufferPtr pdu) {
  ZX_DEBUG_ASSERT(pdu);
  return SendInternal(std::move(pdu), {}, {});
}

bool Bearer::SendInternal(ByteBufferPtr pdu, TransactionCallback callback,
                          ErrorCallback error_callback) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  if (!is_open()) {
    bt_log(TRACE, "att", "bearer closed; cannot send packet");
    return false;
  }

  if (!IsPacketValid(*pdu)) {
    bt_log(DEBUG, "att", "packet has bad length!");
    return false;
  }

  PacketReader reader(pdu.get());
  MethodType type = GetMethodType(reader.opcode());

  TransactionQueue* tq = nullptr;

  switch (type) {
    case MethodType::kCommand:
    case MethodType::kNotification:
      if (callback || error_callback) {
        bt_log(DEBUG, "att", "method not a transaction!");
        return false;
      }

      // Send the command. No flow control is necessary.
      chan_->Send(std::move(pdu));
      return true;

    case MethodType::kRequest:
      tq = &request_queue_;
      break;
    case MethodType::kIndication:
      tq = &indication_queue_;
      break;
    default:
      bt_log(DEBUG, "att", "invalid opcode: %#.2x", reader.opcode());
      return false;
  }

  if (!callback || !error_callback) {
    bt_log(DEBUG, "att", "transaction requires callbacks!");
    return false;
  }

  tq->Enqueue(std::make_unique<PendingTransaction>(reader.opcode(), std::move(callback),
                                                   std::move(error_callback), std::move(pdu)));
  TryStartNextTransaction(tq);

  return true;
}

Bearer::HandlerId Bearer::RegisterHandler(OpCode opcode, Handler handler) {
  ZX_DEBUG_ASSERT(handler);

  if (!is_open())
    return kInvalidHandlerId;

  if (handlers_.find(opcode) != handlers_.end()) {
    bt_log(DEBUG, "att", "can only register one handler per opcode (%#.2x)", opcode);
    return kInvalidHandlerId;
  }

  HandlerId id = NextHandlerId();
  if (id == kInvalidHandlerId)
    return kInvalidHandlerId;

  auto res = handler_id_map_.emplace(id, opcode);
  ZX_ASSERT_MSG(res.second, "handler ID got reused (id: %zu)", id);

  handlers_[opcode] = std::move(handler);

  return id;
}

void Bearer::UnregisterHandler(HandlerId id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(id != kInvalidHandlerId);

  auto iter = handler_id_map_.find(id);
  if (iter == handler_id_map_.end()) {
    bt_log(DEBUG, "att", "cannot unregister unknown handler id: %zu", id);
    return;
  }

  OpCode opcode = iter->second;
  handlers_.erase(opcode);
}

bool Bearer::Reply(TransactionId tid, ByteBufferPtr pdu) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(pdu);

  if (tid == kInvalidTransactionId)
    return false;

  if (!is_open()) {
    bt_log(TRACE, "att", "bearer closed; cannot reply");
    return false;
  }

  if (!IsPacketValid(*pdu)) {
    bt_log(DEBUG, "att", "invalid response PDU");
    return false;
  }

  RemoteTransaction* pending = FindRemoteTransaction(tid);
  if (!pending)
    return false;

  PacketReader reader(pdu.get());

  // Use ReplyWithError() instead.
  if (reader.opcode() == kErrorResponse)
    return false;

  OpCode pending_opcode = (*pending)->opcode;
  if (pending_opcode != MatchingTransactionCode(reader.opcode())) {
    bt_log(DEBUG, "att", "opcodes do not match (pending: %#.2x, given: %#.2x)", pending_opcode,
           reader.opcode());
    return false;
  }

  pending->reset();
  chan_->Send(std::move(pdu));

  return true;
}

bool Bearer::ReplyWithError(TransactionId id, Handle handle, ErrorCode error_code) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  RemoteTransaction* pending = FindRemoteTransaction(id);
  if (!pending)
    return false;

  OpCode pending_opcode = (*pending)->opcode;
  if (pending_opcode == kIndication) {
    bt_log(DEBUG, "att", "cannot respond to an indication with error!");
    return false;
  }

  pending->reset();
  SendErrorResponse(pending_opcode, handle, error_code);

  return true;
}

bool Bearer::IsPacketValid(const ByteBuffer& packet) {
  return packet.size() != 0u && packet.size() <= mtu_;
}

void Bearer::TryStartNextTransaction(TransactionQueue* tq) {
  ZX_DEBUG_ASSERT(tq);

  if (!is_open()) {
    bt_log(TRACE, "att", "Cannot process transactions; bearer is closed");
    return;
  }

  tq->TrySendNext(
      chan_.get(),
      [this](async_dispatcher_t*, async::Task*, zx_status_t status) {
        if (status == ZX_OK)
          ShutDownInternal(true /* due_to_timeout */);
      },
      kTransactionTimeout);
}

void Bearer::SendErrorResponse(OpCode request_opcode, Handle attribute_handle,
                               ErrorCode error_code) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto buffer = NewSlabBuffer(sizeof(Header) + sizeof(ErrorResponseParams));
  ZX_ASSERT(buffer);

  PacketWriter packet(kErrorResponse, buffer.get());
  auto* payload = packet.mutable_payload<ErrorResponseParams>();
  payload->request_opcode = request_opcode;
  payload->attribute_handle = htole16(attribute_handle);
  payload->error_code = error_code;

  chan_->Send(std::move(buffer));
}

void Bearer::HandleEndTransaction(TransactionQueue* tq, const PacketReader& packet) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(is_open());
  ZX_DEBUG_ASSERT(tq);

  if (!tq->current()) {
    bt_log(DEBUG, "att", "received unexpected transaction PDU (opcode: %#.2x)", packet.opcode());
    ShutDown();
    return;
  }

  bool report_error = false;
  OpCode target_opcode;
  ErrorCode error_code = ErrorCode::kNoError;
  Handle attr_in_error = kInvalidHandle;

  if (packet.opcode() == kErrorResponse) {
    // We should never hit this branch for indications.
    ZX_DEBUG_ASSERT(tq->current()->opcode != kIndication);

    if (packet.payload_size() == sizeof(ErrorResponseParams)) {
      report_error = true;

      const auto& payload = packet.payload<ErrorResponseParams>();
      target_opcode = payload.request_opcode;
      error_code = payload.error_code;
      attr_in_error = le16toh(payload.attribute_handle);
    } else {
      bt_log(DEBUG, "att", "received malformed error response");

      // Invalid opcode will fail the opcode comparison below.
      target_opcode = kInvalidOpCode;
    }
  } else {
    target_opcode = MatchingTransactionCode(packet.opcode());
  }

  ZX_DEBUG_ASSERT(tq->current()->opcode != kInvalidOpCode);

  if (tq->current()->opcode != target_opcode) {
    bt_log(DEBUG, "att", "received bad transaction PDU (opcode: %#.2x)", packet.opcode());
    ShutDown();
    return;
  }

  // The transaction is complete.
  auto transaction = tq->ClearCurrent();
  ZX_DEBUG_ASSERT(transaction);

  sm::SecurityLevel security_requirement = CheckSecurity(error_code, chan_->security());
  if (transaction->security_retry_level >= security_requirement ||
      security_requirement <= chan_->security().level()) {
    // Resolve the transaction.
    if (!report_error) {
      transaction->callback(packet);
    } else if (transaction->error_callback) {
      transaction->error_callback(Status(error_code), attr_in_error);
    }

    // Send out the next queued transaction
    TryStartNextTransaction(tq);
    return;
  }

  bt_log(TRACE, "att",
         "Received security error for transaction %#.2hhx; requesting upgrade "
         "to level: %s",
         error_code, sm::LevelToString(security_requirement));
  chan_->UpgradeSecurity(
      security_requirement,
      [self = weak_ptr_factory_.GetWeakPtr(), error_code, attr_in_error, security_requirement,
       t = std::move(transaction)](sm::Status status) mutable {
        // If the security upgrade failed or the bearer got destroyed, then
        // resolve the transaction with the original error.
        if (!self || !status) {
          t->error_callback(Status(error_code), attr_in_error);
          return;
        }

        ZX_DEBUG_ASSERT(self->thread_checker_.IsCreationThreadCurrent());

        // TODO(armansito): Notify the upper layer to re-initiate service
        // discovery and other necessary procedures (see Vol 3, Part C,
        // 10.3.2).

        // Re-send the request as described in Vol 3, Part G, 8.1. Since |t| was
        // originally resolved with an Error Response, it must have come out of
        // |request_queue_|.
        ZX_DEBUG_ASSERT(GetMethodType(t->opcode) == MethodType::kRequest);
        t->security_retry_level = security_requirement;
        self->request_queue_.Enqueue(std::move(t));
        self->TryStartNextTransaction(&self->request_queue_);
      },
      async_get_default_dispatcher());

  // Move on to the next queued transaction.
  TryStartNextTransaction(tq);
}

Bearer::HandlerId Bearer::NextHandlerId() {
  auto id = next_handler_id_;

  // This will stop incrementing if this were overflows and always return
  // kInvalidHandlerId.
  if (next_handler_id_ != kInvalidHandlerId)
    next_handler_id_++;
  return id;
}

Bearer::TransactionId Bearer::NextRemoteTransactionId() {
  auto id = next_remote_transaction_id_;

  next_remote_transaction_id_++;

  // Increment extra in the case of overflow.
  if (next_remote_transaction_id_ == kInvalidTransactionId)
    next_remote_transaction_id_++;

  return id;
}

void Bearer::HandleBeginTransaction(RemoteTransaction* currently_pending,
                                    const PacketReader& packet) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(currently_pending);

  if (currently_pending->has_value()) {
    bt_log(DEBUG, "att", "A transaction is already pending! (opcode: %#.2x)", packet.opcode());
    ShutDown();
    return;
  }

  auto iter = handlers_.find(packet.opcode());
  if (iter == handlers_.end()) {
    bt_log(DEBUG, "att", "no handler registered for opcode %#.2x", packet.opcode());
    SendErrorResponse(packet.opcode(), 0, ErrorCode::kRequestNotSupported);
    return;
  }

  auto id = NextRemoteTransactionId();
  *currently_pending = PendingRemoteTransaction(id, packet.opcode());

  iter->second(id, packet);
}

Bearer::RemoteTransaction* Bearer::FindRemoteTransaction(TransactionId id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (remote_request_ && remote_request_->id == id) {
    return &remote_request_;
  }

  if (remote_indication_ && remote_indication_->id == id) {
    return &remote_indication_;
  }

  bt_log(DEBUG, "att", "id %zu does not match any transaction", id);
  return nullptr;
}

void Bearer::HandlePDUWithoutResponse(const PacketReader& packet) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto iter = handlers_.find(packet.opcode());
  if (iter == handlers_.end()) {
    bt_log(DEBUG, "att", "dropping unhandled packet (opcode: %#.2x)", packet.opcode());
    return;
  }

  iter->second(kInvalidTransactionId, packet);
}

void Bearer::OnChannelClosed() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // This will deactivate the channel and notify |closed_cb_|.
  ShutDown();
}

void Bearer::OnRxBFrame(ByteBufferPtr sdu) {
  ZX_DEBUG_ASSERT(sdu);
  ZX_DEBUG_ASSERT(is_open());
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  uint16_t length = sdu->size();

  TRACE_DURATION("bluetooth", "att::Bearer::OnRxBFrame", "length", length);

  // An ATT PDU should at least contain the opcode.
  if (length < sizeof(OpCode)) {
    bt_log(DEBUG, "att", "PDU too short!");
    ShutDown();
    return;
  }

  if (length > mtu_) {
    bt_log(DEBUG, "att", "PDU exceeds MTU!");
    ShutDown();
    return;
  }

  PacketReader packet(sdu.get());
  switch (GetMethodType(packet.opcode())) {
    case MethodType::kResponse:
      HandleEndTransaction(&request_queue_, packet);
      break;
    case MethodType::kConfirmation:
      HandleEndTransaction(&indication_queue_, packet);
      break;
    case MethodType::kRequest:
      HandleBeginTransaction(&remote_request_, packet);
      break;
    case MethodType::kIndication:
      HandleBeginTransaction(&remote_indication_, packet);
      break;
    case MethodType::kNotification:
    case MethodType::kCommand:
      HandlePDUWithoutResponse(packet);
      break;
    default:
      bt_log(DEBUG, "att", "Unsupported opcode: %#.2x", packet.opcode());
      SendErrorResponse(packet.opcode(), 0, ErrorCode::kRequestNotSupported);
      break;
  }
}

}  // namespace bt::att
