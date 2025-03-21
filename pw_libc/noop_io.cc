// Copyright 2025 The Pigweed Authors
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

#include <cstddef>
#include <cstdio>

#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/util.h"

PW_EXTERN_C_START

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextern-c-compat"

struct __llvm_libc_stdio_cookie {};

#pragma clang diagnostic pop

struct __llvm_libc_stdio_cookie __llvm_libc_stdin_cookie;
struct __llvm_libc_stdio_cookie __llvm_libc_stdout_cookie;
struct __llvm_libc_stdio_cookie __llvm_libc_stderr_cookie;

ssize_t __llvm_libc_stdio_read(void*, char*, size_t) { return 0; }

ssize_t __llvm_libc_stdio_write(void*, const char*, size_t) { return 0; }

PW_EXTERN_C_END
