// Copyright 2019 The Pigweed Authors
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

// When compiling for host using MinGW, use gnu_printf() rather than printf()
// to support %z format specifiers.
#ifdef __USE_MINGW_ANSI_STDIO
#define _PW_PRINTF_FORMAT_TYPE gnu_printf
#else
#define _PW_PRINTF_FORMAT_TYPE printf
#endif  // __USE_MINGW_ANSI_STDIO

#define PW_PRINTF_FORMAT(format_index, parameter_index) \
  __attribute__((format(_PW_PRINTF_FORMAT_TYPE, format_index, parameter_index)))

// Places a variable in the specified linker section and directs the compiler
// to keep the variable, even if it is not used. Depending on the linker
// options, the linker may still remove this section if it is not declared in
// the linker script and marked KEEP.
#ifdef __APPLE__
#define PW_KEEP_IN_SECTION(name) __attribute__((section("__DATA," name), used))
#else
#define PW_KEEP_IN_SECTION(name) __attribute__((section(name), used))
#endif  // __APPLE__

// Indicate to the compiler that the annotated function won't return. Example:
//
//   PW_NO_RETURN void HandleAssertFailure(ErrorCode error_code);
//
#define PW_NO_RETURN __attribute__((noreturn))

// Prevents the compiler from inlining a fuction.
#define PW_NO_INLINE __attribute__((noinline))

// Indicate to the compiler that the given section of code will not be reached.
// Example:
//
//   int main() {
//     InitializeBoard();
//     vendor_StartScheduler();  // Note: vendor forgot noreturn attribute.
//     PW_UNREACHABLE;
//   }
//
#define PW_UNREACHABLE __builtin_unreachable()

// Indicate to a sanitizer compiler runtime to skip the named check in the
// associated function.
// Example:
//
//   uint32_t djb2(const void* buf, size_t len)
//       PW_NO_SANITIZE("unsigned-integer-overflow"){
//     uint32_t hash = 5381;
//     const uint8_t* u8 = static_cast<const uint8_t*>(buf);
//     for (size_t i = 0; i < len; ++i) {
//       hash = (hash * 33) + u8[i]; /* hash * 33 + c */
//     }
//     return hash;
//   }
#ifdef __clang__
#define PW_NO_SANITIZE(check) __attribute__((no_sanitize(check)))
#else
#define PW_NO_SANITIZE(check)
#endif  // __clang__
