// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_

#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <lib/fit/nullable.h>
#include <lib/stdcompat/type_traits.h>

#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/retire_log.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

// In-process data pipeline monitoring service. This issues tokens that accompany data packets
// through various buffers. When tokens are destroyed, their lifetimes are recorded.
//
// Clients may subscribe to alerts for when various values exceed specified thresholds.
//
// TODO(fxbug.dev/71341): Produce pollable statistics about retired tokens
// TODO(fxbug.dev/71342): Timestamp stages of each token
// TODO(fxbug.dev/71342): Provide mechanism to split/merge tokens through chunking/de-chunking
class PipelineMonitor final {
 private:
  using TokenId = uint64_t;

 public:
  // Each Token is created when the monitor "issues" it, at which point it is "in-flight" until the
  // token is "retired" (either explicitly with |Retire()| or by destruction). Tokens model a unique
  // moveable resource. Moved-from Token objects are valid and can take on newly issued tokens from
  // the same PipelineMonitor instance.
  //
  // Tokens that outlive their issuing PipelineMonitor have no effect when retired or destroyed.
  class Token {
   public:
    Token(Token&& other) noexcept : parent_(other.parent_) { *this = std::move(other); }

    Token& operator=(Token&& other) noexcept {
      BT_ASSERT(parent_.get() == other.parent_.get());
      id_ = std::exchange(other.id_, kInvalidTokenId);
      return *this;
    }

    Token() = delete;
    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;

    ~Token() { Retire(); }

    // Explicitly retire to its monitor. This has no effect if the token has already been retired.
    void Retire() {
      if (!bool{parent_}) {
        return;
      }
      if (id_ != kInvalidTokenId) {
        parent_->Retire(this);
      }
      id_ = kInvalidTokenId;
    }

   private:
    friend class PipelineMonitor;
    Token(fxl::WeakPtr<PipelineMonitor> parent, TokenId id) : parent_(std::move(parent)), id_(id) {}

    const fxl::WeakPtr<PipelineMonitor> parent_;
    TokenId id_ = kInvalidTokenId;
  };

  // Alert types used for |SetAlert|. These are used as dimensioned value wrappers whose types
  // identify what kind of value they hold.

  // Alert for max_bytes_in_flight. Fires upon issuing the first token that exceeds the threshold.
  struct MaxBytesInFlightAlert {
    size_t value;
  };

  // Alert for max_bytes_in_flight. Fires upon issuing the first token that exceeds the threshold.
  struct MaxTokensInFlightAlert {
    int64_t value;
  };

  // Alert for token age (duration from issue to retirement). Fires upon retiring the first token
  // that exceeds the threshold.
  struct MaxAgeRetiredAlert {
    zx::duration value;
  };

  // Create a data chunk-tracking service that uses |dispatcher| for timing (may be null to use the
  // default dispatcher). |retire_log| is copied into the class in order to store statistics about
  // retired tokens. Note that the "live" internal log is readable with the |retire_log()| method,
  // not the original instance passed to the ctor (which will not be logged into).
  explicit PipelineMonitor(fit::nullable<async_dispatcher_t*> dispatcher,
                           const internal::RetireLog& retire_log)
      : dispatcher_(dispatcher.value_or(async_get_default_dispatcher())), retire_log_(retire_log) {}

  [[nodiscard]] size_t bytes_issued() const { return bytes_issued_; }
  [[nodiscard]] int64_t tokens_issued() const { return tokens_issued_; }
  [[nodiscard]] size_t bytes_in_flight() const { return bytes_in_flight_; }
  [[nodiscard]] int64_t tokens_in_flight() const {
    BT_ASSERT(issued_tokens_.size() <= std::numeric_limits<int64_t>::max());
    return issued_tokens_.size();
  }
  [[nodiscard]] size_t bytes_retired() const {
    BT_ASSERT(bytes_issued() >= bytes_in_flight());
    return bytes_issued() - bytes_in_flight();
  }
  [[nodiscard]] int64_t tokens_retired() const { return tokens_issued() - tokens_in_flight(); }

  const internal::RetireLog& retire_log() const { return retire_log_; }

  // Start tracking |byte_count| bytes of data. This issues a token that is now considered
  // "in-flight" until it is retired.
  [[nodiscard]] Token Issue(size_t byte_count) {
    // For consistency, complete all token map and counter modifications before processing alerts.
    const auto id = MakeTokenId();
    issued_tokens_.insert_or_assign(id, TokenInfo{async::Now(dispatcher_), byte_count});
    bytes_issued_ += byte_count;
    tokens_issued_++;
    bytes_in_flight_ += byte_count;

    // Process alerts.
    SignalAlertValue<MaxBytesInFlightAlert>(bytes_in_flight());
    SignalAlertValue<MaxTokensInFlightAlert>(tokens_in_flight());
    return Token(weak_ptr_factory_.GetWeakPtr(), id);
  }

  // Subscribes to an alert that fires when the watched value strictly exceeds |threshold|. When
  // that happens, |listener| is called with the alert type containing the actual value and the
  // alert trigger is removed.
  //
  // New alerts will not be signaled until the next event that can change the value (token issued,
  // retired, etc), so |listener| can re-subscribe (but likely at a different threshold to avoid a
  // tight loop of re-subscriptions).
  //
  // For example,
  //   monitor.SetAlert(MaxBytesInFlightAlert{kMaxBytes},
  //                    [](MaxBytesInFlightAlert value) { /* value.value = bytes_in_flight() */ });
  template <typename AlertType>
  void SetAlert(AlertType threshold, fit::callback<void(decltype(threshold))> listener) {
    GetAlertList<AlertType>().push(AlertInfo<AlertType>{threshold, std::move(listener)});
  }

  // Convenience function to set a single listener for all of |alerts|, called if any of the alerts
  // defined by |thresholds| are triggered.
  template <typename... AlertTypes>
  void SetAlerts(fit::function<void(std::variant<cpp20::type_identity_t<AlertTypes>...>)> listener,
                 AlertTypes... thresholds) {
    // This is a fold expression over the comma (,) operator with SetAlert.
    (SetAlert<AlertTypes>(thresholds, listener.share()), ...);
  }

 private:
  // Tracks information for each Token issued out-of-line so that Tokens can be kept small.
  struct TokenInfo {
    zx::time issue_time = zx::time::infinite();
    size_t byte_count = 0;
  };

  // Records alerts and their subscribers. Removed when fired.
  template <typename AlertType>
  struct AlertInfo {
    AlertType threshold;
    fit::callback<void(AlertType)> listener;

    // Used by std::priority_queue through std::less. The logic is intentionally inverted so that
    // the AlertInfo with the smallest threshold appears first.
    bool operator<(const AlertInfo<AlertType>& other) const {
      return this->threshold.value > other.threshold.value;
    }
  };

  // Store subscribers so that the earliest and most likely to fire threshold is highest priority
  // to make testing values against thresholds constant time and fast.
  template <typename T>
  using AlertList = std::priority_queue<AlertInfo<T>>;

  template <typename... AlertTypes>
  using AlertRegistry = std::tuple<AlertList<AlertTypes>...>;

  // Used in Token to represent an inactive Token object (one that does not represent an in-flight
  // token in the monitor).
  static constexpr TokenId kInvalidTokenId = std::numeric_limits<TokenId>::max();

  template <typename AlertType>
  AlertList<AlertType>& GetAlertList() {
    return std::get<AlertList<AlertType>>(alert_registry_);
  }

  // Signal a change in the value watched by |AlertType| and test it against the subscribed
  // alert thresholds. Any thresholds strict exceeded (with std::greater) cause its subscribed
  // listener to be removed and invoked.
  template <typename AlertType>
  void SignalAlertValue(decltype(AlertType::value) value) {
    auto& alert_list = GetAlertList<AlertType>();
    std::vector<decltype(AlertInfo<AlertType>::listener)> listeners;
    while (!alert_list.empty()) {
      auto& top = alert_list.top();
      if (!std::greater()(value, top.threshold.value)) {
        break;
      }

      // std::priority_queue intentionally has const access to top() in order to avoid breaking heap
      // constraints. This cast to remove const and modify top respects that design intent because
      // (1) it doesn't modify element order (2) the intent is to pop the top anyways. It is
      // important to call |listener| after pop in case that call re-subscribes to this call (which
      // could modify the heap top).
      listeners.push_back(std::move(const_cast<AlertInfo<AlertType>&>(top).listener));
      alert_list.pop();
    }

    // Deferring the call to after filtering helps prevent infinite alert loops.
    for (auto& listener : listeners) {
      listener(AlertType{value});
    }
  }

  TokenId MakeTokenId() {
    return std::exchange(next_token_id_, (next_token_id_ + 1) % kInvalidTokenId);
  }

  void Retire(Token* token) {
    // For consistency, complete all token map and counter modifications before processing alerts.
    BT_ASSERT(this == token->parent_.get());
    auto node = issued_tokens_.extract(token->id_);
    BT_ASSERT(bool{node});
    TokenInfo& packet_info = node.mapped();
    bytes_in_flight_ -= packet_info.byte_count;
    const zx::duration age = async::Now(dispatcher_) - packet_info.issue_time;
    retire_log_.Retire(packet_info.byte_count, age);

    // Process alerts.
    SignalAlertValue<MaxAgeRetiredAlert>(age);
  }

  async_dispatcher_t* const dispatcher_ = nullptr;

  internal::RetireLog retire_log_;

  // This is likely not the best choice for memory efficiency and insertion latency (allocation and
  // rehashing are both concerning). A slotmap is likely a good choice here with some tweaks to
  // Token invalidation and an eye for implementation (SG14 slot_map may have O(N) insertion).
  std::unordered_map<TokenId, TokenInfo> issued_tokens_;

  TokenId next_token_id_ = 0;
  size_t bytes_issued_ = 0;
  int64_t tokens_issued_ = 0;
  size_t bytes_in_flight_ = 0;

  // Use a single variable to store all of the alert subscribers. This can be split by type using
  // std::get (see GetAlertList).
  AlertRegistry<MaxBytesInFlightAlert, MaxTokensInFlightAlert, MaxAgeRetiredAlert> alert_registry_;

  fxl::WeakPtrFactory<PipelineMonitor> weak_ptr_factory_{this};
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_
