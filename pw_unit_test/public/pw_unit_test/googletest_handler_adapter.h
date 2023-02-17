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
