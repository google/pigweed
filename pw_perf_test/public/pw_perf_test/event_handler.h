// Copyright 2022 The Pigweed Authors
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

namespace pw::perf_test {

/// Data reported on completion of an iteration.
struct TestIteration {
  uint32_t number = 0;
  float result = 0;
};

/// Data reported for each `Measurement` upon completion of a performance test.
struct TestMeasurement {
  float mean = 0;
  float max = 0;
  float min = 0;
};

/// Stores information on the upcoming collection of tests.
///
/// In order to match gtest, these integer types are not sized
struct TestRunInfo {
  int total_tests = 0;
  int default_iterations = 0;
};

/// Describes the performance test being run.
struct TestCase {
  const char* name = nullptr;
};

/// Collects and reports test results.
///
/// Both the state and the framework classes use these functions to report what
/// happens at each stage.
class EventHandler {
 public:
  virtual ~EventHandler() = default;

  /// A performance test is starting.
  virtual void RunAllTestsStart(const TestRunInfo& test_run_info) = 0;

  /// A performance test has ended.
  virtual void RunAllTestsEnd() = 0;

  /// A performance test case is starting.
  virtual void TestCaseStart(const TestCase& test_case) = 0;

  /// A performance test case has completed an iteration.
  virtual void TestCaseIteration(const TestIteration& test_iteration) = 0;

  /// A performance test case has produced a `Measurement`.
  virtual void TestCaseMeasure(const TestMeasurement& test_measurement) = 0;

  /// A performance test case has ended.
  virtual void TestCaseEnd(const TestCase& test_case) = 0;
};

}  // namespace pw::perf_test
