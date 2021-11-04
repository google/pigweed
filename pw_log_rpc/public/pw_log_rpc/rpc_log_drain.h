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

#include <array>
#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log_rpc/log_filter.h"
#include "pw_multisink/multisink.h"
#include "pw_protobuf/serialized_size.h"
#include "pw_result/result.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {

// RpcLogDrain matches a MultiSink::Drain with with an RPC channel's writer. A
// RPC channel ID identifies this drain. The user must attach this drain
// to a MultiSink that returns a log::LogEntry, and provide a buffer large
// enough to hold the largest log::LogEntry transmittable. The user must call
// Flush(), which, on every call, packs as many log::LogEntry items as possible
// into a log::LogEntries message, writes the message to the provided writer,
// then repeats the process until there are no more entries in the MultiSink or
// the writer failed to write the outgoing package and error_handling is set to
// `kCloseStreamOnWriterError`. When error_handling is `kIgnoreWriterErrors` the
// drain will continue to retrieve log entries out of the MultiSink and attempt
// to send them out ignoring the writer errors without sending a drop count.
// Note: the error handling and drop count reporting might change in the future.
// Log filtering is done using the rules of the Filter provided if any.
class RpcLogDrain : public multisink::MultiSink::Drain {
 public:
  // Dictates how to handle server writer errors.
  enum class LogDrainErrorHandling {
    kIgnoreWriterErrors,
    kCloseStreamOnWriterError,
  };

  // The minimum buffer size, without the message payload or module sizes,
  // needed to retrieve a log::LogEntry from the attached MultiSink. The user
  // must account for the max message size to avoid log entry drops. The dropped
  // field is not accounted since a dropped message has all other fields unset.
  static constexpr size_t kMinEntrySizeWithoutPayload =
      // message
      protobuf::SizeOfFieldKey(1) +
      1  // Assume minimum varint length, skip the payload bytes.
      // line_level
      + protobuf::SizeOfFieldKey(2) +
      protobuf::kMaxSizeBytesUint32
      // flags
      + protobuf::SizeOfFieldKey(3) +
      protobuf::kMaxSizeBytesUint32
      // timestamp or time_since_last_entry
      + protobuf::SizeOfFieldKey(4) +
      protobuf::kMaxSizeBytesInt64
      // Module
      + protobuf::SizeOfFieldKey(7) +
      1;  // Assume minimum varint length, skip the module bytes.
  // The smallest buffer size must be able to fit a typical token size: 4 bytes.
  static constexpr size_t kMinEntryBufferSize = kMinEntrySizeWithoutPayload + 4;

  // When encoding LogEntry in LogEntries, there are kLogEntryEncodeFrameSize
  // bytes added to the encoded LogEntry. This constant and kMinEntryBufferSize
  // can be used to calculate the minimum RPC ChannelOutput buffer size.
  static constexpr size_t kLogEntryEncodeFrameSize =
      protobuf::SizeOfFieldKey(1)  // LogEntry
      + protobuf::kMaxSizeOfLength;

  // Creates a closed log stream with a writer that can be set at a later time.
  // The provided buffer must be large enough to hold the largest transmittable
  // log::LogEntry or a drop count message at the very least. The user can
  // choose to provide a unique mutex for the drain, or share it to save RAM as
  // long as they are aware of contengency issues.
  RpcLogDrain(const uint32_t channel_id,
              ByteSpan log_entry_buffer,
              sync::Mutex& mutex,
              LogDrainErrorHandling error_handling,
              Filter* filter = nullptr)
      : channel_id_(channel_id),
        error_handling_(error_handling),
        server_writer_(),
        log_entry_buffer_(log_entry_buffer),
        committed_entry_drop_count_(0),
        mutex_(mutex),
        filter_(filter) {
    PW_ASSERT(log_entry_buffer.size_bytes() >= kMinEntryBufferSize);
  }

  // Not copyable.
  RpcLogDrain(const RpcLogDrain&) = delete;
  RpcLogDrain& operator=(const RpcLogDrain&) = delete;

  // Configures the drain with a new open server writer if the current one is
  // not open.
  //
  // Return values:
  // OK - Successfully set the new open writer.
  // FAILED_PRECONDITION - The given writer is not open.
  // ALREADY_EXISTS - an open writer is already set.
  Status Open(rpc::RawServerWriter& writer) PW_LOCKS_EXCLUDED(mutex_);

  // Accesses log entries and sends them via the writer. Expected to be called
  // frequently to avoid log drops. If the writer fails to send a packet with
  // multiple log entries, the entries are dropped and a drop message with the
  // count is sent. When error_handling is kCloseStreamOnWriterError, the stream
  // will automatically be closed and Flush will return the writer error.
  //
  // Precondition: the drain must be attached to a MultiSink.
  //
  // Return values:
  // OK - all entries were consumed.
  // ABORTED - there was an error writing the packet, and error_handling equals
  // `kCloseStreamOnWriterError`.
  Status Flush() PW_LOCKS_EXCLUDED(mutex_);

  // Ends RPC log stream without flushing.
  //
  // Return values:
  // OK - successfully closed the server writer.
  // FAILED_PRECONDITION - The given writer is not open.
  // Errors from the underlying writer send packet.
  Status Close() PW_LOCKS_EXCLUDED(mutex_);

  uint32_t channel_id() const { return channel_id_; }

 private:
  enum class LogDrainState {
    kCaughtUp,
    kMoreEntriesRemaining,
  };

  // Fills the outgoing buffer with as many entries as possible.
  LogDrainState EncodeOutgoingPacket(log::LogEntries::MemoryEncoder& encoder,
                                     uint32_t& packed_entry_count_out)
      PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const uint32_t channel_id_;
  const LogDrainErrorHandling error_handling_;
  rpc::RawServerWriter server_writer_ PW_GUARDED_BY(mutex_);
  const ByteSpan log_entry_buffer_ PW_GUARDED_BY(mutex_);
  uint32_t committed_entry_drop_count_ PW_GUARDED_BY(mutex_);
  sync::Mutex& mutex_;
  Filter* filter_;
};

}  // namespace pw::log_rpc
