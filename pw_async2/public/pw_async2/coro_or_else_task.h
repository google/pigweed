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

#include "pw_async2/coro.h"
#include "pw_async2/dispatcher.h"
#include "pw_function/function.h"

namespace pw::async2 {

/// A ``Task`` that delegates to a provided ``Coro<Status>>`` and executes
/// an ``or_else`` handler function on failure.
class CoroOrElseTask : public Task {
 public:
  /// Create a new ``Task`` which runs ``coro``, invoking ``or_else`` on
  /// any non-OK status.
  CoroOrElseTask(Coro<Status>&& coro, pw::Function<void(Status)>&& or_else)
      : coro_(std::move(coro)), or_else_(std::move(or_else)) {}

  /// *Non-atomically* sets `coro`.
  ///
  /// The task must not be `Post`ed when `coro` is changed.
  void SetCoro(Coro<Status>&& coro) {
    PW_ASSERT(!IsRegistered());
    coro_ = std::move(coro);
  }

  /// *Non-atomically* sets `or_else`.
  ///
  /// The task must not be `Post`ed when `or_else` is changed.
  void SetErrorHandler(pw::Function<void(Status)>&& or_else) {
    PW_ASSERT(!IsRegistered());
    or_else_ = std::move(or_else);
  }

 private:
  Poll<> DoPend(Context& cx) final {
    Poll<Status> result = coro_.Pend(cx);
    if (result.IsPending()) {
      return Pending();
    }
    if (!result->ok()) {
      or_else_(*result);
    }
    return Ready();
  }

  Coro<Status> coro_;
  pw::Function<void(Status)> or_else_;
};

}  // namespace pw::async2
