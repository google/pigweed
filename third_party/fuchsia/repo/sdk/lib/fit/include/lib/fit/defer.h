// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FIT_DEFER_H_
#define LIB_FIT_DEFER_H_

#include <utility>

#include "function.h"
#include "nullable.h"

namespace fit {

// A move-only deferred action wrapper with RAII semantics.
// This class is not thread safe.
//
// The wrapper holds a function-like callable target with no arguments
// which it invokes when it goes out of scope unless canceled, called, or
// moved to a wrapper in a different scope.
//
// See |fit::defer()| for idiomatic usage.
template <typename T>
class deferred_action final {
 public:
  // Creates a deferred action without a pending target.
  deferred_action() = default;
  explicit deferred_action(decltype(nullptr)) {}

  // Creates a deferred action with a pending target.
  explicit deferred_action(T target) : target_(std::move(target)) {}

  // Creates a deferred action with a pending target moved from another
  // deferred action, leaving the other one without a pending target.
  deferred_action(deferred_action&& other) : target_(std::move(other.target_)) {
    other.target_.reset();
  }

  // Invokes and releases the deferred action's pending target (if any).
  ~deferred_action() { call(); }

  // Returns true if the deferred action has a pending target.
  explicit operator bool() const { return !!target_; }

  // Invokes and releases the deferred action's pending target (if any),
  // then move-assigns it from another deferred action, leaving the latter
  // one without a pending target.
  deferred_action& operator=(deferred_action&& other) {
    if (&other == this)
      return *this;
    call();
    target_ = std::move(other.target_);
    other.target_.reset();
    return *this;
  }

  // Invokes and releases the deferred action's pending target (if any).
  void call() {
    if (target_) {
      // Move to a local to guard against re-entrance.
      T local_target = std::move(*target_);
      target_.reset();
      local_target();
    }
  }

  // Releases the deferred action's pending target (if any) without
  // invoking it.
  void cancel() { target_.reset(); }
  deferred_action& operator=(decltype(nullptr)) {
    cancel();
    return *this;
  }

  // Assigns a new target to the deferred action.
  deferred_action& operator=(T target) {
    target_ = std::move(target);
    return *this;
  }

  deferred_action(const deferred_action& other) = delete;
  deferred_action& operator=(const deferred_action& other) = delete;

 private:
  nullable<T> target_;
};

template <typename T>
bool operator==(const deferred_action<T>& action, decltype(nullptr)) {
  return !action;
}
template <typename T>
bool operator==(decltype(nullptr), const deferred_action<T>& action) {
  return !action;
}
template <typename T>
bool operator!=(const deferred_action<T>& action, decltype(nullptr)) {
  return !!action;
}
template <typename T>
bool operator!=(decltype(nullptr), const deferred_action<T>& action) {
  return !!action;
}

// Defers execution of a function-like callable target with no arguments
// until the value returned by this function goes out of scope unless canceled,
// called, or moved to a wrapper in a different scope.
//
// // This example prints "Hello..." then "Goodbye!".
// void test() {
//     auto d = fit::defer([]{ puts("Goodbye!"); });
//     puts("Hello...");
// }
//
// // This example prints nothing because the deferred action is canceled.
// void do_nothing() {
//     auto d = fit::defer([]{ puts("I'm not here."); });
//     d.cancel();
// }
//
// // This example shows how the deferred action can be reassigned assuming
// // the new target has the same type and the old one, in this case by
// // representing the target as a |fit::closure|.
// void reassign() {
//     auto d = fit::defer<fit::closure>([] { puts("This runs first."); });
//     d = fit::defer<fit::closure>([] { puts("This runs afterwards."); });
// }
template <typename T>
__attribute__((__warn_unused_result__)) inline deferred_action<T> defer(T target) {
  return deferred_action<T>(std::move(target));
}

// Alias for a deferred_action using a fit::callback.
using deferred_callback = deferred_action<fit::callback<void()>>;

// Defers execution of a fit::callback with no arguments. See |fit::defer| for
// details.
__attribute__((__warn_unused_result__)) inline deferred_callback defer_callback(
    fit::callback<void()> target) {
  return deferred_callback(std::move(target));
}

}  // namespace fit

#endif  // LIB_FIT_DEFER_H_
