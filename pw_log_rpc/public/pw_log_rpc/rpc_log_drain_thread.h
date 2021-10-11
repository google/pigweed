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

#include <span>

#include "pw_log_rpc/log_service.h"
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_multisink/multisink.h"
#include "pw_result/result.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread_core.h"

namespace pw::log_rpc {

// RpcLogDrainThread is a single thread and single MultiSink::Listener that
// manages multiple log streams. It is a suitable option when a minimal
// thread count is desired but comes with the cost of individual log streams
// blocking each other's flushing.
class RpcLogDrainThread final : public thread::ThreadCore,
                                public multisink::MultiSink::Listener {
 public:
  RpcLogDrainThread(multisink::MultiSink& multisink, RpcLogDrainMap& drain_map)
      : drain_map_(drain_map), multisink_(multisink) {}

  void OnNewEntryAvailable() override {
    new_log_available_notification_.release();
  }

  // Sequentially flushes each log stream.
  void Run() override {
    for (auto& drain : drain_map_.drains()) {
      multisink_.AttachDrain(drain);
    }
    multisink_.AttachListener(*this);
    while (true) {
      new_log_available_notification_.acquire();
      for (auto& drain : drain_map_.drains()) {
        drain.Flush().IgnoreError();
      }
    }
  }

  // Opens a server writer to set up an unrequested log stream.
  Status OpenUnrequestedLogStream(uint32_t channel_id,
                                  rpc::Server& rpc_server,
                                  LogService& log_service) {
    rpc::RawServerWriter writer =
        rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
            rpc_server, channel_id, log_service);
    const Result<RpcLogDrain*> drain =
        drain_map_.GetDrainFromChannelId(channel_id);
    PW_TRY(drain.status());
    return drain.value()->Open(writer);
  }

 private:
  sync::ThreadNotification new_log_available_notification_;
  RpcLogDrainMap& drain_map_;
  multisink::MultiSink& multisink_;
};

}  // namespace pw::log_rpc
