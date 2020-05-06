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
#include <stdint.h>

#include "pw_preprocessor/util.h"
#include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"

// This macro implements PW_LOG, using
// PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD. The log level, module token, and
// flags are packed into the payload argument.
//
// To use this macro, implement pw_TokenizerHandleEncodedMessageWithPayload,
// which is defined in pw_tokenizer/tokenize.h. The log metadata can be accessed
// using pw::log_tokenized::Metadata. For example:
//
//   extern "C" void pw_TokenizerHandleEncodedMessageWithPayload(
//       pw_TokenizerPayload payload, const uint8_t data[], size_t size) {
//     pw::log_tokenized::Metadata metadata(payload);
//
//     if (metadata.level() >= kLogLevel && ModuleEnabled(metadata.module())) {
//       EmitLogMessage(data, size, metadata.flags());
//     }
//   }
//
// TODO(hepler): Pack the hashed version of the module name into the payload.
#define PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(              \
    level, flags, message, ...)                                       \
  PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD(                         \
      ((uintptr_t)(level) | 0u /* TODO(hepler): add module token */ | \
       ((uintptr_t)(flags) << (_PW_LOG_TOKENIZED_LEVEL_BITS +         \
                               _PW_LOG_TOKENIZED_MODULE_BITS))),      \
      message,                                                        \
      __VA_ARGS__)

#ifndef PW_LOG_TOKENIZED_FORMAT_STRING
#define PW_LOG_TOKENIZED_FORMAT_STRING(string) PW_LOG_MODULE_NAME ": " string
#endif  // PW_LOG_TOKENIZED_FORMAT_STRING

#ifdef __cplusplus

namespace pw {
namespace log_tokenized {
namespace internal {

// This class, which is aliased to pw::log_tokenized::Metadata below, is used to
// access the log metadata packed into the tokenizer's payload argument.
template <unsigned level_bits,
          unsigned module_bits,
          unsigned flag_bits,
          typename T = uintptr_t>
class GenericMetadata {
 public:
  template <T log_level, T module, T flags>
  static constexpr GenericMetadata Set() {
    static_assert(log_level < (1 << level_bits), "The level is too large!");
    static_assert(module < (1 << module_bits), "The module is too large!");
    static_assert(flags < (1 << flag_bits), "The flags are too large!");

    return GenericMetadata(log_level | (module << level_bits) |
                           (flags << (module_bits + level_bits)));
  }

  constexpr GenericMetadata(T value) : bits_(value) {}

  // The log level of this message.
  constexpr T level() const { return bits_ & Mask<level_bits>(); }

  // The 16 bit tokenized version of the module name (PW_LOG_MODULE_NAME).
  constexpr T module() const {
    return (bits_ >> level_bits) & Mask<module_bits>();
  }

  // The flags provided to the log call.
  constexpr T flags() const {
    return (bits_ >> (level_bits + module_bits)) & Mask<flag_bits>();
  }

 private:
  template <int bits>
  static constexpr T Mask() {
    return (T(1) << bits) - 1;
  }

  T bits_;

  static_assert(level_bits + module_bits + flag_bits <= sizeof(bits_) * 8);
};

// The log level, module token, and flag bits are packed into the tokenizer's
// payload argument, which is typically 32 bits. These macros specify the number
// of bits to use for each field.
#define _PW_LOG_TOKENIZED_LEVEL_BITS 6
#define _PW_LOG_TOKENIZED_MODULE_BITS 16
#define _PW_LOG_TOKENIZED_FLAG_BITS 10

static_assert((_PW_LOG_TOKENIZED_LEVEL_BITS + _PW_LOG_TOKENIZED_MODULE_BITS +
               _PW_LOG_TOKENIZED_FLAG_BITS) == 32,
              "Log metadata must fit in a 32-bit integer");

}  // namespace internal

using Metadata = internal::GenericMetadata<_PW_LOG_TOKENIZED_LEVEL_BITS,
                                           _PW_LOG_TOKENIZED_MODULE_BITS,
                                           _PW_LOG_TOKENIZED_FLAG_BITS>;

}  // namespace log_tokenized
}  // namespace pw

#endif  // __cpluplus
