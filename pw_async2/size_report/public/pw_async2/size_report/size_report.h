// Copyright 2025 The Pigweed Authors
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

#include "pw_allocator/allocator.h"
#include "pw_async2/dispatcher.h"

namespace pw::async2::size_report {

class MockTask : public Task {
 public:
  bool should_complete = false;
  int polled = 0;
  int destroyed = 0;
  Waker last_waker;

 private:
  Poll<> DoPend(Context& cx) override {
    ++polled;
    PW_ASYNC_STORE_WAKER(cx, last_waker, "MockTask is waiting for last_waker");
    if (should_complete) {
      return Ready();
    }
    return Pending();
  }
  void DoDestroy() override { ++destroyed; }
};

allocator::Allocator& GetAllocator();

int SetBaseline(uint32_t mask);

}  // namespace pw::async2::size_report
