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

#include <type_traits>
#include <utility>

#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_function/function.h"

namespace pw::async2 {

/// A co-awaitable object that delegates to a provided ``func``.
///
/// Thie provided ``func`` may be any callable (function, lambda, or similar)
/// which accepts a ``Context&`` and returns a ``Poll<T>``.
template <typename T, typename Func = Function<Poll<T>(Context&)>>
class PendFuncAwaitable {
 public:
  using CallableType = Func;

  /// Create a new object` which delegates ``Pend`` to ``pendable``.
  ///
  /// See class docs for more details.
  constexpr PendFuncAwaitable(Func&& func) : func_(std::forward<Func>(func)) {}

  /// Delegates to the stored ``func`` to get a result ``T`` or wait.
  ///
  /// This method will be called if the object is used by ``co_await``.
  Poll<T> Pend(pw::async2::Context& cx) { return func_(cx); }

 private:
  Func func_;
};

// Template deduction guide to allow inferring T from a Callable's return type.
// This also ensures that when constructing a PendFuncAwaitable from a callable,
// the template Func parameter is set based on the Callable type, which helps
// optimize the amount of storage needed.
template <typename Callable>
PendFuncAwaitable(Callable) -> PendFuncAwaitable<
    typename std::invoke_result<Callable, Context&>::type::OutputType,
    typename std::remove_reference<Callable>::type>;

}  // namespace pw::async2
