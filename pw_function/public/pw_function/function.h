// Copyright 2021 The Pigweed Authors
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

#include "lib/fit/function.h"
#include "pw_function/config.h"

namespace pw {

/// `pw::Function` is a wrapper for an arbitrary callable object. It can be used
/// by callback-based APIs to allow callers to provide any type of callable.
///
/// Example:
/// @code{.cpp}
///
///   template <typename T>
///   bool All(const pw::Vector<T>& items,
///            const pw::Function<bool(const T& item)>& predicate) {
///     for (const T& item : items) {
///       if (!predicate(item)) {
///         return false;
///       }
///     }
///     return true;
///   }
///
///   bool ElementsArePositive(const pw::Vector<int>& items) {
///     return All(items, [](const int& i) { return i > 0; });
///   }
///
///   bool IsEven(const int& i) { return i % 2 == 0; }
///
///   bool ElementsAreEven(const pw::Vector<int>& items) {
///     return All(items, IsEven);
///   }
///
/// @endcode
///
/// @tparam Allocator The Allocator used to dynamically allocate the callable,
/// if it exceeds `inline_target_size` and dynamic allocation is enabled. Its
/// `value_type` is irrelevant, since it must support rebinding.
template <typename FunctionType,
          std::size_t inline_target_size =
              function_internal::config::kInlineCallableSize,
          typename Allocator = PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE>
using Function = fit::function_impl<
    inline_target_size,
    /*require_inline=*/!function_internal::config::kEnableDynamicAllocation,
    FunctionType,
    Allocator>;

/// Version of `pw::Function` that exclusively uses inline storage.
///
/// IMPORTANT: If `pw::Function` is configured to allow dynamic allocations then
/// any attempt to convert `pw::InlineFunction` to `pw::Function` will ALWAYS
/// allocate.
///
// TODO: b/252852651 - Remove warning above when conversion from
// `fit::inline_function` to `fit::function` doesn't allocate anymore.
template <typename FunctionType,
          std::size_t inline_target_size =
              function_internal::config::kInlineCallableSize>
using InlineFunction = fit::inline_function<FunctionType, inline_target_size>;

using Closure = Function<void()>;

/// `pw::Callback` is identical to @cpp_type{pw::Function} except:
///
/// 1. On the first call to invoke a `pw::Callback`, the target function held
///    by the `pw::Callback` cannot be called again.
/// 2. When a `pw::Callback` is invoked for the first time, the target function
///    is released and destructed, along with any resources owned by that
///    function (typically the objects captured by a lambda).
///
/// A `pw::Callback` in the "already called" state has the same state as a
/// `pw::Callback` that has been assigned to `nullptr`.
template <typename FunctionType,
          std::size_t inline_target_size =
              function_internal::config::kInlineCallableSize,
          typename Allocator = PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE>
using Callback = fit::callback_impl<
    inline_target_size,
    /*require_inline=*/!function_internal::config::kEnableDynamicAllocation,
    FunctionType,
    Allocator>;

/// Version of `pw::Callback` that exclusively uses inline storage.
template <typename FunctionType,
          std::size_t inline_target_size =
              function_internal::config::kInlineCallableSize>
using InlineCallback = fit::inline_callback<FunctionType, inline_target_size>;

/// Returns a `Callable` which, when called, invokes `method` on `instance`
/// using the arguments provided.
///
/// This is useful for binding the `this` argument of a callable.
///
/// `pw::bind_member<&T::MethodName>(instance)` is roughly equivalent to
/// `[instance](Arg arg1, ...) { instance->MethodName(arg1, ...) }`, albeit with
/// proper support for overloads and argument forwarding.
template <auto method, typename T>
auto bind_member(T* instance) {
  return fit::bind_member<method, T>(instance);
}

}  // namespace pw
