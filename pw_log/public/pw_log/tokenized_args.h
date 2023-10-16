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
#pragma once

// pw_log backends that use pw_tokenizer and want to support nested tokenization
// define this file under their public_overrides/ directory to activate the
// PW_LOG_TOKEN aliases. If this file does not exist in the log backend,
// arguments behave as basic strings (const char*).
#if __has_include("pw_log_backend/log_backend_uses_pw_tokenizer.h")

#include "pw_tokenizer/nested_tokenization.h"
#include "pw_tokenizer/tokenize.h"

#define PW_LOG_TOKEN_TYPE pw_tokenizer_Token
#define PW_LOG_TOKEN PW_TOKENIZE_STRING
#define PW_LOG_TOKEN_EXPR PW_TOKENIZE_STRING_EXPR
#define PW_LOG_TOKEN_FMT PW_TOKEN_FMT

#else

/// If nested tokenization is supported by the logging backend, this is an
/// alias for `pw_tokenizer_Token`.
///
/// For non-tokenizing backends, defaults to `const char*`.
#define PW_LOG_TOKEN_TYPE const char*

/// If nested tokenization is supported by the logging backend, this is an
/// alias for `PW_TOKENIZE_STRING`. No-op otherwise.
#define PW_LOG_TOKEN(string_literal) string_literal

/// If nested tokenization is supported by the logging backend, this is an
/// alias for `PW_TOKENIZE_STRING_EXPR`. No-op otherwise.
#define PW_LOG_TOKEN_EXPR(string_literal) string_literal

/// If nested tokenization is supported by the logging backend, this is an
/// alias for `PW_TOKEN_FORMAT`.
///
/// For non-tokenizing backends, defaults to the string specifier `%s`.
#define PW_LOG_TOKEN_FMT() "%s"

#endif  //__has_include("log_backend/log_backend_uses_pw_tokenizer.h")
