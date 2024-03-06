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

#include "gtest/gtest.h"
#include "pw_unit_test/event_handler.h"

namespace pw {
namespace unit_test {

/// Adapts a custom ``main()`` function to work with upstream GoogleTest
/// without modification. Custom ``main()`` functions are used for complex
/// testing scenarios, such as on-device testing. Must be paired with a
/// predefined event handler, such as
/// ``pw::unit_test::GoogleTestStyleEventHandler``.
/// See ``pw::unit_test::EventHandler`` for an explanation of each event.
class GoogleTestHandlerAdapter : public testing::EmptyTestEventListener {
 public:
  GoogleTestHandlerAdapter(EventHandler& handler) : handler_(handler) {}
  void OnTestProgramStart(const testing::UnitTest&) override;
  void OnEnvironmentsSetUpEnd(const testing::UnitTest&) override;
  void OnTestSuiteStart(const testing::TestSuite&) override;
  void OnTestStart(const testing::TestInfo&) override;
  void OnTestPartResult(const testing::TestPartResult&) override;
  void OnTestEnd(const testing::TestInfo&) override;
  void OnTestSuiteEnd(const testing::TestSuite&) override;
  void OnEnvironmentsTearDownEnd(const testing::UnitTest& unit_test) override;
  void OnTestProgramEnd(const testing::UnitTest&) override;

 private:
  EventHandler& handler_;
};

}  // namespace unit_test
}  // namespace pw
