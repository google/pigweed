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

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_async2/dispatcher_native.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/status.h"

namespace pw::async2 {
namespace {

constexpr char kNotificationSignal = 'c';

}  // namespace

Status Dispatcher::NativeInit() {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ == -1) {
    PW_LOG_ERROR("Failed to open epoll: %s", std::strerror(errno));
    return Status::Internal();
  }

  int pipefd[2];
  if (pipe2(pipefd, O_DIRECT) == -1) {
    PW_LOG_ERROR("Failed to create pipe: %s", std::strerror(errno));
    return Status::Internal();
  }

  wait_fd_ = pipefd[0];
  notify_fd_ = pipefd[1];

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = wait_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wait_fd_, &event) == -1) {
    PW_LOG_ERROR("Failed to initialize epoll event for dispatcher");
    return Status::Internal();
  }

  return OkStatus();
}

Poll<> Dispatcher::DoRunUntilStalled(Task* task) {
  {
    std::lock_guard lock(dispatcher_lock());
    PW_CHECK(task == nullptr || HasPostedTask(*task),
             "Attempted to run a dispatcher until a task was stalled, "
             "but that task has not been `Post`ed to that `Dispatcher`.");
  }
  while (true) {
    RunOneTaskResult result = RunOneTask(task);
    if (result.completed_main_task() || result.completed_all_tasks()) {
      return Ready();
    }
    if (!result.ran_a_task()) {
      return Pending();
    }
  }
}

void Dispatcher::DoRunToCompletion(Task* task) {
  {
    std::lock_guard lock(dispatcher_lock());
    PW_CHECK(task == nullptr || HasPostedTask(*task),
             "Attempted to run a dispatcher until a task was complete, "
             "but that task has not been `Post`ed to that `Dispatcher`.");
  }
  while (true) {
    RunOneTaskResult result = RunOneTask(task);
    if (result.completed_main_task() || result.completed_all_tasks()) {
      return;
    }
    if (!result.ran_a_task()) {
      SleepInfo sleep_info = AttemptRequestWake();
      if (sleep_info.should_sleep()) {
        NativeWaitForWake();
      }
    }
  }
}

void Dispatcher::NativeWaitForWake() {
  struct epoll_event event;

  int num_fds = epoll_wait(epoll_fd_, &event, 1, /*timeout=*/-1);

  PW_CHECK_INT_EQ(num_fds, 1);

  // Consume the written notification.
  char unused;
  ssize_t bytes_read = read(wait_fd_, &unused, 1);
  PW_CHECK_INT_EQ(bytes_read, 1, "Dispatcher failed to read wake notification");
  PW_DCHECK_INT_EQ(unused, kNotificationSignal);
}

void Dispatcher::DoWake() {
  // Perform a write to unblock the waiting dispatcher.
  ssize_t bytes_written = write(notify_fd_, &kNotificationSignal, 1);
  PW_CHECK_INT_EQ(
      bytes_written, 1, "Dispatcher failed to write wake notification");
}

}  // namespace pw::async2
