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

# Generates a sensor library
#
# Args:
#   OUT_HEADER: The path/to/header.h to generate
#   SOURCES: YAML files defining sensors
#   OUT_INCLUDES: [optional] The include path to expose in the final library, if
#     not defined, the root of the 'out_header' will be used so including the
#     header will be done via '#include "path/to/header.h"'
#   INPUTS: [optional] YAML files included by the sensors, these will be
#     used to optimize re-building.
#   GENERATOR: [optional] Python generator script, if not set, the default
#     Pigweed generator will be used.
#   GENERATOR_ARGS: [optional] Command line arguments to pass to the generator.
#   GENERATOR_INCLUDES: [optional] Include paths to pass to the generator. These
#     are used to resolve the sensor dependencies.
#   PUBLIC_DEPS: [optional] Public dependencies to pass to the final generated
#     target.
#
# Example use:
# pw_sensor_library(my_sensors
#   OUT_HEADER
#     ${CMAKE_BINARY_DIR}/generated/include/my/app/sensors.h
#   OUT_INCLUDES
#     ${CMAKE_BINARY_DIR}/generated/include
#   SOURCES
#     sensors/bma4xx.yaml
#     sensors/bmi160.yaml
#   INPUTS
#     sensors/attributes.yaml
#     sensors/channels.yaml
#     sensors/triggers.yaml
#     sensors/units.yaml
#   GENERATOR
#     scripts/sensor_header_generator.py
#   GENERATOR_ARGS
#     -v
#     -experimental
#   GENERATOR_INCLUDES
#     ${CMAKE_CURRENT_LIST_DIR}
#   PUBLIC_DEPS
#     pw_sensor.types
#     pw_containers
# )
function(pw_sensor_library NAME)
  pw_parse_arguments(
    NUM_POSITIONAL_ARGS
      1
    MULTI_VALUE_ARGS
      INPUTS
      GENERATOR_INCLUDES
      SOURCES
      GENERATOR_ARGS
      PUBLIC_DEPS
      OUT_INCLUDES
    ONE_VALUE_ARGS
      OUT_HEADER
      GENERATOR
    REQUIRED_ARGS
      GENERATOR_INCLUDES
      SOURCES
      OUT_HEADER
  )

  if("${arg_GENERATOR}" STREQUAL "")
    set(arg_GENERATOR "$ENV{PW_ROOT}/pw_sensor/py/pw_sensor/constants_generator.py")

    if("${arg_GENERATOR_ARGS}" STREQUAL "")
      set(arg_GENERATOR_ARGS --package pw.sensor)
    endif()
  endif()

  if(IS_ABSOLUTE "${arg_OUT_HEADER}")
    if(NOT ${arg_OUT_INCLUDES})
      message(FATAL_ERROR "Invalid absolute path OUT_HEADER=${arg_OUT_HEADER}, missing OUT_INCLUDES")
    endif()

    set(output_file "${arg_OUT_HEADER}")
  else()
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${arg_OUT_HEADER}")
    if("${arg_OUT_INCLUDES}" STREQUAL "")
      set(arg_OUT_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
  endif()

  string(REPLACE ";" " " generator_args "${arg_GENERATOR_ARGS}")

  set(include_list)

  foreach(item IN LISTS arg_GENERATOR_INCLUDES)
    list(APPEND include_list "-I" "${item}")
  endforeach()

  add_custom_command(
    OUTPUT ${output_file}
    COMMAND python3
    $ENV{PW_ROOT}/pw_sensor/py/pw_sensor/sensor_desc.py
      ${include_list}
      -g "python3 ${arg_GENERATOR} ${generator_args}"
      -o ${output_file}
      ${arg_SOURCES}
    DEPENDS
      ${arg_GENERATOR}
      $ENV{PW_ROOT}/pw_sensor/py/pw_sensor/sensor_desc.py
      ${arg_INPUTS}
      ${arg_SOURCES}
  )
  add_custom_target(${NAME}.__generate_constants
    DEPENDS
    ${output_file}
  )
  add_library(${NAME} STATIC
      ${output_file}
  )
  target_link_libraries(${NAME} PUBLIC ${arg_PUBLIC_DEPS})
  target_include_directories(${NAME} PUBLIC
      ${arg_OUT_INCLUDES}
  )
  add_dependencies(${NAME} ${NAME}.__generate_constants)
  set_target_properties(${NAME} PROPERTIES LINKER_LANGUAGE CXX)
endfunction(pw_sensor_library)
