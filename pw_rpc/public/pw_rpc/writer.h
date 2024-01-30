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

#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_rpc/internal/lock.h"
#include "pw_status/status.h"

namespace pw::rpc {
namespace internal {
class Call;
}

// The Writer class allows writing requests or responses to a streaming RPC.
// ClientWriter, ClientReaderWriter, ServerWriter, and ServerReaderWriter
// classes can be used as a generic Writer.
class Writer {
 public:
  Writer(const Writer&) = delete;
  Writer(Writer&&) = delete;

  Writer& operator=(const Writer&) = delete;
  Writer& operator=(Writer&&) = delete;

  bool active() const;
  uint32_t channel_id() const;

  Status Write(ConstByteSpan payload) PW_LOCKS_EXCLUDED(internal::rpc_lock());

 private:
  // Only allow Call to inherit from Writer. This guarantees that Writers can
  // always safely downcast to Call.
  friend class internal::Call;

  // Writers cannot be created directly. They may only be used as a reference to
  // an existing call object.
  constexpr Writer() = default;
};

}  // namespace pw::rpc
