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

#include <tuple>
#include <type_traits>

#include "pw_async2/context.h"
#include "pw_async2/poll.h"

namespace pw::async2 {
namespace internal {

/// A pendable function is any function that takes an async context and returns
/// a poll.
template <typename T, typename... Args>
using PendableFunction = Poll<T>(Context&, Args...);

template <typename Func>
struct PendableTraits {
  using Function = Func;
  static constexpr bool kIsPendable = false;
};

template <typename T, typename... Args>
struct PendableTraits<PendableFunction<T, Args...>*> {
  using Output = T;
  using Arguments = std::tuple<Args...>;
  static constexpr bool kIsPendable = true;
};

template <typename T, typename C, typename... Args>
struct PendableTraits<PendableFunction<T, Args...>(C::*)> {
  using Class = C;
  using Output = T;
  using Arguments = std::tuple<Args...>;
  static constexpr bool kIsPendable = true;
};

template <auto Func>
inline constexpr bool IsPendable = PendableTraits<decltype(Func)>::kIsPendable;

}  // namespace internal

/// @submodule{pw_async2,adapters}

/// Wraps a pendable member function in an object which has a single ``Pend``
/// method, allowing it to be used more broadly.
template <auto Func>
class MemberPendableWrapper {
 public:
  using FuncType = decltype(Func);
  using Traits = internal::PendableTraits<FuncType>;
  using Class = typename Traits::Class;
  using Output = typename Traits::Output;
  using Arguments = typename Traits::Arguments;

  MemberPendableWrapper(const MemberPendableWrapper&) = delete;
  MemberPendableWrapper& operator=(const MemberPendableWrapper&) = delete;

  MemberPendableWrapper(MemberPendableWrapper&& other)
      : object_(other.object_), args_(std::move(other.args_)) {
    other.object_ = nullptr;
  }

  MemberPendableWrapper& operator=(MemberPendableWrapper&& other) {
    object_ = other.object_;
    args_ = std::move(other.args_);
    other.object_ = nullptr;
    return *this;
  }

  ~MemberPendableWrapper() = default;

  Poll<Output> Pend(Context& cx) {
    PW_ASSERT(object_ != nullptr);
    auto func = [&](auto&&... args) -> Poll<Output> {
      return std::invoke(
          Func, object_, cx, std::forward<decltype(args)>(args)...);
    };
    Poll<Output> result = std::apply(func, args_);
    if (result.IsReady()) {
      object_ = nullptr;
    }
    return result;
  }

  constexpr bool completed() const { return object_ == nullptr; }

 private:
  template <auto MemberFuncParam,
            typename TraitsParam,
            typename EnableIfParam,
            typename... ArgsParam>
  friend constexpr MemberPendableWrapper<MemberFuncParam> PendableFor(
      typename TraitsParam::Class& obj, ArgsParam&&... args);

  template <
      typename... Args,
      typename = std::enable_if_t<std::is_member_function_pointer_v<FuncType> &&
                                  std::is_constructible_v<Arguments, Args...>>>
  constexpr MemberPendableWrapper(Class& obj, Args&&... args)
      : object_(&obj), args_(std::forward<Args>(args)...) {
    static_assert(std::conjunction_v<std::is_copy_constructible<Args>...>,
                  "Arguments stored in a PendableWrapper must be copyable");
  }
  Class* object_;
  Arguments args_;
};

/// Wraps a pendable free function in an object which has a single ``Pend``
/// method, allowing it to be used more broadly.
template <auto Func>
class FreePendableWrapper {
 public:
  using FuncType = decltype(Func);
  using Traits = internal::PendableTraits<FuncType>;
  using Output = typename Traits::Output;
  using Arguments = typename Traits::Arguments;

  FreePendableWrapper(const FreePendableWrapper&) = delete;
  FreePendableWrapper& operator=(const FreePendableWrapper&) = delete;

  FreePendableWrapper(FreePendableWrapper&&) = default;
  FreePendableWrapper& operator=(FreePendableWrapper&&) = default;

  ~FreePendableWrapper() = default;

  Poll<Output> Pend(Context& cx) {
    auto func = [&](auto&&... args) -> Poll<Output> {
      return std::invoke(Func, cx, std::forward<decltype(args)>(args)...);
    };
    Poll<Output> result = std::apply(func, args_);
    completed_ = result.IsReady();
    return result;
  }

  constexpr bool completed() const { return completed_; }

 private:
  template <auto FreeFuncParam,
            typename TraitsParam,
            typename EnableIfParam,
            typename... ArgsParam>
  friend constexpr FreePendableWrapper<FreeFuncParam> PendableFor(
      ArgsParam&&... args);

  template <typename... Args,
            typename =
                std::enable_if_t<!std::is_member_function_pointer_v<FuncType> &&
                                 std::is_constructible_v<Arguments, Args...>>>
  constexpr FreePendableWrapper(Args&&... args)
      : args_(std::forward<Args>(args)...), completed_(false) {
    static_assert(std::conjunction_v<std::is_copy_constructible<Args>...>,
                  "Arguments stored in a PendableWrapper must be copyable");
  }

  Arguments args_;
  bool completed_;
};

/// Wraps a pendable member function in an object which has a single ``Pend``
/// method, allowing it to be used more broadly.
///
/// The wrapper stores the arguments used to invoke the target function. As a
/// result, the argument types must be copyable.
///
/// The wrapping pendable object can only be used a single time. After its
/// ``Pend`` method returns ``Ready``, it will crash if called again.
template <auto MemberFunc,
          typename Traits = internal::PendableTraits<decltype(MemberFunc)>,
          typename = std::enable_if_t<
              Traits::kIsPendable &&
              std::is_member_function_pointer_v<decltype(MemberFunc)>>,
          typename... Args>
inline constexpr MemberPendableWrapper<MemberFunc> PendableFor(
    typename Traits::Class& obj, Args&&... args) {
  return MemberPendableWrapper<MemberFunc>(obj, std::forward<Args>(args)...);
}

/// Wraps a pendable free function in an object which has a single ``Pend``
/// method, allowing it to be used more broadly.
///
/// The wrapper stores the arguments used to invoke the target function. As a
/// result, the argument types must be copyable.
///
/// The wrapping pendable object can only be used a single time. After its
/// ``Pend`` method returns ``Ready``, it will crash if called again.
template <auto FreeFunc,
          typename Traits = internal::PendableTraits<decltype(FreeFunc)>,
          typename = std::enable_if_t<
              Traits::kIsPendable &&
              !std::is_member_function_pointer_v<decltype(FreeFunc)>>,
          typename... Args>
inline constexpr FreePendableWrapper<FreeFunc> PendableFor(Args&&... args) {
  return FreePendableWrapper<FreeFunc>(std::forward<Args>(args)...);
}

/// @}

}  // namespace pw::async2
