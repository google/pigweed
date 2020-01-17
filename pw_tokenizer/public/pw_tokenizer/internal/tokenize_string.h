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

// Selects the hash macro implementation to use. The implementation selected
// depends on the language (C or C++) and value of PW_TOKENIZER_CFG_HASH_LENGTH.
// The options are:
//
//   - C++ hash constexpr function, which works for any hash length
//   - C 80-character hash macro
//   - C 96-character hash macro
//   - C 128-character hash macro
//
// C hash macros for other lengths may be generated using generate_hash_macro.py
// and added to this file.
#pragma once

#include <stdint.h>

// In C++17, use a constexpr function to calculate the hash.
#if __cpp_constexpr >= 201304L && defined(__cpp_inline_variables)

#include "pw_tokenizer/pw_tokenizer_65599_fixed_length_hash.h"

#define PW_TOKENIZER_STRING_TOKEN(format)                \
  pw::tokenizer::PwTokenizer65599FixedLengthHash(        \
      std::string_view((format), sizeof(format "") - 1), \
      PW_TOKENIZER_CFG_HASH_LENGTH)

#else  // In C or older C++ code, use the hashing macro.

#if PW_TOKENIZER_CFG_HASH_LENGTH == 80

#include "pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_80_hash_macro.h"
#define PW_TOKENIZER_STRING_TOKEN PW_TOKENIZER_65599_FIXED_LENGTH_80_HASH

#elif PW_TOKENIZER_CFG_HASH_LENGTH == 96

#include "pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_96_hash_macro.h"
#define PW_TOKENIZER_STRING_TOKEN PW_TOKENIZER_65599_FIXED_LENGTH_96_HASH

#elif PW_TOKENIZER_CFG_HASH_LENGTH == 128

#include "pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_128_hash_macro.h"
#define PW_TOKENIZER_STRING_TOKEN PW_TOKENIZER_65599_FIXED_LENGTH_128_HASH

#else  // unsupported hash length

// Only hash lengths for which there is a corresponding macro header
// (pw_tokenizer/internal/mash_macro_#.h) are supported. Additional macros may
// be generated with the generate_hash_macro.py function. New macro headers must
// be added to this file.
#error "Unsupported value for PW_TOKENIZER_CFG_HASH_LENGTH"

#endif  // PW_TOKENIZER_CFG_HASH_LENGTH

#endif  // __cpp_constexpr >= 201304L && defined(__cpp_inline_variables)

// The type of the token used in place of a format string.
typedef uint32_t pw_TokenizerStringToken;
