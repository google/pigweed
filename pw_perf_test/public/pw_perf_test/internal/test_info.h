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
#pragma once

#include "pw_perf_test/state.h"

namespace pw::perf_test::internal {

/// Represents a single test case.
///
/// Each instance includes a pointer to a function which constructs and runs the
/// test class. These are statically allocated instead of the test classes, as
/// test classes can be very large.
///
/// This class mimics pw::unit_test::TestInfo.
class TestInfo {
 public:
  TestInfo(const char* test_name, void (*function_body)(State&));

  // Returns the next registered test
  TestInfo* next() const { return next_; }

  void SetNext(TestInfo* next) { next_ = next; }

  void Run(State& state) const { run_(state); }

  const char* test_name() const { return test_name_; }

 private:
  // Function pointer to the code that will be measured
  void (*run_)(State&);

  // Intrusively linked list, this acts as a pointer to the next test
  TestInfo* next_ = nullptr;

  const char* test_name_;
};

}  // namespace pw::perf_test::internal
