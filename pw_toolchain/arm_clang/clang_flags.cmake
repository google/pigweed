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

# Clang isn't a plug-and-play experience for Cortex-M baremetal targets; it's
# missing C runtime libraries, C/C++ standard libraries, and a few other
# things. This template uses the provided cflags, asmflags, ldflags, etc. to
# generate a config that pulls the missing components from an arm-none-eabi-gcc
# compiler on the system PATH. The end result is a clang-based compiler that
# pulls in gcc-provided headers and libraries to complete the toolchain.
#
# This is effectively meant to be the cmake equivalent of clang_config.gni
# which contains helper tools for getting these flags.
function(_pw_get_clang_flags OUTPUT_VARIABLE TYPE)
  execute_process(
    COMMAND python
            $ENV{PW_ROOT}/pw_toolchain/py/pw_toolchain/clang_arm_toolchain.py
            ${TYPE} -- ${ARGN}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_VARIABLE _result
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(${OUTPUT_VARIABLE} ${_result} PARENT_SCOPE)
endfunction()

# This returns the compile flags needed for compiling with clang as a string.
#
# Usage:
#
#   pw_get_clang_compile_flags(OUTPUT_VARIABLE ARCH_FLAG [ARCH_FLAG ...])
#
# Example:
#
#   # Retrieve the compile flags needed for this arch and store them in "result".
#   pw_get_clang_compile_flags(result -mcpu=cortex-m33 -mthumb -mfloat-abi=hard)
#
function(pw_get_clang_compile_flags OUTPUT_VARIABLE)
  _pw_get_clang_flags(_result --cflags ${ARGN})
  set(${OUTPUT_VARIABLE} ${_result} PARENT_SCOPE)
endfunction()

# This returns the link flags needed for compiling with clang as a string.
#
# Usage:
#
#   pw_get_clang_link_flags(OUTPUT_VARIABLE ARCH_FLAG [ARCH_FLAG ...])
#
# Example:
#
#   # Retrieve the compile flags needed for this arch and store them in "result".
#   pw_get_clang_link_flags(result -mcpu=cortex-m33 -mthumb -mfloat-abi=hard)
#
function(pw_get_clang_link_flags OUTPUT_VARIABLE)
  _pw_get_clang_flags(_result --ldflags ${ARGN})
  set(${OUTPUT_VARIABLE} ${_result} PARENT_SCOPE)
endfunction()
