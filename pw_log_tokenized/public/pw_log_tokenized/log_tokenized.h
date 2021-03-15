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
#include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"

// This macro implements PW_LOG using
// PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD or an equivalent alternate macro
// provided by PW_LOG_TOKENIZED_ENCODE_MESSAGE. The log level, module token, and
// flags are packed into the payload argument.
//
// Two strings are tokenized in this macro:
//
//   - The log format string, tokenized in the default tokenizer domain.
//   - PW_LOG_MODULE_NAME, masked to 16 bits and tokenized in the
//     "pw_log_module_names" tokenizer domain.
//
// To use this macro, implement pw_tokenizer_HandleEncodedMessageWithPayload,
// which is defined in pw_tokenizer/tokenize.h. The log metadata can be accessed
// using pw::log_tokenized::Metadata. For example:
//
//   extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
//       pw_tokenizer_Payload payload, const uint8_t data[], size_t size) {
//     pw::log_tokenized::Metadata metadata(payload);
//
//     if (metadata.level() >= kLogLevel && ModuleEnabled(metadata.module())) {
//       EmitLogMessage(data, size, metadata.flags());
//     }
//   }
//
#define PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(                     \
    level, flags, message, ...)                                              \
  do {                                                                       \
    _PW_TOKENIZER_CONST uintptr_t _pw_log_module_token =                     \
        PW_TOKENIZE_STRING_MASK("pw_log_module_names",                       \
                                ((1u << PW_LOG_TOKENIZED_MODULE_BITS) - 1u), \
                                PW_LOG_MODULE_NAME);                         \
    PW_LOG_TOKENIZED_ENCODE_MESSAGE(                                         \
        ((uintptr_t)(level) |                                                \
         (_pw_log_module_token << PW_LOG_TOKENIZED_LEVEL_BITS) |             \
         ((uintptr_t)(flags)                                                 \
          << (PW_LOG_TOKENIZED_LEVEL_BITS + PW_LOG_TOKENIZED_MODULE_BITS))), \
        PW_LOG_TOKENIZED_FORMAT_STRING(message),                             \
        __VA_ARGS__);                                                        \
  } while (0)

#ifdef __cplusplus

namespace pw {
namespace log_tokenized {
namespace internal {

// This class, which is aliased to pw::log_tokenized::Metadata below, is used to
// access the log metadata packed into the tokenizer's payload argument.
template <unsigned kLevelBits,
          unsigned kModuleBits,
          unsigned kFlagBits,
          typename T = uintptr_t>
class GenericMetadata {
 public:
  template <T log_level, T module, T flags>
  static constexpr GenericMetadata Set() {
    static_assert(log_level < (1 << kLevelBits), "The level is too large!");
    static_assert(module < (1 << kModuleBits), "The module is too large!");
    static_assert(flags < (1 << kFlagBits), "The flags are too large!");

    return GenericMetadata(log_level | (module << kLevelBits) |
                           (flags << (kModuleBits + kLevelBits)));
  }

  constexpr GenericMetadata(T value) : bits_(value) {}

  // The log level of this message.
  constexpr T level() const { return bits_ & Mask<kLevelBits>(); }

  // The 16 bit tokenized version of the module name (PW_LOG_MODULE_NAME).
  constexpr T module() const {
    return (bits_ >> kLevelBits) & Mask<kModuleBits>();
  }

  // The flags provided to the log call.
  constexpr T flags() const {
    return (bits_ >> (kLevelBits + kModuleBits)) & Mask<kFlagBits>();
  }

 private:
  template <int bits>
  static constexpr T Mask() {
    return (T(1) << bits) - 1;
  }

  T bits_;

  static_assert(kLevelBits + kModuleBits + kFlagBits <= sizeof(bits_) * 8);
};

}  // namespace internal

using Metadata = internal::GenericMetadata<PW_LOG_TOKENIZED_LEVEL_BITS,
                                           PW_LOG_TOKENIZED_MODULE_BITS,
                                           PW_LOG_TOKENIZED_FLAG_BITS>;

}  // namespace log_tokenized
}  // namespace pw

#endif  // __cpluplus
