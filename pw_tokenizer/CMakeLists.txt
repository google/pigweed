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

pw_add_module_library(pw_tokenizer
  SOURCES
    encode_args.cc
    hash.cc
    tokenize.cc
  PUBLIC_DEPS
    pw_polyfill.overrides
    pw_preprocessor
    pw_span
  PRIVATE_DEPS
    pw_varint
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "")
  target_link_options(pw_tokenizer
    PUBLIC
      "-T${CMAKE_CURRENT_SOURCE_DIR}/pw_tokenizer_linker_sections.ld"
  )
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  target_link_options(pw_tokenizer
    PUBLIC
      "-T${CMAKE_CURRENT_SOURCE_DIR}/add_tokenizer_sections_to_default_script.ld"
      "-L${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()

pw_add_module_library(pw_tokenizer.base64
  SOURCES
    base64.cc
  PUBLIC_DEPS
    pw_base64
    pw_containers
    pw_polyfill.overrides
    pw_preprocessor
    pw_span
)

pw_add_module_library(pw_tokenizer.decoder
  SOURCES
    decode.cc
    detokenize.cc
    token_database.cc
  PUBLIC_DEPS
    pw_span
    pw_tokenizer
  PRIVATE_DEPS
    pw_varint
)

pw_add_facade(pw_tokenizer.global_handler
  SOURCES
    tokenize_to_global_handler.cc
  PUBLIC_DEPS
    pw_tokenizer
  DEFAULT_BACKEND
    pw_build.empty  # Default to an empty backend so the tests can run.
)

pw_add_facade(pw_tokenizer.global_handler_with_payload
  SOURCES
    tokenize_to_global_handler_with_payload.cc
  PUBLIC_DEPS
    pw_tokenizer
  DEFAULT_BACKEND
    pw_build.empty  # Default to an empty backend so the tests can run.
)

# Executable for generating test data for the C++ and Python detokenizers. This
# target should only be built for the host.
add_executable(pw_tokenizer.generate_decoding_test_data EXCLUDE_FROM_ALL
    generate_decoding_test_data.cc)
target_link_libraries(pw_tokenizer.generate_decoding_test_data PRIVATE
    pw_varint pw_tokenizer)
target_compile_options(pw_tokenizer.generate_decoding_test_data PRIVATE
    -Wall -Werror)

# Executable for generating a test ELF file for elf_reader_test.py. A host
# version of this binary is checked in for use in elf_reader_test.py.
add_executable(pw_tokenizer.elf_reader_test_binary EXCLUDE_FROM_ALL
    py/elf_reader_test_binary.c)
target_link_libraries(pw_tokenizer.elf_reader_test_binary PRIVATE
    -Wl,--unresolved-symbols=ignore-all)  # main is not defined
set_target_properties(pw_tokenizer.elf_reader_test_binary PROPERTIES
    OUTPUT_NAME elf_reader_test_binary.elf)

pw_add_test(pw_tokenizer.argument_types_test
  SOURCES
    argument_types_test_c.c
    argument_types_test.cc
  DEPS
    pw_tokenizer
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.base64_test
  SOURCES
    base64_test.cc
  DEPS
    pw_tokenizer.base64
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.decode_test
  SOURCES
    decode_test.cc
  DEPS
    pw_varint
    pw_tokenizer.decoder
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.detokenize_test
  SOURCES
    detokenize_test.cc
  DEPS
    pw_tokenizer.decoder
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.global_handlers_test
  SOURCES
    global_handlers_test_c.c
    global_handlers_test.cc
  DEPS
    pw_tokenizer.global_handler
    pw_tokenizer.global_handler_with_payload
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.hash_test
  SOURCES
    hash_test.cc
  DEPS
    pw_tokenizer
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.token_database_test
  SOURCES
    token_database_test.cc
  DEPS
    pw_tokenizer.decoder
  GROUPS
    modules
    pw_tokenizer
)

pw_add_test(pw_tokenizer.tokenize_test
  SOURCES
    tokenize_test_c.c
    tokenize_test.cc
  DEPS
    pw_varint
    pw_tokenizer
  GROUPS
    modules
    pw_tokenizer
)
