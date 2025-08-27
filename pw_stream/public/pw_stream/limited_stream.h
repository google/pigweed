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

#include "pw_status/try.h"
#include "pw_stream/stream.h"

namespace pw::stream {

/// @submodule{pw_stream,concrete}

/// Wraps a stream to ensure only a limited number of bytes can be written.
///
/// Write attempts which would exceed the current limit will return
/// `OUT_OF_RANGE`.
class LimitedStreamWriter : public NonSeekableWriter {
 public:
  /// Constructs a `LimitedStreamWriter` which wraps another stream.
  ///
  /// @param[in]  writer  The stream to wrap.
  ///
  /// @param[in]  limit   The maximum number of bytes which can be written.
  /// Defaults to `kUnlimited`.
  LimitedStreamWriter(Writer& writer, size_t limit = kUnlimited)
      : writer_(writer), limit_(limit) {}

  /// Returns the current limit of this writer.
  size_t limit() const { return limit_; }

  /// Changes the current limit of this writer.
  ///
  /// NOTE: If the limit is set to a value <= `bytes_written()`, no additional
  /// data can be written.
  void set_limit(size_t limit) { limit_ = limit; }

  /// Returns the number of bytes written to the stream.
  size_t bytes_written() const { return written_; }

 private:
  Writer& writer_;
  size_t limit_;
  size_t written_ = 0;

  /// Returns the number of remaining bytes that can be written before the
  /// limit is reached, or `kUnlimited` if unlimited.
  size_t remain() const {
    if (limit() == kUnlimited) {
      return kUnlimited;
    }
    if (written_ > limit_) {
      return 0;
    }
    return limit_ - written_;
  }

  Status DoWrite(ConstByteSpan data) override {
    // Write all or nothing.
    if (data.size() > remain()) {
      return Status::OutOfRange();
    }
    PW_TRY(writer_.Write(data));
    written_ += data.size();
    return OkStatus();
  }

  size_t ConservativeLimit(LimitType limit_type) const override {
    switch (limit_type) {
      case LimitType::kRead:
        return 0;
      case LimitType::kWrite:
        return std::min(remain(), writer_.ConservativeWriteLimit());
    }
  }
};

/// @}

}  // namespace pw::stream
