// Copyright 2022 The Pigweed Authors
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

#include <new>
#include <type_traits>
#include <utility>

namespace pw {

// Helper type to create a function-local static variable of type T when T has a
// non-trivial destructor. Storing a T in a pw::NoDestructor<T> will prevent
// ~T() from running, even when the variable goes out of scope.
//
// This class is useful when a variable has static storage duration but its type
// has a non-trivial destructor. Destructor ordering is not defined and can
// cause issues in multithreaded environments. Additionally, removing destructor
// calls can save code size.
//
// Do not use pw::NoDestructor<T> with trivially destructible types. Use the
// type directly instead. If the variable can be constexpr, make it constexpr.
//
// pw::NoDestructor<T> provides a similar API to std::optional. Use * or -> to
// access the wrapped type.
//
// Example usage:
//
//   pw::sync::Mutex& GetMutex() {
//     // Use NoDestructor to avoid running the mutex destructor when exit-time
//     // destructors run.
//     static const pw::NoDestructor<pw::sync::Mutex> global_mutex;
//     return *global_mutex;
//   }
//
// WARNING: Misuse of NoDestructor can cause memory leaks and other problems.
// Only skip destructors when you know it is safe to do so.
//
// In Clang, pw::NoDestructor can be replaced with the [[clang::no_destroy]]
// attribute.
template <typename T>
class NoDestructor {
 public:
  static_assert(
      !std::is_trivially_destructible_v<T>,
      "T is trivially destructible; please use a function-local static of type "
      "T directly instead.");

  using value_type = T;

  // Initializes a T in place.
  //
  // This constructor is not constexpr; just write static constexpr T x = ...;
  // if the value should be a constexpr.
  //
  // This overload is disabled when it might collide with copy/move.
  template <typename... Args,
            typename std::enable_if<!std::is_same<void(std::decay_t<Args>&...),
                                                  void(NoDestructor&)>::value,
                                    int>::type = 0>
  explicit NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  // Move or copy from the contained type. This allows for construction from an
  // initializer list, e.g. for std::vector.
  explicit NoDestructor(const T& x) { new (storage_) T(x); }
  explicit NoDestructor(T&& x) { new (storage_) T(std::move(x)); }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  ~NoDestructor() = default;

  const T& operator*() const { return *get(); }
  T& operator*() { return *get(); }

  const T* operator->() const { return get(); }
  T* operator->() { return get(); }

 private:
  const T* get() const {
    return std::launder(reinterpret_cast<const T*>(storage_));
  }
  T* get() { return std::launder(reinterpret_cast<T*>(storage_)); }

  alignas(T) char storage_[sizeof(T)];
};

}  // namespace pw
