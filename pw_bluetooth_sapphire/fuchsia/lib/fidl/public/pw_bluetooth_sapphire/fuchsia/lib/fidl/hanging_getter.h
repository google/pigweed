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

#include <lib/fidl/cpp/clone.h>
#include <pw_assert/assert.h>

#include <optional>
#include <queue>

#include "pw_function/function.h"

namespace bt_lib_fidl {

// HangingGetter generalizes the consumer/producer pattern often involved in
// FIDL hanging get implementations.
//
// USAGE:
//
// Use `Set()` to update the state watched by the FIDL method:
//
//   HangingGetter<int> foo;
//   ...
//   void OnFooUpdated(int new_foo) {
//     foo.Set(new_foo);
//   }
//
// Use `Watch()` to invoke the response callback with any updates to the state
// or defer it until the update happens later:
//
//   void GetFoo(GetFooCallback callback) {
//     foo.Watch(std::move(callback));
//   }
//
// A specialization is provided for state that is a growing collection of state
// updates:
//
//   HangingVectorGetter<int> foo;
//   ...
//   void OnFooUpdated(int new_foo) {
//     foo.Add(new_foo);
//   }

template <typename T, typename C = void(T)>
class HangingGetterBase {
 public:
  using Callback = pw::Callback<C>;
  using Mutator = pw::Function<T(T)>;

  // Returns true if a callback is already assigned to this getter.
  bool armed() const { return !callbacks_.empty(); }

  // Assign |value| to the stored state and notify any pending Watch callbacks.
  void Set(T value) {
    value_.emplace(std::move(value));
    WatchInternal();
  }

  // Mutably access the stored state via function |f| and notify any pending
  // Watch callbacks. The Mutator function |f| will receive the current value
  // and should the return the new value after applying any transformations to
  // it.
  //
  // This is useful for value types that accumulate data. To directly assign a
  // value, use the non-mutator overload instead.
  void Transform(Mutator f) {
    PW_DASSERT(f);

    // Initialize the value if it's not dirty so that the mutator can work with
    // it.
    if (!value_) {
      value_.emplace();
    }

    value_.emplace(f(std::move(value_.value())));
    WatchInternal();
  }

  // Invoke |callback| with any updates to the state. If the state has been
  // accessed via one of the Set() functions since the last call to Watch(),
  // then |callback| will run immediately. Otherwise, |callback| is run next
  // time the state is accessed.
  //
  // Once |callback| runs, the store state becomes cleared. The next call to one
  // of the Set() functions will default-construct a new state.
  //
  // Multiple callbacks can be queued to be notified simultaneously next time
  // the state gets updated. All callbacks will get invoked with a separate copy
  // of the new state, however there is no requirement that the state type T
  // itself be copiable. How the value is passed to the callbacks is left up to
  // the Notify() member function implementation.
  void Watch(Callback callback) {
    callbacks_.push(std::move(callback));
    WatchInternal();
  }

 protected:
  // This member function is called when a value update triggers a registered
  // watcher Callback to be notified. This is abstract to allow HangingGetter
  // variants to apply any necessary transformations between the stored value
  // type "T" and the parameter type of the callback type "C".
  //
  // For example, this is useful when the stored type is a custom accumulator
  // type. A HangingGetterBase implementation can, for example, implement a
  // custom mapping from the stored accumulator to a FIDL struct type that the
  // callback expects.
  virtual void Notify(std::queue<Callback> callbacks, T value) = 0;

 private:
  void WatchInternal() {
    if (!callbacks_.empty() && value_) {
      auto q = std::move(callbacks_);
      auto v = std::move(*value_);
      value_.reset();

      Notify(std::move(q), std::move(v));
    }
  }

  std::optional<T> value_;
  std::queue<Callback> callbacks_;
};

// HangingGetter type where the stored type is identical to the callback
// parameter type. T must be clonable by fidl::Clone().
template <typename T>
class HangingGetter : public HangingGetterBase<T> {
 protected:
  void Notify(std::queue<typename HangingGetter<T>::Callback> callbacks,
              T value) override {
    while (!callbacks.empty()) {
      auto f = std::move(callbacks.front());
      callbacks.pop();
      f(fidl::Clone(value));
    }
  }
};

template <typename T>
class HangingVectorGetter : public HangingGetter<std::vector<T>> {
 public:
  // Insert |value| to the vector and notify any pending Watch callbacks.
  void Add(T&& value) {
    this->Transform([&](auto current) {
      current.push_back(std::forward<T>(value));
      return current;
    });
  }
};

}  // namespace bt_lib_fidl
