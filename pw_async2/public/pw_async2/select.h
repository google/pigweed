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

#include <algorithm>
#include <tuple>
#include <utility>
#include <variant>

#include "pw_async2/dispatcher.h"

namespace pw::async2 {

/// @module{pw_async2}

/// @ingroup pw_async2_utilities
/// @{

template <typename... Pendables>
class Selector;

/// The poll result of a ``Pendable`` within a ``Selector``.
/// This type stores both the index of the pendable, and its returned value on
/// completion, making it possible to identify which pendable completed.
template <size_t I, typename T>
struct SelectResult {
  static constexpr size_t kIndex = I;
  T value;

 private:
  template <typename... Pendables>
  friend class Selector;

  explicit SelectResult(T&& val) : value(std::move(val)) {}
};

/// Indicates that every pendable within a ``Selector`` has completed.
struct AllPendablesCompleted {
  static constexpr size_t kIndex = std::numeric_limits<size_t>::max();
  std::nullopt_t value = std::nullopt;
};

/// @}

/// @}

namespace internal {

template <typename Is, typename... Pendables>
struct SelectVariantType;

template <size_t... Is, typename... Pendables>
struct SelectVariantType<std::index_sequence<Is...>, Pendables...> {
  using type = std::variant<SelectResult<Is, PendOutputOf<Pendables>>...,
                            AllPendablesCompleted>;
};

template <typename ResultVariant,
          typename AllPendablesCompletedHandler,
          typename ReadyHandlers,
          std::size_t... Is>
void VisitSelectResult(ResultVariant&& variant,
                       AllPendablesCompletedHandler&& all_completed,
                       ReadyHandlers&& ready_handlers,
                       std::index_sequence<Is...>) {
  std::visit(
      [&](auto&& arg) {
        using ArgType = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<ArgType, AllPendablesCompleted>) {
          std::invoke(std::forward<AllPendablesCompletedHandler>(all_completed),
                      AllPendablesCompleted{});
        } else {
          std::invoke(std::get<ArgType::kIndex>(
                          std::forward<ReadyHandlers>(ready_handlers)),
                      std::move(arg.value));
        }
      },
      std::forward<ResultVariant>(variant));
}

}  // namespace internal

/// @module{pw_async2}

/// @ingroup pw_async2_utilities
/// @{

/// A pendable value which returns the result of the first of several pendable
/// values to complete.
///
/// Each call to ``Selector::Pend`` polls its pendables until one returns
/// ``Ready``. If a ``Ready`` pendable is found, its value is returned as a
/// ``SelectResult`` indicating the index of the completed pendable.
///
/// If no pendables are ``Ready``, ``Selector::Pend`` returns a ``Pending``.
///
/// If every pendable provided to the ``Selector`` has already completed,
/// ``Pend`` will return an ``AllPendablesCompleted`` value indicating this.
///
/// Example usage:
/// @code{.cpp}
///   Poll<int> TaskOne(Context&) { return Ready(3); }
///   Poll<char> TaskTwo(Context&) { return Ready('c'); }
///
///   Poll<> RaceTasks(Context& cx) {
///     auto task_one = PendableFor<&TaskOne>();
///     auto task_two = PendableFor<&TaskTwo>();
///
///     Selector selector(std::move(task_one), std::move(task_two));
///     auto result = selector.Pend(cx);
///     if (result.IsPending()) {
///       return Pending();
///     }
///
///     VisitSelectResult(
///         *result,
///         [](AllPendablesCompleted) { PW_LOG_INFO("Both tasks completed"); },
///         [](int value) {
///           PW_LOG_INFO("Task one completed with value %d", value);
///         },
///         [](char value) {
///           PW_LOG_INFO("Task two completed with value %c", value);
///         });
///   }
/// @endcode
template <typename... Pendables>
class Selector {
 public:
  static_assert(sizeof...(Pendables) > 0,
                "Cannot select over an empty set of pendables");

  using ResultVariant = typename internal::SelectVariantType<
      std::make_index_sequence<sizeof...(Pendables)>,
      Pendables...>::type;

 public:
  /// Creates a ``SingleSelector`` from a series of pendable values.
  explicit Selector(Pendables&&... pendables)
      : pendables_(std::move(pendables)...) {}

  /// Attempts to complete the stored pendables, returning the first one which
  /// is ready.
  Poll<ResultVariant> Pend(Context& cx) { return PendFrom<0>(cx, false); }

 private:
  /// Pends the pendable stored at ``kTupleIndex``. If it is ``Ready``, returns
  /// its output wrapped in a ``ResultVariant``. Otherwise, continues pending
  /// from ``kTupleIndex + 1``.
  template <size_t kTupleIndex>
  Poll<ResultVariant> PendFrom(Context& cx, bool has_active_pendable) {
    if constexpr (kTupleIndex < sizeof...(Pendables)) {
      auto& pendable = std::get<kTupleIndex>(pendables_);

      if (pendable.completed()) {
        return PendFrom<kTupleIndex + 1>(cx, has_active_pendable);
      }

      using value_type =
          PendOutputOf<std::tuple_element_t<kTupleIndex, decltype(pendables_)>>;
      Poll<value_type> result = pendable.Pend(cx);

      if (result.IsReady()) {
        return Ready(ResultVariant(
            std::in_place_index<kTupleIndex>,
            SelectResult<kTupleIndex, value_type>(std::move(*result))));
      }
      return PendFrom<kTupleIndex + 1>(cx, true);
    } else {
      // All pendables have been polled and none were ready.

      if (!has_active_pendable) {
        return Ready(AllPendablesCompleted{});
      }
      return Pending();
    }
  }

  std::tuple<Pendables...> pendables_;
};

template <typename... Pendables>
Selector(Pendables&&...) -> Selector<Pendables...>;

/// Returns the result of the first of the provided pendables which completes.
///
/// This ``Select`` function is intended for single use only. To iterate over
/// all ready pendables, use ``Selector`` directly.
template <typename... Pendables>
auto Select(Context& cx, Pendables&&... pendables) {
  Selector selector(std::forward<Pendables>(pendables)...);
  return selector.Pend(cx);
}

/// Helper for interacting with a ``ResultVariant`` returned by a call to
/// ``Select``. See ``Selector`` documentation for example usage.
template <typename ResultVariant,
          typename AllPendablesCompletedHandler,
          typename... ReadyHandler>
void VisitSelectResult(
    ResultVariant&& variant,
    AllPendablesCompletedHandler&& on_all_pendables_completed,
    ReadyHandler&&... on_ready) {
  static_assert(std::variant_size_v<std::decay_t<ResultVariant>> - 1 ==
                    sizeof...(ReadyHandler),
                "Number of handlers must match the number of pendables.");
  internal::VisitSelectResult(
      std::forward<ResultVariant>(variant),
      std::forward<AllPendablesCompletedHandler>(on_all_pendables_completed),
      std::forward_as_tuple(std::forward<ReadyHandler>(on_ready)...),
      std::make_index_sequence<sizeof...(ReadyHandler)>{});
}

}  // namespace pw::async2
