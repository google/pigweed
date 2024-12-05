// Copyright 2024 The Pigweed Authors
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

#include "pw_grpc/send_queue.h"

#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::grpc {

void SendQueue::ProcessSendQueue(async::Context&, Status task_status) {
  if (!task_status.ok()) {
    return;
  }

  multibuf::MultiBuf buffer;
  {
    std::lock_guard lock(send_mutex_);
    if (buffer_to_write_.empty()) {
      return;
    }
    buffer = std::move(buffer_to_write_);
  }
  for (const auto& chunk : buffer.Chunks()) {
    if (Status status = socket_.Write(chunk); !status.ok()) {
      PW_LOG_ERROR("Failed to write to socket in SendQueue: %s", status.str());
      return;
    }
  }
}

void SendQueue::QueueSend(multibuf::MultiBuf&& buffer) {
  std::lock_guard lock(send_mutex_);
  buffer_to_write_.PushSuffix(std::move(buffer));
  send_dispatcher_.Cancel(send_task_);
  send_dispatcher_.Post(send_task_);
}

}  // namespace pw::grpc
