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
// This file contains the backend hooks and implementation details for trace.

#pragma once

#include "pw_preprocessor/macro_arg_count.h"
#include "pw_trace_backend/trace_backend.h"

// Default: Flag value if none set
#ifndef PW_TRACE_FLAGS_DEFAULT
#define PW_TRACE_FLAGS_DEFAULT 0
#endif  // PW_TRACE_FLAGS_DEFAULT

// Default: PW_TRACE_TRACE_ID_DEFAULT
#ifndef PW_TRACE_TRACE_ID_DEFAULT
#define PW_TRACE_TRACE_ID_DEFAULT 0
#endif  // PW_TRACE_TRACE_ID_DEFAULT

// Default: PW_TRACE_GROUP_LABEL_DEFAULT
#ifndef PW_TRACE_GROUP_LABEL_DEFAULT
#define PW_TRACE_GROUP_LABEL_DEFAULT ""
#endif  // PW_TRACE_GROUP_LABEL_DEFAULT

// These macros can be used to determine if a trace type contrains span or group
// label
#ifndef PW_TRACE_HAS_TRACE_ID
#define PW_TRACE_HAS_TRACE_ID(TRACE_TYPE)         \
  ((TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_START ||   \
   (TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_INSTANT || \
   (TRACE_TYPE) == PW_TRACE_TYPE_ASYNC_END)
#endif  // PW_TRACE_HAS_TRACE_ID
#ifndef PW_TRACE_HAS_GROUP_LABEL
#define PW_TRACE_HAS_GROUP_LABEL(TRACE_TYPE) (false)
#endif  // PW_TRACE_HAS_GROUP_LABEL

// Default: behaviour for unimplemented trace event types
#ifndef _PW_TRACE_DISABLED
#define _PW_TRACE_DISABLED(...) \
  if (0) {                      \
  }
#endif  // _PW_TRACE_DISABLED

#ifdef PW_TRACE_TYPE_INSTANT  // Disabled if backend doesn't define this
#define _PW_TRACE_INSTANT_ARGS2(flag, label) \
  PW_TRACE(PW_TRACE_TYPE_INSTANT,            \
           flag,                             \
           label,                            \
           PW_TRACE_GROUP_LABEL_DEFAULT,     \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_INSTANT
#define _PW_TRACE_INSTANT_ARGS2(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_INSTANT

#ifdef PW_TRACE_TYPE_INSTANT_GROUP  // Disabled if backend doesn't define this
#define _PW_TRACE_INSTANT_ARGS3(flag, label, group) \
  PW_TRACE(PW_TRACE_TYPE_INSTANT_GROUP,             \
           flag,                                    \
           label,                                   \
           group,                                   \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_INSTANT_GROUP
#define _PW_TRACE_INSTANT_ARGS3(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_INSTANT_GROUP

#ifdef PW_TRACE_TYPE_ASYNC_INSTANT  // Disabled if backend doesn't define this
#define _PW_TRACE_INSTANT_ARGS4(flag, label, group, trace_id) \
  PW_TRACE(PW_TRACE_TYPE_ASYNC_INSTANT, flag, label, group, trace_id)
#else  // PW_TRACE_TYPE_ASYNC_INSTANT
#define _PW_TRACE_INSTANT_ARGS4(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_ASYNC_INSTANT

#ifdef PW_TRACE_TYPE_DURATION_START  // Disabled if backend doesn't define this
#define _PW_TRACE_START_ARGS2(flag, label) \
  PW_TRACE(PW_TRACE_TYPE_DURATION_START,   \
           flag,                           \
           label,                          \
           PW_TRACE_GROUP_LABEL_DEFAULT,   \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_DURATION_START
#define _PW_TRACE_START_ARGS2(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_DURATION_START

#ifdef PW_TRACE_TYPE_DURATION_GROUP_START  // Disabled if backend doesn't define
                                           // this
#define _PW_TRACE_START_ARGS3(flag, label, group) \
  PW_TRACE(PW_TRACE_TYPE_DURATION_GROUP_START,    \
           flag,                                  \
           label,                                 \
           group,                                 \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_DURATION_GROUP_START
#define _PW_TRACE_START_ARGS3(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_DURATION_GROUP_START

#ifdef PW_TRACE_TYPE_ASYNC_START  // Disabled if backend doesn't define this
#define _PW_TRACE_START_ARGS4(flag, label, group, trace_id) \
  PW_TRACE(PW_TRACE_TYPE_ASYNC_START, flag, label, group, trace_id)
#else  // PW_TRACE_TYPE_ASYNC_START
#define _PW_TRACE_START_ARGS4(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_ASYNC_START

#ifdef PW_TRACE_TYPE_DURATION_END  // Disabled if backend doesn't define this
#define _PW_TRACE_END_ARGS2(flag, label) \
  PW_TRACE(PW_TRACE_TYPE_DURATION_END,   \
           flag,                         \
           label,                        \
           PW_TRACE_GROUP_LABEL_DEFAULT, \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_DURATION_END
#define _PW_TRACE_END_ARGS2(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_DURATION_END

#ifdef PW_TRACE_TYPE_DURATION_GROUP_END  // Disabled if backend doesn't define
                                         // this
#define _PW_TRACE_END_ARGS3(flag, label, group) \
  PW_TRACE(PW_TRACE_TYPE_DURATION_GROUP_END,    \
           flag,                                \
           label,                               \
           group,                               \
           PW_TRACE_TRACE_ID_DEFAULT)
#else  // PW_TRACE_TYPE_DURATION_GROUP_END
#define _PW_TRACE_END_ARGS3(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_DURATION_GROUP_END

#ifdef PW_TRACE_TYPE_ASYNC_END  // Disabled if backend doesn't define this
#define _PW_TRACE_END_ARGS4(flag, label, group, trace_id) \
  PW_TRACE(PW_TRACE_TYPE_ASYNC_END, flag, label, group, trace_id)
#else  // PW_TRACE_TYPE_ASYNC_END
#define _PW_TRACE_END_ARGS4(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // PW_TRACE_TYPE_ASYNC_END

#ifndef _PW_TRACE_SCOPE_OBJECT
#define _PW_TRACE_SCOPE_OBJECT(object_name, flag, ...)                 \
  class object_name {                                                  \
   public:                                                             \
    object_name(const char* label) : label_(label) {                   \
      PW_TRACE_START_FLAG((flag), (label_)PW_COMMA_ARGS(__VA_ARGS__)); \
    }                                                                  \
    ~object_name() {                                                   \
      PW_TRACE_END_FLAG((flag), (label_)PW_COMMA_ARGS(__VA_ARGS__));   \
    }                                                                  \
                                                                       \
   private:                                                            \
    const char* label_;                                                \
  }
#endif  // _PW_TRACE_SCOPE_OBJECT

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT)
#define _PW_TRACE_INSTANT_DATA_ARGS5(            \
    flag, label, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_INSTANT,           \
                flag,                            \
                label,                           \
                PW_TRACE_GROUP_LABEL_DEFAULT,    \
                PW_TRACE_TRACE_ID_DEFAULT,       \
                data_format_string,              \
                data,                            \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT)
#define _PW_TRACE_INSTANT_DATA_ARGS5(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT_GROUP)
#define _PW_TRACE_INSTANT_DATA_ARGS6(                   \
    flag, label, group, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_INSTANT_GROUP,            \
                flag,                                   \
                label,                                  \
                group,                                  \
                PW_TRACE_TRACE_ID_DEFAULT,              \
                data_format_string,                     \
                data,                                   \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT_GROUP)
#define _PW_TRACE_INSTANT_DATA_ARGS6(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_INSTANT_GROUP)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_INSTANT)
#define _PW_TRACE_INSTANT_DATA_ARGS7(                             \
    flag, label, group, trace_id, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_ASYNC_INSTANT,                      \
                flag,                                             \
                label,                                            \
                group,                                            \
                trace_id,                                         \
                data_format_string,                               \
                data,                                             \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_INSTANT)
#define _PW_TRACE_INSTANT_DATA_ARGS7(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_INSTANT)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)
#define _PW_TRACE_START_DATA_ARGS5(              \
    flag, label, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_DURATION_START,    \
                flag,                            \
                label,                           \
                PW_TRACE_GROUP_LABEL_DEFAULT,    \
                PW_TRACE_TRACE_ID_DEFAULT,       \
                data_format_string,              \
                data,                            \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)
#define _PW_TRACE_START_DATA_ARGS5(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_GROUP_START)
#define _PW_TRACE_START_DATA_ARGS6(                     \
    flag, label, group, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_DURATION_GROUP_START,     \
                flag,                                   \
                label,                                  \
                group,                                  \
                PW_TRACE_TRACE_ID_DEFAULT,              \
                data_format_string,                     \
                data,                                   \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)
#define _PW_TRACE_START_DATA_ARGS6(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_START)
#define _PW_TRACE_START_DATA_ARGS7(                               \
    flag, label, group, trace_id, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_ASYNC_START,                        \
                flag,                                             \
                label,                                            \
                group,                                            \
                trace_id,                                         \
                data_format_string,                               \
                data,                                             \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_START)
#define _PW_TRACE_START_DATA_ARGS7(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_START)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_END)
#define _PW_TRACE_END_DATA_ARGS5(flag, label, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_DURATION_END,                                   \
                flag,                                                         \
                label,                                                        \
                PW_TRACE_GROUP_LABEL_DEFAULT,                                 \
                PW_TRACE_TRACE_ID_DEFAULT,                                    \
                data_format_string,                                           \
                data,                                                         \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)
#define _PW_TRACE_END_DATA_ARGS5(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_START)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_GROUP_END)
#define _PW_TRACE_END_DATA_ARGS6(                       \
    flag, label, group, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_DURATION_GROUP_END,       \
                flag,                                   \
                label,                                  \
                group,                                  \
                PW_TRACE_TRACE_ID_DEFAULT,              \
                data_format_string,                     \
                data,                                   \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_GROUP_END)
#define _PW_TRACE_END_DATA_ARGS6(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_DURATION_GROUP_END)

#if defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_END)
#define _PW_TRACE_END_DATA_ARGS7(                                 \
    flag, label, group, trace_id, data_format_string, data, size) \
  PW_TRACE_DATA(PW_TRACE_TYPE_ASYNC_END,                          \
                flag,                                             \
                label,                                            \
                group,                                            \
                trace_id,                                         \
                data_format_string,                               \
                data,                                             \
                size)
#else  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_END)
#define _PW_TRACE_END_DATA_ARGS7(...) _PW_TRACE_DISABLED(__VA_ARGS__)
#endif  // defined(PW_TRACE_DATA) && defined(PW_TRACE_TYPE_ASYNC_END)
