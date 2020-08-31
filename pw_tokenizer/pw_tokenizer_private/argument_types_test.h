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

#include <stdint.h>

#include "pw_preprocessor/util.h"
#include "pw_tokenizer/internal/argument_types.h"

PW_EXTERN_C_START

_pw_tokenizer_ArgTypes pw_TestTokenizerNoArgs(void);

_pw_tokenizer_ArgTypes pw_TestTokenizerChar(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerUint8(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerUint16(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerInt32(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerInt64(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerUint64(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerFloat(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerDouble(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerString(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerMutableString(void);

_pw_tokenizer_ArgTypes pw_TestTokenizerIntFloat(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerUint64Char(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerStringString(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerUint16Int(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerFloatString(void);

_pw_tokenizer_ArgTypes pw_TestTokenizerNull(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerPointer(void);
_pw_tokenizer_ArgTypes pw_TestTokenizerPointerPointer(void);

PW_EXTERN_C_END
