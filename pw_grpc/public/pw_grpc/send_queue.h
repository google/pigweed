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
#pragma once

#include <optional>

#include "pw_async/dispatcher.h"
#include "pw_async_basic/dispatcher.h"
#include "pw_bytes/span.h"
#include "pw_containers/intrusive_list.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/thread_core.h"

namespace pw::grpc {

// SendQueue is a queue+thread that serializes sending lists of bytes to
// a stream.
class SendQueue : public thread::ThreadCore {
 public:
  SendQueue(stream::ReaderWriter& socket)
      : socket_(socket),
        send_task_(pw::bind_member<&SendQueue::ProcessSendQueue>(this)) {}

  // Thread safe. Blocks till send is complete. Returns Status from stream
  // write.
  Status SendBytes(ConstByteSpan message) PW_LOCKS_EXCLUDED(send_mutex_);

  // Thread safe. Blocks till send is complete. All messages are sent
  // atomically. Returns union of Status's from stream writes.
  Status SendBytesVector(span<ConstByteSpan> messages)
      PW_LOCKS_EXCLUDED(send_mutex_);

  // ThreadCore impl.
  void Run() override { send_dispatcher_.Run(); }
  // Call before attempting to join thread.
  void RequestStop() { send_dispatcher_.RequestStop(); }

 private:
  struct SendRequest : public IntrusiveList<SendRequest>::Item {
    SendRequest(span<ConstByteSpan> m) : messages(m) {}
    sync::TimedThreadNotification notify;
    Status status = OkStatus();
    span<ConstByteSpan> messages;
  };

  std::optional<std::reference_wrapper<SendRequest>> NextSendRequest()
      PW_LOCKS_EXCLUDED(send_mutex_);
  void QueueSendRequest(SendRequest& request) PW_LOCKS_EXCLUDED(send_mutex_);
  void CancelSendRequest(SendRequest& request) PW_LOCKS_EXCLUDED(send_mutex_);
  void ProcessSendQueue(async::Context& context, Status status)
      PW_LOCKS_EXCLUDED(send_mutex_);

  stream::ReaderWriter& socket_;
  async::BasicDispatcher send_dispatcher_;
  async::Task send_task_;
  sync::Mutex send_mutex_;
  IntrusiveList<SendRequest> send_requests_ PW_GUARDED_BY(send_mutex_);
};

}  // namespace pw::grpc
