// Copyright 2025 The Pigweed Authors
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

#include "pw_bytes/span.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

/// Wraps a fixed-size contiguous MultiBuf and allows for easily writing
/// sequential data into it.
///
/// * A MultiBufWriter is created using Create().
/// * Data can be written with Write() until IsComplete() returns true.
/// * The data can be viewed with U8Span() or TakeMultiBuf().
class MultiBufWriter {
 public:
  /// Creates a MultiBufWriter wrapping a fixed-size, contiguous MultiBuf.
  static pw::Result<MultiBufWriter> Create(
      multibuf::MultiBufAllocator& multibuf_allocator, size_t size);

  // MultiBufWriter is movable but not copyable.
  MultiBufWriter(const MultiBufWriter&) = delete;
  MultiBufWriter& operator=(const MultiBufWriter&) = delete;
  MultiBufWriter(MultiBufWriter&&) = default;
  MultiBufWriter& operator=(MultiBufWriter&&) = default;

  /// Gets a span of the previously-written data.
  ///
  /// After TakeMultiBuf(), this returns an empty span.
  pw::span<uint8_t> U8Span() {
    // ContiguousSpan() cannot fail because Create() uses AllocateContiguous().
    return pw::span(reinterpret_cast<uint8_t*>(buf_.ContiguousSpan()->data()),
                    write_offset_);
  }

  /// Returns true when the MultiBuf is full; i.e., when the total number of
  /// bytes written equals the size passed to Create(). Always returns true
  /// after TakeMultiBuf() is called.
  bool IsComplete() const { return remain() == 0; }

  /// Consumes the underlying MultiBuf.
  ///
  /// After this method is called, this object is reset to an empty state:
  /// No data can be written, and all data accesors will return an empty
  /// result. IsComplete() will return true.
  multibuf::MultiBuf&& TakeMultiBuf() {
    write_offset_ = 0;

    // A moved-from MultiBuf is equivalent to a default, empty MultiBuf.
    return std::move(buf_);
  }

  /// Writes data into the MultiBuf at the next unwritten location.
  Status Write(pw::ConstByteSpan data);

  Status Write(pw::span<const uint8_t> data) {
    return Write(pw::as_bytes(data));
  }

 private:
  MultiBufWriter(multibuf::MultiBuf&& buf) : buf_(std::move(buf)) {}

  /// Returns the number of bytes remaining to be written before IsComplete()
  /// returns true.
  size_t remain() const { return buf_.size() - write_offset_; }

  multibuf::MultiBuf buf_;
  size_t write_offset_ = 0;
};

}  // namespace pw::bluetooth::proxy
