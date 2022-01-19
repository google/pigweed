// Copyright 2021 The Pigweed Authors
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

#include <cstddef>
#include <new>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_function/config.h"
#include "pw_preprocessor/compiler.h"

namespace pw::function_internal {

template <typename T, typename Comparison = bool>
struct NullEq {
  static constexpr bool Test(const T&) { return false; }
};

// Partial specialization for values of T comparable to nullptr.
template <typename T>
struct NullEq<T, decltype(std::declval<T>() == nullptr)> {
  // This is intended to be used for comparing function pointers to nullptr, but
  // the specialization also matches Ts that implicitly convert to a function
  // pointer, such as function types. The compiler may then complain that the
  // comparison is false, as the address is known at compile time and cannot be
  // nullptr. Silence this warning. (The compiler will optimize out the
  // comparison.)
  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Waddress");
  static constexpr bool Test(const T& v) { return v == nullptr; }
  PW_MODIFY_DIAGNOSTICS_POP();
};

// Tests whether a value is considered to be null.
template <typename T>
static constexpr bool IsNull(const T& v) {
  return NullEq<T>::Test(v);
}

// FunctionTarget is an interface for storing a callable object and providing a
// way to invoke it. The GenericFunctionTarget expresses the interface common to
// all pw::Function instantiations. The derived FunctionTarget class adds the
// call operator, which is templated on the function arguments and return type.
class GenericFunctionTarget {
 public:
  constexpr GenericFunctionTarget() = default;

  GenericFunctionTarget(const GenericFunctionTarget&) = delete;
  GenericFunctionTarget(GenericFunctionTarget&&) = delete;
  GenericFunctionTarget& operator=(const GenericFunctionTarget&) = delete;
  GenericFunctionTarget& operator=(GenericFunctionTarget&&) = delete;

  virtual void Destroy() {}

  // Only returns true for NullFunctionTarget.
  virtual bool IsNull() const { return false; }

  // Move initialize the function target to a provided location.
  virtual void MoveInitializeTo(void* ptr) = 0;

 protected:
  ~GenericFunctionTarget() = default;  // The destructor is never called.
};

// FunctionTarget is an interface for storing a callable object and providing a
// way to invoke it.
template <typename Return, typename... Args>
class FunctionTarget : public GenericFunctionTarget {
 public:
  constexpr FunctionTarget() = default;

  // Invoke the callable stored by the function target.
  virtual Return operator()(Args... args) const = 0;

 protected:
  ~FunctionTarget() = default;  // The destructor is never called.
};

// A function target that does not store any callable. Attempting to invoke it
// results in a crash.
class NullFunctionTarget final : public FunctionTarget<void> {
 public:
  constexpr NullFunctionTarget() = default;

  NullFunctionTarget(const NullFunctionTarget&) = delete;
  NullFunctionTarget(NullFunctionTarget&&) = delete;
  NullFunctionTarget& operator=(const NullFunctionTarget&) = delete;
  NullFunctionTarget& operator=(NullFunctionTarget&&) = delete;

  bool IsNull() const final { return true; }

  void operator()() const final { PW_ASSERT(false); }

  void MoveInitializeTo(void* ptr) final { new (ptr) NullFunctionTarget(); }
};

// Function target that stores a callable as a member within the class.
template <typename Callable, typename Return, typename... Args>
class InlineFunctionTarget final : public FunctionTarget<Return, Args...> {
 public:
  explicit InlineFunctionTarget(Callable&& callable)
      : callable_(std::move(callable)) {}

  void Destroy() final { callable_.~Callable(); }

  InlineFunctionTarget(const InlineFunctionTarget&) = delete;
  InlineFunctionTarget& operator=(const InlineFunctionTarget&) = delete;

  InlineFunctionTarget(InlineFunctionTarget&& other)
      : callable_(std::move(other.callable_)) {}
  InlineFunctionTarget& operator=(InlineFunctionTarget&&) = default;

  Return operator()(Args... args) const final {
    return callable_(std::forward<Args>(args)...);
  }

  void MoveInitializeTo(void* ptr) final {
    new (ptr) InlineFunctionTarget(std::move(*this));
  }

 private:
  // This must be mutable to support custom objects that implement operator() in
  // a non-const way.
  mutable Callable callable_;
};

// Function target which stores a callable at a provided location in memory.
// The creating context must ensure that the region is properly sized and
// aligned for the callable.
template <typename Callable, typename Return, typename... Args>
class MemoryFunctionTarget final : public FunctionTarget<Return, Args...> {
 public:
  MemoryFunctionTarget(void* address, Callable&& callable) : address_(address) {
    new (address_) Callable(std::move(callable));
  }

  void Destroy() final {
    // Multiple MemoryFunctionTargets may have referred to the same callable
    // (due to moves), but only one can have a valid pointer to it. The owner is
    // responsible for destructing the callable.
    if (address_ != nullptr) {
      callable().~Callable();
    }
  }

  MemoryFunctionTarget(const MemoryFunctionTarget&) = delete;
  MemoryFunctionTarget& operator=(const MemoryFunctionTarget&) = delete;

  // Transfer the pointer to the initialized callable to this object without
  // reinitializing the callable, clearing the address from the other.
  MemoryFunctionTarget(MemoryFunctionTarget&& other)
      : address_(other.address_) {
    other.address_ = nullptr;
  }
  MemoryFunctionTarget& operator=(MemoryFunctionTarget&&) = default;

  Return operator()(Args... args) const final { return callable()(args...); }

  void MoveInitializeTo(void* ptr) final {
    new (ptr) MemoryFunctionTarget(std::move(*this));
  }

 private:
  Callable& callable() {
    return *std::launder(reinterpret_cast<Callable*>(address_));
  }
  const Callable& callable() const {
    return *std::launder(reinterpret_cast<const Callable*>(address_));
  }

  void* address_;
};

template <size_t kSizeBytes>
using FunctionStorage =
    std::aligned_storage_t<kSizeBytes, alignof(std::max_align_t)>;

// A FunctionTargetHolder stores an instance of a FunctionTarget implementation.
//
// The concrete implementation is initialized in an internal buffer by calling
// one of the initialization functions. After initialization, all
// implementations are accessed through the virtual FunctionTarget base.
template <size_t kSizeBytes, typename Return, typename... Args>
class FunctionTargetHolder {
 public:
  constexpr FunctionTargetHolder() : null_function_ {}
  {}

  FunctionTargetHolder(const FunctionTargetHolder&) = delete;
  FunctionTargetHolder(FunctionTargetHolder&&) = delete;
  FunctionTargetHolder& operator=(const FunctionTargetHolder&) = delete;
  FunctionTargetHolder& operator=(FunctionTargetHolder&&) = delete;

  constexpr void InitializeNullTarget() {
    static_assert(sizeof(NullFunctionTarget) <= kSizeBytes,
                  "NullFunctionTarget must fit within FunctionTargetHolder");
    new (&null_function_) NullFunctionTarget;
  }

  // Initializes an InlineFunctionTarget with the callable, failing if it is too
  // large.
  template <typename Callable>
  void InitializeInlineTarget(Callable callable) {
    using InlineFunctionTarget =
        InlineFunctionTarget<Callable, Return, Args...>;
    static_assert(sizeof(InlineFunctionTarget) <= kSizeBytes,
                  "Inline callable must fit within FunctionTargetHolder");
    new (&bits_) InlineFunctionTarget(std::move(callable));
  }

  // Initializes a MemoryTarget that stores the callable at the provided
  // location.
  template <typename Callable>
  void InitializeMemoryTarget(Callable callable, void* storage) {
    using MemoryFunctionTarget =
        MemoryFunctionTarget<Callable, Return, Args...>;
    static_assert(sizeof(MemoryFunctionTarget) <= kSizeBytes,
                  "MemoryFunctionTarget must fit within FunctionTargetHolder");
    new (&bits_) MemoryFunctionTarget(storage, std::move(callable));
  }

  void DestructTarget() { target().Destroy(); }

  // Initializes the function target within this callable from another target
  // holder's function target.
  void MoveInitializeTargetFrom(FunctionTargetHolder& other) {
    other.target().MoveInitializeTo(&bits_);
  }

  // The stored implementation is accessed by punning to the virtual base class.
  using Target = FunctionTarget<Return, Args...>;
  Target& target() { return *std::launder(reinterpret_cast<Target*>(&bits_)); }
  const Target& target() const {
    return *std::launder(reinterpret_cast<const Target*>(&bits_));
  }

 private:
  // Storage for an implementation of the FunctionTarget interface. Make this a
  // union with NullFunctionTarget so that the constexpr constructor can
  // initialize null_function_ directly.
  union {
    FunctionStorage<kSizeBytes> bits_;
    NullFunctionTarget null_function_;
  };
};

}  // namespace pw::function_internal
