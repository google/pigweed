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
#pragma once

#include "pw_log_tokenized/log_tokenized.h"

#if !defined(__cplusplus) || !defined(__GNUC__) || defined(__clang__)

// If we're not compiling C++ or we're not using GCC, then tokenize the log.
#define PW_HANDLE_LOG PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD

#else  // defined(__cplusplus) && defined(__GNUC__) && !defined(__clang__)

#include <string_view>

#include "pw_log_string/handler.h"

// GCC has a bug resulting in section attributes of templated functions being
// ignored. This in turn means that log tokenization cannot work for templated
// functions, because the token database entries are lost at build time.
// For more information see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70435
//
// To work around this, we defer to string logging only in templated contexts.
//
// __PRETTY_FUNCTION__ is suffixed with ' [with ...]' for templated contexts
// if the class and/or function is templated. For example:
// "Foo() [with T = char]".
#define PW_HANDLE_LOG(...)                                               \
  do {                                                                   \
    if constexpr (std::string_view(__PRETTY_FUNCTION__).back() == ']') { \
      PW_LOG_STRING_HANDLE_MESSAGE(__VA_ARGS__);                         \
    } else {                                                             \
      PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(__VA_ARGS__);      \
    }                                                                    \
  } while (0)

#endif  // !defined(__cplusplus) || !defined(__GNUC__) || defined(__clang__)
