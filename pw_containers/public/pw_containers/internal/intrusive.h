// Copyright 2024 The Pigweed Authors
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

#include <type_traits>

namespace pw::containers::internal {

/// Crashes with a diagnostic message that intrusive containers must be empty
/// before destruction if the given `empty` parameter is not set.
///
/// This function is standalone to avoid using PW_CHECK in a header file.
void CheckIntrusiveContainerIsEmpty(bool empty);

/// Crashes with a diagnostic message that items must not be in an intrusive
/// container before insertion into an intrusive container or before destruction
/// if the given `uncontained` parameter is not set.
///
/// This function is standalone to avoid using PW_CHECK in a header file.
void CheckIntrusiveItemIsUncontained(bool uncontained);

// Gets the element type from an Item. This is used to check that an
// IntrusiveList element class inherits from Item, either directly or through
// another class.
template <typename Item, typename T, bool kIsItem = std::is_base_of<Item, T>()>
struct GetElementTypeFromItem {
  using Type = void;
};

template <typename Item, typename T>
struct GetElementTypeFromItem<Item, T, true> {
  using Type = typename T::ElementType;
};

template <typename Item, typename T>
using ElementTypeFromItem = typename GetElementTypeFromItem<Item, T>::Type;

}  // namespace pw::containers::internal
