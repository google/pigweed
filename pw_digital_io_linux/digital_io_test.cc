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
#include "pw_digital_io/digital_io.h"

#include <linux/gpio.h>

#include <algorithm>
#include <cstdarg>
#include <functional>

#include "pw_digital_io_linux/digital_io.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

// close() and ioctl() are wrapped via --wrap
extern "C" {
decltype(close) __real_close;
decltype(close) __wrap_close;

// ioctl() is actually variadic (third arg is ...), but there's no way
// (https://c-faq.com/varargs/handoff.html) to forward the args when invoked
// that way, so we lie and use void*.
int __real_ioctl(int fd, unsigned long request, void* arg);
int __wrap_ioctl(int fd, unsigned long request, void* arg);
}

namespace pw::digital_io {
namespace {

class DigitalIoTest : public ::testing::Test {
 protected:
  DigitalIoTest() : chip(kChipFd), chip_fd_open_(true) {}

  void SetUp() override {
    ASSERT_EQ(s_current, nullptr);
    s_current = this;
  }

  void TearDown() override { s_current = nullptr; }

  LinuxDigitalIoChip chip;

 private:
  static inline DigitalIoTest* s_current;

  static constexpr int kChipFd = 8999;
  bool chip_fd_open_ = false;

  // Line 0: Input
  static constexpr int kLine0InputFd = 9000;
  uint8_t line0_value_ = 0;
  bool line0_fd_open_ = false;
  bool line0_active_low_ = false;

  // Line 1: Output
  static constexpr int kLine1OutputFd = 9001;
  uint8_t line1_value_ = 0;
  bool line1_fd_open_ = false;
  bool line1_active_low_ = false;

  int DoChipLinehandleIoctl(struct gpiohandle_request* req) {
    uint32_t const direction =
        req->flags & (GPIOHANDLE_REQUEST_OUTPUT | GPIOHANDLE_REQUEST_INPUT);

    // Validate flags.
    if (direction == (GPIOHANDLE_REQUEST_OUTPUT | GPIOHANDLE_REQUEST_INPUT)) {
      PW_LOG_ERROR("%s: OUTPUT and INPUT are mutually exclusive", __FUNCTION__);
      return -1;
    }

    // Only support requesting one line at at time.
    if (req->lines != 1) {
      PW_LOG_ERROR("%s: Unsupported req->lines=%u", __FUNCTION__, req->lines);
      return -1;
    }

    uint32_t const offset = req->lineoffsets[0];
    uint8_t const default_value = req->default_values[0];
    bool const active_low = req->flags & GPIOHANDLE_REQUEST_ACTIVE_LOW;

    switch (offset) {
      case 0:
        if (direction != GPIOHANDLE_REQUEST_INPUT) {
          PW_LOG_ERROR("%s: Line 0 is input-only", __FUNCTION__);
          return -1;
        }
        req->fd = kLine0InputFd;
        line0_fd_open_ = true;
        line0_active_low_ = active_low;
        return 0;
      case 1:
        if (direction != GPIOHANDLE_REQUEST_OUTPUT) {
          PW_LOG_ERROR("%s: Line 1 is output-only", __FUNCTION__);
          return -1;
        }
        req->fd = kLine1OutputFd;
        line1_fd_open_ = true;
        line1_active_low_ = active_low;
        line1_value_ = default_value ^ active_low;
        return 0;
      default:
        PW_LOG_ERROR("%s: Line %u not supported", __FUNCTION__, offset);
        return -1;
    }
  }

  int DoChipIoctl(unsigned long request, void* arg) {
    switch (request) {
      case GPIO_GET_LINEHANDLE_IOCTL:
        return DoChipLinehandleIoctl(
            static_cast<struct gpiohandle_request*>(arg));
      default:
        PW_LOG_ERROR("%s: Unhandled request=0x%lX", __FUNCTION__, request);
        return -1;
    }
  }

  int DoLinehandleGetValues(int fd, struct gpiohandle_data* data) {
    uint8_t value;
    switch (fd) {
      case kLine0InputFd:
        value = line0_value_ ^ line0_active_low_;
        data->values[0] = value;
        PW_LOG_DEBUG("Got line 0 as %u", value);
        return 0;
      case kLine1OutputFd:
        value = line1_value_ ^ line1_active_low_;
        data->values[0] = value;
        PW_LOG_DEBUG("Got line 1 as %u", value);
        return 0;
      default:
        PW_LOG_ERROR("%s: Incorrect fd=%d", __FUNCTION__, fd);
        return -1;
    }
  }

  int DoLinehandleSetValues(int fd, struct gpiohandle_data* data) {
    if (fd != kLine1OutputFd) {
      PW_LOG_ERROR("%s: Incorrect fd=%d", __FUNCTION__, fd);
      return -1;
    }
    line1_value_ = data->values[0] ^ line1_active_low_;
    PW_LOG_DEBUG("Set line 1 to %u", line1_value_);
    return 0;
  }

  int DoLinehandleIoctl(int fd, unsigned long request, void* arg) {
    switch (request) {
      case GPIOHANDLE_GET_LINE_VALUES_IOCTL:
        return DoLinehandleGetValues(fd,
                                     static_cast<struct gpiohandle_data*>(arg));
      case GPIOHANDLE_SET_LINE_VALUES_IOCTL:
        return DoLinehandleSetValues(fd,
                                     static_cast<struct gpiohandle_data*>(arg));
      default:
        PW_LOG_ERROR("%s: Unhandled request=0x%lX", __FUNCTION__, request);
        return -1;
    }
  }

  int DoIoctl(int fd, unsigned long request, void* arg) {
    PW_LOG_DEBUG(
        "%s: fd=%d, request=0x%lX, arg=%p", __FUNCTION__, fd, request, arg);
    if (fd == kChipFd) {
      return DoChipIoctl(request, arg);
    }
    if (fd == kLine0InputFd || fd == kLine1OutputFd) {
      return DoLinehandleIoctl(fd, request, arg);
    }
    return __real_ioctl(fd, request, arg);
  }

  int DoClose(int fd) {
    PW_LOG_DEBUG("%s: fd=%d", __FUNCTION__, fd);
    switch (fd) {
      case kChipFd:
        EXPECT_TRUE(chip_fd_open_);
        chip_fd_open_ = false;
        return 0;
      case kLine0InputFd:
        EXPECT_TRUE(line0_fd_open_);
        line0_fd_open_ = false;
        return 0;
      case kLine1OutputFd:
        EXPECT_TRUE(line1_fd_open_);
        line1_fd_open_ = false;
        return 0;
    }
    return __real_close(fd);
  }

 public:
  static int HandleIoctl(int fd, unsigned long request, void* arg) {
    if (s_current) {
      return s_current->DoIoctl(fd, request, arg);
    }
    return __real_ioctl(fd, request, arg);
  }

  static int HandleClose(int fd) {
    if (s_current) {
      return s_current->DoClose(fd);
    }
    return __real_close(fd);
  }

  void SetLine0State(bool state) { line0_value_ = state; }

  bool GetLine1State() { return line1_value_; }

  void ExpectAllFdsClosed() {
    EXPECT_FALSE(chip_fd_open_);
    EXPECT_FALSE(line0_fd_open_);
    EXPECT_FALSE(line1_fd_open_);
  }
};

extern "C" int __wrap_close(int fd) { return DigitalIoTest::HandleClose(fd); }

extern "C" int __wrap_ioctl(int fd, unsigned long request, void* arg) {
  return DigitalIoTest::HandleIoctl(fd, request, arg);
}

//
// Tests
//

TEST_F(DigitalIoTest, DoInput) {
  LinuxInputConfig config(
      /* index= */ 0,
      /* polarity= */ Polarity::kActiveHigh);

  auto input_result = chip.GetInputLine(config);
  ASSERT_EQ(OkStatus(), input_result.status());
  {
    auto input = std::move(input_result.value());

    ASSERT_EQ(OkStatus(), input.Enable());

    Result<State> state;

    SetLine0State(true);
    state = input.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kActive, state.value());

    SetLine0State(false);
    state = input.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kInactive, state.value());

    ASSERT_EQ(OkStatus(), input.Disable());
  }
  chip.Close();
  ExpectAllFdsClosed();
}

TEST_F(DigitalIoTest, DoInputInvert) {
  LinuxInputConfig config(
      /* index= */ 0,
      /* polarity= */ Polarity::kActiveLow);

  auto input_result = chip.GetInputLine(config);
  ASSERT_EQ(OkStatus(), input_result.status());
  {
    auto input = std::move(input_result.value());

    ASSERT_EQ(OkStatus(), input.Enable());

    Result<State> state;

    SetLine0State(true);
    state = input.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kInactive, state.value());

    SetLine0State(false);
    state = input.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kActive, state.value());

    ASSERT_EQ(OkStatus(), input.Disable());
  }
  chip.Close();
  ExpectAllFdsClosed();
}

TEST_F(DigitalIoTest, DoOutput) {
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveHigh,
      /* default_state= */ State::kActive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  {
    auto output = std::move(output_result.value());

    ASSERT_EQ(OkStatus(), output.Enable());

    ASSERT_TRUE(GetLine1State());  // default

    ASSERT_EQ(OkStatus(), output.SetStateInactive());
    ASSERT_FALSE(GetLine1State());

    ASSERT_EQ(OkStatus(), output.SetStateActive());
    ASSERT_TRUE(GetLine1State());

    ASSERT_EQ(OkStatus(), output.Disable());
  }
  chip.Close();
  ExpectAllFdsClosed();
}

TEST_F(DigitalIoTest, DoOutputInvert) {
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveLow,
      /* default_state= */ State::kActive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  {
    auto output = std::move(output_result.value());

    ASSERT_EQ(OkStatus(), output.Enable());

    ASSERT_FALSE(GetLine1State());  // default

    ASSERT_EQ(OkStatus(), output.SetStateInactive());
    ASSERT_TRUE(GetLine1State());

    ASSERT_EQ(OkStatus(), output.SetStateActive());
    ASSERT_FALSE(GetLine1State());

    ASSERT_EQ(OkStatus(), output.Disable());
  }
  chip.Close();
  ExpectAllFdsClosed();
}

TEST_F(DigitalIoTest, OutputGetState) {
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveHigh,
      /* default_state= */ State::kInactive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  {
    auto output = std::move(output_result.value());

    ASSERT_EQ(OkStatus(), output.Enable());
    ASSERT_FALSE(GetLine1State());  // default

    auto state = output.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kInactive, state.value());

    // Now set state active and get it again.
    ASSERT_EQ(OkStatus(), output.SetStateActive());
    ASSERT_TRUE(GetLine1State());

    state = output.GetState();
    ASSERT_EQ(OkStatus(), state.status());
    ASSERT_EQ(State::kActive, state.value());
  }
  chip.Close();
  ExpectAllFdsClosed();
}

}  // namespace
}  // namespace pw::digital_io
