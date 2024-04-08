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

#include "pw_async2/dispatcher.h"

namespace pw::async2 {

/// A ``Task`` that delegates to a type with a ``Pend`` method.
///
/// The wrapped type must have a ``Pend`` method which accepts a ``Context&``
/// and return a ``Poll<>``.
///
/// If ``Pendable`` is a pointer, ``PendableAsTask`` will dereference it and
/// attempt to invoke ``Pend``.
template <typename Pendable>
class PendableAsTask : public Task {
 public:
  /// Create a new ``Task`` which delegates ``Pend`` to ``pendable``.
  ///
  /// See class docs for more details.
  PendableAsTask(Pendable&& pendable)
      : pendable_(std::forward<Pendable>(pendable)) {}
  Pendable& operator*() { return pendable_; }
  Pendable* operator->() { return &pendable_; }

 private:
  Poll<> DoPend(Context& cx) final {
    if constexpr (std::is_pointer_v<Pendable>) {
      return pendable_->Pend(cx);
    } else {
      return pendable_.Pend(cx);
    }
  }
  Pendable pendable_;
};

}  // namespace pw::async2
