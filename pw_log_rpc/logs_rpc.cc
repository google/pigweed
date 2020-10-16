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

#include "pw_log_rpc/logs_rpc.h"

#include "pw_log/log.h"
#include "pw_log_proto/log.pwpb.h"
#include "pw_status/try.h"

namespace pw::log_rpc {
namespace {

Result<ConstByteSpan> GenerateDroppedEntryMessage(ByteSpan encode_buffer,
                                                  size_t dropped_entries) {
  pw::protobuf::NestedEncoder nested_encoder(encode_buffer);
  pw::log::LogEntry::Encoder encoder(&nested_encoder);
  encoder.WriteDropped(dropped_entries);
  return nested_encoder.Encode();
}

}  // namespace

void Logs::Get(ServerContext&, ConstByteSpan, rpc::RawServerWriter& writer) {
  response_writer_ = std::move(writer);
}

Status Logs::Flush() {
  // If the response writer was not initialized or has since been closed,
  // ignore the flush operation.
  if (!response_writer_.open()) {
    return Status::Ok();
  }

  // If previous calls to flush resulted in dropped entries, generate a
  // dropped entry message and write it before further log messages.
  if (dropped_entries_ > 0) {
    ByteSpan payload = response_writer_.PayloadBuffer();
    Result dropped_log = GenerateDroppedEntryMessage(payload, dropped_entries_);
    PW_TRY(dropped_log.status());
    PW_TRY(response_writer_.Write(dropped_log.value()));
    dropped_entries_ = 0;
  }

  // Write logs to the response writer. An important limitation of this
  // implementation is that if this RPC call fails, the logs are lost -
  // a subsequent call to the RPC will produce a drop count message.
  ByteSpan payload = response_writer_.PayloadBuffer();
  Result possible_logs = log_queue_.PopMultiple(payload);
  PW_TRY(possible_logs.status());
  if (possible_logs.value().entry_count == 0) {
    return Status::Ok();
  }

  Status status = response_writer_.Write(possible_logs.value().entries);
  if (!status.ok()) {
    // On a failure to send logs, track the dropped entries.
    dropped_entries_ = possible_logs.value().entry_count;
    return status;
  }

  return Status::Ok();
}

}  // namespace pw::log_rpc
