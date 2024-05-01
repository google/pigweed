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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <utility>

#include "pw_status/status.h"

namespace pw::digital_io::internal {

// An "owned" file descriptor wrapper which closes the fd on destruction.
// TODO(b/328262654): Move this to out a better place.
class OwnedFd final {
 public:
  explicit OwnedFd(int fd) : fd_(fd) {}
  explicit OwnedFd() : OwnedFd(kInvalid) {}

  ~OwnedFd() { Close(); }

  // Delete copy constructor/assignment to prevent double close.
  OwnedFd(const OwnedFd&) = delete;
  OwnedFd& operator=(const OwnedFd&) = delete;

  // Providing move constructor is required due to custom dtor.
  OwnedFd(OwnedFd&& other) noexcept : fd_(std::exchange(other.fd_, kInvalid)) {}

  OwnedFd& operator=(OwnedFd&& other) noexcept {
    Close();
    fd_ = std::exchange(other.fd_, kInvalid);
    return *this;
  }

  OwnedFd& operator=(int fd) noexcept {
    Close();
    fd_ = fd;
    return *this;
  }

  void Close() {
    if (fd_ != kInvalid) {
      close(fd_);
    }
    fd_ = kInvalid;
  }

  int fd() const { return fd_; }
  bool valid() const { return fd_ != kInvalid; }

  // Helper functions
  template <typename... Args>
  int ioctl(Args&&... args) {
    return ::ioctl(fd_, std::forward<Args>(args)...);
  }

  ssize_t read(void* buf, size_t nbytes) { return ::read(fd_, buf, nbytes); }

  Status SetBlocking(bool blocking) {
    int flags = ::fcntl(fd_, F_GETFL);
    if (flags < 0) {
      return Status::Internal();
    }

    if (blocking) {
      flags &= ~O_NONBLOCK;
    } else {
      flags |= O_NONBLOCK;
    }

    int rc = ::fcntl(fd_, F_SETFL, flags);
    if (rc < 0) {
      return Status::Internal();
    }

    return OkStatus();
  }

 private:
  static constexpr int kInvalid = -1;
  int fd_ = kInvalid;
};

}  // namespace pw::digital_io::internal
