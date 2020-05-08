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

#include <stddef.h>
#include <stdint.h>

#include "pw_preprocessor/util.h"
#include "pw_tokenizer/tokenize.h"

// Encodes a tokenized string and arguments to a buffer on the stack. The buffer
// is passed to the user-defined pw_TokenizerHandleEncodedMessage function. The
// size of the stack-allocated argument encoding buffer is set with the
// PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES option.
//
// The macro's arguments are equivalent to the following function signature:
//
//   TokenizeToGlobalHandler(const char* format,
//                           ...);  /* printf-style arguments */
//
// For example, the following encodes a tokenized string with a value returned
// from a function call. The encoded message is passed to the caller-defined
// pw_TokenizerHandleEncodedMessage function.
//
//   void OutputLastReadSize() {
//     PW_TOKENIZE_TO_GLOBAL_HANDLER("Read %u bytes", ReadSizeBytes());
//   }
//
//   void pw_TokenizerHandleEncodedMessage(const uint8_t encoded_message[],
//                                         size_t size_bytes) {
//     MyProject_EnqueueMessageForUart(buffer, size_bytes);
//   }
//
#define PW_TOKENIZE_TO_GLOBAL_HANDLER(format, ...) \
  PW_TOKENIZE_TO_GLOBAL_HANDLER_DOMAIN(            \
      PW_TOKENIZER_DEFAULT_DOMAIN, format, __VA_ARGS__)

// Same as PW_TOKENIZE_TO_GLOBAL_HANDLER, but tokenizes to the specified domain.
#define PW_TOKENIZE_TO_GLOBAL_HANDLER_DOMAIN(domain, format, ...)   \
  do {                                                              \
    _PW_TOKENIZE_FORMAT_STRING(domain, format, __VA_ARGS__);        \
    _pw_TokenizeToGlobalHandler(_pw_tokenizer_token,                \
                                PW_TOKENIZER_ARG_TYPES(__VA_ARGS__) \
                                    PW_COMMA_ARGS(__VA_ARGS__));    \
  } while (0)

PW_EXTERN_C_START

// This function must be defined by the pw_tokenizer:global_handler backend.
// This function is called with the encoded message by
// pw_TokenizeToGlobalHandler.
void pw_TokenizerHandleEncodedMessage(const uint8_t encoded_message[],
                                      size_t size_bytes);

// This function encodes the tokenized strings. Do not call it directly;
// instead, use the PW_TOKENIZE_TO_GLOBAL_HANDLER macro.
void _pw_TokenizeToGlobalHandler(pw_TokenizerStringToken token,
                                 pw_TokenizerArgTypes types,
                                 ...);

PW_EXTERN_C_END
