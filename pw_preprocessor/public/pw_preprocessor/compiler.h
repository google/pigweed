// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
//
// Preprocessor macros that wrap compiler-specific features.
// This file is used by both C++ and C code.
#pragma once

// Marks a struct or class as packed.
#define PW_PACKED(declaration) declaration __attribute__((packed))

// Marks a function or object as used, ensuring code for it is generated.
#define PW_USED __attribute__((used))

// Prevents generation of a prologue or epilogue for a function. This is
// helpful when implementing the function in assembly.
#define PW_NO_PROLOGUE __attribute__((naked))

// Marks that a function declaration takes a printf-style format string and
// variadic arguments. This allows the compiler to perform check the validity of
// the format string and arguments. This macro must only be on the function
// declaration, not the definition.
//
// The format_index is index of the format string parameter and parameter_index
// is the starting index of the variadic arguments. Indices start at 1. For C++
// class member functions, add one to the index to account for the implicit this
// parameter.
//
// This example shows a function where the format string is argument 2 and the
// varargs start at argument 3.
//
//   int PrintfStyleFunction(char* buffer,
//                           const char* fmt, ...) PW_PRINTF_FORMAT(2,3);
//
//   int PrintfStyleFunction(char* buffer, const char* fmt, ...) {
//     ... implementation here ...
//   }
//
#define PW_PRINTF_FORMAT(format_index, parameter_index) \
  __attribute__((format(printf, format_index, parameter_index)))

// Places a variable in the specified linker section and directs the compiler
// to keep the variable, even if it is not used. Depending on the linker
// options, the linker may still remove this section if it is not declared in
// the linker script and marked KEEP.
#if __APPLE__
#define PW_KEEP_IN_SECTION(name) __attribute__((section("__DATA," name), used))
#else
#define PW_KEEP_IN_SECTION(name) __attribute__((section(name), used))
#endif  // __APPLE__
