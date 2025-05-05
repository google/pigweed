// Copyright 2023 The Pigweed Authors
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
#include "pw_containers/internal/test_helpers.h"

#include "pw_assert/check.h"

namespace pw::containers::test {

void Counter::ObjectCounter::Destructed() {
  PW_CHECK_UINT_GT(
      count_, 0, "Attempted to destroy more objects than were constructed");
  count_ -= 1;
}

Counter::ObjectCounter::~ObjectCounter() {
  PW_CHECK_UINT_EQ(count_, 0, "Objects were constructed but not destroyed");
}

Counter& Counter::operator=(const Counter& other) {
  PW_CHECK_PTR_EQ(this, set_to_this_when_constructed_);
  value = other.value;
  created += 1;
  return *this;
}

Counter& Counter::operator=(Counter&& other) {
  PW_CHECK_PTR_EQ(this, set_to_this_when_constructed_);
  value = other.value;
  other.value = 0;
  moved += 1;
  return *this;
}

Counter::~Counter() {
  PW_CHECK_PTR_EQ(this,
                  set_to_this_when_constructed_,
                  "Destroying uninitialized or corrupted object");
  destroyed += 1;
  objects_.Destructed();
  set_to_this_when_constructed_ = nullptr;
}

int Counter::created = 0;
int Counter::destroyed = 0;
int Counter::moved = 0;
Counter::ObjectCounter Counter::objects_;

}  // namespace pw::containers::test
