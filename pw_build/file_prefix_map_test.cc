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

#include "pw_build_private/file_prefix_map_test.h"

namespace pw::build::test {

static_assert(StringsAreEqual("", ""));
static_assert(StringsAreEqual("test", "test"));
static_assert(!StringsAreEqual("1test", "test"));
static_assert(!StringsAreEqual("test", "test1"));
static_assert(!StringsAreEqual("test", "toast"));
static_assert(!StringsAreEqual("", "test"));

static_assert(StringsAreEqual(PW_BUILD_EXPECTED_SOURCE_PATH, __FILE__),
              "The __FILE__ macro should be " PW_BUILD_EXPECTED_SOURCE_PATH
              ", but it is " __FILE__);

}  // namespace pw::build::test
