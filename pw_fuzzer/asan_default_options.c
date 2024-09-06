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

#include <sanitizer/asan_interface.h>

// Sets the default options for AddressSanitizer.
//
// See https://github.com/google/sanitizers/wiki/AddressSanitizerFlags for
// more details.
const char* __asan_default_options(void) {
  return
      // FuzzTest is not instrumented to avoid polluting the code coverage used
      // to guide fuzzing. It also uses STL containers such as vectors, leading
      // to false positives such as those described in
      // github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow
      "detect_container_overflow=0";
}
