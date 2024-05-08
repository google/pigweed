// Copyright 2024 The Pigweed Authors
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

#include <__verbose_abort>
#include <cstdarg>
#include <cstdlib>

// Inline functions in libc++ headers call this.
[[noreturn]] void std::__libcpp_verbose_abort(const char* format, ...) {
  va_list list;
  va_start(list, format);
  // TODO: https://pwbug.dev/298822102 - Log the message, ideally using pw_log.
  va_end(list);
  std::abort();
}
