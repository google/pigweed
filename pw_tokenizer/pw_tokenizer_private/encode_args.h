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

#include <array>
#include <cstdarg>
#include <cstddef>
#include <span>

#include "pw_tokenizer/config.h"
#include "pw_tokenizer/internal/argument_types.h"
#include "pw_tokenizer/tokenize.h"

namespace pw {
namespace tokenizer {

// Buffer for encoding a tokenized string and arguments.
struct EncodedMessage {
  pw_tokenizer_Token token;
  std::array<uint8_t,
             PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES - sizeof(token)>
      args;
};

static_assert(PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES >=
                  sizeof(pw_tokenizer_Token),
              "PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES must be at least "
              "large enough for a token (4 bytes)");

static_assert(offsetof(EncodedMessage, args) == sizeof(EncodedMessage::token) &&
                  PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES ==
                      sizeof(EncodedMessage),
              "EncodedMessage should not have padding bytes between members");

// Encodes a tokenized string's arguments to a buffer. The
// _pw_tokenizer_ArgTypes parameter specifies the argument types, in place of a
// format string.
size_t EncodeArgs(_pw_tokenizer_ArgTypes types,
                  va_list args,
                  std::span<uint8_t> output);

}  // namespace tokenizer
}  // namespace pw
