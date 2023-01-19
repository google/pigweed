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
//==============================================================================
//

#pragma once

// pw_trace config overrides when the host os is linux

// When pw_trace_tokenized_ANNOTATION == linux_tid_trace_annotation,
// the platform sets trace_id to the Linux thread id when the caller does
// not provide a group.
#define PW_TRACE_HAS_TRACE_ID(TRACE_TYPE)          \
  ((TRACE_TYPE) == PW_TRACE_TYPE_INSTANT ||        \
   (TRACE_TYPE) == PW_TRACE_TYPE_DURATION_START || \
   (TRACE_TYPE) == PW_TRACE_TYPE_DURATION_END ||   \
   (TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_INSTANT ||  \
   (TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_START ||    \
   (TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_END)
