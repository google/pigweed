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

#include <cstddef>

namespace pw {

/// Returns a ``std::byte`` when used as a ``_b`` suffix.
///
/// This is useful for writing byte literals, particularly in tests.
/// To use, add ``using ::pw::operator""_b;`` and then use like ``5_b``
/// in order to create a ``std::byte`` with the contents ``5``.
///
/// This should not be used in header files, as it requires a ``using``
/// declaration that will be publicly exported at whatever level it is
/// used.
constexpr std::byte operator"" _b(unsigned long long value) {
  return std::byte(value);
}

}  // namespace pw
