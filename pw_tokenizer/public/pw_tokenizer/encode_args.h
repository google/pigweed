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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "pw_polyfill/standard.h"
#include "pw_preprocessor/util.h"
#include "pw_tokenizer/internal/argument_types.h"

#if PW_CXX_STANDARD_IS_SUPPORTED(17)

#include <cstring>

#include "pw_span/span.h"
#include "pw_tokenizer/config.h"
#include "pw_tokenizer/tokenize.h"

namespace pw {
namespace tokenizer {

/// Encodes a tokenized string's arguments to a buffer. The
/// @cpp_type{pw_tokenizer_ArgTypes} parameter specifies the argument types, in
/// place of a format string.
///
/// Most tokenization implementations should use the @cpp_class{EncodedMessage}
/// class.
size_t EncodeArgs(pw_tokenizer_ArgTypes types,
                  va_list args,
                  span<std::byte> output);

/// Encodes a tokenized message to a fixed size buffer. By default, the buffer
/// size is set by the @c_macro{PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES}
/// config macro. This class is used to encode tokenized messages passed in from
/// tokenization macros.
///
/// To use `pw::tokenizer::EncodedMessage`, construct it with the token,
/// argument types, and `va_list` from the variadic arguments:
///
/// @code{.cpp}
///   void SendLogMessage(span<std::byte> log_data);
///
///   extern "C" void TokenizeToSendLogMessage(pw_tokenizer_Token token,
///                                            pw_tokenizer_ArgTypes types,
///                                            ...) {
///     va_list args;
///     va_start(args, types);
///     EncodedMessage encoded_message(token, types, args);
///     va_end(args);
///
///     SendLogMessage(encoded_message);  // EncodedMessage converts to span
///   }
/// @endcode
template <size_t kMaxSizeBytes = PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES>
class EncodedMessage {
 public:
  // Encodes a tokenized message to an internal buffer.
  EncodedMessage(pw_tokenizer_Token token,
                 pw_tokenizer_ArgTypes types,
                 va_list args) {
    std::memcpy(data_, &token, sizeof(token));
    size_ =
        sizeof(token) +
        EncodeArgs(types, args, span<std::byte>(data_).subspan(sizeof(token)));
  }

  /// The binary-encoded tokenized message.
  const std::byte* data() const { return data_; }

  /// Returns `data()` as a pointer to `uint8_t` instead of `std::byte`.
  const uint8_t* data_as_uint8() const {
    return reinterpret_cast<const uint8_t*>(data());
  }

  /// The size of the encoded tokenized message in bytes.
  size_t size() const { return size_; }

 private:
  static_assert(kMaxSizeBytes >= sizeof(pw_tokenizer_Token),
                "The encoding buffer must be at least large enough for a token "
                "(4 bytes)");

  std::byte data_[kMaxSizeBytes];
  size_t size_;
};

}  // namespace tokenizer
}  // namespace pw

#endif  // PW_CXX_STANDARD_IS_SUPPORTED(17)

PW_EXTERN_C_START

/// C function that encodes arguments to a tokenized buffer. Use the
/// @cpp_func{pw::tokenizer::EncodeArgs} function from C++.
size_t pw_tokenizer_EncodeArgs(pw_tokenizer_ArgTypes types,
                               va_list args,
                               void* output_buffer,
                               size_t output_buffer_size);
PW_EXTERN_C_END
