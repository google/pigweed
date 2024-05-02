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

#include "pw_digital_io_linux/digital_io.h"

#include <fcntl.h>
#include <linux/gpio.h>

#include "pw_digital_io/digital_io.h"
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
    return Status::Internal();
  }
  if (req.fd < 0) {
    return Status::Internal();
  }
  return OwnedFd(req.fd);
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
    return Status::Internal();
  }

  return OkStatus();
}

}  // namespace pw::digital_io
