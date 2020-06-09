// Copyright 2020 The Pigweed Authors
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

#include <cstddef>
#include <utility>

#include "pw_rpc/channel.h"
#include "pw_rpc/server_context.h"
#include "pw_span/span.h"

namespace pw::rpc::internal {

class Method;
class Packet;

// Internal ServerWriter base class. ServerWriters are used to stream responses.
// Implementations must provide a derived class that provides the interface for
// sending responses.
//
// TODO(hepler): ServerWriters need to be tracked by the server to support
// cancelling / terminating ongoing streaming RPCs.
class BaseServerWriter {
 public:
  constexpr BaseServerWriter(ServerCall& context)
      : context_(context), state_{kOpen} {}

  BaseServerWriter(BaseServerWriter&& other) { *this = std::move(other); }
  BaseServerWriter& operator=(BaseServerWriter&& other);

  BaseServerWriter(const BaseServerWriter&) = delete;
  BaseServerWriter& operator=(const BaseServerWriter&) = delete;

  // True if the ServerWriter is active and ready to send responses.
  bool open() const { return state_ == kOpen; }

  // Closes the ServerWriter, if it is open.
  void close();

 protected:
  constexpr BaseServerWriter() : state_{kClosed} {}

  const Method& method() const { return context_.method(); }

  span<std::byte> AcquireBuffer();

  Status SendAndReleaseBuffer(span<const std::byte> payload);

 private:
  Packet packet() const;

  ServerCall context_;
  span<std::byte> response_;
  enum { kClosed, kOpen } state_;
};

}  // namespace pw::rpc::internal
