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
#
#   IMPLEMENTS_FACADE - this module implements the specified facade
#   PUBLIC_DEPS - public target_link_libraries arguments
#   PRIVATE_DEPS - private target_link_libraries arguments
#
function(pw_auto_add_simple_module MODULE)
  set(multi PUBLIC_DEPS PRIVATE_DEPS TEST_DEPS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "IMPLEMENTS_FACADE" "${multi}")

  file(GLOB all_sources *.cc *.c)

  # Create a library with all source files not ending in _test.
  set(sources "${all_sources}")
  list(FILTER sources EXCLUDE REGEX "_test(\\.cc|(_c)?\\.c)$")
  list(FILTER sources EXCLUDE REGEX "_fuzzer\\.cc$")

  file(GLOB_RECURSE headers *.h)

  if(arg_IMPLEMENTS_FACADE)
    set(groups backends)
    set(facade_dep "${arg_IMPLEMENTS_FACADE}.facade")
  else()
    set(groups modules "${MODULE}")
  endif()

  pw_add_module_library("${MODULE}"
    PUBLIC_DEPS
      ${arg_PUBLIC_DEPS}
      ${facade_dep}
    PRIVATE_DEPS
      ${arg_PRIVATE_DEPS}
    SOURCES
      ${sources}
    HEADERS
      ${headers}
  )

  if(arg_IMPLEMENTS_FACADE)
    target_include_directories("${MODULE}" PUBLIC public_overrides)
  endif()

  pw_auto_add_module_tests("${MODULE}"
    PRIVATE_DEPS
      ${arg_PUBLIC_DEPS}
      ${arg_PRIVATE_DEPS}
      ${arg_TEST_DEPS}
    GROUPS
      ${groups}
  )
endfunction(pw_auto_add_simple_module)

# Creates a test for each source file ending in _test. Tests with mutliple .cc
# files or different dependencies than the module will not work correctly.
#
# Args:
#
#  PRIVATE_DEPS - dependencies to apply to all tests
#  GROUPS - groups in addition to MODULE to which to add these tests
#
function(pw_auto_add_module_tests MODULE)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "PRIVATE_DEPS;GROUPS")

  file(GLOB cc_tests *_test.cc)

  foreach(test IN LISTS cc_tests)
    get_filename_component(test_name "${test}" NAME_WE)

    # Find a .c test corresponding with the test .cc file, if any.
    file(GLOB c_test "${test_name}.c" "${test_name}_c.c")

    pw_add_test("${MODULE}.${test_name}"
      SOURCES
        "${test}"
        ${c_test}
      DEPS
        "$<TARGET_NAME_IF_EXISTS:${MODULE}>"
        ${arg_PRIVATE_DEPS}
      GROUPS
        "${MODULE}"
        ${arg_GROUPS}
    )
  endforeach()
endfunction(pw_auto_add_module_tests)

# Creates a library in a module. The library has access to the public/ include
# directory.
#
# Args:
#
#   SOURCES - source files for this library
#   HEADERS - header files for this library
#   PUBLIC_DEPS - public target_link_libraries arguments
#   PRIVATE_DEPS - private target_link_libraries arguments
#
function(pw_add_module_library NAME)
  set(list_args SOURCES HEADERS PUBLIC_DEPS PRIVATE_DEPS)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${list_args}")

  # Check that the library's name is prefixed by the module name.
  get_filename_component(module "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

  if(NOT "${NAME}" MATCHES "${module}(\\.[^\\.]+)?(\\.facade)?$")
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
      pw_build.strict_warnings
      pw_build.extra_strict_warnings
      ${arg_PRIVATE_DEPS}
  )

  # Libraries require at least one source file.
  if(NOT arg_SOURCES)
    target_sources("${NAME}" PRIVATE $<TARGET_PROPERTY:pw_build.empty,SOURCES>)
  endif()
endfunction(pw_add_module_library)

# Declares a module as a facade.
#
# Facades are declared as two libraries to avoid circular dependencies.
# Libraries that use the facade depend on a library named for the module. The
# module that implements the facade depends on a library named
# MODULE_NAME.facade.
#
# pw_add_facade accepts the same arguments as pw_add_module_library, with the
# following additions:
#
#  DEFAULT_BACKEND - which backend to use by default
#
function(pw_add_facade NAME)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "DEFAULT_BACKEND" "")

  # If no backend is set, a script that displays an error message is used
  # instead. If the facade is used in the build, it fails with this error.
  add_custom_target("${NAME}._no_backend_set_message"
    COMMAND
      python "$ENV{PW_ROOT}/pw_build/py/pw_build/null_backend.py" "${NAME}"
  )
  add_library("${NAME}.NO_BACKEND_SET" INTERFACE)
  add_dependencies("${NAME}.NO_BACKEND_SET" "${NAME}._no_backend_set_message")

  # Set the default backend to the error message if no default is specified.
  if("${arg_DEFAULT_BACKEND}" STREQUAL "")
    set(arg_DEFAULT_BACKEND "${NAME}.NO_BACKEND_SET")
  endif()

  # Declare the backend variable for this facade.
  set("${NAME}_BACKEND" "${arg_DEFAULT_BACKEND}" CACHE STRING
      "Backend for ${NAME}")

  # Define the facade library, which is used by the backend to avoid circular
  # dependencies.
  pw_add_module_library("${NAME}.facade" ${arg_UNPARSED_ARGUMENTS})

  # Define the public-facing library for this facade, which depends on the
  # header files in .facade target and exposes the dependency on the backend.
  add_library("${NAME}" INTERFACE)
  target_link_libraries("${NAME}"
    INTERFACE
      "${NAME}.facade"
      "${${NAME}_BACKEND}"
  )
endfunction(pw_add_facade)

# Sets which backend to use for the given facade.
function(pw_set_backend FACADE BACKEND)
  set("${FACADE}_BACKEND" "${BACKEND}" CACHE STRING "Backend for ${NAME}" FORCE)
endfunction(pw_set_backend)

# Declares a unit test. Creates two targets:
#
#  * <TEST_NAME> - the test executable
#  * <TEST_NAME>.run - builds and runs the test
#
# Args:
#
#   NAME: name to use for the target
#   SOURCES: source files for this test
#   DEPS: libraries on which this test depends
#   GROUPS: groups to which to add this test; if none are specified, the test is
#       added to the 'default' and 'all' groups
#
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
  add_custom_target("${NAME}.run" DEPENDS "${NAME}.stamp")

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
    add_dependencies("pw_run_tests.${group}" "${TEST_NAME}.run")
  endforeach()
endfunction(pw_add_test_to_groups)
