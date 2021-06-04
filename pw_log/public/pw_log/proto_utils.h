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

#include "pw_bytes/span.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_result/result.h"

namespace pw::log {

// Convenience functions to convert from tokenized metadata to the log proto
// format.
//
// Return values:
// Ok - A byte span containing the encoded log proto.
// ResourceExhausted - The provided buffer was not large enough to store the
// proto.
Result<ConstByteSpan> EncodeTokenizedLog(log_tokenized::Metadata metadata,
                                         ConstByteSpan tokenized_data,
                                         int64_t ticks_since_epoch,
                                         ByteSpan encode_buffer);

inline Result<ConstByteSpan> EncodeTokenizedLog(
    log_tokenized::Metadata metadata,
    const uint8_t* tokenized_data,
    size_t tokenized_data_size,
    int64_t ticks_since_epoch,
    ByteSpan encode_buffer) {
  return EncodeTokenizedLog(
      metadata,
      std::as_bytes(std::span(tokenized_data, tokenized_data_size)),
      ticks_since_epoch,
      encode_buffer);
}

}  // namespace pw::log
