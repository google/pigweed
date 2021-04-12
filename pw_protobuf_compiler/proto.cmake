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

# Declares a protocol buffers library. This function creates a library for each
# supported protocol buffer implementation:
#
#   ${NAME}.pwpb - pw_protobuf generated code
#   ${NAME}.nanopb - Nanopb generated code (requires Nanopb)
#
# This function also creates libraries for generating pw_rpc code:
#
#   ${NAME}.nanopb_rpc - generates Nanopb pw_rpc code
#   ${NAME}.raw_rpc - generates raw pw_rpc (no protobuf library) code
#   ${NAME}.pwpb_rpc - (Not implemented) generates pw_protobuf pw_rpc code
#
# Args:
#
#   NAME - the base name of the libraries to create
#   SOURCES - .proto source files
#   DEPS - dependencies on other pw_proto_library targets
#   PREFIX - prefix add to the proto files
#   STRIP_PREFIX - prefix to remove from the proto files
#   INPUTS - files to include along with the .proto files (such as Nanopb
#       .options files
#
function(pw_proto_library NAME)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "STRIP_PREFIX;PREFIX"
      "SOURCES;INPUTS;DEPS")

  if("${arg_SOURCES}" STREQUAL "")
    message(FATAL_ERROR
        "pw_proto_library requires at least one .proto file in SOURCES. No "
        "SOURCES were listed for ${NAME}.")
  endif()

  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")

  # Use INTERFACE libraries to track the proto include paths that are passed to
  # protoc.
  set(include_deps "${arg_DEPS}")
  list(TRANSFORM include_deps APPEND ._includes)

  add_library("${NAME}._includes" INTERFACE)
  target_include_directories("${NAME}._includes" INTERFACE "${out_dir}/sources")
  target_link_libraries("${NAME}._includes" INTERFACE ${include_deps})

  # Generate a file with all include paths needed by protoc. Use the include
  # directory paths and replace ; with \n.
  set(include_file "${out_dir}/include_paths.txt")
  file(GENERATE OUTPUT "${include_file}"
     CONTENT
       "$<JOIN:$<TARGET_PROPERTY:${NAME}._includes,INTERFACE_INCLUDE_DIRECTORIES>,\n>")

  if("${arg_STRIP_PREFIX}" STREQUAL "")
    set(arg_STRIP_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  foreach(path IN LISTS arg_SOURCES arg_INPUTS)
    get_filename_component(abspath "${path}" ABSOLUTE)
    list(APPEND files_to_mirror "${abspath}")
  endforeach()

  # Mirror the sources to the output directory with the specified prefix.
  _pw_rebase_paths(
      sources "${out_dir}/sources/${arg_PREFIX}" "${arg_STRIP_PREFIX}" "${arg_SOURCES}" "")
  _pw_rebase_paths(
      inputs "${out_dir}/sources/${arg_PREFIX}" "${arg_STRIP_PREFIX}" "${arg_INPUTS}" "")

  add_custom_command(
    COMMAND
      python3
      "$ENV{PW_ROOT}/pw_build/py/pw_build/mirror_tree.py"
      --source-root "${arg_STRIP_PREFIX}"
      --directory "${out_dir}/sources/${arg_PREFIX}"
      ${files_to_mirror}
    DEPENDS
      "$ENV{PW_ROOT}/pw_build/py/pw_build/mirror_tree.py"
      ${files_to_mirror}
    OUTPUT
      ${sources} ${inputs}
  )
  add_custom_target("${NAME}._sources" DEPENDS ${sources} ${inputs})

  set(sources_deps "${arg_DEPS}")
  list(TRANSFORM sources_deps APPEND ._sources)

  if(sources_deps)
    add_dependencies("${NAME}._sources" ${sources_deps})
  endif()

  # Create a protobuf target for each supported protobuf library.
  _pw_pwpb_library(
      "${NAME}" "${sources}" "${inputs}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_raw_rpc_library(
      "${NAME}" "${sources}" "${inputs}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_nanopb_library(
      "${NAME}" "${sources}" "${inputs}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_nanopb_rpc_library(
      "${NAME}" "${sources}" "${inputs}" "${arg_DEPS}" "${include_file}" "${out_dir}")
endfunction(pw_proto_library)

function(_pw_rebase_paths VAR OUT_DIR ROOT FILES EXTENSIONS)
  foreach(file IN LISTS FILES)
    get_filename_component(file "${file}" ABSOLUTE)
    file(RELATIVE_PATH file "${ROOT}" "${file}")

    if ("${EXTENSIONS}" STREQUAL "")
      list(APPEND mirrored_files "${OUT_DIR}/${file}")
    else()
      foreach(ext IN LISTS EXTENSIONS)
        get_filename_component(dir "${file}" DIRECTORY)
        get_filename_component(name "${file}" NAME_WE)
        list(APPEND mirrored_files "${OUT_DIR}/${dir}/${name}${ext}")
      endforeach()
    endif()
  endforeach()

  set("${VAR}" "${mirrored_files}" PARENT_SCOPE)
endfunction(_pw_rebase_paths)

# Internal function that invokes protoc through generate_protos.py.
function(_pw_generate_protos
    TARGET LANGUAGE PLUGIN OUTPUT_EXTS INCLUDE_FILE OUT_DIR SOURCES INPUTS DEPS)
  # Determine the names of the compiled output files.
  _pw_rebase_paths(outputs
      "${OUT_DIR}/${LANGUAGE}" "${OUT_DIR}/sources" "${SOURCES}" "${OUTPUT_EXTS}")

  # Export the output files to the caller's scope so it can use them if needed.
  set(generated_outputs "${outputs}" PARENT_SCOPE)

  if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
      get_filename_component(dir "${source_file}" DIRECTORY)
      get_filename_component(name "${source_file}" NAME_WE)
      set(PLUGIN "${dir}/${name}.bat")
  endif()

  set(script "$ENV{PW_ROOT}/pw_protobuf_compiler/py/pw_protobuf_compiler/generate_protos.py")
  add_custom_command(
    COMMAND
      python3
      "${script}"
      --language "${LANGUAGE}"
      --plugin-path "${PLUGIN}"
      --include-file "${INCLUDE_FILE}"
      --compile-dir "${OUT_DIR}/sources"
      --out-dir "${OUT_DIR}/${LANGUAGE}"
      --sources ${SOURCES}
    DEPENDS
      ${script}
      ${SOURCES}
      ${INPUTS}
      ${DEPS}
    OUTPUT
      ${outputs}
  )
  add_custom_target("${TARGET}" DEPENDS ${outputs})
endfunction(_pw_generate_protos)

# Internal function that creates a pwpb proto library.
function(_pw_pwpb_library NAME SOURCES INPUTS DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .pwpb)

  _pw_generate_protos("${NAME}._generate.pwpb"
      pwpb
      "$ENV{PW_ROOT}/pw_protobuf/py/pw_protobuf/plugin.py"
      ".pwpb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${INPUTS}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.pwpb" INTERFACE)
  target_include_directories("${NAME}.pwpb" INTERFACE "${OUT_DIR}/pwpb")
  target_link_libraries("${NAME}.pwpb" INTERFACE pw_protobuf ${DEPS})
  add_dependencies("${NAME}.pwpb" "${NAME}._generate.pwpb")
endfunction(_pw_pwpb_library)

# Internal function that creates a raw_rpc proto library.
function(_pw_raw_rpc_library NAME SOURCES INPUTS DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .raw_rpc)

  _pw_generate_protos("${NAME}._generate.raw_rpc"
      raw_rpc
      "$ENV{PW_ROOT}/pw_rpc/py/pw_rpc/plugin_raw.py"
      ".raw_rpc.pb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${INPUTS}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.raw_rpc" INTERFACE)
  target_include_directories("${NAME}.raw_rpc" INTERFACE "${OUT_DIR}/raw_rpc")
  target_link_libraries("${NAME}.raw_rpc"
    INTERFACE
      pw_rpc.raw
      pw_rpc.server
      ${DEPS}
  )
  add_dependencies("${NAME}.raw_rpc" "${NAME}._generate.raw_rpc")
endfunction(_pw_raw_rpc_library)

# Internal function that creates a nanopb proto library.
function(_pw_nanopb_library NAME SOURCES INPUTS DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .nanopb)

  if("${dir_pw_third_party_nanopb}" STREQUAL "")
    add_custom_target("${NAME}._generate.nanopb"
        cmake -E echo
            ERROR: Attempting to use pw_proto_library, but
            dir_pw_third_party_nanopb is not set. Set dir_pw_third_party_nanopb
            to the path to the Nanopb repository.
      COMMAND
        cmake -E false
      DEPENDS
        ${DEPS}
      SOURCES
        ${SOURCES}
    )
    set(generated_outputs $<TARGET_PROPERTY:pw_build.empty,SOURCES>)
  else()
    _pw_generate_protos("${NAME}._generate.nanopb"
        nanopb
        "${dir_pw_third_party_nanopb}/generator/protoc-gen-nanopb"
        ".pb.h;.pb.c"
        "${INCLUDE_FILE}"
        "${OUT_DIR}"
        "${SOURCES}"
        "${INPUTS}"
        "${DEPS}"
    )
  endif()

  # Create the library with the generated source files.
  add_library("${NAME}.nanopb" EXCLUDE_FROM_ALL ${generated_outputs})
  target_include_directories("${NAME}.nanopb" PUBLIC "${OUT_DIR}/nanopb")
  target_link_libraries("${NAME}.nanopb" PUBLIC pw_third_party.nanopb ${DEPS})
  add_dependencies("${NAME}.nanopb" "${NAME}._generate.nanopb")
endfunction(_pw_nanopb_library)

# Internal function that creates a nanopb_rpc library.
function(_pw_nanopb_rpc_library NAME SOURCES INPUTS DEPS INCLUDE_FILE OUT_DIR)
  # Determine the names of the output files.
  list(TRANSFORM DEPS APPEND .nanopb_rpc)

  _pw_generate_protos("${NAME}._generate.nanopb_rpc"
      nanopb_rpc
      "$ENV{PW_ROOT}/pw_rpc/py/pw_rpc/plugin_nanopb.py"
      ".rpc.pb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${INPUTS}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.nanopb_rpc" INTERFACE)
  target_include_directories("${NAME}.nanopb_rpc"
    INTERFACE
      "${OUT_DIR}/nanopb_rpc"
  )
  target_link_libraries("${NAME}.nanopb_rpc"
    INTERFACE
      "${NAME}.nanopb"
      pw_rpc.nanopb.method_union
      pw_rpc.server
      ${DEPS}
  )
  add_dependencies("${NAME}.nanopb_rpc" "${NAME}._generate.nanopb_rpc")
endfunction(_pw_nanopb_rpc_library)
