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

#include "pw_async2/dispatcher.h"

namespace pw::async2 {

/// @module{pw_async2}

/// @ingroup pw_async2_utilities
/// @{

/// A pendable value which joins together several separate pendable values.
///
/// It will only return ``Ready`` once all of the individual pendables have
/// returned ``Ready``. The resulting ``Ready`` value contains a tuple of
/// the results of joined pendable values.
template <typename... Pendables>
class Join {
 private:
  static constexpr auto kTupleIndexSequence =
      std::make_index_sequence<sizeof...(Pendables)>();
  using TupleOfOutputRvalues = std::tuple<PendOutputOf<Pendables>&&...>;

 public:
  /// Creates a ``Join`` from a series of pendable values.
  explicit Join(Pendables&&... pendables)
      : pendables_(std::move(pendables)...),
        outputs_(Poll<PendOutputOf<Pendables>>(Pending())...) {}

  /// Attempts to complete all of the pendables, returning ``Ready``
  /// with their results if all are complete.
  Poll<TupleOfOutputRvalues> Pend(Context& cx) {
    if (!PendElements(cx, kTupleIndexSequence)) {
      return Pending();
    }
    return TakeOutputs(kTupleIndexSequence);
  }

 private:
  /// Pends all non-completed sub-pendables at indices ``Is...`.
  ///
  /// Returns whether or not all sub-pendables have completed.
  template <size_t... Is>
  bool PendElements(Context& cx, std::index_sequence<Is...>) {
    return (... && PendElement<Is>(cx));
  }

  /// Takes the results of all sub-pendables at indices ``Is...`.
  template <size_t... Is>
  Poll<TupleOfOutputRvalues> TakeOutputs(std::index_sequence<Is...>) {
    return Poll<TupleOfOutputRvalues>(
        std::forward_as_tuple<PendOutputOf<Pendables>...>(TakeOutput<Is>()...));
  }

  /// For pendable at `TupleIndex`, if it has not already returned
  /// a ``Ready`` result, attempts to complete it and store the result in
  /// ``outputs_``.
  ///
  /// Returns whether the sub-pendable has completed.
  template <size_t kTupleIndex>
  bool PendElement(Context& cx) {
    auto& output = std::get<kTupleIndex>(outputs_);
    if (output.IsReady()) {
      return true;
    }
    output = std::get<kTupleIndex>(pendables_).Pend(cx);
    return output.IsReady();
  }

  /// Takes the result of the sub-pendable at index ``kTupleIndex``.
  template <size_t kTupleIndex>
  PendOutputOf<typename std::tuple_element<kTupleIndex,
                                           std::tuple<Pendables...>>::type>&&
  TakeOutput() {
    return std::move(std::get<kTupleIndex>(outputs_).value());
  }

  std::tuple<Pendables...> pendables_;
  std::tuple<Poll<PendOutputOf<Pendables>>...> outputs_;
};

template <typename... Pendables>
Join(Pendables&&...) -> Join<Pendables...>;

}  // namespace pw::async2
