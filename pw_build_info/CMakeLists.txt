# Copyright 2022 The Pigweed Authors
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

# GNU build IDs aren't supported by Windows and macOS.
if((NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows") AND
   (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin"))
  pw_add_module_library(pw_build_info.build_id
    HEADERS
      public/pw_build_info/build_id.h
    PUBLIC_INCLUDES
      public
    PUBLIC_DEPS
      pw_polyfill.cstddef
      pw_polyfill.span
    PUBLIC_LINK_OPTIONS
      -Wl,--build-id=sha1
    SOURCES
      build_id.cc
    PRIVATE_DEPS
      pw_preprocessor
    PRIVATE_COMPILE_OPTIONS
      -Wno-array-bounds
      -Wno-stringop-overflow
  )
endif()

# Automatically add the gnu build ID linker sections when building for Linux.
# macOS and Windows executables are not supported, and embedded targets must
# manually add the snippet to their linker script in a read-only section.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  target_link_options(pw_build_info.build_id
    PUBLIC
      "-T${CMAKE_CURRENT_SOURCE_DIR}/add_build_id_to_default_script.ld"
      "-L${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()
