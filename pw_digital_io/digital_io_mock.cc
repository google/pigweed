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

#define PW_LOG_MODULE_NAME "IO"
#include "pw_digital_io/digital_io_mock.h"

#include "pw_log/log.h"

namespace pw::digital_io {

DigitalInOutMockImpl::DigitalInOutMockImpl(Clock& clock,
                                           pw::InlineDeque<Event>& events)
    : clock_(clock), events_(events) {}

pw::Result<DigitalInOutMockImpl::State> DigitalInOutMockImpl::DoGetState() {
  return events_.back().state;
}

pw::Status DigitalInOutMockImpl::DoSetState(State state) {
  if (events_.full()) {
    events_.pop_front();
  }
  // Log the LED state instead of toggling a physical LED. On host, there is no
  // LED to blink, so this is a portable alternative.
  switch (state) {
    case State::kInactive:
      PW_LOG_INFO("[ ]");
      break;
    case State::kActive:
      PW_LOG_INFO("[*]");
      break;
  }
  events_.push_back({clock_.now(), state});
  return pw::OkStatus();
}

}  // namespace pw::digital_io
