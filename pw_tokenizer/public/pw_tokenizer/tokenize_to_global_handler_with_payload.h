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

#include "pw_preprocessor/util.h"
#include "pw_tokenizer/tokenize.h"

// Like PW_TOKENIZE_TO_GLOBAL_HANDLER, encodes a tokenized string and arguments
// to a buffer on the stack. The macro adds a payload argument, which is passed
// through to the global handler function
// pw_TokenizerHandleEncodedMessageWithPayload, which must be defined by the
// user of pw_tokenizer. The payload type is specified by the
// PW_TOKENIZER_CFG_PAYLOAD_TYPE option and defaults to void*.
//
// For example, the following tokenizes a log string and passes the log level as
// the payload.
/*
     #define LOG_ERROR(...) \
         PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD(kLogLevelError, __VA_ARGS__)

     void pw_TokenizerHandleEncodedMessageWithPayload(
         pw_TokenizerPayload log_level,
         const uint8_t encoded_message[],
         size_t size_bytes) {
       if (log_level >= kLogLevelWarning) {
         MyProject_EnqueueMessageForUart(buffer, size_bytes);
       }
     }
 */
#define PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD(payload, format, ...)      \
  do {                                                                        \
    _PW_TOKENIZE_STRING(format, __VA_ARGS__);                                 \
    pw_TokenizeToGlobalHandlerWithPayload(payload,                            \
                                          _pw_tokenizer_token,                \
                                          PW_TOKENIZER_ARG_TYPES(__VA_ARGS__) \
                                              PW_COMMA_ARGS(__VA_ARGS__));    \
  } while (0)

typedef uintptr_t pw_TokenizerPayload;

// This function must be defined pw_tokenizer:global_handler_with_payload
// backend. This function is called with the encoded message by
// pw_TokenizeToGlobalHandler and a caller-provided payload argument.
PW_EXTERN_C void pw_TokenizerHandleEncodedMessageWithPayload(
    pw_TokenizerPayload payload,
    const uint8_t encoded_message[],
    size_t size_bytes);

// This function encodes the tokenized strings. Do not call it directly;
// instead, use the PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD macro.
PW_EXTERN_C void pw_TokenizeToGlobalHandlerWithPayload(
    pw_TokenizerPayload payload,
    pw_TokenizerStringToken token,
    pw_TokenizerArgTypes types,
    ...);
