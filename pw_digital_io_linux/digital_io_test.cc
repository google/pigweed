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
#include <functional>
#include <memory>
#include <vector>

#include "mock_vfs.h"
#include "pw_digital_io_linux/digital_io.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

namespace pw::digital_io {
namespace {

class DigitalIoTest;

// Represents a mocked in-kernel GPIO line object.
class Line {
 public:
  //
  // Harness-side interface: Intended for use by DigitalIoTest and the
  // MockFile subclasses.
  //

  explicit Line(uint32_t index) : index_(index) {}

  // Get the logical value of the line, respecting active_low.
  Result<bool> GetValue() const {
    // Linux lets you read the value of an output.
    if (requested_ == RequestedState::kNone) {
      PW_LOG_ERROR("Cannot get value of unrequested line");
      return Status::FailedPrecondition();
    }
    return physical_state_ ^ active_low_;
  }

  // Set the logical value of the line, respecting active_low.
  // Returns OK on success; FAILED_PRECONDITION if not requested as output.
  Status SetValue(bool value) {
    if (requested_ != RequestedState::kOutput) {
      PW_LOG_ERROR("Cannot set value of line not requested as output");
      return Status::FailedPrecondition();
    }

    physical_state_ = value ^ active_low_;

    PW_LOG_DEBUG("Set line %u to physical %u", index_, physical_state_);
    return OkStatus();
  }

  Status RequestInput(bool active_low) {
    return DoRequest(RequestedState::kInput, active_low);
  }

  Status RequestOutput(bool active_low) {
    return DoRequest(RequestedState::kOutput, active_low);
  }

  void ClearRequest() { requested_ = RequestedState::kNone; }

  //
  // Test-side interface: Intended for use by the tests themselves.
  //

  enum class RequestedState {
    kNone,    // Not requested by "userspace"
    kInput,   // Requested by "userspace" as an input
    kOutput,  // Requested by "userspace" as an output
  };

  RequestedState requested() const { return requested_; }

  void ForcePhysicalState(bool state) { physical_state_ = state; }

  bool physical_state() const { return physical_state_; }

 private:
  const uint32_t index_;
  bool physical_state_ = false;

  RequestedState requested_ = RequestedState::kNone;
  bool active_low_ = false;

  Status DoRequest(RequestedState request, bool active_low) {
    if (requested_ != RequestedState::kNone) {
      PW_LOG_ERROR("Cannot request already-requested line");
      return Status::FailedPrecondition();
    }
    requested_ = request;
    active_low_ = active_low;
    return OkStatus();
  }
};

// Represents a GPIO line handle, the result of issuing
// GPIO_GET_LINEHANDLE_IOCTL to an open chip file.
class LineHandleFile : public MockFile {
 public:
  LineHandleFile(MockVfs& vfs, const std::string& name, Line& line)
      : MockFile(vfs, name), line_(line) {}

 private:
  Line& line_;

  //
  // MockFile impl.
  //

  int DoClose() override {
    line_.ClearRequest();
    return 0;
  }

  int DoIoctl(unsigned long request, void* arg) override {
    switch (request) {
      case GPIOHANDLE_GET_LINE_VALUES_IOCTL:
        return DoIoctlGetValues(static_cast<struct gpiohandle_data*>(arg));
      case GPIOHANDLE_SET_LINE_VALUES_IOCTL:
        return DoIoctlSetValues(static_cast<struct gpiohandle_data*>(arg));
      default:
        PW_LOG_ERROR("%s: Unhandled request=0x%lX", __FUNCTION__, request);
        return -1;
    }
  }

  // Handle GPIOHANDLE_GET_LINE_VALUES_IOCTL
  int DoIoctlGetValues(struct gpiohandle_data* data) {
    auto result = line_.GetValue();
    if (!result.ok()) {
      return -1;
    }

    data->values[0] = *result;
    return 0;
  }

  // Handle GPIOHANDLE_SET_LINE_VALUES_IOCTL
  int DoIoctlSetValues(struct gpiohandle_data* data) {
    auto status = line_.SetValue(data->values[0]);
    if (!status.ok()) {
      return -1;
    }

    return 0;
  }
};

// Represents an open GPIO chip file, the result of opening /dev/gpiochip*.
class ChipFile : public MockFile {
 public:
  ChipFile(MockVfs& vfs, const std::string& name, std::vector<Line>& lines)
      : MockFile(vfs, name), lines_(lines) {}

 private:
  std::vector<Line>& lines_;

  //
  // MockFile impl.
  //

  int DoIoctl(unsigned long request, void* arg) override {
    switch (request) {
      case GPIO_GET_LINEHANDLE_IOCTL:
        return DoLinehandleIoctl(static_cast<struct gpiohandle_request*>(arg));
      default:
        PW_LOG_ERROR("%s: Unhandled request=0x%lX", __FUNCTION__, request);
        return -1;
    }
  }

  // Handle GPIO_GET_LINEHANDLE_IOCTL
  int DoLinehandleIoctl(struct gpiohandle_request* req) {
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

    if (offset >= lines_.size()) {
      PW_LOG_ERROR("%s: Invalid line offset: %u", __FUNCTION__, offset);
      return -1;
    }
    Line& line = lines_[offset];

    Status status = OkStatus();
    switch (direction) {
      case GPIOHANDLE_REQUEST_OUTPUT:
        status.Update(line.RequestOutput(active_low));
        status.Update(line.SetValue(default_value));
        break;
      case GPIOHANDLE_REQUEST_INPUT:
        status.Update(line.RequestInput(active_low));
        break;
    }
    if (!status.ok()) {
      return -1;
    }

    req->fd = vfs_.InstallNewFile<LineHandleFile>("line-handle", line);
    return 0;
  }
};

// Test fixture for all digtal io tests.
class DigitalIoTest : public ::testing::Test {
 protected:
  void SetUp() override { GetMockVfs().Reset(); }

  void TearDown() override { EXPECT_TRUE(GetMockVfs().AllFdsClosed()); }

  LinuxDigitalIoChip OpenChip() {
    int fd = GetMockVfs().InstallNewFile<ChipFile>("chip", lines_);
    return LinuxDigitalIoChip(fd);
  }

  Line& line0() { return lines_[0]; }
  Line& line1() { return lines_[1]; }

 private:
  std::vector<Line> lines_ = std::vector<Line>{
      Line(0),  // Input
      Line(1),  // Output
  };
};

//
// Tests
//

TEST_F(DigitalIoTest, DoInput) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line0();
  LinuxInputConfig config(
      /* index= */ 0,
      /* polarity= */ Polarity::kActiveHigh);

  auto input_result = chip.GetInputLine(config);
  ASSERT_EQ(OkStatus(), input_result.status());
  auto input = std::move(input_result.value());

  // Enable the input, and ensure it is requested.
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  ASSERT_EQ(OkStatus(), input.Enable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kInput);

  Result<State> state;

  // Force the line high and assert it is seen as active (active high).
  line.ForcePhysicalState(true);
  state = input.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kActive, state.value());

  // Force the line low and assert it is seen as inactive (active high).
  line.ForcePhysicalState(false);
  state = input.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Disable the line and ensure it is no longer requested.
  ASSERT_EQ(OkStatus(), input.Disable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
}

TEST_F(DigitalIoTest, DoInputInvert) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line0();
  LinuxInputConfig config(
      /* index= */ 0,
      /* polarity= */ Polarity::kActiveLow);

  auto input_result = chip.GetInputLine(config);
  ASSERT_EQ(OkStatus(), input_result.status());
  auto input = std::move(input_result.value());

  // Enable the input, and ensure it is requested.
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  ASSERT_EQ(OkStatus(), input.Enable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kInput);

  Result<State> state;

  // Force the line high and assert it is seen as inactive (active low).
  line.ForcePhysicalState(true);
  state = input.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Force the line low and assert it is seen as active (active low).
  line.ForcePhysicalState(false);
  state = input.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kActive, state.value());

  // Disable the line and ensure it is no longer requested.
  ASSERT_EQ(OkStatus(), input.Disable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
}

TEST_F(DigitalIoTest, DoOutput) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveHigh,
      /* default_state= */ State::kActive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  auto output = std::move(output_result.value());

  // Enable the output, and ensure it is requested.
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  ASSERT_EQ(OkStatus(), output.Enable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kOutput);

  // Expect the line to go high, due to default_state=kActive (active high).
  ASSERT_TRUE(line.physical_state());

  // Set the output's state to inactive, and assert it goes low (active high).
  ASSERT_EQ(OkStatus(), output.SetStateInactive());
  ASSERT_FALSE(line.physical_state());

  // Set the output's state to active, and assert it goes high (active high).
  ASSERT_EQ(OkStatus(), output.SetStateActive());
  ASSERT_TRUE(line.physical_state());

  // Disable the line and ensure it is no longer requested.
  ASSERT_EQ(OkStatus(), output.Disable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  // NOTE: We do not assert line.physical_state() here.
  // See the warning on LinuxDigitalOut in docs.rst.
}

TEST_F(DigitalIoTest, DoOutputInvert) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveLow,
      /* default_state= */ State::kActive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  auto output = std::move(output_result.value());

  // Enable the output, and ensure it is requested.
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  ASSERT_EQ(OkStatus(), output.Enable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kOutput);

  // Expect the line to stay low, due to default_state=kActive (active low).
  ASSERT_FALSE(line.physical_state());

  // Set the output's state to inactive, and assert it goes high (active low).
  ASSERT_EQ(OkStatus(), output.SetStateInactive());
  ASSERT_TRUE(line.physical_state());

  // Set the output's state to active, and assert it goes low (active low).
  ASSERT_EQ(OkStatus(), output.SetStateActive());
  ASSERT_FALSE(line.physical_state());

  // Disable the line and ensure it is no longer requested.
  ASSERT_EQ(OkStatus(), output.Disable());
  ASSERT_EQ(line.requested(), Line::RequestedState::kNone);
  // NOTE: We do not assert line.physical_state() here.
  // See the warning on LinuxDigitalOut in docs.rst.
}

// Verify we can get the state of an output.
TEST_F(DigitalIoTest, OutputGetState) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* index= */ 1,
      /* polarity= */ Polarity::kActiveHigh,
      /* default_state= */ State::kInactive);

  auto output_result = chip.GetOutputLine(config);
  ASSERT_EQ(OkStatus(), output_result.status());
  auto output = std::move(output_result.value());

  ASSERT_EQ(OkStatus(), output.Enable());

  // Expect the line to stay low, due to default_state=kInactive (active high).
  ASSERT_FALSE(line.physical_state());

  Result<State> state;

  // Verify GetState() returns the expected state: inactive (default_state).
  state = output.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Set the output's state to active, then verify GetState() returns the
  // new expected state.
  ASSERT_EQ(OkStatus(), output.SetStateActive());
  state = output.GetState();
  ASSERT_EQ(OkStatus(), state.status());
  ASSERT_EQ(State::kActive, state.value());
}

}  // namespace
}  // namespace pw::digital_io
