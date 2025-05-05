// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <iterator>
#include <type_traits>

namespace pw::containers::internal {

template <typename Iterator>
using EnableIfInputIterator = std::enable_if_t<std::is_convertible_v<
    typename std::iterator_traits<Iterator>::iterator_category,
    std::input_iterator_tag>>;

template <typename Iterator>
using EnableIfForwardIterator = std::enable_if_t<std::is_convertible_v<
    typename std::iterator_traits<Iterator>::iterator_category,
    std::forward_iterator_tag>>;

}  // namespace pw::containers::internal
