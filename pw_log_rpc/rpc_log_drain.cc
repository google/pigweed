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

#include "pw_log_rpc/rpc_log_drain.h"

#include <mutex>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_string/string_builder.h"

namespace pw::log_rpc {
namespace {
// When encoding LogEntry in LogEntries, there are kLogEntryEncodeFrameSize
// bytes added to the encoded LogEntry.
constexpr size_t kLogEntryEncodeFrameSize =
    protobuf::SizeOfFieldKey(1)  // LogEntry
    + protobuf::kMaxSizeOfLength;

// Creates an encoded drop message on the provided buffer.
Result<ConstByteSpan> CreateEncodedDropMessage(
    uint32_t drop_count, ByteSpan encoded_drop_message_buffer) {
  StringBuffer<RpcLogDrain::kMaxDropMessageSize> message;
  message.Format(RpcLogDrain::kDropMessageFormatString,
                 static_cast<unsigned int>(drop_count));

  // Encode message in protobuf.
  log::LogEntry::MemoryEncoder encoder(encoded_drop_message_buffer);
  encoder.WriteMessage(std::as_bytes(std::span(std::string_view(message))));
  encoder.WriteLineLevel(PW_LOG_LEVEL_WARN & PW_LOG_LEVEL_BITMASK);
  PW_TRY(encoder.status());
  return ConstByteSpan(encoder);
}
}  // namespace

Status RpcLogDrain::Open(rpc::RawServerWriter& writer) {
  if (!writer.active()) {
    return Status::FailedPrecondition();
  }
  std::lock_guard lock(mutex_);
  if (server_writer_.active()) {
    return Status::AlreadyExists();
  }
  server_writer_ = std::move(writer);
  return OkStatus();
}

Status RpcLogDrain::Flush() {
  PW_CHECK_NOTNULL(multisink_);

  LogDrainState log_sink_state = LogDrainState::kMoreEntriesRemaining;
  std::lock_guard lock(mutex_);
  do {
    if (!server_writer_.active()) {
      return Status::Unavailable();
    }
    log::LogEntries::MemoryEncoder encoder(server_writer_.PayloadBuffer());
    uint32_t packed_entry_count = 0;
    log_sink_state = EncodeOutgoingPacket(encoder, packed_entry_count);
    if (const Status status = server_writer_.Write(encoder); !status.ok()) {
      committed_entry_drop_count_ += packed_entry_count;
      if (error_handling_ == LogDrainErrorHandling::kCloseStreamOnWriterError) {
        server_writer_.Finish().IgnoreError();
        return Status::Aborted();
      }
    }
  } while (log_sink_state == LogDrainState::kMoreEntriesRemaining);
  return OkStatus();
}

RpcLogDrain::LogDrainState RpcLogDrain::EncodeOutgoingPacket(
    log::LogEntries::MemoryEncoder& encoder, uint32_t& packed_entry_count_out) {
  const size_t total_buffer_size = encoder.ConservativeWriteLimit();
  do {
    // Get entry and drop count from drain.
    uint32_t drop_count = 0;
    Result<multisink::MultiSink::Drain::PeekedEntry> possible_entry =
        PeekEntry(log_entry_buffer_, drop_count);
    if (possible_entry.status().IsResourceExhausted()) {
      continue;
    }

    // Report drop count if messages were dropped.
    if (committed_entry_drop_count_ > 0 || drop_count > 0) {
      // Reuse the log_entry_buffer_ to send a drop message.
      const Result<ConstByteSpan> drop_message_result =
          CreateEncodedDropMessage(committed_entry_drop_count_ + drop_count,
                                   log_entry_buffer_);
      // Add encoded drop messsage if fits in buffer.
      if (drop_message_result.ok() &&
          drop_message_result.value().size() + kLogEntryEncodeFrameSize <
              encoder.ConservativeWriteLimit()) {
        PW_CHECK_OK(encoder.WriteBytes(
            static_cast<uint32_t>(log::LogEntries::Fields::ENTRIES),
            drop_message_result.value()));
        committed_entry_drop_count_ = 0;
      }
      if (possible_entry.ok()) {
        PW_CHECK_OK(PeekEntry(log_entry_buffer_, drop_count).status());
      }
    }

    if (possible_entry.status().IsOutOfRange()) {
      return LogDrainState::kCaughtUp;  // There are no more entries.
    }
    // At this point all expected error modes have been handled.
    PW_CHECK_OK(possible_entry.status());

    // Check if the entry fits in encoder buffer.
    const size_t encoded_entry_size =
        possible_entry.value().entry().size() + kLogEntryEncodeFrameSize;
    if (encoded_entry_size + kLogEntryEncodeFrameSize > total_buffer_size) {
      // Entry is larger than the entire available buffer.
      ++committed_entry_drop_count_;
      PW_CHECK_OK(PopEntry(possible_entry.value()));
      continue;
    } else if (encoded_entry_size > encoder.ConservativeWriteLimit()) {
      // Entry does not fit in the partially filled encoder buffer. Notify the
      // caller there are more entries to send.
      return LogDrainState::kMoreEntriesRemaining;
    }

    // Encode log entry and remove it from multisink.
    PW_CHECK_OK(encoder.WriteBytes(
        static_cast<uint32_t>(log::LogEntries::Fields::ENTRIES),
        possible_entry.value().entry()));
    PW_CHECK_OK(PopEntry(possible_entry.value()));
    ++packed_entry_count_out;
  } while (true);
}

Status RpcLogDrain::Close() {
  std::lock_guard lock(mutex_);
  return server_writer_.Finish();
}

}  // namespace pw::log_rpc
