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

# Module configuration

pw_add_module_config(pw_containers_CONFIG)

pw_add_library(pw_containers.config INTERFACE
  HEADERS
    public/pw_containers/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_containers_CONFIG}
)

# Libraries

pw_add_library(pw_containers INTERFACE
  PUBLIC_DEPS
    pw_containers.algorithm
    pw_containers.flat_map
    pw_containers.inline_deque
    pw_containers.inline_queue
    pw_containers.intrusive_list
    pw_containers.vector
)

pw_add_library(pw_containers.algorithm INTERFACE
  HEADERS
    public/pw_containers/algorithm.h
    public/pw_containers/internal/algorithm_internal.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers.filtered_view INTERFACE
  HEADERS
    public/pw_containers/filtered_view.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_preprocessor
)

pw_add_library(pw_containers.flat_map INTERFACE
  HEADERS
    public/pw_containers/flat_map.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.assert
)

pw_add_library(pw_containers.inline_deque INTERFACE
  HEADERS
    public/pw_containers/inline_deque.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.assert
    pw_containers._constexpr_tag
    pw_containers._raw_storage
    pw_preprocessor
    pw_span
)

pw_add_library(pw_containers.inline_queue INTERFACE
  HEADERS
    public/pw_containers/inline_queue.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.inline_deque
)

pw_add_library(pw_containers.iterator INTERFACE
  HEADERS
    public/pw_containers/iterator.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers._raw_storage INTERFACE
  HEADERS
    public/pw_containers/internal/raw_storage.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers._constexpr_tag INTERFACE
  HEADERS
    public/pw_containers/internal/constexpr_tag.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers._test_helpers STATIC
  HEADERS
    pw_containers_private/test_helpers.h
  SOURCES
    test_helpers.cc
  PUBLIC_INCLUDES
    .
  PRIVATE_DEPS
    pw_assert
)

pw_add_library(pw_containers.to_array INTERFACE
  HEADERS
    public/pw_containers/to_array.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers.inline_var_len_entry_queue STATIC
  HEADERS
    public/pw_containers/inline_var_len_entry_queue.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers._constexpr_tag
    pw_containers._raw_storage
    pw_varint
  PRIVATE_DEPS
    pw_assert
  SOURCES
    inline_var_len_entry_queue.c
)

pw_add_library(pw_containers.vector INTERFACE
  HEADERS
    public/pw_containers/vector.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_preprocessor
)

pw_add_library(pw_containers.wrapped_iterator INTERFACE
  HEADERS
    public/pw_containers/wrapped_iterator.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_containers.intrusive_item STATIC
  HEADERS
    public/pw_containers/internal/intrusive_item.h
  PUBLIC_INCLUDES
    public
  SOURCES
    intrusive_item.cc
  PRIVATE_DEPS
    pw_assert
)

pw_add_library(pw_containers.intrusive_list_common INTERFACE
  HEADERS
    public/pw_containers/internal/intrusive_list.h
    public/pw_containers/internal/intrusive_list_item.h
    public/pw_containers/internal/intrusive_list_iterator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.intrusive_item
)

pw_add_library(pw_containers.intrusive_forward_list INTERFACE
  HEADERS
    public/pw_containers/intrusive_forward_list.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.config
    pw_containers.intrusive_list_common
)

pw_add_library(pw_containers.intrusive_list INTERFACE
  HEADERS
    public/pw_containers/intrusive_list.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.config
    pw_containers.intrusive_list_common
    pw_containers.legacy_intrusive_list
)

pw_add_library(pw_containers.legacy_intrusive_list INTERFACE
  HEADERS
    public/pw_containers/internal/legacy_intrusive_list.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.intrusive_forward_list
)

pw_add_library(pw_containers.aa_tree STATIC
  HEADERS
    public/pw_containers/internal/aa_tree.h
    public/pw_containers/internal/aa_tree_item.h
    public/pw_containers/internal/aa_tree_iterator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes.packed_ptr
    pw_containers.intrusive_item
    pw_function
  SOURCES
    aa_tree.cc
    aa_tree_item.cc
)

pw_add_library(pw_containers.intrusive_map INTERFACE
  HEADERS
    public/pw_containers/intrusive_map.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.aa_tree
)

pw_add_library(pw_containers.intrusive_multimap INTERFACE
  HEADERS
    public/pw_containers/intrusive_multimap.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.aa_tree
)

pw_add_library(pw_containers.intrusive_multiset INTERFACE
  HEADERS
    public/pw_containers/intrusive_multiset.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.aa_tree
)

pw_add_library(pw_containers.intrusive_set INTERFACE
  HEADERS
    public/pw_containers/intrusive_set.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.aa_tree
)

pw_add_test(pw_containers.algorithm_test
  SOURCES
    algorithm_test.cc
  PRIVATE_DEPS
    pw_containers.algorithm
    pw_containers.flat_map
    pw_containers.intrusive_list
    pw_containers.vector
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.filtered_view_test
  SOURCES
    filtered_view_test.cc
  PRIVATE_DEPS
    pw_containers.algorithm
    pw_containers.filtered_view
    pw_containers.flat_map
    pw_containers.intrusive_list
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.flat_map_test
  SOURCES
    flat_map_test.cc
  PRIVATE_DEPS
    pw_containers.flat_map
    pw_polyfill
  GROUPS
    modules
    pw_containers
    pw_polyfill
)

pw_add_test(pw_containers.inline_deque_test
  SOURCES
    inline_deque_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.algorithm
    pw_containers.inline_deque
    pw_containers._test_helpers
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.inline_queue_test
  SOURCES
    inline_queue_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.algorithm
    pw_containers.inline_queue
    pw_containers._test_helpers
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.raw_storage_test
  SOURCES
    raw_storage_test.cc
  PRIVATE_DEPS
    pw_containers._raw_storage
    pw_containers._test_helpers
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.to_array_test
  SOURCES
    to_array_test.cc
  PRIVATE_DEPS
    pw_containers.to_array
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.inline_var_len_entry_queue_test
  SOURCES
    pw_containers_private/inline_var_len_entry_queue_test_oracle.h
    inline_var_len_entry_queue_test.cc
  PRIVATE_DEPS
    pw_assert
    pw_bytes
    pw_containers.inline_var_len_entry_queue
)

pw_add_test(pw_containers.vector_test
  SOURCES
    vector_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers._test_helpers
    pw_containers.vector
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.wrapped_iterator_test
  SOURCES
    wrapped_iterator_test.cc
  PRIVATE_DEPS
    pw_containers.wrapped_iterator
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_forward_list_test
  SOURCES
    intrusive_forward_list_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_forward_list
    pw_containers.vector
    pw_preprocessor
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_list_test
  SOURCES
    intrusive_list_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_list
    pw_containers.vector
    pw_preprocessor
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_map_test
  SOURCES
    intrusive_map_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_map
    pw_containers.intrusive_multimap
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_multimap_test
  SOURCES
    intrusive_multimap_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_map
    pw_containers.intrusive_multimap
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_set_test
  SOURCES
    intrusive_set_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_set
    pw_containers.intrusive_multiset
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_multiset_test
  SOURCES
    intrusive_multiset_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_set
    pw_containers.intrusive_multiset
    pw_span
  GROUPS
    modules
    pw_containers
)

pw_add_test(pw_containers.intrusive_item_test
  SOURCES
    intrusive_item_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_containers.intrusive_forward_list
    pw_containers.intrusive_list
    pw_containers.intrusive_map
    pw_containers.intrusive_multimap
    pw_containers.intrusive_set
    pw_containers.intrusive_multiset
  GROUPS
    modules
    pw_containers
)

add_subdirectory(examples)
