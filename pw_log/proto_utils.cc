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

#include "pw_log/proto_utils.h"

#include "pw_log/levels.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_protobuf/wire_format.h"

namespace pw::log {

Result<ConstByteSpan> EncodeTokenizedLog(pw::log_tokenized::Metadata metadata,
                                         ConstByteSpan tokenized_data,
                                         int64_t ticks_since_epoch,
                                         ByteSpan encode_buffer) {
  // Encode message to the LogEntry protobuf.
  LogEntry::MemoryEncoder encoder(encode_buffer);

  encoder.WriteMessage(tokenized_data)
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  encoder
      .WriteLineLevel((metadata.level() & PW_LOG_LEVEL_BITMASK) |
                      ((metadata.line_number() << PW_LOG_LEVEL_BITS) &
                       ~PW_LOG_LEVEL_BITMASK))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  if (metadata.flags() != 0) {
    encoder.WriteFlags(metadata.flags())
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  }
  encoder.WriteTimestamp(ticks_since_epoch)
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

  PW_TRY(encoder.status());
  return ConstByteSpan(encoder);
}

}  // namespace pw::log
