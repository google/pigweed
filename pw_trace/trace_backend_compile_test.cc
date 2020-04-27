// Copyright 2020 The Pigweed Authors
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
//
// This is a simple test that ensures a provided trace backend compiles.

#define PW_TRACE_MODULE_NAME "TST"
#include "gtest/gtest.h"
#include "pw_trace/trace.h"

namespace {

void TraceFunction() { PW_TRACE_FUNCTION(); }
void TraceFunctionGroup() { PW_TRACE_FUNCTION("FunctionGroup"); }

const char kSomeData[] = "SOME DATA";

}  // namespace

TEST(BasicTrace, Instant) { PW_TRACE_INSTANT("Test"); }

TEST(BasicTrace, InstantGroup) { PW_TRACE_INSTANT("Test", "group"); }

TEST(BasicTrace, Duration) {
  PW_TRACE_START("Test");
  PW_TRACE_END("Test");
}

TEST(BasicTrace, DurationGroup) {
  PW_TRACE_START("Parent", "group");
  PW_TRACE_START("Child", "group");
  PW_TRACE_END("child", "group");
  PW_TRACE_START("Other Child", "group");
  PW_TRACE_END("Other Child", "group");
  PW_TRACE_END("Parent", "group");
}

TEST(BasicTrace, Async) {
  uint32_t trace_id = 1;
  PW_TRACE_START("label for start", "group", trace_id);
  PW_TRACE_INSTANT("label for step", "group", trace_id);
  PW_TRACE_END("label for end", "group", trace_id);
}

TEST(BasicTrace, Scope) { PW_TRACE_SCOPE("scoped trace"); }

TEST(BasicTrace, ScopeGroup) {
  PW_TRACE_SCOPE("scoped group trace", "group");
  { PW_TRACE_SCOPE("sub scoped group trace", "group"); }
}

TEST(BasicTrace, Function) { TraceFunction(); }

TEST(BasicTrace, FunctionGroup) { TraceFunctionGroup(); }

TEST(BasicTrace, InstantData) {
  PW_TRACE_INSTANT_DATA("Test", "s", kSomeData, sizeof(kSomeData));
}

TEST(BasicTrace, InstantGroupData) {
  PW_TRACE_INSTANT_DATA("Test", "Group", "s", kSomeData, sizeof(kSomeData));
}

TEST(BasicTrace, DurationData) {
  PW_TRACE_START_DATA("Test", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("Test", "s", kSomeData, sizeof(kSomeData));
}

TEST(BasicTrace, DurationGroupData) {
  PW_TRACE_START_DATA("Parent", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_START_DATA("Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_START_DATA(
      "Other Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("Other Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("Parent", "group", "s", kSomeData, sizeof(kSomeData));
}

TEST(BasicTrace, AsyncData) {
  uint32_t trace_id = 1;
  PW_TRACE_START_DATA(
      "label for start", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_INSTANT_DATA(
      "label for step", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA(
      "label for end", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
}
