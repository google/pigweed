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

#include "pw_function/scope_guard.h"

#include <utility>

#include "pw_function/function.h"
#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(ScopeGuard, ExecutesLambda) {
  bool executed = false;
  {
    ScopeGuard guarded_lambda([&] { executed = true; });
    EXPECT_FALSE(executed);
  }
  EXPECT_TRUE(executed);
}

static bool static_executed = false;
void set_static_executed() { static_executed = true; }

TEST(ScopeGuard, ExecutesFunction) {
  {
    ScopeGuard guarded_function(set_static_executed);

    EXPECT_FALSE(static_executed);
  }
  EXPECT_TRUE(static_executed);
}

TEST(ScopeGuard, ExecutesPwFunction) {
  bool executed = false;
  pw::Function<void()> pw_function([&]() { executed = true; });
  {
    ScopeGuard guarded_pw_function(std::move(pw_function));
    EXPECT_FALSE(executed);
  }
  EXPECT_TRUE(executed);
}

TEST(ScopeGuard, Dismiss) {
  bool executed = false;
  {
    ScopeGuard guard([&] { executed = true; });
    EXPECT_FALSE(executed);
    guard.Dismiss();
    EXPECT_FALSE(executed);
  }
  EXPECT_FALSE(executed);
}

TEST(ScopeGuard, MoveConstructor) {
  bool executed = false;
  ScopeGuard first_guard([&] { executed = true; });
  {
    ScopeGuard second_guard(std::move(first_guard));
    EXPECT_FALSE(executed);
  }
  EXPECT_TRUE(executed);
}

TEST(ScopeGuard, MoveOperator) {
  bool executed = false;
  ScopeGuard first_guard([&] { executed = true; });
  {
    ScopeGuard second_guard = std::move(first_guard);
    EXPECT_FALSE(executed);
  }
  EXPECT_TRUE(executed);
}

}  // namespace
}  // namespace pw
