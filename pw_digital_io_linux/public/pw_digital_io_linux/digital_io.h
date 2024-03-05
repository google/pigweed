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

#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdint>
#include <memory>

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io/polarity.h"
#include "pw_result/result.h"

namespace pw::digital_io {

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

 private:
  static constexpr int kInvalid = -1;
  int fd_ = kInvalid;
};

struct LinuxConfig {
  uint32_t index;
  Polarity polarity;

  LinuxConfig(uint32_t index, Polarity polarity)
      : index(index), polarity(polarity) {}
  uint32_t GetFlags() const;
};

struct LinuxInputConfig final : LinuxConfig {
  LinuxInputConfig(uint32_t index, Polarity polarity)
      : LinuxConfig(index, polarity) {}
  uint32_t GetFlags() const;
};

struct LinuxOutputConfig final : LinuxConfig {
  State default_state;

  LinuxOutputConfig(uint32_t index, Polarity polarity, State default_state)
      : LinuxConfig(index, polarity), default_state(default_state) {}
  uint32_t GetFlags() const;
};

class LinuxDigitalIn;
class LinuxDigitalOut;

/// Represents an open handle to a Linux GPIO chip (e.g. /dev/gpiochip0).
class LinuxDigitalIoChip final {
  friend class LinuxDigitalIn;
  friend class LinuxDigitalOut;

 private:
  // Implementation
  class Impl {
   public:
    Impl(int fd) : fd_(fd) {}

    Result<OwnedFd> GetLineHandle(uint32_t offset,
                                  uint32_t flags,
                                  uint8_t default_value = 0);

   private:
    OwnedFd fd_;
  };

  std::shared_ptr<Impl> impl_;

 public:
  explicit LinuxDigitalIoChip(int fd) : impl_(std::make_shared<Impl>(fd)) {}

  static Result<LinuxDigitalIoChip> Open(const char* path);

  void Close() { impl_ = nullptr; }

  Result<LinuxDigitalIn> GetInputLine(const LinuxInputConfig& config);

  Result<LinuxDigitalOut> GetOutputLine(const LinuxOutputConfig& config);
};

class LinuxDigitalIn final : public DigitalIn {
  friend class LinuxDigitalIoChip;

 private:
  explicit LinuxDigitalIn(std::shared_ptr<LinuxDigitalIoChip::Impl> chip,
                          const LinuxInputConfig& config)
      : chip_(std::move(chip)), config_(config) {}

  Status DoEnable(bool enable) override;
  Result<State> DoGetState() override;

  bool enabled() { return fd_.valid(); }

  std::shared_ptr<LinuxDigitalIoChip::Impl> chip_;
  LinuxInputConfig const config_;
  OwnedFd fd_;
};

class LinuxDigitalOut final : public DigitalInOut {
  friend class LinuxDigitalIoChip;

 private:
  explicit LinuxDigitalOut(std::shared_ptr<LinuxDigitalIoChip::Impl> chip,
                           const LinuxOutputConfig& config)
      : chip_(std::move(chip)), config_(config) {}

  Status DoEnable(bool enable) override;
  Result<State> DoGetState() override;
  Status DoSetState(State level) override;

  bool enabled() { return fd_.valid(); }

  std::shared_ptr<LinuxDigitalIoChip::Impl> chip_;
  LinuxOutputConfig const config_;
  OwnedFd fd_;
};

}  // namespace pw::digital_io
