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

#include "pw_chrono/system_clock.h"

namespace pw::grpc {

namespace {

constexpr auto kSendTimeout =
    chrono::SystemClock::for_at_least(std::chrono::seconds(1));

}

std::optional<std::reference_wrapper<SendQueue::SendRequest>>
SendQueue::NextSendRequest() {
  std::lock_guard lock(send_mutex_);
  if (send_requests_.empty()) {
    return std::nullopt;
  }
  auto& front = send_requests_.front();
  send_requests_.pop_front();
  return front;
}

void SendQueue::ProcessSendQueue(async::Context&, Status status) {
  if (!status.ok()) {
    return;
  }

  auto request = NextSendRequest();
  while (request.has_value()) {
    for (auto message : request->get().messages) {
      request->get().status.Update(socket_.Write(message));
    }
    request->get().notify.release();
    request = NextSendRequest();
  }
}

void SendQueue::QueueSendRequest(SendRequest& request) {
  std::lock_guard lock(send_mutex_);
  send_requests_.push_back(request);
  send_dispatcher_.Cancel(send_task_);
  send_dispatcher_.Post(send_task_);
}

void SendQueue::CancelSendRequest(SendRequest& request) {
  std::lock_guard lock(send_mutex_);
  send_requests_.remove(request);
}

Status SendQueue::SendBytes(ConstByteSpan message) {
  std::array<ConstByteSpan, 1> messages = {message};
  return SendBytesVector(messages);
}

Status SendQueue::SendBytesVector(span<ConstByteSpan> messages) {
  SendRequest request(messages);
  QueueSendRequest(request);
  if (!request.notify.try_acquire_for(kSendTimeout)) {
    CancelSendRequest(request);
    return Status::DeadlineExceeded();
  }

  return request.status;
}

}  // namespace pw::grpc
