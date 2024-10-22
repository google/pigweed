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
#include <mutex>
#include <queue>
#include <vector>

#include "mock_vfs.h"
#include "pw_digital_io_linux/digital_io.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_unit_test/framework.h"

namespace pw::digital_io {
namespace {

using namespace std::chrono_literals;

class DigitalIoTest;
class LineHandleFile;
class LineEventFile;

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

  Status RequestInput(LineHandleFile* handle, bool active_low) {
    PW_TRY(DoRequest(RequestedState::kInput, active_low));
    current_line_handle_ = handle;
    return OkStatus();
  }

  Status RequestInputInterrupt(LineEventFile* handle, bool active_low) {
    PW_TRY(DoRequest(RequestedState::kInputInterrupt, active_low));
    current_event_handle_ = handle;
    return OkStatus();
  }

  Status RequestOutput(LineHandleFile* handle, bool active_low) {
    PW_TRY(DoRequest(RequestedState::kOutput, active_low));
    current_line_handle_ = handle;
    return OkStatus();
  }

  void ClearRequest() {
    requested_ = RequestedState::kNone;
    current_line_handle_ = nullptr;
    current_event_handle_ = nullptr;
  }

  //
  // Test-side interface: Intended for use by the tests themselves.
  //

  enum class RequestedState {
    kNone,            // Not requested by "userspace"
    kInput,           // Requested by "userspace" as an input
    kInputInterrupt,  // Requested by "userspace" as an interrupt (event)
    kOutput,          // Requested by "userspace" as an output
  };

  RequestedState requested() const { return requested_; }
  LineHandleFile* current_line_handle() const { return current_line_handle_; }
  LineEventFile* current_event_handle() const { return current_event_handle_; }

  void ForcePhysicalState(bool state) { physical_state_ = state; }

  bool physical_state() const { return physical_state_; }

 private:
  const uint32_t index_;
  bool physical_state_ = false;

  RequestedState requested_ = RequestedState::kNone;
  bool active_low_ = false;

  LineHandleFile* current_line_handle_ = nullptr;
  LineEventFile* current_event_handle_ = nullptr;

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

#define EXPECT_LINE_NOT_REQUESTED(line) \
  EXPECT_EQ(line.requested(), Line::RequestedState::kNone)
#define EXPECT_LINE_REQUESTED_OUTPUT(line) \
  EXPECT_EQ(line.requested(), Line::RequestedState::kOutput)
#define EXPECT_LINE_REQUESTED_INPUT(line) \
  EXPECT_EQ(line.requested(), Line::RequestedState::kInput)
#define EXPECT_LINE_REQUESTED_INPUT_INTERRUPT(line) \
  EXPECT_EQ(line.requested(), Line::RequestedState::kInputInterrupt)

// Represents a GPIO line handle, the result of issuing
// GPIO_GET_LINEHANDLE_IOCTL to an open chip file.
class LineHandleFile : public MockFile {
 public:
  LineHandleFile(MockVfs& vfs, int eventfd, const std::string& name, Line& line)
      : MockFile(vfs, eventfd, name), line_(line) {}

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

// Represents a GPIO line event handle, the result of issuing
// GPIO_GET_LINEEVENT_IOCTL to an open chip file.
class LineEventFile final : public MockFile {
 public:
  LineEventFile(MockVfs& vfs,
                int eventfd,
                const std::string& name,
                Line& line,
                uint32_t event_flags)
      : MockFile(vfs, eventfd, name), line_(line), event_flags_(event_flags) {}

  void EnqueueEvent(const struct gpioevent_data& event) {
    static_assert(GPIOEVENT_REQUEST_RISING_EDGE == GPIOEVENT_EVENT_RISING_EDGE);
    static_assert(GPIOEVENT_REQUEST_FALLING_EDGE ==
                  GPIOEVENT_EVENT_FALLING_EDGE);
    if ((event.id & event_flags_) == 0) {
      return;
    }

    {
      std::lock_guard lock(mutex_);
      event_queue_.push(event);
    }

    // Make this file's fd readable (one token).
    WriteEventfd();
  }

 private:
  Line& line_;
  uint32_t const event_flags_;
  std::queue<struct gpioevent_data> event_queue_;
  pw::sync::Mutex mutex_;

  // Hide these
  using MockFile::ReadEventfd;
  using MockFile::WriteEventfd;

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
      // Unlinke LineHandleFile, this only supports "get", as it is only for
      // inputs.
      default:
        PW_LOG_ERROR("%s: Unhandled request=0x%lX", __FUNCTION__, request);
        return -1;
    }
  }

  int DoIoctlGetValues(struct gpiohandle_data* data) {
    auto result = line_.GetValue();
    if (!result.ok()) {
      return -1;
    }

    data->values[0] = *result;
    return 0;
  }

  ssize_t DoRead(void* buf, size_t count) override {
    // Consume the readable state of the eventfd (one token).
    PW_CHECK_INT_EQ(ReadEventfd(), 1);  // EFD_SEMAPHORE

    std::lock_guard lock(mutex_);

    // Pop the event from the queue.
    PW_CHECK(!event_queue_.empty());
    struct gpioevent_data event = event_queue_.front();
    if (count < sizeof(event)) {
      return -1;
    }
    event_queue_.pop();

    memcpy(buf, &event, sizeof(event));
    return sizeof(event);
  }
};

// Represents an open GPIO chip file, the result of opening /dev/gpiochip*.
class ChipFile : public MockFile {
 public:
  ChipFile(MockVfs& vfs,
           int eventfd,
           const std::string& name,
           std::vector<Line>& lines)
      : MockFile(vfs, eventfd, name), lines_(lines) {}

 private:
  std::vector<Line>& lines_;

  //
  // MockFile impl.
  //

  int DoIoctl(unsigned long request, void* arg) override {
    switch (request) {
      case GPIO_GET_LINEHANDLE_IOCTL:
        return DoLinehandleIoctl(static_cast<struct gpiohandle_request*>(arg));
      case GPIO_GET_LINEEVENT_IOCTL:
        return DoLineeventIoctl(static_cast<struct gpioevent_request*>(arg));
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

    auto file = vfs().MakeFile<LineHandleFile>("line-handle", line);
    // Ownership: The vfs owns this file, but the line borrows a reference to
    // it. This is safe because the file's Close() method undoes that borrow.

    Status status = OkStatus();
    switch (direction) {
      case GPIOHANDLE_REQUEST_OUTPUT:
        status.Update(line.RequestOutput(file.get(), active_low));
        status.Update(line.SetValue(default_value));
        break;
      case GPIOHANDLE_REQUEST_INPUT:
        status.Update(line.RequestInput(file.get(), active_low));
        break;
    }
    if (!status.ok()) {
      return -1;
    }

    req->fd = vfs().InstallFile(std::move(file));
    return 0;
  }

  int DoLineeventIoctl(struct gpioevent_request* req) {
    uint32_t const direction = req->handleflags & (GPIOHANDLE_REQUEST_OUTPUT |
                                                   GPIOHANDLE_REQUEST_INPUT);
    bool const active_low = req->handleflags & GPIOHANDLE_REQUEST_ACTIVE_LOW;
    uint32_t const offset = req->lineoffset;

    if (direction != GPIOHANDLE_REQUEST_INPUT) {
      PW_LOG_ERROR("%s: Only input is supported by this ioctl", __FUNCTION__);
      return -1;
    }

    if (offset >= lines_.size()) {
      PW_LOG_ERROR("%s: Invalid line offset: %u", __FUNCTION__, offset);
      return -1;
    }
    Line& line = lines_[offset];

    auto file =
        vfs().MakeFile<LineEventFile>("line-event", line, req->eventflags);
    // Ownership: The vfs() owns this file, but the line borrows a reference to
    // it. This is safe because the file's Close() method undoes that borrow.

    Status status = line.RequestInputInterrupt(file.get(), active_low);
    if (!status.ok()) {
      return -1;
    }

    req->fd = vfs().InstallFile(std::move(file));
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
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveHigh);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input, chip.GetInputLine(config));

  // Enable the input, and ensure it is requested.
  EXPECT_LINE_NOT_REQUESTED(line);
  PW_TEST_ASSERT_OK(input.Enable());
  EXPECT_LINE_REQUESTED_INPUT(line);

  Result<State> state;

  // Force the line high and assert it is seen as active (active high).
  line.ForcePhysicalState(true);
  state = input.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kActive, state.value());

  // Force the line low and assert it is seen as inactive (active high).
  line.ForcePhysicalState(false);
  state = input.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Disable the line and ensure it is no longer requested.
  PW_TEST_ASSERT_OK(input.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
}

TEST_F(DigitalIoTest, DoInputInvert) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line0();
  LinuxInputConfig config(
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveLow);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input, chip.GetInputLine(config));

  // Enable the input, and ensure it is requested.
  EXPECT_LINE_NOT_REQUESTED(line);
  PW_TEST_ASSERT_OK(input.Enable());
  EXPECT_LINE_REQUESTED_INPUT(line);

  Result<State> state;

  // Force the line high and assert it is seen as inactive (active low).
  line.ForcePhysicalState(true);
  state = input.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Force the line low and assert it is seen as active (active low).
  line.ForcePhysicalState(false);
  state = input.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kActive, state.value());

  // Disable the line and ensure it is no longer requested.
  PW_TEST_ASSERT_OK(input.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
}

TEST_F(DigitalIoTest, DoOutput) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* gpio_index= */ 1,
      /* gpio_polarity= */ Polarity::kActiveHigh,
      /* gpio_default_state== */ State::kActive);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto output, chip.GetOutputLine(config));

  // Enable the output, and ensure it is requested.
  EXPECT_LINE_NOT_REQUESTED(line);
  PW_TEST_ASSERT_OK(output.Enable());
  EXPECT_LINE_REQUESTED_OUTPUT(line);

  // Expect the line to go high, due to default_state=kActive (active high).
  ASSERT_TRUE(line.physical_state());

  // Set the output's state to inactive, and assert it goes low (active high).
  PW_TEST_ASSERT_OK(output.SetStateInactive());
  ASSERT_FALSE(line.physical_state());

  // Set the output's state to active, and assert it goes high (active high).
  PW_TEST_ASSERT_OK(output.SetStateActive());
  ASSERT_TRUE(line.physical_state());

  // Disable the line and ensure it is no longer requested.
  PW_TEST_ASSERT_OK(output.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
  // NOTE: We do not assert line.physical_state() here.
  // See the warning on LinuxDigitalOut in docs.rst.
}

TEST_F(DigitalIoTest, DoOutputInvert) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* gpio_index= */ 1,
      /* gpio_polarity= */ Polarity::kActiveLow,
      /* gpio_default_state== */ State::kActive);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto output, chip.GetOutputLine(config));

  // Enable the output, and ensure it is requested.
  EXPECT_LINE_NOT_REQUESTED(line);
  PW_TEST_ASSERT_OK(output.Enable());
  EXPECT_LINE_REQUESTED_OUTPUT(line);

  // Expect the line to stay low, due to default_state=kActive (active low).
  ASSERT_FALSE(line.physical_state());

  // Set the output's state to inactive, and assert it goes high (active low).
  PW_TEST_ASSERT_OK(output.SetStateInactive());
  ASSERT_TRUE(line.physical_state());

  // Set the output's state to active, and assert it goes low (active low).
  PW_TEST_ASSERT_OK(output.SetStateActive());
  ASSERT_FALSE(line.physical_state());

  // Disable the line and ensure it is no longer requested.
  PW_TEST_ASSERT_OK(output.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
  // NOTE: We do not assert line.physical_state() here.
  // See the warning on LinuxDigitalOut in docs.rst.
}

// Verify we can get the state of an output.
TEST_F(DigitalIoTest, OutputGetState) {
  LinuxDigitalIoChip chip = OpenChip();

  auto& line = line1();
  LinuxOutputConfig config(
      /* gpio_index= */ 1,
      /* gpio_polarity= */ Polarity::kActiveHigh,
      /* gpio_default_state== */ State::kInactive);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto output, chip.GetOutputLine(config));

  PW_TEST_ASSERT_OK(output.Enable());

  // Expect the line to stay low, due to default_state=kInactive (active high).
  ASSERT_FALSE(line.physical_state());

  Result<State> state;

  // Verify GetState() returns the expected state: inactive (default_state).
  state = output.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kInactive, state.value());

  // Set the output's state to active, then verify GetState() returns the
  // new expected state.
  PW_TEST_ASSERT_OK(output.SetStateActive());
  state = output.GetState();
  PW_TEST_ASSERT_OK(state.status());
  ASSERT_EQ(State::kActive, state.value());
}

//
// Input interrupts
//

TEST_F(DigitalIoTest, DoInputInterruptsEnabledBefore) {
  LinuxDigitalIoChip chip = OpenChip();
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto notifier, LinuxGpioNotifier::Create());

  auto& line = line0();
  LinuxInputConfig config(
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveHigh);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input,
                               chip.GetInterruptLine(config, notifier));

  EXPECT_LINE_NOT_REQUESTED(line);

  // Have to set a handler before we can enable interrupts.
  PW_TEST_ASSERT_OK(input.SetInterruptHandler(InterruptTrigger::kActivatingEdge,
                                              [](State) {}));

  // pw_digital_io says the line should be enabled before calling
  // EnableInterruptHandler(), but we explicitly support it being called with
  // the line disabled to avoid an unnecessary file close/reopen.
  PW_TEST_ASSERT_OK(input.EnableInterruptHandler());
  PW_TEST_ASSERT_OK(input.Enable());

  // Interrupts requested; should be a line event handle.
  EXPECT_LINE_REQUESTED_INPUT_INTERRUPT(line);

  // Disable; nothing should be requested.
  PW_TEST_ASSERT_OK(input.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
}

TEST_F(DigitalIoTest, DoInputInterruptsEnabledAfter) {
  LinuxDigitalIoChip chip = OpenChip();
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto notifier, LinuxGpioNotifier::Create());

  auto& line = line0();
  LinuxInputConfig config(
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveHigh);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input,
                               chip.GetInterruptLine(config, notifier));

  EXPECT_LINE_NOT_REQUESTED(line);

  PW_TEST_ASSERT_OK(input.Enable());

  // No interrupts requested; should be a normal line handle.
  EXPECT_LINE_REQUESTED_INPUT(line);

  // Interrupts requested while enabled; should be a line event handle.
  // Have to set a handler before we can enable interrupts.
  PW_TEST_ASSERT_OK(input.SetInterruptHandler(InterruptTrigger::kActivatingEdge,
                                              [](State) {}));
  PW_TEST_ASSERT_OK(input.EnableInterruptHandler());
  EXPECT_LINE_REQUESTED_INPUT_INTERRUPT(line);

  // Interrupts disabled while enabled; should revert to a normal line handle.
  PW_TEST_ASSERT_OK(input.DisableInterruptHandler());
  EXPECT_LINE_REQUESTED_INPUT(line);

  // Disable; nothing should be requested.
  PW_TEST_ASSERT_OK(input.Disable());
  EXPECT_LINE_NOT_REQUESTED(line);
}

TEST_F(DigitalIoTest, DoInputInterruptsReadOne) {
  LinuxDigitalIoChip chip = OpenChip();
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto notifier, LinuxGpioNotifier::Create());

  auto& line = line0();
  LinuxInputConfig config(
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveHigh);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input,
                               chip.GetInterruptLine(config, notifier));

  std::vector<State> interrupts;
  auto handler = [&interrupts](State state) {
    PW_LOG_DEBUG("Interrupt handler fired with state=%s",
                 state == State::kActive ? "active" : "inactive");
    interrupts.push_back(state);
  };

  PW_TEST_ASSERT_OK(
      input.SetInterruptHandler(InterruptTrigger::kActivatingEdge, handler));

  // pw_digital_io says the line should be enabled before calling
  // EnableInterruptHandler(), but we explicitly support it being called with
  // the line disabled to avoid an unnecessary file close/reopen.
  PW_TEST_ASSERT_OK(input.EnableInterruptHandler());
  PW_TEST_ASSERT_OK(input.Enable());

  EXPECT_LINE_REQUESTED_INPUT_INTERRUPT(line);
  LineEventFile* evt = line.current_event_handle();
  ASSERT_NE(evt, nullptr);

  evt->EnqueueEvent({
      .timestamp = 1122334455667788,
      .id = GPIOEVENT_EVENT_RISING_EDGE,
  });

  constexpr int timeout = 0;  // Don't block
  PW_LOG_DEBUG("WaitForEvents(%d)", timeout);
  PW_TEST_ASSERT_OK_AND_ASSIGN(unsigned int count,
                               notifier->WaitForEvents(timeout));
  EXPECT_EQ(count, 1u);

  EXPECT_EQ(interrupts,
            std::vector<State>({
                State::kActive,
            }));
}

TEST_F(DigitalIoTest, DoInputInterruptsThread) {
  LinuxDigitalIoChip chip = OpenChip();
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto notifier, LinuxGpioNotifier::Create());

  auto& line = line0();
  LinuxInputConfig config(
      /* gpio_index= */ 0,
      /* gpio_polarity= */ Polarity::kActiveHigh);

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto input,
                               chip.GetInterruptLine(config, notifier));

  constexpr unsigned int kCount = 10;
  struct {
    sync::TimedThreadNotification done;
    std::vector<State> interrupts;

    void HandleInterrupt(State state) {
      interrupts.push_back(state);
      if (interrupts.size() == kCount) {
        done.release();
      }
    }
  } context;

  auto handler = [&context](State state) {
    PW_LOG_DEBUG("Interrupt handler fired with state=%s",
                 state == State::kActive ? "active" : "inactive");
    context.HandleInterrupt(state);
  };

  PW_TEST_ASSERT_OK(
      input.SetInterruptHandler(InterruptTrigger::kBothEdges, handler));

  // pw_digital_io says the line should be enabled before calling
  // EnableInterruptHandler(), but we explicitly support it being called with
  // the line disabled to avoid an unnecessary file close/reopen.
  PW_TEST_ASSERT_OK(input.EnableInterruptHandler());
  PW_TEST_ASSERT_OK(input.Enable());

  // Run a notifier thread.
  pw::Thread notif_thread(pw::thread::stl::Options(), *notifier);

  EXPECT_LINE_REQUESTED_INPUT_INTERRUPT(line);
  LineEventFile* evt = line.current_event_handle();
  ASSERT_NE(evt, nullptr);

  // Feed the line with events.
  auto nth_event = [](unsigned int i) -> uint32_t {
    return (i % 2) ? GPIOEVENT_EVENT_FALLING_EDGE : GPIOEVENT_EVENT_RISING_EDGE;
  };
  auto nth_state = [](unsigned int i) -> State {
    return (i % 2) ? State::kInactive : State::kActive;
  };

  for (unsigned int i = 0; i < kCount; i++) {
    evt->EnqueueEvent({
        .timestamp = 1122334400000000u + i,
        .id = nth_event(i),
    });
  }

  // Wait for the notifier to pick them all up.
  constexpr auto kWaitForDataTimeout = 1000ms;
  ASSERT_TRUE(context.done.try_acquire_for(kWaitForDataTimeout));

  // Stop the notifier thread.
  notifier->CancelWait();
  notif_thread.join();

  // Verify we received all of the expected callbacks.
  EXPECT_EQ(context.interrupts.size(), kCount);
  for (unsigned int i = 0; i < kCount; i++) {
    EXPECT_EQ(context.interrupts[i], nth_state(i));
  }
}

}  // namespace
}  // namespace pw::digital_io
