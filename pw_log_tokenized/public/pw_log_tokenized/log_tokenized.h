// Copyright 2021 The Pigweed Authors
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

#include <stdint.h>

#include "pw_log_tokenized/config.h"
#include "pw_preprocessor/util.h"
#include "pw_tokenizer/tokenize.h"

// TODO(hepler): Remove these includes.
#ifdef __cplusplus
#include "pw_log_tokenized/metadata.h"
#endif  // __cplusplus

#ifdef _PW_LOG_TOKENIZED_GLOBAL_HANDLER_BACKWARDS_COMPAT
#include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"
#endif  // _PW_LOG_TOKENIZED_GLOBAL_HANDLER_BACKWARDS_COMPAT

#undef _PW_LOG_TOKENIZED_GLOBAL_HANDLER_BACKWARDS_COMPAT

// This macro implements PW_LOG using pw_tokenizer. Users must implement
// pw_log_tokenized_HandleLog(uint32_t metadata, uint8_t* buffer, size_t size).
// The log level, module token, and flags are packed into the metadata argument.
//
// Two strings are tokenized in this macro:
//
//   - The log format string, tokenized in the default tokenizer domain.
//   - Log module name, masked to 16 bits and tokenized in the
//     "pw_log_module_names" tokenizer domain.
//
// To use this macro, implement pw_log_tokenized_HandleLog(), which is defined
// in pw_log_tokenized/handler.h. The log metadata can be accessed using
// pw::log_tokenized::Metadata. For example:
//
//   extern "C" void pw_log_tokenized_HandleLog(
//       uint32_t payload, const uint8_t data[], size_t size) {
//     pw::log_tokenized::Metadata metadata(payload);
//
//     if (metadata.level() >= kLogLevel && ModuleEnabled(metadata.module())) {
//       EmitLogMessage(data, size, metadata.flags());
//     }
//   }
//
#define PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(                     \
    level, module, flags, message, ...)                                      \
  do {                                                                       \
    _PW_TOKENIZER_CONST uintptr_t _pw_log_tokenized_module_token =           \
        PW_TOKENIZE_STRING_MASK("pw_log_module_names",                       \
                                ((1u << PW_LOG_TOKENIZED_MODULE_BITS) - 1u), \
                                module);                                     \
    const uintptr_t _pw_log_tokenized_level = level;                         \
    PW_LOG_TOKENIZED_ENCODE_MESSAGE(                                         \
        (_PW_LOG_TOKENIZED_LEVEL(_pw_log_tokenized_level) |                  \
         _PW_LOG_TOKENIZED_MODULE(_pw_log_tokenized_module_token) |          \
         _PW_LOG_TOKENIZED_FLAGS(flags) | _PW_LOG_TOKENIZED_LINE(__LINE__)), \
        PW_LOG_TOKENIZED_FORMAT_STRING(message),                             \
        __VA_ARGS__);                                                        \
  } while (0)

// If the level field is present, clamp it to the maximum value.
#if PW_LOG_TOKENIZED_LEVEL_BITS == 0
#define _PW_LOG_TOKENIZED_LEVEL(value) ((uintptr_t)0)
#else
#define _PW_LOG_TOKENIZED_LEVEL(value)                   \
  (value < ((uintptr_t)1 << PW_LOG_TOKENIZED_LEVEL_BITS) \
       ? value                                           \
       : ((uintptr_t)1 << PW_LOG_TOKENIZED_LEVEL_BITS) - 1)
#endif  // PW_LOG_TOKENIZED_LEVEL_BITS

// If the line number field is present, shift it to its position. Set it to zero
// if the line number is too large for PW_LOG_TOKENIZED_LINE_BITS.
#if PW_LOG_TOKENIZED_LINE_BITS == 0
#define _PW_LOG_TOKENIZED_LINE(line) ((uintptr_t)0)
#else
#define _PW_LOG_TOKENIZED_LINE(line)                                \
  ((uintptr_t)(line < (1 << PW_LOG_TOKENIZED_LINE_BITS) ? line : 0) \
   << PW_LOG_TOKENIZED_LEVEL_BITS)
#endif  // PW_LOG_TOKENIZED_LINE_BITS

// If the flags field is present, mask it and shift it to its position.
#if PW_LOG_TOKENIZED_FLAG_BITS == 0
#define _PW_LOG_TOKENIZED_FLAGS(value) ((uintptr_t)0)
#else
#define _PW_LOG_TOKENIZED_FLAGS(value)                                       \
  (((uintptr_t)(value) & (((uintptr_t)1 << PW_LOG_TOKENIZED_FLAG_BITS) - 1)) \
   << (PW_LOG_TOKENIZED_LEVEL_BITS + PW_LOG_TOKENIZED_LINE_BITS))
#endif  // PW_LOG_TOKENIZED_FLAG_BITS

// If the module field is present, shift it to its position.
#if PW_LOG_TOKENIZED_MODULE_BITS == 0
#define _PW_LOG_TOKENIZED_MODULE(value) ((uintptr_t)0)
#else
#define _PW_LOG_TOKENIZED_MODULE(value)                  \
  ((uintptr_t)(value) << ((PW_LOG_TOKENIZED_LEVEL_BITS + \
                           PW_LOG_TOKENIZED_LINE_BITS +  \
                           PW_LOG_TOKENIZED_FLAG_BITS)))
#endif  // PW_LOG_TOKENIZED_MODULE_BITS

#define PW_LOG_TOKENIZED_ENCODE_MESSAGE(metadata, format, ...)               \
  do {                                                                       \
    PW_TOKENIZE_FORMAT_STRING(                                               \
        PW_TOKENIZER_DEFAULT_DOMAIN, UINT32_MAX, format, __VA_ARGS__);       \
    _pw_log_tokenized_EncodeTokenizedLog(metadata,                           \
                                         _pw_tokenizer_token,                \
                                         PW_TOKENIZER_ARG_TYPES(__VA_ARGS__) \
                                             PW_COMMA_ARGS(__VA_ARGS__));    \
  } while (0)

PW_EXTERN_C_START

void _pw_log_tokenized_EncodeTokenizedLog(uint32_t metadata,
                                          pw_tokenizer_Token token,
                                          pw_tokenizer_ArgTypes types,
                                          ...);

PW_EXTERN_C_END
