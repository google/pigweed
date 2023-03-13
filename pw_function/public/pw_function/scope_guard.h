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

#include <utility>

namespace pw {

/// `ScopeGuard` ensures that the specified functor is executed no matter how
/// the current scope exits, unless it is dismissed.
///
/// Example:
///
/// @code
///   pw::Status SomeFunction() {
///     PW_TRY(OperationOne());
///     ScopeGuard undo_operation_one(UndoOperationOne);
///     PW_TRY(OperationTwo());
///     ScopeGuard undo_operation_two(UndoOperationTwo);
///     PW_TRY(OperationThree());
///     undo_operation_one.Dismiss();
///     undo_operation_two.Dismiss();
///     return pw::OkStatus();
///   }
/// @endcode
template <typename Functor>
class ScopeGuard {
 public:
  constexpr ScopeGuard(Functor&& functor)
      : functor_(std::forward<Functor>(functor)), dismissed_(false) {}

  constexpr ScopeGuard(ScopeGuard&& other) noexcept
      : functor_(std::move(other.functor_)), dismissed_(other.dismissed_) {
    other.dismissed_ = true;
  }

  template <typename OtherFunctor>
  constexpr ScopeGuard(ScopeGuard<OtherFunctor>&& other)
      : functor_(std::move(other.functor_)), dismissed_(other.dismissed_) {
    other.dismissed_ = true;
  }

  ~ScopeGuard() {
    if (dismissed_) {
      return;
    }
    functor_();
  }

  ScopeGuard& operator=(ScopeGuard&& other) noexcept {
    functor_ = std::move(other.functor_);
    dismissed_ = std::move(other.dismissed_);
    other.dismissed_ = true;
  }

  ScopeGuard() = delete;
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;

  /// Dismisses the `ScopeGuard`, meaning it will no longer execute the
  /// `Functor` when it goes out of scope.
  void Dismiss() { dismissed_ = true; }

 private:
  template <typename OtherFunctor>
  friend class ScopeGuard;

  Functor functor_;
  bool dismissed_;
};

// Enable type deduction for a compatible function pointer.
template <typename Function>
ScopeGuard(Function()) -> ScopeGuard<Function (*)()>;

}  // namespace pw
