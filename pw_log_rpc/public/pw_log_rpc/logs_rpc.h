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

#pragma once

#include "pw_log/log.h"
#include "pw_log_multisink/log_queue.h"
#include "pw_log_proto/log.raw_rpc.pb.h"

namespace pw::log_rpc {

// The Logs RPC service will send logs when requested by Get(). For now, Get()
// requests result in a stream of responses, containing all log entries from
// the attached log queue.
//
// The Get() method will return logs in the current queue immediately, but
// someone else is responsible for pumping the log queue using Flush().
class Logs final : public pw::log::generated::Logs<Logs> {
 public:
  Logs(LogQueue& log_queue) : log_queue_(log_queue), dropped_entries_(0) {}

  // RPC API for the Logs that produces a log stream. This method will
  // return immediately, another class must call Flush() to push logs from
  // the queue to this stream.
  void Get(ServerContext&, ConstByteSpan, rpc::RawServerWriter& writer);

  // Interface for the owner of the service instance to flush all existing
  // logs to the writer, if one is attached.
  Status Flush();

  // Interface for the owner of the service instance to close the RPC, if
  // one is attached.
  void Finish() { response_writer_.Finish(); }

 private:
  LogQueue& log_queue_;
  rpc::RawServerWriter response_writer_;
  size_t dropped_entries_;
};

}  // namespace pw::log_rpc
