# Copyright 2025 The Pigweed Authors
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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

# Path to flatc
set(pw_flatbuffers_FLATC flatc CACHE STRING "Flatbuffers compiler")
# Global args for flatc
set(pw_flatbuffers_FLATC_FLAGS "" CACHE LIST "Global args to flatc")

# If set, this is added to the generated library as a public dependency. Since
# flatbuffers is a header only library, this should be a library containing all
# the headers as HEADERS, then exposing the include path as a public include,
# so anything depending on flatbuffers can properly find the headers. This is
# intended for pigweed itself to use when working with the installed
# flatbuffers package, but any client can use this to substitute their
# flatbuffers implementation as well, if they are not relying on the pigweed
# provided implementation. The user may also opt to add this dependency to the
# DEPS argument instead.

if ("${dir_pw_third_party_flatbuffers}" STREQUAL "")
  set(pw_flatbuffers_LIBRARY "" CACHE STRING "Dependency name for flatbuffers headers")
else()
  set(pw_flatbuffers_LIBRARY pw_third_party.flatbuffers CACHE STRING "Dependency name for flatbuffers headers")
endif()

function(pw_flatbuffer_library NAME)
  pw_parse_arguments(
    NUM_POSITIONAL_ARGS
      1
    MULTI_VALUE_ARGS
      SOURCES
      DEPS
      FLATC_FLAGS
    REQUIRED_ARGS
      SOURCES
  )

  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
  set(out_sources_dir "${out_dir}/sources")

  # Create a set of files with absolute paths which will be the files we
  # mirror over to the build directory
  pw_rebase_paths(files_to_mirror "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${arg_SOURCES}" "")
  # Create the set of files in files_to_mirror, but replaced with the
  # absolute destination path to the build directory
  pw_rebase_paths(sources "${out_sources_dir}"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${arg_SOURCES}" "")

  add_custom_command(
    COMMAND
      python3
      "$ENV{PW_ROOT}/pw_build/py/pw_build/mirror_tree.py"
      --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
      --directory "${out_sources_dir}"
      ${files_to_mirror}
    DEPENDS
      "$ENV{PW_ROOT}/pw_build/py/pw_build/mirror_tree.py"
      ${files_to_mirror}
    OUTPUT
      ${sources}
  )
  add_custom_target("${NAME}._sources" DEPENDS ${sources})

  # Generate the C++ headers.
  foreach(source IN LISTS sources)
    cmake_path(GET source FILENAME source_filename)

    cmake_path(RELATIVE_PATH source BASE_DIRECTORY "${out_sources_dir}"
               OUTPUT_VARIABLE rel_source)
    cmake_path(REMOVE_FILENAME rel_source OUTPUT_VARIABLE rel_source_dir)

    cmake_path(REMOVE_EXTENSION source_filename OUTPUT_VARIABLE
               generated_filename)
    cmake_path(APPEND_STRING generated_filename "_generated.h")

    cmake_path(SET generated_directory "${out_dir}")
    cmake_path(APPEND generated_directory "cpp" ${rel_source_dir})

    cmake_path(SET output "${generated_directory}")
    cmake_path(APPEND output ${generated_filename})

    add_custom_command(
      COMMAND "${pw_flatbuffers_FLATC}" --cpp ${pw_flatbuffers_FLATC_FLAGS} ${arg_FLATC_FLAGS}
              -o "${generated_directory}" ${source}
      DEPENDS ${source}
      OUTPUT ${output}
    )
    list(APPEND outputs "${output}")
  endforeach()
  add_custom_target("${NAME}._generated_sources" DEPENDS ${outputs})

  # Add the ${NAME}.cpp library using the generated C++ headers.
  pw_add_library_generic("${NAME}.cpp" INTERFACE
    PUBLIC_INCLUDES
      "${out_dir}/cpp"
    PUBLIC_DEPS
      ${pw_flatbuffers_LIBRARY}
      ${arg_DEPS}
  )
  add_dependencies("${NAME}.cpp"
                   "${NAME}._sources" "${NAME}._generated_sources")
endfunction()
