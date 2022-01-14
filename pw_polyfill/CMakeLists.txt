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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_module_library(pw_polyfill
  HEADERS
    public/pw_polyfill/language_feature_macros.h
    public/pw_polyfill/standard.h
  PUBLIC_INCLUDES
    public
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_POLYFILL)
  zephyr_link_libraries(pw_polyfill)
endif()

# TODO(pwbug/602): Remove this overrides target by migrating all users to
# explicitly depend on the polyfill(s) they require.
pw_add_module_library(pw_polyfill.overrides
  PUBLIC_DEPS
    pw_polyfill.bit
    pw_polyfill.cstddef
    pw_polyfill.iterator
    pw_polyfill.span
    pw_polyfill.type_traits
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_POLYFILL_OVERRIDES)
  zephyr_link_libraries(pw_polyfill.overrides)
endif()

# Provides <bit>'s std::endian.
pw_add_module_library(pw_polyfill.bit
  HEADERS
    public_overrides/bit
    standard_library_public/pw_polyfill/standard_library/bit.h
  PUBLIC_INCLUDES
    public_overrides
    standard_library_public
  PUBLIC_DEPS
    pw_polyfill.standard_library
)

# Provides <cstddef>'s std::byte.
pw_add_module_library(pw_polyfill.cstddef
  HEADERS
    public_overrides/cstddef
    standard_library_public/pw_polyfill/standard_library/cstddef.h
  PUBLIC_INCLUDES
    public_overrides
    standard_library_public
  PUBLIC_DEPS
    pw_polyfill.standard_library
)

# TODO(pwbug/603): Remove this polyfill.
pw_add_module_library(pw_polyfill.iterator
  HEADERS
    public_overrides/iterator
    standard_library_public/pw_polyfill/standard_library/iterator.h
  PUBLIC_INCLUDES
    public_overrides
    standard_library_public
  PUBLIC_DEPS
    pw_polyfill.standard_library
)

# Provides <span>.
pw_add_module_library(pw_polyfill.span
  PUBLIC_DEPS
    pw_span
)

# TODO(pwbug/603): Remove this polyfill.
pw_add_module_library(pw_polyfill.type_traits
  HEADERS
    public_overrides/type_traits
    standard_library_public/pw_polyfill/standard_library/type_traits.h
  PUBLIC_INCLUDES
    public_overrides
    standard_library_public
  PUBLIC_DEPS
    pw_polyfill.standard_library
)

pw_add_module_library(pw_polyfill.standard_library
  HEADERS
    standard_library_public/pw_polyfill/standard_library/namespace.h
  PUBLIC_INCLUDES
    standard_library_public
)
