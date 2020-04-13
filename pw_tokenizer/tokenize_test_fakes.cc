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

// This file provide stub implementations for the function projects are expected
// to provide when PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER is set.

#include <cstddef>
#include <cstdint>

#include "pw_tokenizer/tokenize.h"

#if PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER

PW_EXTERN_C void pw_TokenizerHandleEncodedMessage(
    const uint8_t encoded_message[], size_t size_bytes) {
  PW_UNUSED(encoded_message[0]);
  PW_UNUSED(size_bytes);
}

PW_EXTERN_C void pw_TokenizerHandleEncodedMessageWithPayload(
    pw_TokenizerPayload payload,
    const uint8_t encoded_message[],
    size_t size_bytes) {
  PW_UNUSED(payload);
  PW_UNUSED(encoded_message[0]);
  PW_UNUSED(size_bytes);
}

#endif  // PW_TOKENIZER_CFG_ENABLE_TOKENIZE_TO_GLOBAL_HANDLER
