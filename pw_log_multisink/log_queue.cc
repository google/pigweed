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

#include "pw_log_multisink/log_queue.h"

#include "pw_log/levels.h"
#include "pw_log_proto/log.pwpb.h"
#include "pw_protobuf/wire_format.h"
#include "pw_status/try.h"

namespace pw::log_rpc {
namespace {

using pw::protobuf::WireType;
constexpr std::byte kLogKey = static_cast<std::byte>(pw::protobuf::MakeKey(
    static_cast<uint32_t>(pw::log::LogEntries::Fields::ENTRIES),
    WireType::kDelimited));

}  // namespace

Status LogQueue::PushTokenizedMessage(ConstByteSpan message,
                                      uint32_t flags,
                                      uint32_t level,
                                      uint32_t line,
                                      uint32_t thread,
                                      int64_t timestamp) {
  pw::protobuf::NestedEncoder nested_encoder(encode_buffer_);
  pw::log::LogEntry::Encoder encoder(&nested_encoder);
  Status status;

  encoder.WriteMessageTokenized(message);
  encoder.WriteLineLevel(
      (level & PW_LOG_LEVEL_BITMASK) |
      ((line << PW_LOG_LEVEL_BITWIDTH) & ~PW_LOG_LEVEL_BITMASK));
  encoder.WriteFlags(flags);
  encoder.WriteThreadTokenized(thread);

  // TODO(prashanthsw): Add support for delta encoding of the timestamp.
  encoder.WriteTimestamp(timestamp);

  if (dropped_entries_ > 0) {
    encoder.WriteDropped(dropped_entries_);
  }

  ConstByteSpan log_entry;
  status = nested_encoder.Encode(&log_entry);
  if (!status.ok() || log_entry.size_bytes() > max_log_entry_size_) {
    // If an encoding failure occurs or the constructed log entry is larger
    // than the configured max size, map the error to INTERNAL. If the
    // underlying allocation of this encode buffer or the nested encoding
    // sequencing are at fault, they are not the caller's responsibility. If
    // the log entry is larger than the max allowed size, the log is dropped
    // intentionally, and it is expected that the caller accepts this
    // possibility.
    status = PW_STATUS_INTERNAL;
  } else {
    // Try to push back the encoded log entry.
    status = ring_buffer_.TryPushBack(log_entry, std::byte(kLogKey));
  }

  if (!status.ok()) {
    // The ring buffer may hit the RESOURCE_EXHAUSTED state, causing us
    // to drop packets. However, this check captures all failures from
    // Encode and TryPushBack, as any failure here causes packet drop.
    dropped_entries_++;
    latest_dropped_timestamp_ = timestamp;
    return status;
  }

  dropped_entries_ = 0;
  return Status::Ok();
}

Result<LogEntries> LogQueue::Pop(LogEntriesBuffer entry_buffer) {
  size_t ring_buffer_entry_size = 0;
  PW_TRY(pop_status_for_test_);
  // The caller must provide a buffer that is at minimum max_log_entry_size, to
  // ensure that the front entry of the ring buffer can be popped.
  PW_DCHECK_UINT_GE(entry_buffer.size_bytes(), max_log_entry_size_);
  PW_TRY(ring_buffer_.PeekFrontWithPreamble(entry_buffer,
                                            &ring_buffer_entry_size));
  PW_DCHECK_OK(ring_buffer_.PopFront());

  return LogEntries{
      .entries = ConstByteSpan(entry_buffer.first(ring_buffer_entry_size)),
      .entry_count = 1};
}

LogEntries LogQueue::PopMultiple(LogEntriesBuffer entries_buffer) {
  size_t offset = 0;
  size_t entry_count = 0;

  // The caller must provide a buffer that is at minimum max_log_entry_size, to
  // ensure that the front entry of the ring buffer can be popped.
  PW_DCHECK_UINT_GE(entries_buffer.size_bytes(), max_log_entry_size_);

  while (ring_buffer_.EntryCount() > 0 &&
         (entries_buffer.size_bytes() - offset) > max_log_entry_size_) {
    const Result<LogEntries> result = Pop(entries_buffer.subspan(offset));
    if (!result.ok()) {
      break;
    }
    offset += result.value().entries.size_bytes();
    entry_count += result.value().entry_count;
  }

  return LogEntries{.entries = ConstByteSpan(entries_buffer.first(offset)),
                    .entry_count = entry_count};
}

}  // namespace pw::log_rpc
