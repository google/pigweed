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

#include "pw_log_sink/log_sink.h"

#include <atomic>
#include <cstring>
#include <mutex>

#include "pw_log/levels.h"
#include "pw_log_proto/log.pwpb.h"
#include "pw_protobuf/wire_format.h"
#include "pw_status/try.h"
#include "pw_string/string_builder.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::log_sink {
namespace {
// TODO: Make buffer sizes configurable.
constexpr size_t kMaxMessageStringSize = 32;
constexpr size_t kEncodeBufferSize = 128;

size_t drop_count = 0;

// The sink list and its corresponding lock are Meyer's singletons, to ensure
// they are constructed before use. This enables us to use logging before C++
// global construction has completed.
IntrusiveList<Sink>& sink_list() {
  static IntrusiveList<Sink> sink_list;
  return sink_list;
}

pw::sync::InterruptSpinLock& sink_list_lock() {
  // TODO(pwbug/304): Make lock selection configurable, some applications may
  // not be able to tolerate interrupt jitter and may prefer a pw::sync::Mutex.
  static pw::sync::InterruptSpinLock sink_list_lock;
  return sink_list_lock;
}

}  // namespace

// This is a fully loaded, inefficient-at-the-callsite, log implementation.
extern "C" void pw_LogSink_Log(int level,
                               unsigned int flags,
                               const char* /* module_name */,
                               const char* /* file_name */,
                               int line_number,
                               const char* /* function_name */,
                               const char* message,
                               ...) {
  // Encode message to the LogEntry protobuf.
  std::byte encode_buffer[kEncodeBufferSize];
  pw::protobuf::NestedEncoder nested_encoder(encode_buffer);
  pw::log::LogEntry::Encoder encoder(&nested_encoder);

  encoder.WriteLineLevel(
      (level & PW_LOG_LEVEL_BITMASK) |
      ((line_number << PW_LOG_LEVEL_BITWIDTH) & ~PW_LOG_LEVEL_BITMASK));
  encoder.WriteFlags(flags);

  // TODO(pwbug/301): Insert reasonable values for thread and timestamp.
  encoder.WriteTimestamp(0);

  // Accumulate the log message in this buffer, then output it.
  pw::StringBuffer<kMaxMessageStringSize> buffer;
  va_list args;

  va_start(args, message);
  buffer.FormatVaList(message, args);
  va_end(args);
  encoder.WriteMessageString(buffer.c_str());
  encoder.WriteThreadString("");

  ConstByteSpan log_entry;
  Status status = nested_encoder.Encode(&log_entry);
  bool is_entry_valid = buffer.status().ok() && status.ok();

  // TODO(pwbug/305): Consider using a shared buffer between users. For now,
  // only lock after completing the encoding.
  {
    const std::lock_guard<pw::sync::InterruptSpinLock> lock(sink_list_lock());

    // If no sinks are configured, ignore the message. When sinks are attached,
    // they will receive this drop count to indicate logs drop to early boot.
    // The drop count is cleared after it is sent to a sink, so sinks attached
    // later will not receive drop counts from early boot.
    if (sink_list().size() == 0) {
      drop_count++;
      return;
    }

    // If an encoding failure occurs or the constructed log entry is larger
    // than the maximum allowed size, the log is dropped.
    if (!is_entry_valid) {
      drop_count++;
    }

    // Push entries to all attached sinks. This is a synchronous operation, so
    // attached sinks should avoid blocking when processing entries. If the log
    // entry is not valid, only the drop notification is sent to the sinks.
    for (auto& sink : sink_list()) {
      // The drop count is always provided before sending entries, to ensure the
      // sink processes drops in-order.
      if (drop_count > 0) {
        sink.HandleDropped(drop_count);
      }
      if (is_entry_valid) {
        sink.HandleEntry(log_entry);
      }
    }
    // All sinks have been notified of any drops.
    drop_count = 0;
  }
}

void AddSink(Sink& sink) {
  const std::lock_guard lock(sink_list_lock());
  sink_list().push_back(sink);
}

void RemoveSink(Sink& sink) {
  const std::lock_guard lock(sink_list_lock());
  sink_list().remove(sink);
}

}  // namespace pw::log_sink
