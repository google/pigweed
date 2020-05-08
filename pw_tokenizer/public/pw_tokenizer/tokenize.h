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
#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/concat.h"
#include "pw_preprocessor/macro_arg_count.h"
#include "pw_preprocessor/util.h"
#include "pw_tokenizer/internal/argument_types.h"
#include "pw_tokenizer/internal/tokenize_string.h"

// Strings may optionally be tokenized to a domain. Strings in different domains
// can be processed separately by the token database tools. Each domain in use
// must have a corresponding section declared in the linker script. See
// pw_tokenizer_linker_sections.ld for more details.
//
// If no domain is specified, this default is used.
#define PW_TOKENIZER_DEFAULT_DOMAIN "default"

// Tokenizes a string literal and converts it to a pw_TokenizerStringToken. This
// expression can be assigned to a local or global variable, but cannot be used
// in another expression. For example:
//
//   constexpr uint32_t global = PW_TOKENIZE_STRING("Wow!");  // This works.
//
//   void SomeFunction() {
//     constexpr uint32_t token = PW_TOKENIZE_STRING("Cool!");  // This works.
//
//     DoSomethingElse(PW_TOKENIZE_STRING("Lame!"));  // This does NOT work.
//   }
//
#define PW_TOKENIZE_STRING(string_literal) \
  PW_TOKENIZE_STRING_DOMAIN(PW_TOKENIZER_DEFAULT_DOMAIN, string_literal)

// Same as PW_TOKENIZE_STRING, but tokenizes to the specified domain.
#define PW_TOKENIZE_STRING_DOMAIN(domain, string_literal)                     \
  /* assign to a variable */ PW_TOKENIZER_STRING_TOKEN(string_literal);       \
                                                                              \
  /* Declare the format string as an array in the special tokenized string */ \
  /* section, which should be excluded from the final binary. Use __LINE__ */ \
  /* to create unique names for the section and variable, which avoids     */ \
  /* compiler warnings.                                                    */ \
  static _PW_TOKENIZER_CONST char PW_CONCAT(                                  \
      _pw_tokenizer_string_literal_DO_NOT_USE_,                               \
      __LINE__)[] _PW_TOKENIZER_SECTION(domain) = string_literal

// Encodes a tokenized string and arguments to the provided buffer. The size of
// the buffer is passed via a pointer to a size_t. After encoding is complete,
// the size_t is set to the number of bytes written to the buffer.
//
// The macro's arguments are equivalent to the following function signature:
//
//   TokenizeToBuffer(void* buffer,
//                    size_t* buffer_size_pointer,
//                    const char* format,
//                    ...);  /* printf-style arguments */
//
// For example, the following encodes a tokenized string with a temperature to a
// buffer. The buffer is passed to a function to send the message over a UART.
//
//   uint8_t buffer[32];
//   size_t size_bytes = sizeof(buffer);
//   PW_TOKENIZE_TO_BUFFER(
//       buffer, &size_bytes, "Temperature (C): %0.2f", temperature_c);
//   MyProject_EnqueueMessageForUart(buffer, size);
//
#define PW_TOKENIZE_TO_BUFFER(buffer, buffer_size_pointer, format, ...) \
  PW_TOKENIZE_TO_BUFFER_DOMAIN(PW_TOKENIZER_DEFAULT_DOMAIN,             \
                               buffer,                                  \
                               buffer_size_pointer,                     \
                               format,                                  \
                               __VA_ARGS__)

// Same as PW_TOKENIZE_TO_BUFFER, but tokenizes to the specified domain.
#define PW_TOKENIZE_TO_BUFFER_DOMAIN(                        \
    domain, buffer, buffer_size_pointer, format, ...)        \
  do {                                                       \
    _PW_TOKENIZE_FORMAT_STRING(domain, format, __VA_ARGS__); \
    _pw_TokenizeToBuffer(buffer,                             \
                         buffer_size_pointer,                \
                         _pw_tokenizer_token,                \
                         PW_TOKENIZER_ARG_TYPES(__VA_ARGS__) \
                             PW_COMMA_ARGS(__VA_ARGS__));    \
  } while (0)

// Encodes a tokenized string and arguments to a buffer on the stack. The
// provided callback is called with the encoded data. The size of the
// stack-allocated argument encoding buffer is set with the
// PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES option.
//
// The macro's arguments are equivalent to the following function signature:
//
//   TokenizeToCallback(void (*callback)(const uint8_t* data, size_t size),
//                      const char* format,
//                      ...);  /* printf-style arguments */
//
// For example, the following encodes a tokenized string with a sensor name and
// floating point data. The encoded message is passed directly to the
// MyProject_EnqueueMessageForUart function, which the caller provides as a
// callback.
//
//   void MyProject_EnqueueMessageForUart(const uint8_t* buffer,
//                                        size_t size_bytes) {
//     uart_queue_write(uart_instance, buffer, size_bytes);
//   }
//
//   void LogSensorValue(const char* sensor_name, float value) {
//     PW_TOKENIZE_TO_CALLBACK(MyProject_EnqueueMessageForUart,
//                             "%s: %f",
//                             sensor_name,
//                             value);
//   }
//
#define PW_TOKENIZE_TO_CALLBACK(callback, format, ...) \
  PW_TOKENIZE_TO_CALLBACK_DOMAIN(                      \
      PW_TOKENIZER_DEFAULT_DOMAIN, callback, format, __VA_ARGS__)

#define PW_TOKENIZE_TO_CALLBACK_DOMAIN(domain, callback, format, ...) \
  do {                                                                \
    _PW_TOKENIZE_FORMAT_STRING(domain, format, __VA_ARGS__);          \
    _pw_TokenizeToCallback(callback,                                  \
                           _pw_tokenizer_token,                       \
                           PW_TOKENIZER_ARG_TYPES(__VA_ARGS__)        \
                               PW_COMMA_ARGS(__VA_ARGS__));           \
  } while (0)

PW_EXTERN_C_START

// These functions encode the tokenized strings. These should not be called
// directly. Instead, use the corresponding PW_TOKENIZE_TO_* macros above.
void _pw_TokenizeToBuffer(void* buffer,
                          size_t* buffer_size_bytes,  // input and output arg
                          pw_TokenizerStringToken token,
                          pw_TokenizerArgTypes types,
                          ...);

void _pw_TokenizeToCallback(void (*callback)(const uint8_t* encoded_message,
                                             size_t size_bytes),
                            pw_TokenizerStringToken token,
                            pw_TokenizerArgTypes types,
                            ...);

// This empty function allows the compiler to check the format string.
inline void pw_TokenizerCheckFormatString(const char* format, ...)
    PW_PRINTF_FORMAT(1, 2);

inline void pw_TokenizerCheckFormatString(const char* format, ...) {
  PW_UNUSED(format);
}

PW_EXTERN_C_END

// These macros implement string tokenization. They should not be used directly;
// use one of the PW_TOKENIZE_* macros above instead.

// This macro takes a printf-style format string and corresponding arguments. It
// checks that the arguments are correct, stores the format string in a special
// section, and calculates the string's token at compile time.
// clang-format off
#define _PW_TOKENIZE_FORMAT_STRING(domain, format, ...)                     \
  if (0) { /* Do not execute to prevent double evaluation of the arguments. */ \
    pw_TokenizerCheckFormatString(format PW_COMMA_ARGS(__VA_ARGS__));          \
  }                                                                            \
                                                                               \
  /* Check that the macro is invoked with a supported number of arguments. */  \
  static_assert(                                                               \
      PW_ARG_COUNT(__VA_ARGS__) <= PW_TOKENIZER_MAX_SUPPORTED_ARGS,            \
      "Tokenized strings cannot have more than "                               \
      PW_STRINGIFY(PW_TOKENIZER_MAX_SUPPORTED_ARGS) " arguments; "             \
      PW_STRINGIFY(PW_ARG_COUNT(__VA_ARGS__)) " arguments were used for "      \
      #format " (" #__VA_ARGS__ ")");                                          \
                                                                               \
  /* Tokenize the string to a pw_TokenizerStringToken at compile time. */      \
  _PW_TOKENIZER_CONST pw_TokenizerStringToken _pw_tokenizer_token =            \
      PW_TOKENIZE_STRING_DOMAIN(domain, format)

// clang-format on

#ifdef __cplusplus  // use constexpr for C++
#define _PW_TOKENIZER_CONST constexpr
#else  // use const for C
#define _PW_TOKENIZER_CONST const
#endif  // __cplusplus

// _PW_TOKENIZER_SECTION places the format string in a special .pw_tokenized
// linker section. Host-side decoding tools read the strings from this section
// to build a database of tokenized strings.
//
// This section should be declared as type INFO so that it is excluded from the
// final binary. To declare the section, as well as the .pw_tokenizer_info
// metadata section, add the following to the linker script's SECTIONS command:
//
//   .pw_tokenizer_info 0x0 (INFO) :
//   {
//     KEEP(*(.pw_atokenizer_info))
//   }
//
//   .pw_tokenized.default 0x0 (INFO) :
//   {
//     KEEP(*(.pw_tokenized.default.*))
//   }
//
//
// If custom tokenization domains are used, a section must be declared for each
// domain:
//
//   .pw_tokenized.YOUR_CUSTOM_TOKENIZATION_DOMAIN 0x0 (INFO) :
//   {
//     KEEP(*(.pw_tokenized.YOUR_CUSTOM_TOKENIZATION_DOMAIN.*))
//   }
//
// A linker script snippet that provides these sections is provided in the file
// pw_tokenizer_linker_sections.ld. This file may be directly included into
// existing linker scripts.
//
// The tokenized string sections can also be managed without linker script
// modifications, though this is not recommended. The section can be extracted
// and removed from the ELF with objcopy:
//
//   objcopy --only-section .pw_tokenize* <ORIGINAL_ELF> <OUTPUT_ELF>
//   objcopy --remove-section .pw_tokenize* <ORIGINAL_ELF>
//
// OUTPUT_ELF will be an ELF with only the tokenized strings, and the original
// ELF file will have the sections removed.
//
// Without the above linker script modifications, the section garbage collection
// option (--gc-sections) removes the tokenized string sections. To avoid
// editing the target linker script, a separate metadata ELF can be linked
// without --gc-sections to preserve the tokenized data.
//
// pw_tokenizer is intended for use with ELF files only. Mach-O files (macOS
// executables) do not support section names longer than 16 characters, so a
// short, dummy section name is used on macOS.
#if __APPLE__
#define _PW_TOKENIZER_SECTION(unused_domain) \
  PW_KEEP_IN_SECTION(".pw." PW_STRINGIFY(__LINE__))
#else
#define _PW_TOKENIZER_SECTION(domain) \
  PW_KEEP_IN_SECTION(".pw_tokenized." domain "." PW_STRINGIFY(__LINE__))
#endif  // __APPLE__
