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

include("$ENV{PW_ROOT}/pw_build/pigweed.cmake")

# Examples

pw_add_test(pw_containers.examples.flat_map
  PRIVATE_DEPS
    pw_containers.flat_map
  SOURCES
    flat_map.cc
)

pw_add_test(pw_containers.examples.intrusive_forward_list
  PRIVATE_DEPS
    pw_containers.intrusive_forward_list
  SOURCES
    intrusive_forward_list.cc
)

pw_add_test(pw_containers.examples.intrusive_list
  PRIVATE_DEPS
    pw_containers.intrusive_list
  SOURCES
    intrusive_list.cc
)

pw_add_test(pw_containers.examples.intrusive_map
  PRIVATE_DEPS
    pw_containers.intrusive_map
  SOURCES
    intrusive_map.cc
)

pw_add_test(pw_containers.examples.intrusive_multimap
  PRIVATE_DEPS
    pw_containers.intrusive_multimap
  SOURCES
    intrusive_multimap.cc
)

pw_add_test(pw_containers.examples.multiple_containers
  PRIVATE_DEPS
    pw_containers.intrusive_list
    pw_containers.intrusive_map
    pw_result
    pw_status
  SOURCES
    multiple_containers.cc
)

pw_add_test(pw_containers.examples.vector
  PRIVATE_DEPS
    pw_function
    pw_status
    pw_containers.vector
  SOURCES
    vector.cc
)

pw_add_test(pw_containers.examples.wrapped_iterator
  PRIVATE_DEPS
    pw_containers.wrapped_iterator
  SOURCES
    wrapped_iterator.cc
)
