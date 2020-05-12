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
#include "pw_trace_tokenized/internal/trace_tokenized_internal.h"

// Configurable options

// Since not all strings are tokenizeable, labels can be passed as arguments.
// PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES configures the maximum number of
// characters to include, if more are provided the string will be clipped.
#ifndef PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES
#define PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES 20
#endif  // PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES

// PW_TRACE_TIME_TYPE sets the type for trace time.
#ifndef PW_TRACE_TIME_TYPE
#define PW_TRACE_TIME_TYPE uint32_t
#endif  // PW_TRACE_TIME_TYPE

// PW_TRACE_GET_TIME is the macro which is called to get the current time for a
// trace event. It's default is to use pw_trace_GetTraceTime() which needs to be
// provided by the platform.
#ifndef PW_TRACE_GET_TIME
#define PW_TRACE_GET_TIME() pw_trace_GetTraceTime()
extern PW_TRACE_TIME_TYPE pw_trace_GetTraceTime();
#endif  // PW_TRACE_GET_TIME

// PW_TRACE_GET_TIME_TICKS_PER_SECOND is the macro which is called to determine
// the unit of the trace time. It's default is to use
// pw_trace_GetTraceTimeTicksPerSecond() which needs to be provided by the
// platform.
#ifndef PW_TRACE_GET_TIME_TICKS_PER_SECOND
#define PW_TRACE_GET_TIME_TICKS_PER_SECOND() \
  pw_trace_GetTraceTimeTicksPerSecond()
extern size_t pw_trace_GetTraceTimeTicksPerSecond();
#endif  // PW_TRACE_GET_TIME_TICKS_PER_SECOND

// PW_TRACE_GET_TIME_DELTA is te macro which is called to determine
// the delta between two PW_TRACE_TIME_TYPE variables. It should return a
// delta of the two times, in the same type.
// The default implementation just subtracts the two, which is suitable if
// values either never wrap, or are unsigned and do not wrap multiple times
// between trace events. If either of these are not the case a different
// implemention should be used.
#ifndef PW_TRACE_GET_TIME_DELTA
#define PW_TRACE_GET_TIME_DELTA(last_time, current_time) \
  ((current_time) - (last_time))
#ifdef __cplusplus
static_assert(
    std::is_unsigned<PW_TRACE_TIME_TYPE>::value,
    "Default time delta implementation only works for unsigned time types.");
#endif  // __cplusplus
#endif  // PW_TRACE_GET_TIME_DELTA

// PW_TRACE_LOCK is called when a new event is being processed to ensure only
// one event is sent to the sinks at a time. Is is also called when registering
// and unregistering callbacks and sinks.
#ifndef PW_TRACE_LOCK
#define PW_TRACE_LOCK()
#endif  // PW_TRACE_LOCK

// PW_TRACE_UNLOCK is called after sending the data to all the sinks.
#ifndef PW_TRACE_UNLOCK
#define PW_TRACE_UNLOCK()
#endif  // PW_TRACE_UNLOCK

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
#define PW_TRACE_REF(event_type, module, label, flags, group)   \
  PW_TOKENIZE_STRING(PW_STRINGIFY(event_type) "|" PW_STRINGIFY( \
      flags) "|" module "|" group "|" label)

#define PW_TRACE_REF_DATA(event_type, module, label, flags, group, type) \
  PW_TOKENIZE_STRING(PW_STRINGIFY(event_type) "|" PW_STRINGIFY(          \
      flags) "|" module "|" group "|" label "|" type)
