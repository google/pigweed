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

cmake_minimum_required(VERSION 3.16)

# The PW_ROOT environment variable should be set in bootstrap. If it is not set,
# set it to the root of the Pigweed repository.
if("$ENV{PW_ROOT}" STREQUAL "")
  get_filename_component(pw_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  message("The PW_ROOT environment variable is not set; "
          "using ${pw_root} within CMake")
  set(ENV{PW_ROOT} "${pw_root}")
endif()

# Wrapper around cmake_parse_arguments that fails with an error if any arguments
# remained unparsed.
macro(pw_parse_arguments_strict function start_arg options one multi)
  cmake_parse_arguments(PARSE_ARGV
      "${start_arg}" arg "${options}" "${one}" "${multi}"
  )
  if(NOT "${arg_UNPARSED_ARGUMENTS}" STREQUAL "")
    set(_all_args ${options} ${one} ${multi})
    message(FATAL_ERROR
        "Unexpected arguments to ${function}: ${arg_UNPARSED_ARGUMENTS}\n"
        "Valid arguments: ${_all_args}"
    )
  endif()
endmacro()

# Checks that one or more variables are set. This is used to check that
# arguments were provided to a function. Fails with FATAL_ERROR if
# ${ARG_PREFIX}${name} is empty. The FUNCTION_NAME is used in the error message.
# If FUNCTION_NAME is "", it is set to CMAKE_CURRENT_FUNCTION.
#
# Usage:
#
#   pw_require_args(FUNCTION_NAME ARG_PREFIX ARG_NAME [ARG_NAME ...])
#
# Examples:
#
#   # Checks that arg_FOO is non-empty, using the current function name.
#   pw_require_args("" arg_ FOO)
#
#   # Checks that FOO and BAR are non-empty, using function name "do_the_thing".
#   pw_require_args(do_the_thing "" FOO BAR)
#
macro(pw_require_args FUNCTION_NAME ARG_PREFIX)
  if("${FUNCTION_NAME}" STREQUAL "")
    set(_pw_require_args_FUNCTION_NAME "${CMAKE_CURRENT_FUNCTION}")
  else()
    set(_pw_require_args_FUNCTION_NAME "${FUNCTION_NAME}")
  endif()

  foreach(name IN ITEMS ${ARGN})
    if("${${ARG_PREFIX}${name}}" STREQUAL "")
      message(FATAL_ERROR "A value must be provided for ${name} in "
          "${_pw_require_args_FUNCTION_NAME}.")
    endif()
  endforeach()
endmacro()

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
  pw_parse_arguments_strict(pw_auto_add_simple_module 1
      ""
      "IMPLEMENTS_FACADE"
      "PUBLIC_DEPS;PRIVATE_DEPS;TEST_DEPS"
  )

  file(GLOB all_sources *.cc *.c)

  # Create a library with all source files not ending in _test.
  set(sources "${all_sources}")
  list(FILTER sources EXCLUDE REGEX "_test(\\.cc|(_c)?\\.c)$")  # *_test.cc
  list(FILTER sources EXCLUDE REGEX "^test(\\.cc|(_c)?\\.c)$")  # test.cc
  list(FILTER sources EXCLUDE REGEX "_fuzzer\\.cc$")

  file(GLOB_RECURSE headers *.h)

  if(arg_IMPLEMENTS_FACADE)
    set(groups backends)
  else()
    set(groups modules "${MODULE}")
  endif()

  pw_add_module_library("${MODULE}"
    IMPLEMENTS_FACADES
      ${arg_IMPLEMENTS_FACADE}
    PUBLIC_DEPS
      ${arg_PUBLIC_DEPS}
    PRIVATE_DEPS
      ${arg_PRIVATE_DEPS}
    SOURCES
      ${sources}
    HEADERS
      ${headers}
  )

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
  pw_parse_arguments_strict(pw_auto_add_module_tests 1
      ""
      ""
      "PRIVATE_DEPS;GROUPS"
  )

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

# Sets the provided variable to the multi_value_keywords from pw_add_library.
macro(_pw_add_library_multi_value_args variable)
  set("${variable}" SOURCES HEADERS
                    PUBLIC_DEPS PRIVATE_DEPS
                    PUBLIC_INCLUDES PRIVATE_INCLUDES
                    PUBLIC_DEFINES PRIVATE_DEFINES
                    PUBLIC_COMPILE_OPTIONS PRIVATE_COMPILE_OPTIONS
                    PUBLIC_LINK_OPTIONS PRIVATE_LINK_OPTIONS "${ARGN}")
endmacro()

# pw_add_library: Creates a CMake library target.
#
# Required Args:
#
#   <name> - The name of the library target to be created.
#   <type> - The library type which must be INTERFACE, OBJECT, STATIC, SHARED,
#            or MODULE.
#
# Optional Args:
#
#   SOURCES - source files for this library
#   HEADERS - header files for this library
#   PUBLIC_DEPS - public target_link_libraries arguments
#   PRIVATE_DEPS - private target_link_libraries arguments
#   PUBLIC_INCLUDES - public target_include_directories argument
#   PRIVATE_INCLUDES - public target_include_directories argument
#   PUBLIC_DEFINES - public target_compile_definitions arguments
#   PRIVATE_DEFINES - private target_compile_definitions arguments
#   PUBLIC_COMPILE_OPTIONS - public target_compile_options arguments
#   PRIVATE_COMPILE_OPTIONS - private target_compile_options arguments
#   PUBLIC_LINK_OPTIONS - public target_link_options arguments
#   PRIVATE_LINK_OPTIONS - private target_link_options arguments
function(pw_add_library NAME TYPE)
  set(num_positional_args 2)
  set(option_args)
  set(one_value_args)
  _pw_add_library_multi_value_args(multi_value_args)
  pw_parse_arguments_strict(
      pw_add_library "${num_positional_args}" "${option_args}"
      "${one_value_args}" "${multi_value_args}")

  add_library("${NAME}" "${TYPE}" EXCLUDE_FROM_ALL)

  # Instead of forking all of the code below or injecting an empty source file,
  # conditionally select PUBLIC vs INTERFACE depending on the type.
  if("${TYPE}" STREQUAL "INTERFACE")
    set(public_or_interface INTERFACE)
    if(NOT "${arg_SOURCES}" STREQUAL "")
      message(
        SEND_ERROR "${NAME} cannot have sources as it's an INTERFACE library")
    endif(NOT "${arg_SOURCES}" STREQUAL "")
  else()  # ${TYPE} != INTERFACE
    set(public_or_interface PUBLIC)
    if("${arg_SOURCES}" STREQUAL "")
      message(
        SEND_ERROR "${NAME} must have SOURCES as it's not an INTERFACE library")
    endif("${arg_SOURCES}" STREQUAL "")
  endif("${TYPE}" STREQUAL "INTERFACE")

  target_sources("${NAME}" PRIVATE ${arg_SOURCES} ${arg_HEADERS})

  # CMake 3.22 does not have a notion of target_headers yet, so in the mean
  # time we ask for headers to be specified for consistency with GN & Bazel and
  # to improve the IDE experience. However, we do want to ensure all the headers
  # which are otherwise ignored by CMake are present.
  #
  # See https://gitlab.kitware.com/cmake/cmake/-/issues/22468 for adding support
  # to CMake to associate headers with targets properly for CMake 3.23.
  foreach(header IN ITEMS ${arg_HEADERS})
    get_filename_component(header "${header}" ABSOLUTE)
    if(NOT EXISTS ${header})
      message(FATAL_ERROR "Header not found: \"${header}\"")
    endif()
  endforeach()

  # Public and private target_include_directories.
  target_include_directories("${NAME}" PRIVATE ${arg_PRIVATE_INCLUDES})
  target_include_directories(
        "${NAME}" ${public_or_interface} ${arg_PUBLIC_INCLUDES})

  # Public and private target_link_libraries.
  if(NOT "${arg_SOURCES}" STREQUAL "")
    target_link_libraries("${NAME}" PRIVATE ${arg_PRIVATE_DEPS})
  endif(NOT "${arg_SOURCES}" STREQUAL "")
  target_link_libraries("${NAME}" ${public_or_interface} ${arg_PUBLIC_DEPS})

  # The target_compile_options are always added before target_link_libraries'
  # target_compile_options. In order to support the enabling of warning
  # compile options injected via arg_PRIVATE_DEPS (target_link_libraries
  # dependencies) that are partially disabled via arg_PRIVATE_COMPILE_OPTIONS (
  # target_compile_options), the defines, compile options, and link options are
  # also added as target_link_libraries dependencies. This enables reasonable
  # ordering between the enabling (target_link_libraries) and disabling (
  # now also target_link_libraries) of compiler warnings.

  # Add the NAME._config target_link_libraries dependency with the
  # PRIVATE_DEFINES, PRIVATE_COMPILE_OPTIONS, and PRIVATE_LINK_OPTIONS.
  if(NOT "${TYPE}" STREQUAL "INTERFACE")
    target_link_libraries("${NAME}" PRIVATE "${NAME}._config")
    add_library("${NAME}._config" INTERFACE EXCLUDE_FROM_ALL)
    if(NOT "${arg_PRIVATE_DEFINES}" STREQUAL "")
      target_compile_definitions(
          "${NAME}._config" INTERFACE ${arg_PRIVATE_DEFINES})
    endif(NOT "${arg_PRIVATE_DEFINES}" STREQUAL "")
    if(NOT "${arg_PRIVATE_COMPILE_OPTIONS}" STREQUAL "")
      target_compile_options(
          "${NAME}._config" INTERFACE ${arg_PRIVATE_COMPILE_OPTIONS})
    endif(NOT "${arg_PRIVATE_COMPILE_OPTIONS}" STREQUAL "")
    if(NOT "${arg_PRIVATE_LINK_OPTIONS}" STREQUAL "")
      target_link_options("${NAME}._config" INTERFACE ${arg_PRIVATE_LINK_OPTIONS})
    endif(NOT "${arg_PRIVATE_LINK_OPTIONS}" STREQUAL "")
  endif(NOT "${TYPE}" STREQUAL "INTERFACE")

  # Add the NAME._public_config target_link_libraries dependency with the
  # PUBLIC_DEFINES, PUBLIC_COMPILE_OPTIONS, and PUBLIC_LINK_OPTIONS.
  add_library("${NAME}._public_config" INTERFACE EXCLUDE_FROM_ALL)
  target_link_libraries(
      "${NAME}" ${public_or_interface} "${NAME}._public_config")
  if(NOT "${arg_PUBLIC_DEFINES}" STREQUAL "")
    target_compile_definitions(
        "${NAME}._public_config" INTERFACE ${arg_PUBLIC_DEFINES})
  endif(NOT "${arg_PUBLIC_DEFINES}" STREQUAL "")
  if(NOT "${arg_PUBLIC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(
        "${NAME}._public_config" INTERFACE ${arg_PUBLIC_COMPILE_OPTIONS})
  endif(NOT "${arg_PUBLIC_COMPILE_OPTIONS}" STREQUAL "")
  if(NOT "${arg_PUBLIC_LINK_OPTIONS}" STREQUAL "")
    target_link_options(
        "${NAME}._public_config" INTERFACE ${arg_PUBLIC_LINK_OPTIONS})
  endif(NOT "${arg_PUBLIC_LINK_OPTIONS}" STREQUAL "")
endfunction(pw_add_library)

# Checks that the library's name is prefixed by the relative path with dot
# separators instead of forward slashes. Ignores paths not under the root
# directory.
#
# Optional Args:
#
#   REMAP_PREFIXES - support remapping a prefix for checks
#
function(_pw_check_name_is_relative_to_root NAME ROOT)
  pw_parse_arguments_strict(_pw_check_name_is_relative_to_root
      2 "" "" "REMAP_PREFIXES")

  file(RELATIVE_PATH rel_path "${ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}")
  if("${rel_path}" MATCHES "^\\.\\.")
    return()  # Ignore paths not under ROOT
  endif()

  list(LENGTH arg_REMAP_PREFIXES remap_arg_count)
  if("${remap_arg_count}" EQUAL 2)
    list(GET arg_REMAP_PREFIXES 0 from_prefix)
    list(GET arg_REMAP_PREFIXES 1 to_prefix)
    string(REGEX REPLACE "^${from_prefix}" "${to_prefix}" rel_path "${rel_path}")
  elseif(NOT "${remap_arg_count}" EQUAL 0)
    message(FATAL_ERROR
        "If REMAP_PREFIXES is specified, exactly two arguments must be given.")
  endif()

  if(NOT "${rel_path}" MATCHES "^\\.\\..*")
    string(REPLACE "/" "." dot_rel_path "${rel_path}")
    if(NOT "${NAME}" MATCHES "^${dot_rel_path}(\\.[^\\.]+)?(\\.facade)?$")
      message(FATAL_ERROR
          "Module libraries under ${ROOT} must match the module name or be in "
          "the form 'PATH_TO.THE_TARGET.NAME'. The library '${NAME}' does not "
          "match. Expected ${dot_rel_path}.LIBRARY_NAME"
      )
    endif()
  endif()
endfunction(_pw_check_name_is_relative_to_root)

# Creates a pw module library.
#
# Required Args:
#
#   <name> - The name of the library target to be created.
#
# Optional Args:
#
#   IMPLEMENTS_FACADES - which facades this module library implements
#   SOURCES - source files for this library
#   HEADERS - header files for this library
#   PUBLIC_DEPS - public target_link_libraries arguments
#   PRIVATE_DEPS - private target_link_libraries arguments
#   PUBLIC_INCLUDES - public target_include_directories argument
#   PRIVATE_INCLUDES - public target_include_directories argument
#   PUBLIC_DEFINES - public target_compile_definitions arguments
#   PRIVATE_DEFINES - private target_compile_definitions arguments
#   PUBLIC_COMPILE_OPTIONS - public target_compile_options arguments
#   PRIVATE_COMPILE_OPTIONS - private target_compile_options arguments
#   PUBLIC_LINK_OPTIONS - public target_link_options arguments
#   PRIVATE_LINK_OPTIONS - private target_link_options arguments
#
function(pw_add_module_library NAME)
  _pw_add_library_multi_value_args(multi_value_args IMPLEMENTS_FACADES)
  pw_parse_arguments_strict(pw_add_module_library 1 "" "" "${multi_value_args}")

  _pw_check_name_is_relative_to_root("${NAME}" "$ENV{PW_ROOT}"
    REMAP_PREFIXES
      third_party pw_third_party
  )

  # Use STATIC libraries if sources are provided, else instantiate an INTERFACE
  # library. Also conditionally select PUBLIC vs INTERFACE depending on this
  # selection.
  if(NOT "${arg_SOURCES}" STREQUAL "")
    set(type STATIC)
    set(public_or_interface PUBLIC)
  else("${arg_SOURCES}" STREQUAL "")
    set(type INTERFACE)
    set(public_or_interface INTERFACE)
  endif(NOT "${arg_SOURCES}" STREQUAL "")

  pw_add_library(${NAME} ${type}
    SOURCES
      ${arg_SOURCES}
    HEADERS
      ${arg_HEADERS}
    PUBLIC_DEPS
      # TODO(b/232141950): Apply compilation options that affect ABI
      # globally in the CMake build instead of injecting them into libraries.
      pw_build
      ${arg_PUBLIC_DEPS}
    PRIVATE_DEPS
      pw_build.warnings
      ${arg_PRIVATE_DEPS}
    PUBLIC_INCLUDES
      ${arg_PUBLIC_INCLUDES}
    PRIVATE_INCLUDES
      ${arg_PRIVATE_INCLUDES}
    PUBLIC_DEFINES
      ${arg_PUBLIC_DEFINES}
    PRIVATE_DEFINES
      ${arg_PRIVATE_DEFINES}
    PUBLIC_COMPILE_OPTIONS
      ${arg_PUBLIC_COMPILE_OPTIONS}
    PRIVATE_COMPILE_OPTIONS
      ${arg_PRIVATE_COMPILE_OPTIONS}
    PUBLIC_LINK_OPTIONS
      ${arg_PUBLIC_LINK_OPTIONS}
    PRIVATE_LINK_OPTIONS
      ${arg_PRIVATE_LINK_OPTIONS}
  )

  # TODO(b/235273746): Deprecate this legacy implicit PUBLIC_INCLUDES.
  if("${arg_PUBLIC_INCLUDES}" STREQUAL "")
    target_include_directories("${NAME}" ${public_or_interface} public)
  endif("${arg_PUBLIC_INCLUDES}" STREQUAL "")

  if(NOT "${arg_IMPLEMENTS_FACADES}" STREQUAL "")
    # TODO(b/235273746): Deprecate this legacy implicit PUBLIC_INCLUDES.
    if("${arg_PUBLIC_INCLUDES}" STREQUAL "")
      target_include_directories(
        "${NAME}" ${public_or_interface} public_overrides)
    endif("${arg_PUBLIC_INCLUDES}" STREQUAL "")

    set(facades ${arg_IMPLEMENTS_FACADES})
    list(TRANSFORM facades APPEND ".facade")
    target_link_libraries("${NAME}" ${public_or_interface} ${facades})
  endif(NOT "${arg_IMPLEMENTS_FACADES}" STREQUAL "")
endfunction(pw_add_module_library)

# Declares a module as a facade.
#
# Facades are declared as two libraries to avoid circular dependencies.
# Libraries that use the facade depend on a library named for the module. The
# module that implements the facade depends on a library named
# MODULE_NAME.facade.
#
# pw_add_facade accepts the same arguments as pw_add_module_library, except for
# IMPLEMENTS_FACADES. It also accepts the following argument:
#
#  DEFAULT_BACKEND - which backend to use by default
#
function(pw_add_facade NAME)
  _pw_add_library_multi_value_args(list_args)
  pw_parse_arguments_strict(pw_add_facade 1 "" "DEFAULT_BACKEND" "${list_args}")

  # If no backend is set, a script that displays an error message is used
  # instead. If the facade is used in the build, it fails with this error.
  add_custom_target("${NAME}._no_backend_set_message"
    COMMAND
      "${CMAKE_COMMAND}" -E echo
        "ERROR: Attempted to build the ${NAME} facade with no backend."
        "Configure the ${NAME} backend using pw_set_backend or remove all dependencies on it."
        "See https://pigweed.dev/pw_build."
    COMMAND
      "${CMAKE_COMMAND}" -E false
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

  # This target is never used; it simply tests that the specified backend
  # actually exists in the build. The generator expression will fail to evaluate
  # if the target is not defined.
  add_custom_target(_pw_check_that_backend_for_${NAME}_is_defined
    COMMAND
      ${CMAKE_COMMAND} -E echo "$<TARGET_PROPERTY:${${NAME}_BACKEND},TYPE>"
  )

  # Define the facade library, which is used by the backend to avoid circular
  # dependencies.
  add_library("${NAME}.facade" INTERFACE)
  target_include_directories("${NAME}.facade" INTERFACE public)
  target_link_libraries("${NAME}.facade" INTERFACE ${arg_PUBLIC_DEPS})

  # Define the public-facing library for this facade, which depends on the
  # header files in .facade target and exposes the dependency on the backend.
  pw_add_module_library("${NAME}"
    SOURCES
      ${arg_SOURCES}
    HEADERS
      ${arg_HEADERS}
    PUBLIC_DEPS
      "${NAME}.facade"
      "${${NAME}_BACKEND}"
  )
endfunction(pw_add_facade)

# Sets which backend to use for the given facade.
function(pw_set_backend FACADE BACKEND)
  set("${FACADE}_BACKEND" "${BACKEND}" CACHE STRING "Backend for ${FACADE}" FORCE)
endfunction(pw_set_backend)

# Zephyr specific wrapper for pw_set_backend, selects the default zephyr backend based on a Kconfig while
# still allowing the application to set the backend if they choose to
function(pw_set_zephyr_backend_ifdef COND FACADE BACKEND)
  get_property(result CACHE "${FACADE}_BACKEND" PROPERTY TYPE)
  if(${${COND}})
    if("${result}" STREQUAL "")
      pw_set_backend("${FACADE}" "${BACKEND}")
    endif()
  endif()
endfunction()

# Set up the default pw_build_DEFAULT_MODULE_CONFIG.
set("pw_build_DEFAULT_MODULE_CONFIG" pw_build.empty CACHE STRING
    "Default implementation for all Pigweed module configurations.")

# Declares a module configuration variable for module libraries to depend on.
# Configs should be set to libraries which can be used to provide defines
# directly or though included header files.
#
# The configs can be selected either through the pw_set_module_config function
# to set the pw_build_DEFAULT_MODULE_CONFIG used by default for all Pigweed
# modules or by selecting a specific one for the given NAME'd configuration.
#
# Args:
#
#   NAME: name to use for the target which can be depended on for the config.
function(pw_add_module_config NAME)
  pw_parse_arguments_strict(pw_add_module_config 1 "" "" "")

  # Declare the module configuration variable for this module.
  set("${NAME}" "${pw_build_DEFAULT_MODULE_CONFIG}"
      CACHE STRING "Module configuration for ${NAME}")
endfunction(pw_add_module_config)

# Sets which config library to use for the given module.
#
# This can be used to set a specific module configuration or the default
# module configuration used for all Pigweed modules:
#
#   pw_set_module_config(pw_build_DEFAULT_MODULE_CONFIG my_config)
#   pw_set_module_config(pw_foo_CONFIG my_foo_config)
function(pw_set_module_config NAME LIBRARY)
  pw_parse_arguments_strict(pw_set_module_config 2 "" "" "")

  # Update the module configuration variable.
  set("${NAME}" "${LIBRARY}" CACHE STRING "Config for ${NAME}" FORCE)
endfunction(pw_set_module_config)

set(pw_unit_test_MAIN pw_unit_test.simple_printing_main CACHE STRING
    "Implementation of a main function for ``pw_test`` unit test binaries.")

set(pw_unit_test_GOOGLETEST_BACKEND pw_unit_test.light CACHE STRING
    "CMake target which implements GoogleTest, by default pw_unit_test.light \
     is used. You could, for example, point this at pw_third_party.googletest \
     if using upstream GoogleTest directly on your host for GoogleMock.")

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
#   DEFINES: defines to set for the source files in this test
#   GROUPS: groups to which to add this test; if none are specified, the test is
#       added to the 'default' and 'all' groups
#
function(pw_add_test NAME)
  pw_parse_arguments_strict(pw_add_test 1 "" "" "SOURCES;DEPS;DEFINES;GROUPS")

  add_executable("${NAME}" EXCLUDE_FROM_ALL ${arg_SOURCES})
  target_link_libraries("${NAME}"
    PRIVATE
      pw_unit_test
      ${pw_unit_test_MAIN}
      ${arg_DEPS}
  )
  target_compile_definitions("${NAME}"
    PRIVATE
      ${arg_DEFINES}
  )

  # Tests require at least one source file.
  if(NOT arg_SOURCES)
    target_sources("${NAME}" PRIVATE $<TARGET_PROPERTY:pw_build.empty,SOURCES>)
  endif()

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

# Adds compiler options to all targets built by CMake. Flags may be added any
# time after this function is defined. The effect is global; all targets added
# before or after a pw_add_global_compile_options call will be built with the
# flags, regardless of where the files are located.
#
# pw_add_global_compile_options takes one optional named argument:
#
#   LANGUAGES: Which languages (ASM, C, CXX) to apply the options to. Flags
#       apply to all languages by default.
#
# All other arguments are interpreted as compiler options.
function(pw_add_global_compile_options)
  cmake_parse_arguments(PARSE_ARGV 0 args "" "" "LANGUAGES")

  set(supported_build_languages ASM C CXX)

  if(NOT args_LANGUAGES)
    set(args_LANGUAGES ${supported_build_languages})
  endif()

  # Check the selected language.
  foreach(lang IN LISTS args_LANGUAGES)
    if(NOT "${lang}" IN_LIST supported_build_languages)
      message(FATAL_ERROR "'${lang}' is not a supported language. "
              "Supported languages: ${supported_build_languages}")
    endif()
  endforeach()

  # Enumerate which flags variables to set.
  foreach(lang IN LISTS args_LANGUAGES)
    list(APPEND cmake_flags_variables "CMAKE_${lang}_FLAGS")
  endforeach()

  # Set each flag for each specified flags variable.
  foreach(variable IN LISTS cmake_flags_variables)
    foreach(flag IN LISTS args_UNPARSED_ARGUMENTS)
      set(${variable} "${${variable}} ${flag}" CACHE INTERNAL "" FORCE)
    endforeach()
  endforeach()
endfunction(pw_add_global_compile_options)
