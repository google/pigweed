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

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/inline_deque.h"
#include "pw_digital_io/digital_io.h"
#include "pw_result/result.h"

namespace pw::digital_io {

/// Mock implementation of `DigitalInOut` for testing.
///
/// Records the times at which the state is changed using a provided clock. This
/// class cannot be instantiated directly. Instead, use
/// ``DigitalInOutMock<kCapacity>``.
class DigitalInOutMockImpl : public pw::digital_io::DigitalInOut {
 public:
  using State = ::pw::digital_io::State;
  using Clock = ::pw::chrono::VirtualSystemClock;

  struct Event {
    pw::chrono::SystemClock::time_point timestamp;
    State state;
  };

  pw::InlineDeque<Event>& events() { return events_; }

 protected:
  DigitalInOutMockImpl(Clock& clock, pw::InlineDeque<Event>& events);

 private:
  pw::Status DoEnable(bool) override { return pw::OkStatus(); }

  pw::Result<State> DoGetState() override;

  pw::Status DoSetState(State state) override;

  Clock& clock_;
  pw::InlineDeque<Event>& events_;
};

template <size_t kCapacity>
class DigitalInOutMock : public DigitalInOutMockImpl {
 public:
  static_assert(kCapacity > 0);

  DigitalInOutMock() : DigitalInOutMock(Clock::RealClock()) {}
  DigitalInOutMock(Clock& clock) : DigitalInOutMockImpl(clock, events_) {
    PW_CHECK_OK(SetState(State::kInactive));
  }

 private:
  pw::InlineDeque<Event, kCapacity> events_;
};

}  // namespace pw::digital_io
