# Copyright 2024 The Pigweed Authors
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
include($ENV{PW_ROOT}/pw_tls_client/backend.cmake)

pw_add_facade(pw_tls_client.pw_tls_client INTERFACE
  BACKEND
    pw_tls_client.pw_tls_client_BACKEND
  HEADERS
    public/pw_tls_client/options.h
    public/pw_tls_client/session.h
    public/pw_tls_client/status.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_result
    pw_status
    pw_stream
    pw_string
)

pw_add_facade(pw_tls_client.time INTERFACE
  BACKEND
    pw_tls_client.time_BACKEND
  PUBLIC_LINK_OPTIONS
    -Wl,--wrap=time
    -Wl,--wrap=gettimeofday
)

function(generate_build_time_header NAME)
  pw_parse_arguments(
    NUM_POSITIONAL_ARGS
      1
  )

  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
  set(output "${out_dir}/build_time.h")
  get_filename_component(output_include_path "${output}" DIRECTORY)

  set(gen_header "$ENV{PW_ROOT}/pw_tls_client/generate_build_time_header.py")
  LIST(APPEND gen_header_cmd python3 "${gen_header}" ${output})

  add_custom_command(
    COMMAND
      ${gen_header_cmd}
    DEPENDS
      ${gen_header}
    OUTPUT
      ${output})

  add_custom_target("${NAME}._generate" DEPENDS ${output})

  pw_add_library_generic("${NAME}" INTERFACE
    PUBLIC_INCLUDES
      ${output_include_path}
  )
  add_dependencies("${NAME}" "${NAME}._generate")
endfunction()

generate_build_time_header(pw_tls_client.build_time_header)

pw_add_library(pw_tls_client.build_time STATIC
  SOURCES
    build_time.cc
  PUBLIC_DEPS
    pw_tls_client.build_time_header
    pw_tls_client.time.facade
)
