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

#include "pw_assert/assert.h"
#include "pw_async2/future.h"

namespace pw::async2::experimental {

template <typename T>
class ValueFuture;

/// A one-to-many provider for a single value.
///
/// A `BroadcastValueProvider` can vend multiple `ValueFuture` objects. When the
/// provider is resolved, all futures vended by it are completed with the same
/// value.
///
/// This provider is multi-shot: after `Resolve` is called, new futures can
/// be retrieved with `Get` to wait for the next `Resolve` event.
template <typename T>
class BroadcastValueProvider {
 public:
  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// Multiple futures can be retrieved and will pend concurrently.
  ValueFuture<T> Get();

  /// Resolves every pending `ValueFuture` with a copy of the provided value.
  void Resolve(T value);

 private:
  template <typename Resolver>
  void ResolveAll(Resolver resolver);

  ListFutureProvider<ValueFuture<T>> provider_;
};

/// A one-to-one provider for a single value.
///
/// An `ValueProvider` can only vend one `ValueFuture` at a time.
///
/// This provider is multi-shot: after `Resolve` is called, a new future can
/// be retrieved with `Get` to wait for the next `Resolve` event.
template <typename T>
class ValueProvider {
 public:
  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// If a future has already been vended and is still pending, this crashes.
  ValueFuture<T> Get();

  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// If a future has already been vended and is still pending, this will
  /// return `std::nullopt`.
  std::optional<ValueFuture<T>> TryGet();

  /// Returns `true` if the provider stores a pending future.
  bool has_future() { return provider_.has_future(); }

  /// Resolves the pending `ValueFuture` with a the provided value.
  void Resolve(T value);

  /// Resolves the pending `ValueFuture` by constructing its value in-place.
  template <typename... Args>
  void Resolve(std::in_place_t, Args&&... args);

 private:
  SingleFutureProvider<ValueFuture<T>> provider_;
};

/// A future that holds a single value.
///
/// A `ValueFuture` is a concrete `Future` implementation that is vended by a
/// `ValueProvider` or a `BroadcastValueProvider`. It waits until the provider
/// resolves it with a value.
template <typename T>
class ValueFuture : public ListableFutureWithWaker<ValueFuture<T>, T> {
 public:
  using Base = ListableFutureWithWaker<ValueFuture<T>, T>;
  using typename Base::Provider;

  ValueFuture(ValueFuture&& other) noexcept
      : Base(Base::kMovedFrom), value_(std::move(other.value_)) {
    Base::MoveFrom(other);
  }

  ValueFuture& operator=(ValueFuture&& other) noexcept {
    if (this != &other) {
      value_ = std::move(other.value_);
      Base::MoveFrom(other);
    }
    return *this;
  }

 private:
  friend Base;
  friend class ValueProvider<T>;
  friend class BroadcastValueProvider<T>;

  static constexpr const char kWaitReason[] = "ValueFuture";

  explicit ValueFuture(Provider& provider) : Base(provider) {}
  explicit ValueFuture(SingleFutureProvider<ValueFuture<T>>& provider)
      : Base(provider) {}

  template <typename Setter>
  void ResolveFunc(Setter setter) {
    {
      std::lock_guard guard(Base::lock());
      PW_ASSERT(!value_.has_value());
      setter(value_);
      this->unlist();
    }
    Base::Wake();
  }

  void Resolve(T value) {
    ResolveFunc([&value](std::optional<T>& dst) { dst = value; });
  }

  void Resolve(T&& value) {
    ResolveFunc([val = std::move(value)](std::optional<T>& dst) {
      dst = std::move(val);
    });
  }

  template <typename... Args>
  void Resolve(std::in_place_t, Args&&... args) {
    ResolveFunc([&](std::optional<T>& dst) {
      dst.emplace(std::forward<Args>(args)...);
    });
  }

  Poll<T> DoPend(Context&) {
    std::lock_guard guard(Base::lock());
    if (value_.has_value()) {
      T value = std::move(value_.value());
      value_.reset();
      return Ready(std::move(value));
    }
    return Pending();
  }

  std::optional<T> value_;
};

template <typename T>
ValueFuture<T> BroadcastValueProvider<T>::Get() {
  return ValueFuture<T>(provider_);
}

template <typename T>
void BroadcastValueProvider<T>::Resolve(T value) {
  while (!provider_.empty()) {
    ValueFuture<T>& future = provider_.Pop();
    future.Resolve(value);
  }
}

template <typename T>
ValueFuture<T> ValueProvider<T>::Get() {
  PW_ASSERT(!has_future());
  return ValueFuture<T>(provider_);
}

template <typename T>
std::optional<ValueFuture<T>> ValueProvider<T>::TryGet() {
  if (has_future()) {
    return std::nullopt;
  }
  return ValueFuture<T>(provider_);
}

template <typename T>
void ValueProvider<T>::Resolve(T value) {
  if (provider_.has_future()) {
    provider_.Take().Resolve(value);
  }
}

template <typename T>
template <typename... Args>
void ValueProvider<T>::Resolve(std::in_place_t, Args&&... args) {
  if (provider_.has_future()) {
    provider_.Take().Resolve(std::in_place, std::forward<Args>(args)...);
  }
}

}  // namespace pw::async2::experimental
