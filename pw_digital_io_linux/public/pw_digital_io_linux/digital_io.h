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

#include <cstdint>
#include <memory>

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io/polarity.h"
#include "pw_digital_io_linux/internal/owned_fd.h"
#include "pw_digital_io_linux/notifier.h"
#include "pw_result/result.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::digital_io {

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

class LinuxDigitalInInterrupt;
class LinuxDigitalIn;
class LinuxDigitalOut;

/// Represents an open handle to a Linux GPIO chip (e.g. /dev/gpiochip0).
class LinuxDigitalIoChip final {
  friend class LinuxDigitalInInterrupt;
  friend class LinuxDigitalIn;
  friend class LinuxDigitalOut;
  using OwnedFd = internal::OwnedFd;

 private:
  // Implementation
  class Impl {
   public:
    Impl(int fd) : fd_(fd) {}

    Result<OwnedFd> GetLineHandle(uint32_t offset,
                                  uint32_t flags,
                                  uint8_t default_value = 0);

    Result<OwnedFd> GetLineEventHandle(uint32_t offset,
                                       uint32_t handle_flags,
                                       uint32_t event_flags);

   private:
    OwnedFd fd_;
  };

  std::shared_ptr<Impl> impl_;

 public:
  explicit LinuxDigitalIoChip(int fd) : impl_(std::make_shared<Impl>(fd)) {}

  static Result<LinuxDigitalIoChip> Open(const char* path);

  void Close() { impl_ = nullptr; }

  Result<LinuxDigitalInInterrupt> GetInterruptLine(
      const LinuxInputConfig& config,
      std::shared_ptr<LinuxGpioNotifier> notifier);

  Result<LinuxDigitalIn> GetInputLine(const LinuxInputConfig& config);

  Result<LinuxDigitalOut> GetOutputLine(const LinuxOutputConfig& config);
};

class LinuxDigitalInInterrupt final : public DigitalInInterrupt {
  friend class LinuxDigitalIoChip;

 private:
  explicit LinuxDigitalInInterrupt(
      std::shared_ptr<LinuxDigitalIoChip::Impl> chip,
      const LinuxInputConfig& config,
      std::shared_ptr<LinuxGpioNotifier> notifier)
      : impl_(std::make_shared<Impl>(chip, config, notifier)) {}

  // DigitalInInterrupt impl.
  // These simply forward to the private impl class.
  Status DoEnable(bool enable) override { return impl_->DoEnable(enable); }

  Result<State> DoGetState() override { return impl_->DoGetState(); }

  Status DoSetInterruptHandler(InterruptTrigger trigger,
                               InterruptHandler&& handler) override {
    return impl_->DoSetInterruptHandler(trigger, std::move(handler));
  }

  Status DoEnableInterruptHandler(bool enable) override {
    return impl_->DoEnableInterruptHandler(enable);
  }

  // Internal impl for shared_ptr
  // As a nested class, this doesn't really need to implement
  // DigitalInInterrupt, but it makes things clearer and easy to forward calls
  // from the containing class.
  class Impl final : public DigitalInInterrupt,
                     public LinuxGpioNotifier::Handler {
   public:
    explicit Impl(std::shared_ptr<LinuxDigitalIoChip::Impl> chip,
                  const LinuxInputConfig& config,
                  std::shared_ptr<LinuxGpioNotifier> notifier)
        : chip_(std::move(chip)),
          config_(config),
          notifier_(std::move(notifier)) {}

    ~Impl() override;

    // The notifier holds a reference to this object, so disable
    // the ability to copy or move it.
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    // DigitalInInterrupt impl.
    Status DoEnable(bool enable) override;
    Result<State> DoGetState() override;
    Status DoSetInterruptHandler(InterruptTrigger trigger,
                                 InterruptHandler&& handler) override;
    Status DoEnableInterruptHandler(bool enable) override;

   private:
    // The parent chip object.
    const std::shared_ptr<LinuxDigitalIoChip::Impl> chip_;

    // The desired configuration of this line.
    LinuxInputConfig const config_;

    // The notifier is inherently thread-safe.
    const std::shared_ptr<LinuxGpioNotifier> notifier_;

    // Line handle or line event fd, depending on fd_is_event_handle_.
    internal::OwnedFd fd_;

    // The type of file currently in fd_:
    //   true: fd_ is a "lineevent" file (from GPIO_GET_LINEEVENT_IOCTL).
    //   false: fd_ is a "linehandle" file (from GPIO_GET_LINEHANDLE_IOCTL).
    bool fd_is_event_handle_ = false;

    // Interrupts have been requested by user via DoEnableInterruptHandler().
    bool interrupts_desired_ = false;

    // The handler and trigger configured by DoSetInterruptHandler().
    InterruptHandler handler_ = nullptr;
    InterruptTrigger trigger_ = {};
    uint32_t handler_generation_ = 0;

    // Guards access to line state, primarily for synchronizing with
    // interrupt callbacks.
    sync::Mutex mutex_;

    //
    // Methods
    //

    // LinuxGpioNotifier::Handler impl.
    void HandleEvents() override PW_LOCKS_EXCLUDED(mutex_);

    // Private methods
    Status OpenHandle() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    void CloseHandle() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    Status SubscribeEvents() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    Status UnsubscribeEvents() PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    uint32_t GetEventFlags() const PW_SHARED_LOCKS_REQUIRED(mutex_);
    bool enabled() const PW_SHARED_LOCKS_REQUIRED(mutex_) {
      return fd_.valid();
    }
    bool interrupts_enabled() const PW_SHARED_LOCKS_REQUIRED(mutex_) {
      return enabled() && interrupts_desired_;
    }
  };  // class Impl

  const std::shared_ptr<Impl> impl_;
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
  internal::OwnedFd fd_;
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
  internal::OwnedFd fd_;
};

}  // namespace pw::digital_io
