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

// |printf()|-like formatting functions that output/append to C++ strings.

#pragma once

#include <stdarg.h>

#include <string>

namespace bt_lib_cpp_string {

// Formats |printf()|-like input and returns it as an |std::string|.
[[nodiscard, gnu::format(printf, 1, 2)]] std::string StringPrintf(
    const char* format, ...);

// Formats |vprintf()|-like input and returns it as an |std::string|.
[[nodiscard]] std::string StringVPrintf(const char* format, va_list ap);

// Formats |printf()|-like input and appends it to |*dest|.
[[gnu::format(printf, 2, 3)]] void StringAppendf(std::string* dest,
                                                 const char* format,
                                                 ...);

// Formats |vprintf()|-like input and appends it to |*dest|.
void StringVAppendf(std::string* dest, const char* format, va_list ap);

}  // namespace bt_lib_cpp_string
