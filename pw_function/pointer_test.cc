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

#include "pw_function/pointer.h"

#include "gtest/gtest.h"
#include "pw_function/function.h"

namespace pw::function {
namespace {

extern "C" void pw_function_test_InvokeFromCApi(void (*function)(void* context),
                                                void* context) {
  function(context);
}

extern "C" int pw_function_test_Sum(int (*summer)(int a, int b, void* context),
                                    void* context) {
  return summer(60, 40, context);
}

TEST(StaticInvoker, Function_NoArguments) {
  int value = 0;
  Function<void()> set_value_to_42([&value] { value += 42; });

  pw_function_test_InvokeFromCApi(GetFunctionPointer(set_value_to_42),
                                  &set_value_to_42);

  EXPECT_EQ(value, 42);

  pw_function_test_InvokeFromCApi(
      GetFunctionPointer<decltype(set_value_to_42)>(), &set_value_to_42);

  EXPECT_EQ(value, 84);
}

TEST(StaticInvoker, Function_WithArguments) {
  int sum = 0;
  Function<int(int, int)> sum_stuff([&sum](int a, int b) {
    sum += a + b;
    return a + b;
  });

  EXPECT_EQ(100,
            pw_function_test_Sum(GetFunctionPointer(sum_stuff), &sum_stuff));

  EXPECT_EQ(sum, 100);

  EXPECT_EQ(100,
            pw_function_test_Sum(GetFunctionPointer<decltype(sum_stuff)>(),
                                 &sum_stuff));

  EXPECT_EQ(sum, 200);
}

TEST(StaticInvoker, Callback_NoArguments) {
  int value = 0;
  Callback<void()> set_value_to_42([&value] { value += 42; });

  pw_function_test_InvokeFromCApi(GetFunctionPointer(set_value_to_42),
                                  &set_value_to_42);

  EXPECT_EQ(value, 42);
}

TEST(StaticInvoker, Callback_WithArguments) {
  int sum = 0;
  Callback<int(int, int)> sum_stuff([&sum](int a, int b) {
    sum += a + b;
    return a + b;
  });

  EXPECT_EQ(100,
            pw_function_test_Sum(GetFunctionPointer<decltype(sum_stuff)>(),
                                 &sum_stuff));

  EXPECT_EQ(sum, 100);
}

TEST(StaticInvoker, Lambda_NoArguments) {
  int value = 0;
  auto set_value_to_42([&value] { value += 42; });

  pw_function_test_InvokeFromCApi(GetFunctionPointer(set_value_to_42),
                                  &set_value_to_42);

  EXPECT_EQ(value, 42);

  pw_function_test_InvokeFromCApi(
      GetFunctionPointer<decltype(set_value_to_42)>(), &set_value_to_42);

  EXPECT_EQ(value, 84);
}

TEST(StaticInvoker, Lambda_WithArguments) {
  int sum = 0;
  auto sum_stuff = [&sum](int a, int b) {
    sum += a + b;
    return a + b;
  };

  EXPECT_EQ(100,
            pw_function_test_Sum(GetFunctionPointer(sum_stuff), &sum_stuff));

  EXPECT_EQ(sum, 100);

  EXPECT_EQ(100,
            pw_function_test_Sum(GetFunctionPointer<decltype(sum_stuff)>(),
                                 &sum_stuff));

  EXPECT_EQ(sum, 200);
}

}  // namespace
}  // namespace pw::function
