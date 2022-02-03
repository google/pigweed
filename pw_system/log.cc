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

#include "pw_system_private/log.h"

#include <array>
#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/proto_utils.h"
#include "pw_log_rpc/rpc_log_drain.h"
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_multisink/multisink.h"
#include "pw_result/result.h"
#include "pw_string/string_builder.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_system/config.h"
#include "pw_system_private/rpc.h"
#include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"

namespace pw::system {
namespace {

using log_rpc::RpcLogDrain;

// Storage container for MultiSink used for deferred logging.
std::array<std::byte, PW_SYSTEM_LOG_BUFFER_SIZE> log_buffer;

// Buffer used to encode each log entry before saving into log buffer.
sync::InterruptSpinLock log_encode_lock;
std::array<std::byte, PW_SYSTEM_MAX_LOG_ENTRY_SIZE> log_encode_buffer
    PW_GUARDED_BY(log_encode_lock);

// String-only logs may need to be formatted first. This buffer is required
// so the format string may be passed to the proto log encode.
std::array<std::byte, PW_SYSTEM_MAX_LOG_ENTRY_SIZE> log_format_buffer
    PW_GUARDED_BY(log_encode_lock);

// To save RAM, share the mutex and buffer between drains, since drains are
// flushed sequentially.
sync::Mutex drains_mutex;
// Buffer to decode and remove entries from log buffer, to send to a drain.
std::array<std::byte, PW_SYSTEM_MAX_LOG_ENTRY_SIZE> log_decode_buffer
    PW_GUARDED_BY(drains_mutex);

std::array<RpcLogDrain, 1> drains{{
    RpcLogDrain(kDefaultChannelId,
                log_decode_buffer,
                drains_mutex,
                RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
}};

log_rpc::RpcLogDrainMap drain_map(drains);

const int64_t boot_time_count =
    pw::chrono::SystemClock::now().time_since_epoch().count();

// TODO(amontanez): Is there a helper to subtract RPC overhead?
constexpr size_t kMaxPackedLogMessagesSize =
    PW_SYSTEM_MAX_TRANSMISSION_UNIT - 32;

std::array<std::byte, kMaxPackedLogMessagesSize> log_packing_buffer;

}  // namespace

// Deferred log buffer, for storing log entries while logging_thread_ streams
// them independently.
multisink::MultiSink& GetMultiSink() {
  static multisink::MultiSink multisink(log_buffer);
  return multisink;
}

log_rpc::RpcLogDrainThread& GetLogThread() {
  static log_rpc::RpcLogDrainThread logging_thread(
      GetMultiSink(), drain_map, log_packing_buffer);
  return logging_thread;
}

log_rpc::LogService& GetLogService() {
  static log_rpc::LogService log_service(drain_map);
  return log_service;
}

// Provides time since boot in units defined by the target's pw_chrono backend.
int64_t GetTimestamp() {
  return pw::chrono::SystemClock::now().time_since_epoch().count() -
         boot_time_count;
}

// Implementation for tokenized log handling. This will be optimized out for
// devices that only use string logging.
extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
    pw_tokenizer_Payload payload, const uint8_t message[], size_t size_bytes) {
  log_tokenized::Metadata metadata = payload;
  const int64_t timestamp = GetTimestamp();

  std::lock_guard lock(log_encode_lock);
  Result<ConstByteSpan> encoded_log_result = log::EncodeTokenizedLog(
      metadata, message, size_bytes, timestamp, log_encode_buffer);
  if (!encoded_log_result.ok()) {
    GetMultiSink().HandleDropped();
    return;
  }
  GetMultiSink().HandleEntry(encoded_log_result.value());
}

// Implementation for string log handling. This will be optimized out for
// devices that only use tokenized logging.
extern "C" void pw_log_string_HandleMessage(int level,
                                            unsigned int flags,
                                            const char* module_name,
                                            const char* file_name,
                                            int line_number,
                                            const char* message,
                                            ...) {
  const int64_t timestamp = GetTimestamp();

  std::lock_guard lock(log_encode_lock);
  StringBuilder message_builder(log_format_buffer);
  va_list args;
  va_start(args, message);
  message_builder.FormatVaList(message, args);
  va_end(args);

  Result<ConstByteSpan> encoded_log_result =
      log::EncodeLog(level,
                     flags,
                     module_name,
                     file_name,
                     line_number,
                     timestamp,
                     message_builder.view(),
                     log_encode_buffer);
  if (!encoded_log_result.ok()) {
    GetMultiSink().HandleDropped();
    return;
  }
  GetMultiSink().HandleEntry(encoded_log_result.value());
}

}  // namespace pw::system
