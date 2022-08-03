# Copyright 2021 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_module_library(pw_third_party.fuchsia.fit
  HEADERS
    repo/sdk/lib/fit/include/lib/fit/function.h
    repo/sdk/lib/fit/include/lib/fit/nullable.h
    repo/sdk/lib/fit/include/lib/fit/traits.h
    repo/sdk/lib/fit/include/lib/fit/function_internal.h
    repo/sdk/lib/fit/include/lib/fit/utility_internal.h
  PUBLIC_INCLUDES
    repo/sdk/lib/fit/include
  PUBLIC_DEPS
    pw_third_party.fuchsia.stdcompat
)

pw_add_module_library(pw_third_party.fuchsia.stdcompat
  HEADERS
    repo/sdk/lib/stdcompat/include/lib/stdcompat/optional.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/type_traits.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/utility.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/version.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/internal/constructors.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/internal/exception.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/internal/storage.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/internal/type_traits.h
    repo/sdk/lib/stdcompat/include/lib/stdcompat/internal/utility.h
  PUBLIC_INCLUDES
    repo/sdk/lib/stdcompat/include
)
