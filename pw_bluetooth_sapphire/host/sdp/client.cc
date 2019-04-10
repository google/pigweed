// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client.h"

#include <lib/async/default.h>

namespace bt {
namespace sdp {

namespace {

constexpr zx::duration kTransactionTimeout = zx::sec(5);

class Impl final : public Client {
 public:
  explicit Impl(fbl::RefPtr<l2cap::Channel> channel);

  virtual ~Impl() override;

 private:
  void ServiceSearchAttributes(
      std::unordered_set<common::UUID> search_pattern,
      const std::unordered_set<AttributeId>& req_attributes,
      SearchResultCallback result_cb,
      async_dispatcher_t* cb_dispatcher) override;

  // Information about a transaction that hasn't finished yet.
  struct Transaction {
    Transaction(TransactionId id, ServiceSearchAttributeRequest req,
                SearchResultCallback cb, async_dispatcher_t* disp);
    // The TransactionId used for this request.  This will be reused until the
    // transaction is complete.
    TransactionId id;
    // The Request for this transaction,
    ServiceSearchAttributeRequest request;
    // Callback for results
    SearchResultCallback callback;
    // The dispatcher that result callbacks get called on
    async_dispatcher_t* dispatcher;
    // The response, built from successive responses from the remote server.
    ServiceSearchAttributeResponse response;
  };

  // Callbacks for l2cap::Channel
  void OnRxFrame(common::ByteBufferPtr sdu);
  void OnChannelClosed();

  // Finishes a pending transaction on this client, completing their callbacks.
  void Finish(TransactionId id);

  // Cancels a pending transaction this client has started, completing the
  // callback with the given status.
  void Cancel(TransactionId id, Status status);

  // Get the next available transaction id
  TransactionId GetNextId();

  // The channel that this client is running on.
  l2cap::ScopedChannel channel_;
  // THe next transaction id that we should use
  TransactionId next_tid_;
  // Any transactions that have been sent but are not completed.
  std::unordered_map<TransactionId, Transaction> pending_;
  // Timeouts for the transactions that are pending.
  std::unordered_map<TransactionId, async::TaskClosure> timeouts_;

  fxl::WeakPtrFactory<Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Impl);
};

Impl::Impl(fbl::RefPtr<l2cap::Channel> channel)
    : channel_(std::move(channel)), next_tid_(0), weak_ptr_factory_(this) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  bool activated = channel_->Activate(
      [self](auto packet) {
        if (self) {
          self->OnRxFrame(std::move(packet));
        }
      },
      [self] {
        if (self) {
          self->OnChannelClosed();
        }
      },
      async_get_default_dispatcher());
  if (!activated) {
    bt_log(INFO, "sdp", "failed to activate channel");
    channel_ = nullptr;
  }
}

Impl::~Impl() {
  while (!pending_.empty()) {
    Cancel(pending_.begin()->first, Status(common::HostError::kCanceled));
  }
}

void Impl::ServiceSearchAttributes(
    std::unordered_set<common::UUID> search_pattern,
    const std::unordered_set<AttributeId>& req_attributes,
    SearchResultCallback result_cb, async_dispatcher_t* cb_dispatcher) {
  ServiceSearchAttributeRequest req;
  req.set_search_pattern(std::move(search_pattern));
  if (req_attributes.empty()) {
    req.AddAttributeRange(0, 0xFFFF);
  } else {
    for (const auto& id : req_attributes) {
      req.AddAttribute(id);
    }
  }
  TransactionId next = GetNextId();

  if (!channel_ || !channel_->Send(req.GetPDU(next))) {
    bt_log(INFO, "sdp", "Failed to send search request: link closed");
    return;
  }

  pending_.try_emplace(next, next, std::move(req), std::move(result_cb),
                       cb_dispatcher);
  auto [iter, placed] = timeouts_.try_emplace(next);
  ZX_DEBUG_ASSERT_MSG(placed, "Should not have repeat transaction ID %u", next);
  auto& timeout_task = iter->second;
  // Timeouts are held in this so it is safe to use.
  timeout_task.set_handler(
      [this, next]() { Cancel(next, Status(common::HostError::kTimedOut)); });
  timeout_task.PostDelayed(async_get_default_dispatcher(), kTransactionTimeout);
}

void Impl::Finish(TransactionId id) {
  auto node = pending_.extract(id);
  ZX_DEBUG_ASSERT(node);
  auto& state = node.mapped();
  auto timeout_node = timeouts_.extract(id);
  ZX_DEBUG_ASSERT(timeout_node);
  timeout_node.mapped().Cancel();
  if (!state.callback) {
    return;
  }
  FXL_DCHECK(state.response.complete());
  async::PostTask(state.dispatcher, [cb = std::move(state.callback),
                                     response = std::move(state.response)] {
    size_t count = response.num_attribute_lists();
    for (size_t idx = 0; idx < count; idx++) {
      if (!cb(Status(), response.attributes(idx))) {
        return;
      }
    }
    cb(Status(common::HostError::kNotFound), {});
  });
}

Impl::Transaction::Transaction(TransactionId id,
                               ServiceSearchAttributeRequest req,
                               SearchResultCallback cb,
                               async_dispatcher_t* disp)
    : id(id),
      request(std::move(req)),
      callback(std::move(cb)),
      dispatcher(disp) {}

void Impl::Cancel(TransactionId id, Status status) {
  auto node = pending_.extract(id);
  if (!node) {
    return;
  }
  async::PostTask(node.mapped().dispatcher,
                  [callback = std::move(node.mapped().callback),
                   status = std::move(status)] { callback(status, {}); });
}

void Impl::OnRxFrame(common::ByteBufferPtr data) {
  // Each SDU in SDP is one request or one response. Core 5.0 Vol 3 Part B, 4.2
  common::PacketView<sdp::Header> packet(data.get());
  size_t pkt_params_len = data->size() - sizeof(Header);
  uint16_t params_len = betoh16(packet.header().param_length);
  if (params_len != pkt_params_len) {
    bt_log(INFO, "sdp", "bad params length (len %zu != %u), dropping",
           pkt_params_len, params_len);
    return;
  }
  packet.Resize(params_len);
  TransactionId tid = letoh16(packet.header().tid);
  auto it = pending_.find(tid);
  if (it == pending_.end()) {
    bt_log(INFO, "sdp", "Impl rx unknown transaction id (%u)", tid);
    return;
  }
  auto& transaction = it->second;
  Status parse_status = transaction.response.Parse(packet.payload_data());
  if (!parse_status) {
    if (parse_status.error() == common::HostError::kInProgress) {
      bt_log(INFO, "sdp", "Requesting continuation of id (%u)", tid);
      transaction.request.SetContinuationState(
          transaction.response.ContinuationState());
      if (!channel_->Send(transaction.request.GetPDU(tid))) {
        bt_log(INFO, "sdp", "Failed to request continuation of transaction!");
      }
      return;
    }
    bt_log(INFO, "sdp", "Failed to parse packet for tid %u: %s", tid,
           parse_status.ToString().c_str());
    // Drop the transaction with the error.
    Cancel(tid, parse_status);
    return;
  }
  if (transaction.response.complete()) {
    Finish(tid);
  }
}

void Impl::OnChannelClosed() {
  bt_log(INFO, "sdp", "client channel closed");
  channel_ = nullptr;
  while (!pending_.empty()) {
    Cancel(pending_.begin()->first,
           Status(common::HostError::kLinkDisconnected));
  }
}

TransactionId Impl::GetNextId() {
  TransactionId next = next_tid_++;
  ZX_DEBUG_ASSERT(pending_.size() < std::numeric_limits<TransactionId>::max());
  while (pending_.count(next)) {
    next = next_tid_++;  // Note: overflow is fine
  }
  return next;
}

}  // namespace

std::unique_ptr<Client> Client::Create(fbl::RefPtr<l2cap::Channel> channel) {
  return std::make_unique<Impl>(std::move(channel));
}

}  // namespace sdp
}  // namespace bt
