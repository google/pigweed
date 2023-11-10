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

#include "pw_perf_test/internal/test_info.h"

#include "pw_perf_test/internal/framework.h"

namespace pw::perf_test::internal {

TestInfo::TestInfo(const char* test_name, void (*function_body)(State&))
    : run_(function_body), test_name_(test_name) {
  // Once a TestInfo object is created by the macro, this adds itself to the
  // list of registered tests
  Framework::Get().RegisterTest(*this);
}

}  // namespace pw::perf_test::internal
