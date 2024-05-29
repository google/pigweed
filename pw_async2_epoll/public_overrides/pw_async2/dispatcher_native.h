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

#include <vector>

#include "pw_assert/assert.h"
#include "pw_async2/dispatcher_base.h"

namespace pw::async2 {

class Dispatcher final : public DispatcherImpl<Dispatcher> {
 public:
  Dispatcher() { PW_ASSERT_OK(NativeInit()); }
  Dispatcher(Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  Dispatcher& operator=(Dispatcher&) = delete;
  Dispatcher& operator=(Dispatcher&&) = delete;
  ~Dispatcher() final { Deregister(); }

  Status NativeInit();

  enum FileDescriptorType {
    kReadable = 1 << 0,
    kWritable = 1 << 1,
    kReadWrite = kReadable | kWritable,
  };

  Status NativeRegisterFileDescriptor(int fd, FileDescriptorType type);
  Status NativeUnregisterFileDescriptor(int fd);

  void NativeAddReadWakerForFileDescriptor(int fd, Waker&& waker) {
    NativeAddWakerForFileDescriptor(
        fd, FileDescriptorType::kReadable, std::move(waker));
  }

  void NativeAddWriteWakerForFileDescriptor(int fd, Waker&& waker) {
    NativeAddWakerForFileDescriptor(
        fd, FileDescriptorType::kWritable, std::move(waker));
  }

 private:
  static constexpr size_t kMaxEventsToProcessAtOnce = 5;

  struct FdWaker {
    int fd;
    FileDescriptorType type;
    Waker waker;
  };

  void DoWake() final;
  Poll<> DoRunUntilStalled(Task* task);
  void DoRunToCompletion(Task* task);
  friend class DispatcherImpl<Dispatcher>;

  Status NativeWaitForWake();
  void NativeFindAndWakeFileDescriptor(int fd, FileDescriptorType type);

  void NativeAddWakerForFileDescriptor(int fd,
                                       FileDescriptorType type,
                                       Waker&& waker) {
    fd_wakers_.push_back({fd, type, std::move(waker)});
  }

  int epoll_fd_;
  int notify_fd_;
  int wait_fd_;

  std::vector<FdWaker> fd_wakers_;
};

}  // namespace pw::async2
