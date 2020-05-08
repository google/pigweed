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

// This file defines the functions that encode tokenized logs at runtime. These
// are the only pw_tokenizer functions present in a binary that tokenizes
// strings. All other tokenizing code is resolved at compile time.

#include "pw_tokenizer/tokenize.h"

#include <cstring>

#include "pw_polyfill/language_features.h"  // static_assert
#include "pw_tokenizer_private/encode_args.h"

namespace pw {
namespace tokenizer {

extern "C" void _pw_TokenizeToBuffer(void* buffer,
                                     size_t* buffer_size_bytes,
                                     pw_TokenizerStringToken token,
                                     pw_TokenizerArgTypes types,
                                     ...) {
  if (*buffer_size_bytes < sizeof(token)) {
    *buffer_size_bytes = 0;
    return;
  }

  std::memcpy(buffer, &token, sizeof(token));

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes =
      EncodeArgs(types,
                 args,
                 span<uint8_t>(static_cast<uint8_t*>(buffer) + sizeof(token),
                               *buffer_size_bytes - sizeof(token)));
  va_end(args);

  *buffer_size_bytes = sizeof(token) + encoded_bytes;
}

extern "C" void _pw_TokenizeToCallback(
    void (*callback)(const uint8_t* encoded_message, size_t size_bytes),
    pw_TokenizerStringToken token,
    pw_TokenizerArgTypes types,
    ...) {
  EncodedMessage encoded;
  encoded.token = token;

  va_list args;
  va_start(args, types);
  const size_t encoded_bytes = EncodeArgs(types, args, encoded.args);
  va_end(args);

  callback(reinterpret_cast<const uint8_t*>(&encoded),
           sizeof(encoded.token) + encoded_bytes);
}

}  // namespace tokenizer
}  // namespace pw
