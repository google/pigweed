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

#include "pw_trace/trace.h"

#ifdef __cplusplus
#error "This file must be compiled as plain C to verify C compilation works."
#endif  // __cplusplus

void BasicTraceTestPlainC() {
  PW_TRACE_INSTANT("Test");

  PW_TRACE_START("Test");
  PW_TRACE_END("Test");

  PW_TRACE_START("Parent", "group");
  PW_TRACE_START("Child", "group");
  PW_TRACE_END("Child", "group");
  PW_TRACE_INSTANT("Test", "group");
  PW_TRACE_START("Other Child", "group");
  PW_TRACE_END("Other Child", "group");
  PW_TRACE_END("Parent", "group");

  PW_TRACE_START("label for start", "group", 1);
  PW_TRACE_INSTANT("label for step", "group", 1);
  PW_TRACE_END("label for end", "group", 1);

  const char kSomeData[] = "SOME DATA";
  PW_TRACE_INSTANT_DATA("Test", "s", kSomeData, sizeof(kSomeData));

  PW_TRACE_START_DATA("Parent", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_START_DATA("Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_INSTANT_DATA("Test", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_START_DATA(
      "Other Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("Other Child", "group", "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA("Parent", "group", "s", kSomeData, sizeof(kSomeData));

  uint32_t trace_id = 1;
  PW_TRACE_START_DATA(
      "label for start", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_INSTANT_DATA(
      "label for step", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
  PW_TRACE_END_DATA(
      "label for end", "group", trace_id, "s", kSomeData, sizeof(kSomeData));
}
