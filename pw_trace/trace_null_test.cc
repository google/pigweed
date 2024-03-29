// Copyright 2021 The Pigweed Authors
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

#include "pw_trace/internal/null.h"
#include "pw_unit_test/framework.h"

#define PW_TRACE_MODULE_NAME "this test!"

extern "C" bool CTest();

namespace {

TEST(TraceNull, PW_TRACE) { PW_TRACE(0, 1, "label", "group", 2); }

TEST(TraceNull, PW_TRACE_DATA) {
  PW_TRACE_DATA(0, 1, "label", "group", 2, "type", "data", 1);
}

}  // namespace
