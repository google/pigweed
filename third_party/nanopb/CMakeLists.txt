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

set(dir_pw_third_party_nanopb "" CACHE PATH "Path to the Nanopb installation.")
option(pw_third_party_nanopb_ADD_SUBDIRECTORY
    "Whether to add the dir_pw_third_party_nanopb subdirectory" OFF)

if("${dir_pw_third_party_nanopb}" STREQUAL "")
  return()
elseif(pw_third_party_nanopb_ADD_SUBDIRECTORY)
  add_subdirectory("${dir_pw_third_party_nanopb}" third_party/nanopb)
endif()

set(nanopb_main_library
    $<IF:$<TARGET_EXISTS:protobuf-nanopb-static>,protobuf-nanopb-static,protobuf-nanopb>)

add_library(pw_third_party.nanopb INTERFACE)
target_link_libraries(pw_third_party.nanopb INTERFACE "${nanopb_main_library}")
target_include_directories(pw_third_party.nanopb
  INTERFACE
    "${dir_pw_third_party_nanopb}"
)
