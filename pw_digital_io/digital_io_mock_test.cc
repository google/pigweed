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

#include "pw_digital_io/digital_io_mock.h"

#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::OkStatus;
using ::pw::digital_io::DigitalInOutMock;
using ::pw::digital_io::State;

template <typename T>
T pop_front(pw::InlineDeque<T>& deque) {
  T v = std::move(deque.front());
  deque.pop_front();
  return v;
}

TEST(DigitalInOutMock, RecordsEvents) {
  static constexpr const auto kMaxQueuedEvents = 4;
  DigitalInOutMock<kMaxQueuedEvents> mock;
  ASSERT_EQ(mock.events().size(), 1);
  EXPECT_EQ(pop_front(mock.events()).state, State::kInactive);
  EXPECT_EQ(mock.events().size(), 0);

  EXPECT_EQ(mock.SetStateInactive(), OkStatus());
  EXPECT_EQ(mock.events().size(), 1);
  EXPECT_EQ(pop_front(mock.events()).state, State::kInactive);
  EXPECT_EQ(mock.events().size(), 0);

  EXPECT_EQ(mock.SetStateActive(), OkStatus());
  ASSERT_EQ(mock.events().size(), 1);
  EXPECT_EQ(pop_front(mock.events()).state, State::kActive);
  EXPECT_EQ(mock.events().size(), 0);
}

}  // namespace