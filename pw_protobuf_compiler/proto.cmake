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
#
function(pw_proto_library NAME)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPS")

  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/protos")

  # Use INTERFACE libraries to track the proto include paths that are passed to
  # protoc.
  set(include_deps "${arg_DEPS}")
  list(TRANSFORM include_deps APPEND ._includes)

  add_library("${NAME}._includes" INTERFACE)
  target_include_directories("${NAME}._includes" INTERFACE ".")
  target_link_libraries("${NAME}._includes" INTERFACE ${include_deps})

  # Generate a file with all include paths needed by protoc.
  set(include_file "${out_dir}/${NAME}.include_paths.txt")
  file(GENERATE OUTPUT "${include_file}"
     CONTENT
       "$<TARGET_PROPERTY:${NAME}._includes,INTERFACE_INCLUDE_DIRECTORIES>")

  # Create a protobuf target for each supported protobuf library.
  _pw_pwpb_library(
      "${NAME}" "${arg_SOURCES}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_raw_rpc_library(
      "${NAME}" "${arg_SOURCES}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_nanopb_library(
      "${NAME}" "${arg_SOURCES}" "${arg_DEPS}" "${include_file}" "${out_dir}")
  _pw_nanopb_rpc_library(
      "${NAME}" "${arg_SOURCES}" "${arg_DEPS}" "${include_file}" "${out_dir}")
endfunction(pw_proto_library)

# Internal function that invokes protoc through generate_protos.py.
function(_pw_generate_protos
      TARGET LANGUAGE PLUGIN OUTPUT_EXTS INCLUDE_FILE OUT_DIR SOURCES DEPS)
  # Determine the names of the output files.
  foreach(extension IN LISTS OUTPUT_EXTS)
    foreach(source_file IN LISTS SOURCES)
      get_filename_component(dir "${source_file}" DIRECTORY)
      get_filename_component(name "${source_file}" NAME_WE)
      list(APPEND outputs "${OUT_DIR}/${dir}/${name}${extension}")
    endforeach()
  endforeach()

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
      python
      "${script}"
      --language "${LANGUAGE}"
      --plugin-path "${PLUGIN}"
      --module-path "${CMAKE_CURRENT_SOURCE_DIR}"
      --include-file "${INCLUDE_FILE}"
      --out-dir "${OUT_DIR}"
      ${ARGN}
      ${SOURCES}
    DEPENDS
      ${SOURCES}
      ${script}
      ${DEPS}
    OUTPUT
      ${outputs}
  )
  add_custom_target("${TARGET}" DEPENDS ${outputs})
endfunction(_pw_generate_protos)

# Internal function that creates a pwpb proto library.
function(_pw_pwpb_library NAME SOURCES DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .pwpb)

  _pw_generate_protos("${NAME}.generate.pwpb"
      pwpb
      "$ENV{PW_ROOT}/pw_protobuf/py/pw_protobuf/plugin.py"
      ".pwpb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.pwpb" INTERFACE)
  target_include_directories("${NAME}.pwpb" INTERFACE "${OUT_DIR}")
  target_link_libraries("${NAME}.pwpb" INTERFACE pw_protobuf ${DEPS})
  add_dependencies("${NAME}.pwpb" "${NAME}.generate.pwpb")
endfunction(_pw_pwpb_library)

# Internal function that creates a raw_rpc proto library.
function(_pw_raw_rpc_library NAME SOURCES DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .raw_rpc)

  _pw_generate_protos("${NAME}.generate.raw_rpc"
      raw_rpc
      "$ENV{PW_ROOT}/pw_rpc/py/pw_rpc/plugin_raw.py"
      ".raw_rpc.pb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.raw_rpc" INTERFACE)
  target_include_directories("${NAME}.raw_rpc" INTERFACE "${OUT_DIR}")
  target_link_libraries("${NAME}.raw_rpc"
    INTERFACE
      pw_rpc.raw
      pw_rpc.server
      ${DEPS}
  )
  add_dependencies("${NAME}.raw_rpc" "${NAME}.generate.raw_rpc")
endfunction(_pw_raw_rpc_library)

# Internal function that creates a nanopb proto library.
function(_pw_nanopb_library NAME SOURCES DEPS INCLUDE_FILE OUT_DIR)
  list(TRANSFORM DEPS APPEND .nanopb)

  set(nanopb_dir "$<TARGET_PROPERTY:$<IF:$<TARGET_EXISTS:protobuf-nanopb-static>,protobuf-nanopb-static,pw_build.empty>,SOURCE_DIR>")
  set(nanopb_plugin
      "$<IF:$<TARGET_EXISTS:protobuf-nanopb-static>,${nanopb_dir}/generator/protoc-gen-nanopb,COULD_NOT_FIND_protobuf-nanopb-static_TARGET_PLEASE_SET_UP_NANOPB>")

  _pw_generate_protos("${NAME}.generate.nanopb"
      nanopb
      "${nanopb_plugin}"
      ".pb.h;.pb.c"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${DEPS}"
      --include-paths "${nanopb_dir}/generator/proto"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.nanopb" EXCLUDE_FROM_ALL ${generated_outputs})
  target_include_directories("${NAME}.nanopb" PUBLIC "${OUT_DIR}")
  target_link_libraries("${NAME}.nanopb" PUBLIC pw_third_party.nanopb ${DEPS})
  add_dependencies("${NAME}.nanopb" "${NAME}.generate.nanopb")
endfunction(_pw_nanopb_library)

# Internal function that creates a nanopb_rpc library.
function(_pw_nanopb_rpc_library NAME SOURCES DEPS INCLUDE_FILE OUT_DIR)
  # Determine the names of the output files.
  list(TRANSFORM DEPS APPEND .nanopb_rpc)

  _pw_generate_protos("${NAME}.generate.nanopb_rpc"
      nanopb_rpc
      "$ENV{PW_ROOT}/pw_rpc/py/pw_rpc/plugin_nanopb.py"
      ".rpc.pb.h"
      "${INCLUDE_FILE}"
      "${OUT_DIR}"
      "${SOURCES}"
      "${DEPS}"
  )

  # Create the library with the generated source files.
  add_library("${NAME}.nanopb_rpc" INTERFACE)
  target_include_directories("${NAME}.nanopb_rpc" INTERFACE "${OUT_DIR}")
  target_link_libraries("${NAME}.nanopb_rpc"
    INTERFACE
      "${NAME}.nanopb"
      pw_rpc.nanopb.method_union
      pw_rpc.server
      ${DEPS}
  )
  add_dependencies("${NAME}.nanopb_rpc" "${NAME}.generate.nanopb_rpc")
endfunction(_pw_nanopb_rpc_library)
