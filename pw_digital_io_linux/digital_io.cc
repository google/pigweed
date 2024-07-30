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
#define PW_LOG_MODULE_NAME "pw_digital_io_linux"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_digital_io_linux/digital_io.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>

#include <memory>

#include "log_errno.h"
#include "pw_digital_io/digital_io.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

// NOTE: Currently only the v1 userspace ABI is supported.
// TODO: https://pwbug.dev/328268007 - Add support for v2.

namespace pw::digital_io {
namespace {

using internal::OwnedFd;

Result<State> FdGetState(OwnedFd& fd) {
  struct gpiohandle_data req = {};

  if (fd.ioctl(GPIOHANDLE_GET_LINE_VALUES_IOCTL, &req) < 0) {
    LOG_ERROR_WITH_ERRNO("GPIOHANDLE_GET_LINE_VALUES_IOCTL failed:", errno);
    return Status::Internal();
  }

  return req.values[0] ? State::kActive : State::kInactive;
}

}  // namespace

// TODO(jrreinhart): Support other flags, e.g.:
// GPIOHANDLE_REQUEST_OPEN_DRAIN,
// GPIOHANDLE_REQUEST_OPEN_SOURCE,
// GPIOHANDLE_REQUEST_BIAS_PULL_UP,
// GPIOHANDLE_REQUEST_BIAS_PULL_DOWN,
// GPIOHANDLE_REQUEST_BIAS_DISABLE,

uint32_t LinuxConfig::GetFlags() const {
  uint32_t flags = 0;

  switch (polarity) {
    case Polarity::kActiveHigh:
      break;
    case Polarity::kActiveLow:
      flags |= GPIOHANDLE_REQUEST_ACTIVE_LOW;
      break;
  }

  return flags;
}

uint32_t LinuxInputConfig::GetFlags() const {
  uint32_t flags = LinuxConfig::GetFlags();
  flags |= GPIOHANDLE_REQUEST_INPUT;
  return flags;
}

uint32_t LinuxOutputConfig::GetFlags() const {
  uint32_t flags = LinuxConfig::GetFlags();
  flags |= GPIOHANDLE_REQUEST_OUTPUT;
  return flags;
}

//
// LinuxDigitalIoChip
//

Result<LinuxDigitalIoChip> LinuxDigitalIoChip::Open(const char* path) {
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    // TODO(jrreinhart): Map errno to Status
    LOG_ERROR_WITH_ERRNO("Could not open %s:", errno, path);
    return Status::Internal();
  }
  return LinuxDigitalIoChip(fd);
}

Result<OwnedFd> LinuxDigitalIoChip::Impl::GetLineHandle(uint32_t offset,
                                                        uint32_t flags,
                                                        uint8_t default_value) {
  struct gpiohandle_request req = {
      .lineoffsets = {offset},
      .flags = flags,
      .default_values = {default_value},
      .consumer_label = "pw_digital_io_linux",
      .lines = 1,
      .fd = -1,
  };
  if (fd_.ioctl(GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
    LOG_ERROR_WITH_ERRNO("GPIO_GET_LINEHANDLE_IOCTL failed:", errno);
    return Status::Internal();
  }
  if (req.fd < 0) {
    return Status::Internal();
  }
  return OwnedFd(req.fd);
}

Result<OwnedFd> LinuxDigitalIoChip::Impl::GetLineEventHandle(
    uint32_t offset, uint32_t handle_flags, uint32_t event_flags) {
  struct gpioevent_request req = {
      .lineoffset = offset,
      .handleflags = handle_flags,
      .eventflags = event_flags,
      .consumer_label = "pw_digital_io_linux",
      .fd = -1,
  };
  if (fd_.ioctl(GPIO_GET_LINEEVENT_IOCTL, &req) < 0) {
    LOG_ERROR_WITH_ERRNO("GPIO_GET_LINEEVENT_IOCTL failed:", errno);
    return Status::Internal();
  }
  if (req.fd < 0) {
    return Status::Internal();
  }
  return OwnedFd(req.fd);
}

Result<LinuxDigitalInInterrupt> LinuxDigitalIoChip::GetInterruptLine(
    const LinuxInputConfig& config,
    std::shared_ptr<LinuxGpioNotifier> notifier) {
  if (!impl_) {
    return Status::FailedPrecondition();
  }
  return LinuxDigitalInInterrupt(impl_, config, notifier);
}

Result<LinuxDigitalIn> LinuxDigitalIoChip::GetInputLine(
    const LinuxInputConfig& config) {
  if (!impl_) {
    return Status::FailedPrecondition();
  }
  return LinuxDigitalIn(impl_, config);
}

Result<LinuxDigitalOut> LinuxDigitalIoChip::GetOutputLine(
    const LinuxOutputConfig& config) {
  if (!impl_) {
    return Status::FailedPrecondition();
  }
  return LinuxDigitalOut(impl_, config);
}

//
// LinuxDigitalInInterrupt
//

LinuxDigitalInInterrupt::Impl::~Impl() {
  std::lock_guard lock(mutex_);

  // Explicitly close in order to unregister.
  CloseHandle();
}

uint32_t LinuxDigitalInInterrupt::Impl::GetEventFlags() const {
  if (handler_ == nullptr) {
    return 0;
  }
  switch (trigger_) {
    case InterruptTrigger::kActivatingEdge:
      return (config_.polarity == Polarity::kActiveHigh)
                 ? GPIOEVENT_REQUEST_RISING_EDGE
                 : GPIOEVENT_REQUEST_FALLING_EDGE;
    case InterruptTrigger::kDeactivatingEdge:
      return (config_.polarity == Polarity::kActiveHigh)
                 ? GPIOEVENT_REQUEST_FALLING_EDGE
                 : GPIOEVENT_REQUEST_RISING_EDGE;
    case InterruptTrigger::kBothEdges:
      return GPIOEVENT_REQUEST_BOTH_EDGES;
  }
}

Status LinuxDigitalInInterrupt::Impl::SubscribeEvents() {
  PW_CHECK(fd_is_event_handle_);

  // NOTE: Passing a normal reference is a little risky, especially since the
  // notifier doesn't even save it; it puts it in the kernel epoll object. To
  // make this safe, we unsubscribe from events in the destructor.
  return notifier_->RegisterLine(fd_.fd(), *this);
}

Status LinuxDigitalInInterrupt::Impl::UnsubscribeEvents() {
  PW_CHECK(fd_is_event_handle_);
  return notifier_->UnregisterLine(fd_.fd());
}

void LinuxDigitalInInterrupt::Impl::CloseHandle() {
  if (!enabled()) {
    return;
  }

  if (fd_is_event_handle_) {
    if (auto status = UnsubscribeEvents(); !status.ok()) {
      PW_LOG_WARN("Failed to unsubscribe events: %s", status.str());
    }
  }

  // Close the open file handle and release the line request.
  fd_.Close();
}

Status LinuxDigitalInInterrupt::Impl::OpenHandle() {
  if (enabled() && interrupts_desired_ == fd_is_event_handle_) {
    // Already enabled with the right file type. Nothing to do.
    return OkStatus();
  }

  // Close the file if it is already open, so it can be re-requested.
  CloseHandle();

  if (interrupts_desired_) {
    // Open a lineevent handle; lineevent_create enables IRQs.
    PW_LOG_INFO("Interrupts desired; Opening a line event handle");
    PW_TRY_ASSIGN(fd_,
                  chip_->GetLineEventHandle(
                      config_.index, config_.GetFlags(), GetEventFlags()));
    fd_is_event_handle_ = true;

    if (auto status = SubscribeEvents(); !status.ok()) {
      // Don't use CloseHandle since that will attempt to unsubscribe.
      fd_.Close();
      return status;
    }
  } else {
    // Open a regular linehandle
    PW_LOG_INFO("Interrupts not desired; Opening a normal line handle");
    PW_TRY_ASSIGN(fd_, chip_->GetLineHandle(config_.index, config_.GetFlags()));
    fd_is_event_handle_ = false;
  }

  return OkStatus();
}

Status LinuxDigitalInInterrupt::Impl::DoEnable(bool enable) {
  std::lock_guard lock(mutex_);
  if (enable) {
    return OpenHandle();
  } else {
    CloseHandle();
  }
  return OkStatus();
}

// Backend-specific note:
// Unlike baremetal implementations, this operation is expensive.
Status LinuxDigitalInInterrupt::Impl::DoEnableInterruptHandler(bool enable) {
  std::lock_guard lock(mutex_);

  if (enable && !handler_) {
    // PRE: When enabling, a handler must have been set using
    // DoSetInterruptHandler().
    return Status::FailedPrecondition();
  }

  // Because this is expensive, we explicitly support enabling the interrupt
  // handler before enabling the line.
  interrupts_desired_ = enable;
  if (enabled()) {
    // Line is currently enabled (handle open). Re-open if necessary.
    return OpenHandle();
  } else {
    // Proper handle will be opened on next DoEnable().
    return OkStatus();
  }
}

Status LinuxDigitalInInterrupt::Impl::DoSetInterruptHandler(
    InterruptTrigger trigger, InterruptHandler&& handler) {
  std::lock_guard lock(mutex_);

  // Enforce interface preconditions.
  if (handler && handler_) {
    // PRE: If setting a handler, no handler is permitted to be currently set.
    return Status::FailedPrecondition();
  }
  if (!handler && interrupts_enabled()) {
    // PRE: When cleaing a handler, the interrupt handler must be disabled.
    return Status::FailedPrecondition();
  }

  ++handler_generation_;
  handler_ = std::move(handler);
  trigger_ = trigger;
  return OkStatus();
}

void LinuxDigitalInInterrupt::Impl::HandleEvents() {
  InterruptHandler saved_handler{};
  uint32_t current_handler_generation{};
  State state{};

  {
    std::lock_guard lock(mutex_);

    // Possible race condition: We could receive a latent event after events
    // were disabled.
    if (!interrupts_enabled()) {
      return;
    }

    // Consume the event from the event handle.
    errno = 0;
    struct gpioevent_data event;
    ssize_t nread = fd_.read(&event, sizeof(event));
    if (nread < static_cast<ssize_t>(sizeof(event))) {
      LOG_ERROR_WITH_ERRNO("Failed to read from line event handle:", errno);
      return;
    }

    PW_LOG_DEBUG("Got GPIO event: timestamp=%llu, id=%s",
                 static_cast<unsigned long long>(event.timestamp),
                 event.id == GPIOEVENT_EVENT_RISING_EDGE    ? "RISING_EDGE"
                 : event.id == GPIOEVENT_EVENT_FALLING_EDGE ? "FALLING_EDGE"
                                                            : "<unknown>");

    // Note that polarity (ACTIVE_LOW) is already accounted for
    // by the kernel; see gpiod_get_value_cansleep().
    switch (event.id) {
      case GPIOEVENT_EVENT_RISING_EDGE:
        // "RISING_EDGE" always means inactive -> active.
        state = State::kActive;
        break;
      case GPIOEVENT_EVENT_FALLING_EDGE:
        // "FALLING_EDGE" always means active -> inactive.
        state = State::kInactive;
        break;
      default:
        PW_LOG_ERROR("Unexpected event.id = %u", event.id);
        return;
    }

    // Borrow the handler while we handle the interrupt, so we can invoked it
    // without holding the mutex. Do this only after all calls that could fail.
    std::swap(saved_handler, handler_);
    current_handler_generation = handler_generation_;
  }

  // Invoke the handler without holding the mutex.
  if (saved_handler) {
    saved_handler(state);
  }

  // Restore the saved handler.
  {
    std::lock_guard lock(mutex_);

    // While we had the mutex released, a consumer could have called
    // DoEnable, DoEnableInterruptHandler, or DoSetInterruptHandler.
    // Only restore the saved handler if the consumer didn't call
    // DoSetInterruptHandler.
    if (handler_generation_ == current_handler_generation) {
      std::swap(handler_, saved_handler);
    }
  }
}

Result<State> LinuxDigitalInInterrupt::Impl::DoGetState() {
  return FdGetState(fd_);
}

//
// LinuxDigitalIn
//

Status LinuxDigitalIn::DoEnable(bool enable) {
  if (enable) {
    if (enabled()) {
      return OkStatus();
    }
    PW_TRY_ASSIGN(fd_, chip_->GetLineHandle(config_.index, config_.GetFlags()));
  } else {
    // Close the open file handle and release the line request.
    fd_.Close();
  }
  return OkStatus();
}

Result<State> LinuxDigitalIn::DoGetState() { return FdGetState(fd_); }

//
// LinuxDigitalOut
//

Status LinuxDigitalOut::DoEnable(bool enable) {
  if (enable) {
    if (enabled()) {
      return OkStatus();
    }
    uint8_t default_value = config_.default_state == State::kActive;
    PW_TRY_ASSIGN(
        fd_,
        chip_->GetLineHandle(config_.index, config_.GetFlags(), default_value));
  } else {
    // Close the open file handle and release the line request.
    fd_.Close();
  }
  return OkStatus();
}

Result<State> LinuxDigitalOut::DoGetState() { return FdGetState(fd_); }

Status LinuxDigitalOut::DoSetState(State level) {
  struct gpiohandle_data req = {
      .values = {(level == State::kActive) ? uint8_t(1) : uint8_t(0)},
  };

  if (fd_.ioctl(GPIOHANDLE_SET_LINE_VALUES_IOCTL, &req) < 0) {
    LOG_ERROR_WITH_ERRNO("GPIOHANDLE_SET_LINE_VALUES_IOCTL failed:", errno);
    return Status::Internal();
  }

  return OkStatus();
}

}  // namespace pw::digital_io
