# Copyright 2023 The Pigweed Authors
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

# This function creates a library under the specified ${NAME} which provides a
# generated token database for a given ELF file using pw_tokenizer/database.py.
#
# Produces the ${NAME} token database.
#
# Args:
#
#   NAME - name of the library to create
#   COMMIT - Deletes temporary tokens in memory and on disk when a CSV exists
#       within a commit.
#   CREATE - Create a database. Must be set to one of the supported database
#       types: "csv" or "binary".
#   DATABASE - If updating a database, path to an existing database in the
#       source tree; optional if creating a database, but may provide an output
#       directory path to override the default of
#       "$target_gen_dir/$target_name.[csv/binary]"
#   DEPS - CMake targets to build prior to generating the database; artifacts
#       from these targets are NOT implicitly used for database generation.
#   DOMAIN - If provided, extract strings from tokenization domains matching
#       this regular expression.
#   TARGET - CMake target (executable or library) from which to add tokens;
#       this target is also added to deps.
function(pw_tokenizer_database NAME)
  pw_parse_arguments(
    NUM_POSITIONAL_ARGS
      1
    ONE_VALUE_ARGS
      COMMIT
      CREATE
      DATABASE
      DEPS
      DOMAIN
      TARGET
    REQUIRED_ARGS
      TARGET
  )

  if(NOT DEFINED arg_CREATE AND NOT DEFINED arg_DATABASE)
    message(FATAL_ERROR "pw_tokenizer_database requires a `database` "
            "variable, unless 'CREATE' is specified")
  endif()

  set(_create "")
  if(DEFINED arg_CREATE)
    if (NOT (${arg_CREATE} STREQUAL "csv" OR ${arg_CREATE} STREQUAL "binary"))
      message(FATAL_ERROR "'CREATE' must be \"csv\" or \"binary\".")
    endif()
    set(_create ${arg_CREATE})
    set(_create_new_database TRUE)
  endif()

  set(_database "")
  if(DEFINED arg_DATABASE)
    set(_database ${arg_DATABASE})
  else()
    # Default to appending the create type as the extension.
    set(_database ${NAME}.${_create})
  endif()

  set(_domain "")
  if(DEFINED arg_DOMAIN)
    set(_domain "#${arg_DOMAIN}")
  endif()

  add_library(${NAME} INTERFACE)
  add_dependencies(${NAME} INTERFACE ${NAME}_generated_token_db)

  if (DEFINED _create_new_database)
    add_custom_command(
        COMMENT "Generating the ${_database} token database"
        COMMAND
          ${Python3_EXECUTABLE}
          "$ENV{PW_ROOT}/pw_tokenizer/py/pw_tokenizer/database.py" create
          --database ${_database}
          --type ${_create}
          "$<TARGET_FILE:${arg_TARGET}>${_domain}"
          --force
        DEPENDS
          ${arg_DEPS}
          ${arg_TARGET}
        OUTPUT ${_database} POST_BUILD
    )
  else()
    set(_discard_temporary "")
    if(DEFINED arg_COMMIT)
      set(_discard_temporary "--discard-temporary ${arg_COMMIT}")
    endif()

    add_custom_command(
        COMMENT "Updating the ${_database} token database"
        COMMAND
          ${Python3_EXECUTABLE}
          "$ENV{PW_ROOT}/pw_tokenizer/py/pw_tokenizer/database.py" add
          --database ${_database}
          ${_discard_temporary}
          "$<TARGET_FILE:${arg_TARGET}>${_domain}"
        DEPENDS
          ${arg_DEPS}
          ${arg_TARGET}
        OUTPUT ${_database} POST_BUILD
    )
  endif()

  add_custom_target(${NAME}_generated_token_db
    DEPENDS ${_database}
  )
endfunction()
