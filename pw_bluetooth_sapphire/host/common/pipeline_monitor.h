// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_

#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/fit/nullable.h>
#include <zircon/assert.h>

#include <limits>
#include <unordered_map>

#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

// In-process data pipeline monitoring service. This issues tokens that accompany data packets
// through various buffers. When tokens are destroyed, their lifetimes are recorded.
// TODO(xow): Set thresholds and subscribe to exceed warnings
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
      ZX_ASSERT(parent_.get() == other.parent_.get());
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
    Token(fxl::WeakPtr<PipelineMonitor> parent, TokenId id) : parent_(parent), id_(id) {}

    const fxl::WeakPtr<PipelineMonitor> parent_;
    TokenId id_ = kInvalidTokenId;
  };

  explicit PipelineMonitor(fit::nullable<async_dispatcher_t*> dispatcher)
      : dispatcher_(dispatcher.value_or(async_get_default_dispatcher())) {}

  [[nodiscard]] size_t bytes_issued() const { return bytes_issued_; }
  [[nodiscard]] int64_t tokens_issued() const { return tokens_issued_; }
  [[nodiscard]] size_t bytes_in_flight() const { return bytes_in_flight_; }
  [[nodiscard]] int64_t tokens_in_flight() const {
    ZX_ASSERT(issued_tokens_.size() <= std::numeric_limits<int64_t>::max());
    return issued_tokens_.size();
  }
  [[nodiscard]] size_t bytes_retired() const {
    ZX_ASSERT(bytes_issued() >= bytes_in_flight());
    return bytes_issued() - bytes_in_flight();
  }
  [[nodiscard]] int64_t tokens_retired() const { return tokens_issued() - tokens_in_flight(); }

  // Start tracking |byte_count| bytes of data. This issues a token that is now considered
  // "in-flight" until it is retired.
  [[nodiscard]] Token Issue(size_t byte_count) {
    const auto id = MakeTokenId();
    issued_tokens_.insert_or_assign(id, TokenInfo{async::Now(dispatcher_), byte_count});
    bytes_issued_ += byte_count;
    tokens_issued_++;
    bytes_in_flight_ += byte_count;
    return Token(weak_ptr_factory_.GetWeakPtr(), id);
  }

 private:
  // Tracks information for each Token issued out-of-line so that Tokens can be kept small.
  struct TokenInfo {
    zx::time issue_time = zx::time::infinite();
    size_t byte_count = 0;
  };

  // Used in Token to represent an inactive Token object (one that does not represent an in-flight
  // token in the monitor).
  static constexpr TokenId kInvalidTokenId = std::numeric_limits<TokenId>::max();

  TokenId MakeTokenId() {
    return std::exchange(next_token_id_, (next_token_id_ + 1) % kInvalidTokenId);
  }

  void Retire(Token* token) {
    ZX_ASSERT(this == token->parent_.get());
    auto node = issued_tokens_.extract(token->id_);
    ZX_ASSERT(bool{node});
    TokenInfo& packet_info = node.mapped();
    bytes_in_flight_ -= packet_info.byte_count;
  };

  async_dispatcher_t* const dispatcher_ = nullptr;

  // This is likely not the best choice for memory efficiency and insertion latency (allocation and
  // rehashing are both concerning). A slotmap is likely a good choice here with some tweaks to
  // Token invalidation and an eye for implementation (SG14 slot_map may have O(N) insertion).
  std::unordered_map<TokenId, TokenInfo> issued_tokens_;

  TokenId next_token_id_ = 0;
  size_t bytes_issued_ = 0;
  int64_t tokens_issued_ = 0;
  size_t bytes_in_flight_ = 0;

  fxl::WeakPtrFactory<PipelineMonitor> weak_ptr_factory_{this};
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_PIPELINE_MONITOR_H_
