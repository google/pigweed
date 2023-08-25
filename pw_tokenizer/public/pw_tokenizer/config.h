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

// Configuration macros for the tokenizer module.
#pragma once

/// For a tokenized string with arguments, the types of the arguments are
/// encoded in either 4 bytes (`uint32_t`) or 8 bytes (`uint64_t`). 4 bytes
/// supports up to 14 tokenized string arguments; 8 bytes supports up to 29
/// arguments. Using 8 bytes increases code size for 32-bit machines.
///
/// Argument types are encoded two bits per argument, in little-endian order.
/// The 4 or 6 least-significant bits, respectively, store the number of
/// arguments, while the remaining bits encode the argument types.
#ifndef PW_TOKENIZER_CFG_ARG_TYPES_SIZE_BYTES
#define PW_TOKENIZER_CFG_ARG_TYPES_SIZE_BYTES 4
#endif  // PW_TOKENIZER_CFG_ARG_TYPES_SIZE_BYTES

/// Maximum number of characters to hash in C. In C code, strings shorter than
/// this length are treated as if they were zero-padded up to the length.
/// Strings that are the same length and share a common prefix longer than this
/// value hash to the same value. Increasing `PW_TOKENIZER_CFG_C_HASH_LENGTH`
/// increases the compilation time for C due to the complexity of the hashing
/// macros.
///
/// `PW_TOKENIZER_CFG_C_HASH_LENGTH` has no effect on C++ code. In C++, hashing
/// is done with a `constexpr` function instead of a macro. There are no string
/// length limitations and compilation times are unaffected by this macro.
///
/// Only hash lengths for which there is a corresponding macro header
/// (`pw_tokenizer/internal/pw_tokenizer_65599_fixed_length_#_hash_macro.`) are
/// supported. Additional macros may be generated with the
/// `generate_hash_macro.py` function. New macro headers must then be added to
/// `pw_tokenizer/internal/tokenize_string.h`.
///
/// This MUST match the value of `DEFAULT_C_HASH_LENGTH` in
/// `pw_tokenizer/py/pw_tokenizer/tokens.py`.
#ifndef PW_TOKENIZER_CFG_C_HASH_LENGTH
#define PW_TOKENIZER_CFG_C_HASH_LENGTH 128
#endif  // PW_TOKENIZER_CFG_C_HASH_LENGTH

/// `PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES` is deprecated. It is used as
/// the default value for pw_log_tokenized's
/// @c_macro{PW_LOG_TOKENIZED_ENCODING_BUFFER_SIZE_BYTES}. This value should not
/// be configured; set @c_macro{PW_LOG_TOKENIZED_ENCODING_BUFFER_SIZE_BYTES}
/// instead.
#ifndef PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES
#define PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES 52
#endif  // PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES
