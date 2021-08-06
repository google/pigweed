// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include "pw_assert/assert.h"
#include "pw_containers/intrusive_list.h"
#include "pw_result/result.h"
#include "pw_transfer/handler.h"

namespace pw::transfer::internal {

// Information about a single transfer.
class Context {
 public:
  enum Type { kRead, kWrite };

  constexpr Context()
      : type_(kRead),
        handler_(nullptr),
        offset_(0),
        pending_bytes_(0),
        max_chunk_size_bytes_(std::numeric_limits<uint32_t>::max()) {}

  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;

  Status Start(Type type, Handler& handler);
  void Finish(Status status);

  constexpr bool active() const { return handler_ != nullptr; }

  constexpr uint32_t transfer_id() const { return handler().id(); }

  stream::Reader& reader() const {
    PW_DASSERT(type_ == kRead);
    return handler().reader();
  }

  stream::Writer& writer() const {
    PW_DASSERT(type_ == kWrite);
    return handler().writer();
  }

  constexpr uint32_t offset() const { return offset_; }
  constexpr void set_offset(size_t offset) { offset_ = offset; }

  constexpr uint32_t pending_bytes() const { return pending_bytes_; }
  constexpr void set_pending_bytes(size_t pending_bytes) {
    pending_bytes_ = pending_bytes;
  }

  constexpr uint32_t max_chunk_size_bytes() const {
    return max_chunk_size_bytes_;
  }
  constexpr void set_max_chunk_size_bytes(size_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
  }

 private:
  constexpr Handler& handler() {
    PW_DASSERT(active());
    return *handler_;
  }

  constexpr const Handler& handler() const {
    PW_DASSERT(active());
    return *handler_;
  }

  Type type_;
  Handler* handler_;
  size_t offset_;
  size_t pending_bytes_;
  size_t max_chunk_size_bytes_;
};

class ContextPool {
 public:
  constexpr ContextPool(Context::Type type,
                        IntrusiveList<internal::Handler>& handlers)
      : type_(type), handlers_(handlers) {}

  Result<Context*> GetOrStartTransfer(uint32_t id);

 private:
  // TODO(frolv): Initially, only one transfer at a time is supported. Once that
  // is updated, this should be made configurable.
  static constexpr int kMaxConcurrentTransfers = 1;

  Context::Type type_;
  std::array<Context, kMaxConcurrentTransfers> transfers_;
  IntrusiveList<internal::Handler>& handlers_;
};

}  // namespace pw::transfer::internal
