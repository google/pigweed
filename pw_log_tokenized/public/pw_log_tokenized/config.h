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

#include <assert.h>

#include "pw_log/options.h"
#include "pw_preprocessor/concat.h"

// This macro takes the PW_LOG format string and optionally transforms it. By
// default, the PW_LOG_MODULE_NAME is prepended to the string if present.
#ifndef PW_LOG_TOKENIZED_FORMAT_STRING

#define PW_LOG_TOKENIZED_FORMAT_STRING(string) \
  PW_CONCAT(PW_LOG_TOKENIZED_FMT_, PW_LOG_MODULE_NAME_DEFINED)(string)

#define PW_LOG_TOKENIZED_FMT_0(string) string
#define PW_LOG_TOKENIZED_FMT_1(string) PW_LOG_MODULE_NAME " " string

#endif  // PW_LOG_TOKENIZED_FORMAT_STRING

// The log level, module token, and flag bits are packed into the tokenizer's
// payload argument, which is typically 32 bits. These macros specify the number
// of bits to use for each field.
#ifndef PW_LOG_TOKENIZED_LEVEL_BITS
#define PW_LOG_TOKENIZED_LEVEL_BITS 6
#endif  // PW_LOG_TOKENIZED_LEVEL_BITS

#ifndef PW_LOG_TOKENIZED_MODULE_BITS
#define PW_LOG_TOKENIZED_MODULE_BITS 16
#endif  // PW_LOG_TOKENIZED_MODULE_BITS

#ifndef PW_LOG_TOKENIZED_FLAG_BITS
#define PW_LOG_TOKENIZED_FLAG_BITS 10
#endif  // PW_LOG_TOKENIZED_FLAG_BITS

static_assert((PW_LOG_TOKENIZED_LEVEL_BITS + PW_LOG_TOKENIZED_MODULE_BITS +
               PW_LOG_TOKENIZED_FLAG_BITS) == 32,
              "Log metadata must fit in a 32-bit integer");

// The macro to use to tokenize the log and its arguments. Defaults to
// PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD. Projects may define their own
// version of this macro that uses a different underlying function, if desired.
#ifndef PW_LOG_TOKENIZED_ENCODE_MESSAGE
#define PW_LOG_TOKENIZED_ENCODE_MESSAGE \
  PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD
#endif  // PW_LOG_TOKENIZED_ENCODE_MESSAGE
