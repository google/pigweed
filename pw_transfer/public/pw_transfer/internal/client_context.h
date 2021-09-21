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

#include <variant>

#include "pw_assert/assert.h"
#include "pw_function/function.h"
#include "pw_stream/stream.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer {

class Client;

namespace internal {

class ClientContext : public Context {
 public:
  constexpr ClientContext()
      : internal::Context(),
        client_(nullptr),
        on_completion_(nullptr),
        is_last_chunk_(false) {}

  constexpr bool active() const { return client_ != nullptr; }

  constexpr bool is_read_transfer() const {
    // A read transfer reads data from the server and writes it to the client.
    return std::holds_alternative<stream::Writer*>(stream_);
  }
  constexpr bool is_write_transfer() const { return !is_read_transfer(); }

  constexpr bool is_last_chunk() const { return is_last_chunk_; }
  constexpr void set_is_last_chunk(bool is_last_chunk) {
    is_last_chunk_ = is_last_chunk;
  }

  constexpr Client& client() {
    PW_DASSERT(active());
    return *client_;
  }

  constexpr stream::Reader& reader() const {
    PW_DASSERT(active() && is_write_transfer());
    return *std::get<stream::Reader*>(stream_);
  }

  constexpr stream::Writer& writer() const {
    PW_DASSERT(active() && is_read_transfer());
    return *std::get<stream::Writer*>(stream_);
  }

  void StartRead(Client& client,
                 uint32_t transfer_id,
                 stream::Writer& writer,
                 Function<void(Status)> on_completion);

  void StartWrite(Client& client,
                  uint32_t transfer_id,
                  stream::Reader& reader,
                  Function<void(Status)> on_completion);

  void Finish(Status status) {
    PW_DASSERT(active());
    on_completion_(status);
    client_ = nullptr;
  }

 private:
  Client* client_;
  std::variant<std::monostate, stream::Reader*, stream::Writer*> stream_;
  Function<void(Status)> on_completion_;
  bool is_last_chunk_;
};

}  // namespace internal
}  // namespace pw::transfer
