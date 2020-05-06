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
include_guard(GLOBAL)

# Create an empty, dummy source file for use by non-INTERFACE libraries, which
# require at least one source file.
set(_pw_empty_source_file "${CMAKE_BINARY_DIR}/pw_empty_source_file.cc")
file(WRITE "${_pw_empty_source_file}" "")

# Automatically creates a library and test targets for the files in a module.
# This function is only suitable for simple modules that meet the following
# requirements:
#
#  - The module exposes exactly one library.
#  - All source files in the module directory are included in the library.
#  - Each test in the module has
#    - exactly one source .cc file,
#    - optionally, one .c source with the same base name as the .cc file,
#    - only a dependency on the main module library.
#  - The module is not a facade.
#
# Modules that do not meet these requirements may not use
# pw_auto_add_simple_module. Instead, define the module's libraries and tests
# with pw_add_module_library, pw_add_facade, pw_add_test, and the standard CMake
# functions, such as add_library, target_link_libraries, etc.
#
# This function does the following:
#
#   1. Find all .c and .cc files in the module's root directory.
#   2. Create a library with the module name using pw_add_module_library with
#      all source files that do not end with _test.cc.
#   3. Declare a test for each source file that ends with _test.cc.
#
# Args:
#   IMPLEMENTS_FACADE: this module implements the specified facade
#   PUBLIC_DEPS: public target_link_libraries arguments
#   PRIVATE_DEPS: private target_link_libraries arguments
#
function(pw_auto_add_simple_module MODULE)
  set(multi PUBLIC_DEPS PRIVATE_DEPS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "IMPLEMENTS_FACADE" "${multi}")

  file(GLOB all_sources *.cc *.c)

  # Create a library with all source files not ending in _test.
  set(sources "${all_sources}")
  list(FILTER sources EXCLUDE REGEX "_test.cc$")

  file(GLOB_RECURSE headers *.h)

  if(arg_IMPLEMENTS_FACADE)
    set(groups backends)
    set(deps PUBLIC_DEPS "${arg_IMPLEMENTS_FACADE}.facade")
  else()
    set(groups modules "${MODULE}")
  endif()

  pw_add_module_library("${MODULE}"
    PUBLIC_DEPS
      ${arg_PUBLIC_DEPS}
    PRIVATE_DEPS
      ${arg_PRIVATE_DEPS}
    SOURCES
      ${sources}
    HEADERS
      ${headers}
    ${deps}
  )

  # Create a test for each source file ending in _test. Tests with mutliple .cc
  # files or different dependencies than the module will not work correctly.
  set(tests "${all_sources}")
  list(FILTER tests INCLUDE REGEX "_test.cc$")

  foreach(test IN LISTS tests)
    get_filename_component(test_name "${test}" NAME_WE)

    # Find a .c test corresponding with the test .cc file, if any.
    list(FILTER c_test INCLUDE REGEX "^${test_name}.c$")

    pw_add_test("${MODULE}.${test_name}"
      SOURCES
        "${test}"
        ${c_test}
      DEPS
        "${MODULE}"
        ${arg_PUBLIC_DEPS}
        ${arg_PRIVATE_DEPS}
      GROUPS
        "${groups}"
    )
  endforeach()
endfunction(pw_auto_add_simple_module)

# Creates a library in a module. The library has access to the public/ include
# directory.
#
# Args:
#   SOURCES: source files for this library
#   HEADERS: header files for this library
#   PUBLIC_DEPS: public target_link_libraries arguments
#   PRIVATE_DEPS: private target_link_libraries arguments
function(pw_add_module_library NAME)
  set(list_args SOURCES HEADERS PUBLIC_DEPS PRIVATE_DEPS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${list_args}")

  # Check that the library's name is prefixed by the module name.
  get_filename_component(module "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

  if(NOT "${NAME}" MATCHES "^${module}(\\.[^\\.]+)?(\\.facade|\\.backend)?$")
    message(FATAL_ERROR
        "Module libraries must match the module name or be in the form "
        "'MODULE_NAME.LIBRARY_NAME'. The library '${NAME}' does not match."
    )
  endif()

  add_library("${NAME}" EXCLUDE_FROM_ALL ${arg_HEADERS} ${arg_SOURCES})
  target_include_directories("${NAME}" PUBLIC public/)
  target_link_libraries("${NAME}"
    PUBLIC
      pw_build
      ${arg_PUBLIC_DEPS}
    PRIVATE
      ${arg_PRIVATE_DEPS}
  )

  # Libraries require at least one source file.
  if(NOT arg_SOURCES)
    target_sources("${NAME}" PRIVATE "${_pw_empty_source_file}")
  endif()
endfunction(pw_add_module_library)

# Declares a module as a facade.
#
# Facades are declared as two libraries to avoid circular dependencies.
# Libraries that use the facade depend on a library named for the module. The
# module that implements the facade depends on a library named
# MODULE_NAME.facade.
#
# pw_add_facade accepts the same arguments as pw_add_module_library.
function(pw_add_facade NAME)
  pw_add_module_library("${NAME}.facade" ${ARGN})

  # Use a library with an empty source instead of an INTERFACE library so that
  # the library can have a private dependency on the backend.
  add_library("${NAME}" OBJECT EXCLUDE_FROM_ALL "${_pw_empty_source_file}")
  target_link_libraries("${NAME}"
    PUBLIC
      "${NAME}.facade"
      "${NAME}.backend"
  )
endfunction(pw_add_facade)

# Declares a unit test. Creates two targets:
#
#  - <TEST_NAME>: the test executable
#  - <TEST_NAME>_run: builds and runs the test
#
# Args:
#   NAME: name to use for the target
#   SOURCES: source files for this test
#   DEPS: libraries on which this test depends
#   GROUPS: groups to which to add this test; if none are specified, the test is
#       added to the default and all groups
function(pw_add_test NAME)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPS;GROUPS")

  add_executable("${NAME}" EXCLUDE_FROM_ALL ${arg_SOURCES})
  target_link_libraries("${NAME}"
    PRIVATE
      pw_unit_test
      pw_unit_test.main
      ${arg_DEPS}
  )

  # Define a target for running the test. The target creates a stamp file to
  # indicate successful test completion. This allows running tests in parallel
  # with Ninja's full dependency resolution.
  add_custom_command(
    COMMAND
      # TODO(hepler): This only runs local test binaries. Execute a test runner
      #     instead to support device test runs.
      "$<TARGET_FILE:${NAME}>"
    COMMAND
      "${CMAKE_COMMAND}" -E touch "${NAME}.stamp"
    DEPENDS
      "${NAME}"
    OUTPUT
      "${NAME}.stamp"
  )
  add_custom_target("${NAME}_run" DEPENDS "${NAME}.stamp")

  # Always add tests to the "all" group. If no groups are provided, add the
  # test to the "default" group.
  if(arg_GROUPS)
    set(groups all ${arg_GROUPS})
  else()
    set(groups all default)
  endif()

  list(REMOVE_DUPLICATES groups)
  pw_add_test_to_groups("${NAME}" ${groups})
endfunction(pw_add_test)

# Adds a test target to the specified test groups. Test groups can be built with
# the pw_tests_GROUP_NAME target or executed with the pw_run_tests_GROUP_NAME
# target.
function(pw_add_test_to_groups TEST_NAME)
  foreach(group IN LISTS ARGN)
    if(NOT TARGET "pw_tests.${group}")
      add_custom_target("pw_tests.${group}")
      add_custom_target("pw_run_tests.${group}")
    endif()

    add_dependencies("pw_tests.${group}" "${TEST_NAME}")
    add_dependencies("pw_run_tests.${group}" "${TEST_NAME}_run")
  endforeach()
endfunction(pw_add_test_to_groups)

# Declare top-level targets for tests.
add_custom_target(pw_tests.default)
add_custom_target(pw_run_tests.default)

add_custom_target(pw_tests DEPENDS pw_tests.default)
add_custom_target(pw_run_tests DEPENDS pw_run_tests.default)

# Define the standard Pigweed compile options.
add_library(_pw_reduced_size_copts INTERFACE)
target_compile_options(_pw_reduced_size_copts
  INTERFACE
    "-fno-common"
    "-fno-exceptions"
    "-ffunction-sections"
    "-fdata-sections"
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

add_library(_pw_strict_warnings_copts INTERFACE)
target_compile_options(_pw_strict_warnings_copts
  INTERFACE
    "-Wall"
    "-Wextra"
    # Make all warnings errors, except for the exemptions below.
    "-Werror"
    "-Wno-error=cpp"  # preprocessor #warning statement
    "-Wno-error=deprecated-declarations"  # [[deprecated]] attribute
    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
)

add_library(_pw_cpp17_copts INTERFACE)
target_compile_options(_pw_cpp17_copts
  INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
    # Allow uses of the register keyword, which may appear in C headers.
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
)

# Target that specifies the standard Pigweed build options.
add_library(pw_build INTERFACE)
target_compile_options(pw_build INTERFACE "-g")
target_link_libraries(pw_build
  INTERFACE
    _pw_reduced_size_copts
    _pw_strict_warnings_copts
    _pw_cpp17_copts
)
target_compile_options(pw_build
  INTERFACE
    # Force the compiler use colorized output. This is required for Ninja.
    $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
)
