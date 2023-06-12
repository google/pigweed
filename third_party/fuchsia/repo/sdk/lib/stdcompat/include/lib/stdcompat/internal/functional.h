// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_INTERNAL_FUNCTIONAL_H_
#define LIB_STDCOMPAT_INTERNAL_FUNCTIONAL_H_

#include <cstddef>
#include <functional>

#include "../type_traits.h"
#include "../utility.h"

namespace cpp17 {
namespace internal {

// Definitions for the invoke functions in internal/type_traits.h.
// These collectively implement INVOKE from [func.require] ¶ 1.
template <typename MemFn, typename Class, typename T, typename... Args>
constexpr auto invoke(MemFn Class::*f, T&& obj, Args&&... args)
    -> std::enable_if_t<invoke_pmf_base<MemFn, Class, T>,
                        decltype((std::forward<T>(obj).*f)(std::forward<Args>(args)...))> {
  return (std::forward<T>(obj).*f)(std::forward<Args>(args)...);
}

template <typename MemFn, typename Class, typename T, typename... Args>
constexpr auto invoke(MemFn Class::*f, T&& obj, Args&&... args)
    -> std::enable_if_t<invoke_pmf_refwrap<MemFn, Class, T>,
                        decltype((obj.get().*f)(std::forward<Args>(args)...))> {
  return (obj.get().*f)(std::forward<Args>(args)...);
}

template <typename MemFn, typename Class, typename T, typename... Args>
constexpr auto invoke(MemFn Class::*f, T&& obj, Args&&... args)
    -> std::enable_if_t<invoke_pmf_other<MemFn, Class, T>,
                        decltype(((*std::forward<T>(obj)).*f)(std::forward<Args>(args)...))> {
  return (*std::forward<T>(obj).*f)(std::forward<Args>(args)...);
}

template <typename MemObj, typename Class, typename T>
constexpr auto invoke(MemObj Class::*f, T&& obj)
    -> std::enable_if_t<invoke_pmd_base<MemObj, Class, T>, decltype(std::forward<T>(obj).*f)> {
  return std::forward<T>(obj).*f;
}

template <typename MemObj, typename Class, typename T>
constexpr auto invoke(MemObj Class::*f, T&& obj)
    -> std::enable_if_t<invoke_pmd_refwrap<MemObj, Class, T>, decltype(obj.get().*f)> {
  return obj.get().*f;
}

template <typename MemObj, typename Class, typename T>
constexpr auto invoke(MemObj Class::*f, T&& obj)
    -> std::enable_if_t<invoke_pmd_other<MemObj, Class, T>, decltype((*std::forward<T>(obj)).*f)> {
  return (*std::forward<T>(obj)).*f;
}

template <typename F, typename... Args>
constexpr auto invoke(F&& f, Args&&... args)
    -> decltype(std::forward<F>(f)(std::forward<Args>(args)...)) {
  return std::forward<F>(f)(std::forward<Args>(args)...);
}

}  // namespace internal
}  // namespace cpp17

namespace cpp20 {
namespace internal {

template <typename Invocable, typename BoundTuple, std::size_t... Is, typename... CallArgs>
constexpr decltype(auto) invoke_with_bound(Invocable&& invocable, BoundTuple&& bound_args,
                                           std::index_sequence<Is...>, CallArgs&&... call_args) {
  return ::cpp17::internal::invoke(std::forward<Invocable>(invocable),
                                   std::get<Is>(std::forward<BoundTuple>(bound_args))...,
                                   std::forward<CallArgs>(call_args)...);
}

template <typename FD, typename... BoundArgs>
class front_binder {
  using bound_indices = std::index_sequence_for<BoundArgs...>;

 public:
  template <typename F, typename... Args>
  explicit constexpr front_binder(cpp17::in_place_t, F&& f, Args&&... args) noexcept(
      cpp17::conjunction_v<std::is_nothrow_constructible<FD, F>,
                           std::is_nothrow_constructible<BoundArgs, Args>...>)
      : fd_(std::forward<F>(f)), bound_args_(std::forward<Args>(args)...) {
    // [func.bind.front] ¶ 2
    static_assert(cpp17::is_constructible_v<FD, F>,
                  "Must be able to construct decayed callable type.");
    static_assert(cpp17::is_move_constructible_v<FD>, "Callable type must be move-constructible.");
    static_assert(cpp17::conjunction_v<std::is_constructible<BoundArgs, Args>...>,
                  "Must be able to construct decayed bound argument types.");
    static_assert(cpp17::conjunction_v<std::is_move_constructible<BoundArgs>...>,
                  "Bound argument types must be move-constructible.");
  }

  constexpr front_binder(const front_binder&) = default;
  constexpr front_binder& operator=(const front_binder&) = default;
  constexpr front_binder(front_binder&&) noexcept = default;
  constexpr front_binder& operator=(front_binder&&) noexcept = default;

  template <typename... CallArgs>
  constexpr cpp17::invoke_result_t<FD&, BoundArgs&..., CallArgs&&...>
  operator()(CallArgs&&... call_args) & noexcept(
      cpp17::is_nothrow_invocable_v<FD&, BoundArgs&..., CallArgs&&...>) {
    return invoke_with_bound(fd_, bound_args_, bound_indices(),
                             std::forward<CallArgs>(call_args)...);
  }

  template <typename... CallArgs>
  constexpr cpp17::invoke_result_t<const FD&, const BoundArgs&..., CallArgs&&...>
  operator()(CallArgs&&... call_args) const& noexcept(
      cpp17::is_nothrow_invocable_v<const FD&, const BoundArgs&..., CallArgs&&...>) {
    return invoke_with_bound(fd_, bound_args_, bound_indices(),
                             std::forward<CallArgs>(call_args)...);
  }

  template <typename... CallArgs>
  constexpr cpp17::invoke_result_t<FD&&, BoundArgs&&..., CallArgs&&...>
  operator()(CallArgs&&... call_args) && noexcept(
      cpp17::is_nothrow_invocable_v<FD&&, BoundArgs&&..., CallArgs&&...>) {
    return invoke_with_bound(std::move(fd_), std::move(bound_args_), bound_indices(),
                             std::forward<CallArgs>(call_args)...);
  }

  template <typename... CallArgs>
  constexpr cpp17::invoke_result_t<const FD&&, const BoundArgs&&..., CallArgs&&...>
  operator()(CallArgs&&... call_args) const&& noexcept(
      cpp17::is_nothrow_invocable_v<const FD&&, const BoundArgs&&..., CallArgs&&...>) {
    return invoke_with_bound(std::move(fd_), std::move(bound_args_), bound_indices(),
                             std::forward<CallArgs>(call_args)...);
  }

 private:
  FD fd_;
  std::tuple<BoundArgs...> bound_args_;
};

template <typename F, typename... BoundArgs>
using front_binder_t = front_binder<std::decay_t<F>, std::decay_t<BoundArgs>...>;

}  // namespace internal
}  // namespace cpp20

#endif  // LIB_STDCOMPAT_INTERNAL_FUNCTIONAL_H_
