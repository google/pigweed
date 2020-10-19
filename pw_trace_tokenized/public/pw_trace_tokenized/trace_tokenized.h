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
//==============================================================================
//
// This file provides the interface for working with the tokenized trace
// backend.

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef PW_TRACE_GET_TIME_DELTA
#ifdef __cplusplus
#include <type_traits>
#endif  // __cplusplus
#endif  // PW_TRACE_GET_TIME_DELTA

#include "pw_tokenizer/tokenize.h"
#include "pw_trace_tokenized/config.h"
#include "pw_trace_tokenized/internal/trace_tokenized_internal.h"

#ifdef __cplusplus
namespace pw {
namespace trace {

using EventType = pw_trace_EventType;

class TokenizedTraceImpl {
 public:
  void Enable(bool enable) { enabled_ = enable; }
  bool IsEnabled() const { return enabled_; }

  void HandleTraceEvent(uint32_t trace_token,
                        EventType event_type,
                        const char* module,
                        uint32_t trace_id,
                        uint8_t flags,
                        const void* data_buffer,
                        size_t data_size);

 private:
  PW_TRACE_TIME_TYPE last_trace_time_ = 0;
  bool enabled_ = false;
};

// A singleton object of the TokenizedTraceImpl class which can be used to
// interface with trace using the C++ API.
// Example: pw::trace::TokenizedTrace::Instance().Enable(true);
class TokenizedTrace {
 public:
  static TokenizedTraceImpl& Instance() { return instance_; };

 private:
  static TokenizedTraceImpl instance_;
};

}  // namespace trace
}  // namespace pw
#endif  // __cplusplus

// PW_TRACE_SET_ENABLED is used to enable or disable tracing.
#define PW_TRACE_SET_ENABLED(enabled) pw_trace_Enable(enabled)

// PW_TRACE_REF provides the uint32_t token value for a specific trace event.
// this can be used in the callback to perform specific actions for that trace.
// All the fields must match exactly to generate the correct trace reference.
// If the trace does not have a group, use PW_TRACE_GROUP_LABEL_DEFAULT.
//
// For example this can be used to skip a specific trace:
//   pw_trace_TraceEventReturnFlags TraceEventCallback(
//       uint32_t trace_ref,
//       pw_trace_EventType event_type,
//       const char* module,
//       uint32_t trace_id,
//       uint8_t flags) {
//     auto skip_trace_ref = PW_TRACE_REF(PW_TRACE_TYPE_INSTANT,
//                                        "test_module",    // Module
//                                        "test_label",     // Label
//                                        PW_TRACE_FLAGS_DEFAULT,
//                                        PW_TRACE_GROUP_LABEL_DEFAULT);
//     if (trace_ref == skip_trace_ref) {
//       return PW_TRACE_EVENT_RETURN_FLAGS_SKIP_EVENT;
//     }
//     return 0;
//   }
//
// The above trace ref would provide the tokenize value for the string:
//     "1|0|test_module||test_label"
//
// Another example:
//    #define PW_TRACE_MODULE test_module
//    PW_TRACE_INSTANT_DATA_FLAG(2, "label", "group", id, "%d", 5, 1);
// Would internally generate a token value for the string:
//    "1|2|test_module|group|label|%d"
// The trace_id, and data value are runtime values and not included in the
// token string.
#define PW_TRACE_REF(event_type, module, label, flags, group)          \
  PW_TOKENIZE_STRING_DOMAIN("trace",                                   \
                            PW_STRINGIFY(event_type) "|" PW_STRINGIFY( \
                                flags) "|" module "|" group "|" label)

#define PW_TRACE_REF_DATA(event_type, module, label, flags, group, type)    \
  PW_TOKENIZE_STRING_DOMAIN(                                                \
      "trace",                                                              \
      PW_STRINGIFY(event_type) "|" PW_STRINGIFY(flags) "|" module "|" group \
                                                       "|" label "|" type)
