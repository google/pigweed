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

// Configurable options for the tokenized trace module.
#pragma once

/// @module{pw_trace_tokenized}

/// Since not all strings are tokenizeable, labels can be passed as arguments.
/// `PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES` configures the maximum number of
/// characters to include, if more are provided the string will be clipped.
#ifndef PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES
#define PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES 20
#endif  // PW_TRACE_CONFIG_ARG_LABEL_SIZE_BYTES

/// The number of events which can be queued up internally. This is needed to
/// support concurrent trace events.
#ifndef PW_TRACE_QUEUE_SIZE_EVENTS
#define PW_TRACE_QUEUE_SIZE_EVENTS 5
#endif  // PW_TRACE_QUEUE_SIZE_EVENTS

// --- Config options for time source ----

/// The type for trace time.
#ifndef PW_TRACE_TIME_TYPE
#define PW_TRACE_TIME_TYPE uint32_t
#endif  // PW_TRACE_TIME_TYPE

/// The macro which is called to get the current time for a trace event. It
/// must be provided by the platform.
extern PW_TRACE_TIME_TYPE pw_trace_GetTraceTime(void);

/// The function which is called to determine the unit of the trace time. It
/// must be provided by the platform.
extern size_t pw_trace_GetTraceTimeTicksPerSecond(void);

/// The macro which is called to determine the delta between two
/// `PW_TRACE_TIME_TYPE` variables. It should return a delta of the two times,
/// in the same type. The default implementation just subtracts the two, which
/// is suitable if values either never wrap, or are unsigned and do not wrap
/// multiple times between trace events. If either of these are not the case a
/// different implementation should be used.
#ifndef PW_TRACE_GET_TIME_DELTA
#define PW_TRACE_GET_TIME_DELTA(last_time, current_time) \
  ((current_time) - (last_time))
#ifdef __cplusplus

#include <type_traits>

static_assert(
    std::is_unsigned<PW_TRACE_TIME_TYPE>::value,
    "Default time delta implementation only works for unsigned time types.");
#endif  // __cplusplus
#endif  // PW_TRACE_GET_TIME_DELTA

// --- Config options for callbacks ----

/// The maximum number of event callbacks which can be registered at a time.
#ifndef PW_TRACE_CONFIG_MAX_EVENT_CALLBACKS
#define PW_TRACE_CONFIG_MAX_EVENT_CALLBACKS 2
#endif  // PW_TRACE_CONFIG_MAX_EVENT_CALLBACKS

/// The maximum number of encoded event sinks which can be registered at a time.
#ifndef PW_TRACE_CONFIG_MAX_SINKS
#define PW_TRACE_CONFIG_MAX_SINKS 2
#endif  // PW_TRACE_CONFIG_MAX_SINKS

// --- Config options for optional trace buffer ---

/// The size in bytes of the optional trace buffer. The buffer is automatically
/// registered at boot if the buffer size is not 0.
#ifndef PW_TRACE_BUFFER_SIZE_BYTES
#define PW_TRACE_BUFFER_SIZE_BYTES 256
#endif  // PW_TRACE_BUFFER_SIZE_BYTES

/// The maximum size any individual encoded trace event could be. This is used
/// internally to buffer up a sample before saving into the buffer.
#ifndef PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES
/// The below calculation is provided to help determine a suitable value, using
/// the max data size bytes.
#ifndef PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES
#define PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES (32)
#endif  // PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES

#ifndef PW_TRACE_BUFFER_MAX_HEADER_SIZE_BYTES
#define PW_TRACE_BUFFER_MAX_HEADER_SIZE_BYTES                                  \
  (pw::varint::kMaxVarint64SizeBytes) +     /* worst case delta time varint */ \
      (sizeof(uint32_t)) +                  /* trace token size */             \
      (pw::varint::kMaxVarint64SizeBytes) + /* worst case trace id varint */
#endif  // PW_TRACE_BUFFER_MAX_HEADER_SIZE_BYTES

#define PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES \
  PW_TRACE_BUFFER_MAX_HEADER_SIZE_BYTES + PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES
#endif  // PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES

/// @}
