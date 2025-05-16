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

#if !__has_include("pw_build/internal/test_header_with_dep.h")
#error "pw_build/internal/test_header_with_dep.h must be visible"
#endif  // __has_include("pw_build/internal/test_header_with_dep.h")

#if __has_include("pw_build/internal/test_header_without_dep.h")
#define NO_DEP_HEADER_IS_VISIBLE 1
#else
#define NO_DEP_HEADER_IS_VISIBLE 0
#endif  // __has_include("pw_build/internal/test_header_without_dep.h")

static_assert(SANDBOX_ENABLED != NO_DEP_HEADER_IS_VISIBLE,
              "With sandboxing enabled, the header without a dependency must "
              "not be visible");
