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

#include <functional>
#include <type_traits>

#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_function/function.h"

namespace pw::async2 {
namespace internal {

template <typename T = ReadyType, bool kReschedule = false>
class CallbackTask : public Task {
 public:
  using ValueType = T;
  using Pendable = Function<Poll<T>(Context&)>;

  using Callback = std::conditional_t<std::is_same_v<T, ReadyType>,
                                      Function<void()>,
                                      Function<void(T)>>;

  CallbackTask(Callback&& callback, Pendable&& pendable)
      : callback_(std::move(callback)), pendable_(std::move(pendable)) {}

 private:
  Poll<> DoPend(Context& cx) final {
    Poll<T> poll = pendable_(cx);
    if (poll.IsPending()) {
      return Pending();
    }

    if constexpr (std::is_same_v<T, ReadyType>) {
      callback_();
    } else {
      callback_(std::move(*poll));
    }

    if constexpr (kReschedule) {
      cx.ReEnqueue();
      return Pending();
    } else {
      return Ready();
    }
  }

  Callback callback_;
  Pendable pendable_;
};

}  // namespace internal

/// @submodule{pw_async2,adapters}

/// A ``Task`` which pends a pendable function and invokes a provided callback
/// with its output when it returns ``Ready``.
///
/// A ``OneshotCallbackTask`` terminates after the underlying pendable returns
/// ``Ready`` for the first time. Following this, the pendable will no longer be
/// polled, and the callback function will not be invoked again.
template <typename T = ReadyType>
using OneshotCallbackTask = internal::CallbackTask<T, false>;

/// A ``Task`` which pends a pendable function and invokes a provided callback
/// with its output when it returns ``Ready``.
///
/// A ``RecurringCallbackTask`` never completes; it reschedules itself after
/// each ``Ready`` returned by the underlying pendable. This makes it suitable
/// to interface with pendables which continually return values, such as a data
/// stream.
template <typename T = ReadyType>
using RecurringCallbackTask = internal::CallbackTask<T, true>;

template <auto kFunc,
          typename Callback,
          typename T = typename UnwrapPoll<
              std::invoke_result_t<decltype(kFunc), Context&>>::Type>
OneshotCallbackTask<T> OneshotCallbackTaskFor(Callback&& callback) {
  return OneshotCallbackTask<T>(std::forward<Callback>(callback),
                                std::forward<decltype(kFunc)>(kFunc));
}

template <
    auto kMemberFunc,
    typename Class,
    typename Callback,
    typename T = typename UnwrapPoll<
        std::invoke_result_t<decltype(kMemberFunc), Class&, Context&>>::Type>
OneshotCallbackTask<T> OneshotCallbackTaskFor(Class& obj, Callback&& callback) {
  return OneshotCallbackTask<T>(
      std::forward<Callback>(callback),
      [&obj](Context& cx) { return std::invoke(kMemberFunc, &obj, cx); });
}

template <auto kFunc,
          typename Callback,
          typename T = typename UnwrapPoll<
              std::invoke_result_t<decltype(kFunc), Context&>>::Type>
RecurringCallbackTask<T> RecurringCallbackTaskFor(Callback&& callback) {
  return RecurringCallbackTask<T>(std::forward<Callback>(callback),
                                  std::forward<decltype(kFunc)>(kFunc));
}

template <
    auto kMemberFunc,
    typename Class,
    typename Callback,
    typename T = typename UnwrapPoll<
        std::invoke_result_t<decltype(kMemberFunc), Class&, Context&>>::Type>
RecurringCallbackTask<T> RecurringCallbackTaskFor(Class& obj,
                                                  Callback&& callback) {
  return RecurringCallbackTask<T>(
      std::forward<Callback>(callback),
      [&obj](Context& cx) { return std::invoke(kMemberFunc, &obj, cx); });
}

/// @}

}  // namespace pw::async2
