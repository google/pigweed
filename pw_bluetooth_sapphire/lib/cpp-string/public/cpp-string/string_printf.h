// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// |printf()|-like formatting functions that output/append to C++ strings.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_STRING_PRINTF_H_
#define SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_STRING_PRINTF_H_

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

#endif  // SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_STRING_PRINTF_H_
