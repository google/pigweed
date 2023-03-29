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

#include "pw_string/format.h"

#include <cstdio>

namespace pw::string {

StatusWithSize Format(span<char> buffer, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const StatusWithSize result = FormatVaList(buffer, format, args);
  va_end(args);

  return result;
}

StatusWithSize FormatVaList(span<char> buffer,
                            const char* format,
                            va_list args) {
  if (buffer.empty()) {
    return StatusWithSize::ResourceExhausted();
  }

  const int result = std::vsnprintf(buffer.data(), buffer.size(), format, args);

  // If an error occurred, the number of characters written is unknown.
  // Discard any output by terminating the buffer.
  if (result < 0) {
    buffer[0] = '\0';
    return StatusWithSize::InvalidArgument();
  }

  // If result >= buffer.size(), the output was truncated and null-terminated.
  if (static_cast<unsigned>(result) >= buffer.size()) {
    return StatusWithSize::ResourceExhausted(buffer.size() - 1);
  }

  return StatusWithSize(result);
}

Status Format(InlineString<>& string, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const Status status = FormatVaList(string, format, args);
  va_end(args);

  return status;
}

Status FormatVaList(InlineString<>& string, const char* format, va_list args) {
  Status status;
  string.resize_and_overwrite([&](char* buffer, size_t capacity) {
    // The vsnprintf buffer size includes a byte for the null terminator.
    StatusWithSize result =
        FormatVaList(span(buffer + string.size(), capacity + 1 - string.size()),
                     format,
                     args);
    status = result.status();
    return string.size() + result.size();
  });
  return status;
}

Status FormatOverwrite(InlineString<>& string, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const Status status = FormatOverwriteVaList(string, format, args);
  va_end(args);

  return status;
}

}  // namespace pw::string
