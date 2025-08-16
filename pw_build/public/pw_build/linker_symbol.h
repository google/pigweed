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

#include <cstdint>
#include <type_traits>

#include "lib/stdcompat/bit.h"

namespace pw {

/// @module{pw_build}

/// Represents a symbol provided by the linker, i.e. via a linker script.
///
/// LinkerSymbol objects are used with linker-provided symbols that don't have
/// storage (which is common), and only provide a value e.g.
///
/// @code
///   MY_LINKER_VARIABLE = 42
/// @endcode
///
/// LinkerSymbol objects are not actual variables (do not have storage) and thus
/// cannot be created; they can only be used with an `extern "C"` declaration.
/// The purpose is to communicate *values* from the linker script to C++ code.
///
/// Example:
///
/// @code{.cpp}
///   #include "pw_build/linker_symbol.h"
///
///   extern "C" pw::LinkerSymbol<uint32_t> MY_LINKER_VARIABLE;
///
///   uint32_t GetMyLinkerVariable() {
///     return MY_LINKER_VARIABLE.value();
///   }
/// @endcode
///
/// @tparam T   The type of the value communicated by the linker, defaulting to
///             `uintptr_t`. Must be an integral or enum type, no larger than
///             `uintptr_t`.
template <class T = uintptr_t>
class LinkerSymbol {
 public:
  LinkerSymbol() = delete;
  ~LinkerSymbol() = delete;
  LinkerSymbol(const LinkerSymbol&) = delete;
  LinkerSymbol(const LinkerSymbol&&) = delete;
  LinkerSymbol& operator=(const LinkerSymbol&) = delete;
  LinkerSymbol& operator=(const LinkerSymbol&&) = delete;

  /// Gets the value of this linker symbol, converted to the specified type.
  T value() const {
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
    static_assert(sizeof(T) <= sizeof(uintptr_t));
    return static_cast<T>(raw_value());
  }

 private:
  /// Gets the raw value of this linker symbol.
  uintptr_t raw_value() const { return cpp20::bit_cast<uintptr_t>(this); }
};

}  // namespace pw
