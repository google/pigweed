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
#include "pw_log/proto_utils.h"
#include "pw_log_rpc/rpc_log_drain.h"
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_multisink/multisink.h"
#include "pw_result/result.h"
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

}  // namespace

// Deferred log buffer, for storing log entries while logging_thread_ streams
// them independently.
multisink::MultiSink& GetMultiSink() {
  static multisink::MultiSink multisink(log_buffer);
  return multisink;
}

log_rpc::RpcLogDrainThread& GetLogThread() {
  static log_rpc::RpcLogDrainThread logging_thread(GetMultiSink(), drain_map);
  return logging_thread;
}

log_rpc::LogService& GetLogService() {
  static log_rpc::LogService log_service(drain_map);
  return log_service;
}

int64_t GetTimestamp() {
  // TODO(cachinchilla): update method to get timestamp according to target.
  return 0;
}

// TODO (cachinchilla): Finish this!
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

}  // namespace pw::system
