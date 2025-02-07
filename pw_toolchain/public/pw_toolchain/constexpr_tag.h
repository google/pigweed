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

namespace pw {

/// Tag type used to differentiate between `constexpr` and non-`constexpr`
/// constructors. Do NOT use this feature for new classes! It should only be
/// used to add a `constexpr` constructor to an existing class in limited
/// circumstances.
///
/// Specifically, some compilers are more likely to constant initialize global
/// variables that have `constexpr` constructors. For large non-zero objects,
/// this can increase binary size compared to runtime initialization. Non-zero
/// constant initialized globals are typically placed in `.data` or `.rodata`
/// instead of `.bss`.
///
/// Adding `constexpr` to a constructor may affect existing users if their
/// compiler constant initializes globals that were runtime initialized
/// previously. To maintain previous behavior, add a new `constexpr` constructor
/// with `ConstexprTag` instead of changing the existing constructor.
///
/// Prefer using `pw::kConstexpr` to select a `constexpr`-tagged constructor,
/// rather than initializing a `pw::ConstexprTag`.
///
/// @warning Do NOT rely on whether a constructor is `constexpr` or not to
/// control whether global variables are constant initialized. To control
/// constant initialization, explicitly annotate global variables as `constinit`
/// / `PW_CONSTINIT`, `constexpr`, or `pw::RuntimeInitGlobal`. Compilers can
/// constant initialize globals that:
/// - are not declared `constinit` or `constexpr`,
/// - do not have a `constexpr` constructor,
/// - or perform non-`constexpr` actions during construction, such as calling
///   non-`constexpr` functions or placement new.
struct ConstexprTag {
  explicit constexpr ConstexprTag() = default;
};

/// Value used to select a `constexpr` constructor tagged with
/// `pw::ConstexprTag`.
inline constexpr ConstexprTag kConstexpr{};

}  // namespace pw
