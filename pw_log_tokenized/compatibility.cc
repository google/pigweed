// Copyright 2023 The Pigweed Authors
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

// If the project is still using pw_tokenizer's global handler with payload
// facade, then define its functions as used by pw_log_tokenized.

#include <cstdarg>

#include "pw_log_tokenized/handler.h"
#include "pw_preprocessor/compiler.h"
#include "pw_tokenizer/encode_args.h"
#include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"

// If the new API is in use, define pw_tokenizer_HandleEncodedMessageWithPayload
// to redirect to it, in case there are any direct calls to it. Only projects
// that use the base64_over_hdlc backend will have been updated to the new API.
#if PW_LOG_TOKENIZED_BACKEND_USES_NEW_API

extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
    uint32_t metadata, const uint8_t encoded_message[], size_t size_bytes) {
  pw_log_tokenized_HandleLog(metadata, encoded_message, size_bytes);
}

#else  // If the new API is not in use, implement it to redirect to the old API.

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                           const uint8_t encoded_message[],
                                           size_t size_bytes) {
  pw_tokenizer_HandleEncodedMessageWithPayload(
      metadata, encoded_message, size_bytes);
}

#endif  // PW_LOG_TOKENIZED_BACKEND_USES_NEW_API

// Implement the global tokenized log handler function, which is identical
// This function is the same as _pw_log_tokenized_EncodeTokenizedLog().
extern "C" void _pw_tokenizer_ToGlobalHandlerWithPayload(
    uint32_t metadata,
    pw_tokenizer_Token token,
    pw_tokenizer_ArgTypes types,
    ...) {
  va_list args;
  va_start(args, types);
  pw::tokenizer::EncodedMessage<> encoded_message(token, types, args);
  va_end(args);

  pw_log_tokenized_HandleLog(
      metadata, encoded_message.data_as_uint8(), encoded_message.size());
}
