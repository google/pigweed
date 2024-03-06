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

#include <array>
#include <cstddef>

#include "pw_unit_test/event_handler.h"

namespace pw::unit_test {

/// Event handler adapter that allows for multiple event handlers to be
/// registered and used during test runs.
template <size_t kNumHandlers>
class MultiEventHandler : public EventHandler {
 public:
  template <
      typename... EventHandlers,
      typename = std::enable_if_t<sizeof...(EventHandlers) == kNumHandlers>>
  constexpr MultiEventHandler(EventHandlers&... event_handlers)
      : event_handlers_{&event_handlers...} {}

  void TestProgramStart(const ProgramSummary& program_summary) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestProgramStart(program_summary);
    }
  }
  void EnvironmentsSetUpEnd() override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->EnvironmentsSetUpEnd();
    }
  }
  void TestSuiteStart(const TestSuite& test_suite) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestSuiteStart(test_suite);
    }
  }
  void TestSuiteEnd(const TestSuite& test_suite) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestSuiteEnd(test_suite);
    }
  }
  void EnvironmentsTearDownEnd() override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->EnvironmentsTearDownEnd();
    }
  }
  void TestProgramEnd(const ProgramSummary& program_summary) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestProgramEnd(program_summary);
    }
  }
  void RunAllTestsStart() override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->RunAllTestsStart();
    }
  }
  void RunAllTestsEnd(const RunTestsSummary& run_tests_summary) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->RunAllTestsEnd(run_tests_summary);
    }
  }
  void TestCaseStart(const TestCase& test_case) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestCaseStart(test_case);
    }
  }
  void TestCaseEnd(const TestCase& test_case, TestResult result) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestCaseEnd(test_case, result);
    }
  }
  void TestCaseExpect(const TestCase& test_case,
                      const TestExpectation& expectation) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestCaseExpect(test_case, expectation);
    }
  }
  void TestCaseDisabled(const TestCase& test_case) override {
    for (EventHandler* event_handler : event_handlers_) {
      event_handler->TestCaseDisabled(test_case);
    }
  }

 private:
  static_assert(kNumHandlers > 0);
  std::array<EventHandler*, kNumHandlers> event_handlers_;
};

}  // namespace pw::unit_test
