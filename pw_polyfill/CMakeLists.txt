# Copyright 2020 The Pigweed Authors
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

add_library(pw_polyfill INTERFACE)
target_include_directories(pw_polyfill INTERFACE public standard_library_public)

add_library(pw_polyfill.overrides INTERFACE)
target_link_libraries(pw_polyfill.overrides INTERFACE pw_polyfill)
target_include_directories(pw_polyfill.overrides
  INTERFACE
    public_overrides
    standard_library_public
)
target_compile_options(pw_polyfill.overrides INTERFACE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/language_features.h"
)
