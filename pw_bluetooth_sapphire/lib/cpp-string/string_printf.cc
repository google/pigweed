// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <cpp-string/string_printf.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

namespace bt_lib_cpp_string {
namespace {

void StringVAppendfHelper(std::string* dest, const char* format, va_list ap) {
  // Size of the small stack buffer to use. This should be kept in sync
  // with the numbers in StringPrintfTest.
  constexpr size_t kStackBufferSize = 1024u;

  char stack_buf[kStackBufferSize];
  // |result| is the number of characters that would have been written if
  // kStackBufferSize were sufficiently large, not counting the terminating null
  // character. |vsnprintf()| always null-terminates!
  int result = vsnprintf(stack_buf, kStackBufferSize, format, ap);
  if (result < 0) {
    // As far as I can tell, we'd only get |EOVERFLOW| if the result is so large
    // that it can't be represented by an |int| (in which case retrying would be
    // futile), so Chromium's implementation is wrong.
    return;
  }

  // Only append what fit into our stack buffer.
  // Strings that are too long will be truncated.
  size_t actual_len_excluding_null = result;
  if (actual_len_excluding_null > kStackBufferSize - 1) {
    actual_len_excluding_null = kStackBufferSize - 1;
  }
  dest->append(stack_buf, actual_len_excluding_null);
}

}  // namespace

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string rv;
  StringVAppendf(&rv, format, ap);
  va_end(ap);
  return rv;
}

std::string StringVPrintf(const char* format, va_list ap) {
  std::string rv;
  StringVAppendf(&rv, format, ap);
  return rv;
}

void StringAppendf(std::string* dest, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringVAppendf(dest, format, ap);
  va_end(ap);
}

void StringVAppendf(std::string* dest, const char* format, va_list ap) {
  int old_errno = errno;
  StringVAppendfHelper(dest, format, ap);
  errno = old_errno;
}

}  // namespace bt_lib_cpp_string
