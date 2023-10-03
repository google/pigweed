// Copyright 2023 The Pigweed Authors
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

#include "pw_polyfill/standard_library/namespace.h"

_PW_POLYFILL_BEGIN_NAMESPACE_STD

template <typename T>
T* addressof(T& arg) noexcept {
  return reinterpret_cast<T*>(
      const_cast<char*>(reinterpret_cast<const volatile char*>(&arg)));
}

template <typename T>
T* addressof(T&& arg) = delete;

template <typename T>
struct pointer_traits;

template <typename T>
struct pointer_traits<T*> {
  using pointer = T*;
  using element_type = T;
  using difference_type = decltype((const char*)1 - (const char*)1);
};

_PW_POLYFILL_END_NAMESPACE_STD
