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

pw_add_module_config(pw_assert_CONFIG)

pw_add_module_library(pw_assert
  PUBLIC_DEPS
    pw_assert.check
    pw_assert.assert
    pw_assert.config
)

pw_add_module_library(pw_assert.config
  HEADERS
    public/pw_assert/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_assert_CONFIG}
)

# Deprecate this once all users have been migrated off pw_assert_BACKEND to
# instead set pw_assert.assert_BACKEND and pw_assert.check_BACKEND and use the
# separate assert and check facades directly.
if(DEFINED pw_assert_BACKEND)
  message(DEPRECATION "pw_set_backend(pw_assert ...) (AKA pw_assert_BACKEND) "
                      "has been deprecated and replaced with the "
                      "pw_assert.check and pw_assert.assert facades.")
  pw_set_backend(pw_assert.check "${pw_assert_BACKEND}")
  pw_set_backend(pw_assert.assert pw_assert.assert_compatibility_backend)

  pw_add_module_library(pw_assert.facade
    PUBLIC_DEPS
      pw_assert.check.facade
  )
endif()

pw_add_facade(pw_assert.assert
  HEADERS
    public/pw_assert/assert.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.config
)

pw_add_facade(pw_assert.check
  HEADERS
    public/pw_assert/check.h
    public/pw_assert/internal/check_impl.h
    public/pw_assert/short.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.config
    pw_preprocessor
)

# Warning: The assert facade is in a transitional state, and this target is
# likely to be removed as the pw_assert API is reassessed. (b/235149326)
pw_add_module_library(pw_assert.assert_compatibility_backend
  HEADERS
    assert_compatibility_public_overrides/pw_assert_backend/assert_backend.h
  PUBLIC_INCLUDES
    assert_compatibility_public_overrides
  PUBLIC_DEPS
    pw_preprocessor
)

pw_add_module_library(pw_assert.print_and_abort
  HEADERS
    public/pw_assert/internal/print_and_abort.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.config
)

# This backend to pw_assert's PW_CHECK()/PW_DCHECK() macros prints the assert
# expression, evaluated expression, file/line number, function, and user message
# with printf, then aborts. It is intended for use with host builds.
pw_add_module_library(pw_assert.print_and_abort_check_backend
  HEADERS
    print_and_abort_check_public_overrides/pw_assert_backend/check_backend.h
  PUBLIC_INCLUDES
    print_and_abort_check_public_overrides
  PUBLIC_DEPS
    pw_assert.print_and_abort
)

# This backend to pw_assert's PW_ASSERT()/PW_DASSERT() macros prints the assert
# expression, file/line number, and function with printf, then aborts. It is
# intended for use with host builds.
pw_add_module_library(pw_assert.print_and_abort_assert_backend
  HEADERS
    print_and_abort_assert_public_overrides/pw_assert_backend/assert_backend.h
  PUBLIC_INCLUDES
    print_and_abort_assert_public_overrides
  PUBLIC_DEPS
    pw_assert.config
    pw_assert.print_and_abort
)

pw_add_test(pw_assert.assert_facade_test
  SOURCES
    assert_facade_test.cc
    fake_backend.cc
    public/pw_assert/internal/check_impl.h
    pw_assert_test/fake_backend.h
  DEPS
    pw_assert
    pw_status
    pw_string
  GROUPS
    modules
    pw_assert
)

if((NOT "${pw_assert.assert_BACKEND}" STREQUAL
    "pw_assert.assert.NO_BACKEND_SET") AND
   (NOT "${pw_assert.check_BACKEND}" STREQUAL
    "pw_assert.check.NO_BACKEND_SET"))
  pw_add_test(pw_assert.assert_backend_compile_test
    SOURCES
      assert_backend_compile_test.cc
      assert_backend_compile_test_c.c
    DEPS
      pw_assert
      pw_status
    GROUPS
      modules
      pw_assert
  )
endif()
