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

#include "pw_containers/optional_tuple.h"

namespace examples {

pw::OptionalTuple<int, bool, const char*> ProcessData(int input) {
  pw::OptionalTuple<int, bool, const char*> result(
      pw::kTupleNull, false, "even");

  // Elements can be referenced by index or type.
  if (input != 10) {
    result.emplace<0>(input);
  }

  if (input % 2 == 0) {
    result.emplace<bool>(true);
  } else {
    result.emplace<const char*>("odd");
  }

  return result;
}

bool CheckData() {
  pw::OptionalTuple<int, bool, const char*> tuple = ProcessData(10);

  // Use has_value() to check if a value is set.
  if (tuple.has_value<int>()) {
    return false;
  }

  // reset() clears an element.
  tuple.reset<const char*>();

  // Use value() to access an element. Crashes if the value is not set.
  return tuple.value<1>();
}

}  // namespace examples

#include "pw_unit_test/framework.h"

namespace {

TEST(OptionalTupleExample, ProcessData) { EXPECT_TRUE(examples::CheckData()); }

}  // namespace
