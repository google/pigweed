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

#include <array>
#include <cstddef>

#include "pw_assert/assert.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::stream {

// General-purpose writer interface.
class Writer {
 public:
  // There are no requirements around if or how a `Writer` should call Flush()
  // at the time of object destruction. In general, do not assume a `Writer`
  // will automatically call Flush when destroyed.
  virtual ~Writer() = default;

  // Write data to this stream Writer. If the writer runs out of resources, it
  // will return Status::RESOURCE_EXHAUSTED. The derived class choses whether
  // to do partial writes in this case, or abort the entire write operation.
  //
  // Derived classes should NOT try to override these public write methods.
  // Instead, provide an implementation by overriding DoWrite().
  Status Write(span<const std::byte> data) {
    PW_DCHECK(data.empty() || data.data() != nullptr);
    return DoWrite(data);
  }
  Status Write(const void* data, size_t size_bytes) {
    return Write(span(static_cast<const std::byte*>(data), size_bytes));
  }

  // Flush any buffered data, finalizing all writes.
  //
  // Generally speaking, the scope that instantiates the concrete `Writer`
  // class should be in charge of calling `Flush()`, and functions that only
  // have access to the Writer interface should avoid calling this function.
  virtual Status Flush() { return Status::OK; }

 private:
  virtual Status DoWrite(span<const std::byte> data) = 0;
};

}  // namespace pw::stream
