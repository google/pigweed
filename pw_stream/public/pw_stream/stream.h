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
#include <span>

#include "pw_assert/light.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::stream {

// General-purpose writer interface.
class Writer {
 public:
  // There are no requirements around if or how a `Writer` should call Flush()
  // at the time of object destruction.
  virtual ~Writer() = default;

  // Write data to this stream Writer. Data is
  // not guaranteed to be fully written out to final resting place on Write
  // return.
  //
  // If the writer is unable to fully accept the input data size it will abort
  // the write and return RESOURCE_EXHAUSTED.
  //
  // If the writer has been exhausted and is and can no longer accept additional
  // bytes it will return OUT_OF_RANGE. This is similar to EndOfFile. Write will
  // only return OUT_OF_RANGE if ConservativeWriteLimit() is and will remain
  // zero. A Write operation that is successful and also exhausts the writer
  // returns OK, with all following calls returning OUT_OF_RANGE. When
  // ConservativeWriteLimit() is greater than zero, a Write that is a number of
  // bytes beyond what will exhaust the Write will abort and return
  // RESOURCE_EXHAUSTED rather than OUT_OF_RANGE because the writer is still
  // able to write bytes.
  //
  // Derived classes should NOT try to override the public Write methods.
  // Instead, provide an implementation by overriding DoWrite().
  //
  // Returns:
  //
  // OK - successful write/enqueue of data.
  // FAILED_PRECONDITION - writer unable/not in state to accept data.
  // RESOURCE_EXHAUSTED - unable to write all of requested data at this time. No
  //     data written.
  // OUT_OF_RANGE - Writer has been exhausted, similar to EOF. No data written,
  //     no more will be written.
  Status Write(ConstByteSpan data) {
    PW_DASSERT(data.empty() || data.data() != nullptr);
    return DoWrite(data);
  }
  Status Write(const void* data, size_t size_bytes) {
    return Write(std::span(static_cast<const std::byte*>(data), size_bytes));
  }
  Status Write(const std::byte b) { return Write(&b, 1); }

  // Probable (not guaranteed) minimum number of bytes at this time that can be
  // written. This number is advisory and not guaranteed to write without a
  // RESOURCE_EXHAUSTED or OUT_OF_RANGE. As Writer processes/handles enqueued of
  // other contexts write data this number can go up or down for some Writers.
  // Returns zero if, in the current state, Write() would not return
  // Status::Ok().
  //
  // Returns std::numeric_limits<size_t>::max() if the implementation has no
  // limits on write sizes.
  virtual size_t ConservativeWriteLimit() const {
    return std::numeric_limits<size_t>::max();
  }

 private:
  virtual Status DoWrite(ConstByteSpan data) = 0;
};

// General-purpose reader interface
class Reader {
 public:
  virtual ~Reader() = default;

  // Read data from this stream Reader. If any number of bytes are read return
  // OK with a span of the actual byte read.
  //
  // If the reader has been exhausted and is and can no longer read additional
  // bytes it will return OUT_OF_RANGE. This is similar to EndOfFile. Read will
  // only return OUT_OF_RANGE if ConservativeReadLimit() is and will remain
  // zero. A Read operation that is successful and also exhausts the reader
  // returns OK, with all following calls returning OUT_OF_RANGE.
  //
  // Derived classes should NOT try to override these public read methods.
  // Instead, provide an implementation by overriding DoRead().
  //
  // Returns:
  //
  // OK with span of bytes read - success, between 1 and dest.size_bytes() were
  //     read.
  // FAILED_PRECONDITION - Reader unable/not in state to read data.
  // RESOURCE_EXHAUSTED - unable to read any bytes at this time. No bytes read.
  //     Try again once bytes become available.
  // OUT_OF_RANGE - Reader has been exhausted, similar to EOF. No bytes read, no
  //     more will be read.
  Result<ByteSpan> Read(ByteSpan dest) {
    PW_DASSERT(dest.empty() || dest.data() != nullptr);
    StatusWithSize result = DoRead(dest);

    if (result.ok()) {
      return dest.first(result.size());
    } else {
      return result.status();
    }
  }
  Result<ByteSpan> Read(void* dest, size_t size_bytes) {
    return Read(std::span(static_cast<std::byte*>(dest), size_bytes));
  }

  // Probable (not guaranteed) minimum number of bytes at this time that can be
  // read. This number is advisory and not guaranteed to read full number of
  // requested bytes or without a RESOURCE_EXHAUSTED or OUT_OF_RANGE. As Reader
  // processes/handles/receives enqueued data or other contexts read data this
  // number can go up or down for some Readers.
  // Returns zero if, in the current state, Read() would not return
  // Status::Ok().
  //
  // Returns std::numeric_limits<size_t>::max() if the implementation imposes no
  // limits on read sizes.
  virtual size_t ConservativeReadLimit() const {
    return std::numeric_limits<size_t>::max();
  }

 private:
  virtual StatusWithSize DoRead(ByteSpan dest) = 0;
};

}  // namespace pw::stream
