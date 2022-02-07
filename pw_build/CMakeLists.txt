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

# IMPORTANT: The compilation flags in this file must be kept in sync with
#            the GN flags //pw_build/BUILD.gn.

# Target that specifies the standard Pigweed build options.
add_library(pw_build INTERFACE)
target_compile_options(pw_build INTERFACE "-g")
target_link_libraries(pw_build
  INTERFACE
    pw_build.reduced_size
    pw_build.cpp17
)
target_compile_options(pw_build
  INTERFACE
    # Force the compiler use colorized output. This is required for Ninja.
    $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
)
if(ZEPHYR_PIGWEED_MODULE_DIR)
  target_link_libraries(pw_build INTERFACE zephyr_interface)
endif()

# Declare top-level targets for tests.
add_custom_target(pw_tests.default)
add_custom_target(pw_run_tests.default)

add_custom_target(pw_tests DEPENDS pw_tests.default)
add_custom_target(pw_run_tests DEPENDS pw_run_tests.default)

# Define the standard Pigweed compile options.
add_library(pw_build.reduced_size INTERFACE)
target_compile_options(pw_build.reduced_size
  INTERFACE
    "-fno-common"
    "-fno-exceptions"
    "-ffunction-sections"
    "-fdata-sections"
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

# Define the standard Pigweed compile options.
#
# The pw_build.warnings library is used by upstream Pigweed targets to add
# compiler warnings to the build.
#
# Toolchains may override these warnings by setting pw_build_WARNINGS:
#
#   set(pw_build_WARNINGS my_warnings CACHE STRING "" FORCE)
#
set(pw_build_WARNINGS pw_build.strict_warnings
    CACHE STRING "Warnings libraries to use for Pigweed upstream code")

add_library(pw_build.warnings INTERFACE)
target_link_libraries(pw_build.warnings INTERFACE ${pw_build_WARNINGS})

# TODO(hepler): These Zephyr exceptions should be made by overriding
#     pw_build_WARNINGS.
add_library(pw_build.strict_warnings INTERFACE)
if(NOT ZEPHYR_PIGWEED_MODULE_DIR)
  # Only include these flags if we're not building with Zephyr.
  set(strict_warnings_cond "-Wcast-qual" "-Wundef")
endif()
target_compile_options(pw_build.strict_warnings
  INTERFACE
    "-Wall"
    "-Wextra"
    "-Wimplicit-fallthrough"
    ${strict_warnings_cond}
    "-Wpointer-arith"

    # Make all warnings errors, except for the exemptions below.
    "-Werror"
    "-Wno-error=cpp"  # preprocessor #warning statement
    "-Wno-error=deprecated-declarations"  # [[deprecated]] attribute

    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
)

add_library(pw_build.extra_strict_warnings INTERFACE)
if(NOT ZEPHYR_PIGWEED_MODULE_DIR)
  # Only include these flags if we're not building with Zephyr.
  set(extra_strict_warnings_cond "-Wredundant-decls")
endif()
target_compile_options(pw_build.extra_strict_warnings
  INTERFACE
    "-Wshadow"
    ${extra_strict_warnings_cond}
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
)

add_library(pw_build.cpp17 INTERFACE)
target_compile_options(pw_build.cpp17
  INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
    # Allow uses of the register keyword, which may appear in C headers.
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
)

# Create an empty source file and library for general use.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/empty_file.c" "")
add_library(pw_build.empty OBJECT "${CMAKE_CURRENT_BINARY_DIR}/empty_file.c" "")
