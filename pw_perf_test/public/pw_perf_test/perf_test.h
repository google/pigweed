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

#include "pw_perf_test/event_handler.h"
#include "pw_perf_test/internal/test_info.h"
#include "pw_perf_test/state.h"
#include "pw_preprocessor/arguments.h"

#define PW_PERF_TEST(name, function, ...)                             \
  const ::pw::perf_test::internal::TestInfo PwPerfTest_##name(        \
      #name, [](::pw::perf_test::State& pw_perf_test_state) {         \
        static_cast<void>(                                            \
            function(pw_perf_test_state PW_COMMA_ARGS(__VA_ARGS__))); \
      })

#define PW_PERF_TEST_SIMPLE(name, function, ...)            \
  PW_PERF_TEST(                                             \
      name,                                                 \
      [](::pw::perf_test::State& pw_perf_test_simple_state, \
         const auto&... args) {                             \
        while (pw_perf_test_simple_state.KeepRunning()) {   \
          function(args...);                                \
        }                                                   \
      },                                                    \
      __VA_ARGS__)

namespace pw::perf_test {

void RunAllTests(EventHandler& handler);

}  // namespace pw::perf_test
