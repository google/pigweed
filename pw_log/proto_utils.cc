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

#include <span>
#include <string_view>

#include "pw_bytes/endian.h"
#include "pw_log/levels.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_protobuf/wire_format.h"

namespace pw::log {

Result<ConstByteSpan> EncodeLog(int level,
                                unsigned int flags,
                                std::string_view module_name,
                                std::string_view thread_name,
                                std::string_view file_name,
                                int line_number,
                                int64_t ticks_since_epoch,
                                std::string_view message,
                                ByteSpan encode_buffer) {
  // Encode message to the LogEntry protobuf.
  LogEntry::MemoryEncoder encoder(encode_buffer);

  if (message.empty()) {
    return Status::InvalidArgument();
  }

  // Defer status checks until the end.
  Status status = encoder.WriteMessage(std::as_bytes(std::span(message)));
  status = encoder.WriteLineLevel(PackLineLevel(line_number, level));
  if (flags != 0) {
    status = encoder.WriteFlags(flags);
  }
  status = encoder.WriteTimestamp(ticks_since_epoch);

  // Module name and file name may or may not be present.
  if (!module_name.empty()) {
    status = encoder.WriteModule(std::as_bytes(std::span(module_name)));
  }
  if (!file_name.empty()) {
    status = encoder.WriteFile(std::as_bytes(std::span(file_name)));
  }
  if (!thread_name.empty()) {
    status = encoder.WriteThread(std::as_bytes(std::span(thread_name)));
  }
  PW_TRY(encoder.status());
  return ConstByteSpan(encoder);
}

LogEntry::MemoryEncoder CreateEncoderAndEncodeTokenizedLog(
    pw::log_tokenized::Metadata metadata,
    ConstByteSpan tokenized_data,
    int64_t ticks_since_epoch,
    ByteSpan encode_buffer) {
  // Encode message to the LogEntry protobuf.
  LogEntry::MemoryEncoder encoder(encode_buffer);

  // Defer status checks until the end.
  Status status = encoder.WriteMessage(tokenized_data);
  status = encoder.WriteLineLevel(
      PackLineLevel(metadata.line_number(), metadata.level()));
  if (metadata.flags() != 0) {
    status = encoder.WriteFlags(metadata.flags());
  }
  status = encoder.WriteTimestamp(ticks_since_epoch);
  if (metadata.module() != 0) {
    const uint32_t little_endian_module =
        bytes::ConvertOrderTo(std::endian::little, metadata.module());
    status =
        encoder.WriteModule(std::as_bytes(std::span(&little_endian_module, 1)));
  }
  return encoder;
}

}  // namespace pw::log
