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
//
// -----------------------------------------------------------------------------
// File: result.h
// -----------------------------------------------------------------------------
//
// An `Result<T>` represents a union of an `pw::Status` object and an object of
// type `T`. The `Result<T>` will either contain an object of type `T`
// (indicating a successful operation), or an error (of type `Status`)
// explaining why such a value is not present.
//
// In general, check the success of an operation returning a `Result<T>` like
// you would an `pw::Status` by using the `ok()` member function.
//
// Example:
//
//   Result<Foo> result = Calculation();
//   if (result.ok()) {
//     result->DoSomethingCool();
//   } else {
//     PW_LOG_ERROR("Calculation failed: %s",  result.status().str());
//   }
#pragma once

#include <exception>
#include <functional>
#include <initializer_list>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "pw_preprocessor/compiler.h"
#include "pw_result/internal/result_internal.h"
#include "pw_status/status.h"

namespace pw {

/// @module{pw_result}

// Returned Result objects may not be ignored.
template <typename T>
class [[nodiscard]] Result;

/// Error propagation primitive: value-or error
///
/// The `pw::Result<T>` class template is a union of a `pw::Status` object and
/// an object of type `T`. `Result<t>` models an object that is either a usable
/// object, or an error (of type `pw::Status`) explaining why such an object is
/// not present. It is typically the return value of a function which may fail.
///
/// ### Checking for success
///
/// `Result<T>` can never hold an @OK status; instead, the presence of an
/// object of type `T` indicates success. Instead of checking for a `kOk` value,
/// use the `Result<T>::ok()` member function. (It is for this reason, and code
/// readability, that using the `ok()` function is preferred for `pw::Status` as
/// well.)
///
/// Example:
///
/// @code{.cpp}
///   Result<Foo> result = DoBigCalculationThatCouldFail();
///   if (result.ok()) {
///     result->DoSomethingCool();
///   } else {
///     PW_LOG_ERROR("Calculation failed: %s", result.status().str());
///   }
/// @endcode
///
/// ### Accessing objects
///
/// Accessing the object held by a `Result<T>` should be performed via
/// `operator*` or `operator->`, after a call to `ok()` confirms that the
/// `Result<T>` holds an object of type `T`:
///
/// Example:
///
/// @code{.cpp}
///   Result<int> i = GetCount();
///   if (i.ok()) {
///     updated_total += *i
///   }
/// @endcode
///
/// Using `Result<T>::value()` when no valid value is present will
/// trigger a `PW_ASSERT`.
///
/// Example:
///
/// @code{.cpp}
///   Result<Foo> result = DoBigCalculationThatCouldFail();
///   const Foo& foo = result.value();  // Crash/exception if no value present
///   foo.DoSomethingCool();
/// @endcode
///
/// ### Constructing result objects
///
/// A `Result<T*>` can be constructed from a null pointer like any other pointer
/// value, and the result will be that `ok()` returns `true` and `value()`
/// returns `nullptr`. Checking the value of pointer in a `Result<T>` generally
/// requires a bit more care, to ensure both that a value is present and that
/// value is not null:
///
/// @code{.cpp}
///  Result<Foo*> result = LookUpTheFoo(arg);
///  if (!result.ok()) {
///    PW_LOG_ERROR("Unable to look up the Foo: %s", result.status().str());
///  } else if (*result == nullptr) {
///    PW_LOG_ERROR("Unexpected null pointer");
///  } else {
///    (*result)->DoSomethingCool();
///  }
/// @endcode
///
/// Example factory implementation returning `Result<T>`:
///
/// @code{.cpp}
///  Result<Foo> FooFactory::MakeFoo(int arg) {
///    if (arg <= 0) {
///      return pw::Status::InvalidArgument();
///    }
///    return Foo(arg);
///  }
/// @endcode
template <typename T>
class Result : private internal_result::StatusOrData<T>,
               private internal_result::CopyCtorBase<T>,
               private internal_result::MoveCtorBase<T>,
               private internal_result::CopyAssignBase<T>,
               private internal_result::MoveAssignBase<T> {
  template <typename U>
  friend class Result;

  using Base = internal_result::StatusOrData<T>;

 public:
  /// This instance data provides a generic `value_type` member for use within
  /// generic programming. This usage is analogous to that of
  /// `std::optional::value_type`.
  typedef T value_type;

  // Constructors

  /// Constructs a new `Result<T>` with an `pw::Status::Unknown()` status.
  ///
  /// This constructor is marked `explicit` to prevent usages in return values
  /// (e.g. `return {}`) under the misconception that `Result<std::vector<int>>`
  /// (for example) will be initialized with an empty vector.
  explicit constexpr Result();

  /// `Result<T>` is copy constructible if `T` is copy constructible.
  constexpr Result(const Result&) = default;
  /// `Result<T>` is copy assignable if `T` is copy constructible and copy
  /// assignable.
  constexpr Result& operator=(const Result&) = default;

  /// `Result<T>` is move constructible if `T` is move constructible.
  constexpr Result(Result&&) = default;
  /// `Result<T>` is move assignable if `T` is move constructible and move
  /// assignable.
  constexpr Result& operator=(Result&&) = default;

  // Converting Constructors

  /// Constructs a new `Result<T>` from an `pw::Result<U>`, when `T` is
  /// constructible from `U`.
  ///
  /// To avoid ambiguity, these constructors are disabled if `T` is also
  /// constructible from `Result<U>`. This constructor is explicit if and only
  /// if the corresponding construction of `T` from `U` is explicit. (This
  /// constructor inherits its explicitness from the underlying constructor.)
  template <
      typename U,
      std::enable_if_t<
          std::conjunction<
              std::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::is_convertible<const U&, T>,
              std::negation<internal_result::
                                IsConstructibleOrConvertibleFromResult<T, U>>>::
              value,
          int> = 0>
  constexpr Result(const Result<U>& other)  // NOLINT
      : Base(static_cast<const typename Result<U>::Base&>(other)) {}
  template <
      typename U,
      std::enable_if_t<
          std::conjunction<
              std::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::negation<std::is_convertible<const U&, T>>,
              std::negation<internal_result::
                                IsConstructibleOrConvertibleFromResult<T, U>>>::
              value,
          int> = 0>
  explicit constexpr Result(const Result<U>& other)
      : Base(static_cast<const typename Result<U>::Base&>(other)) {}

  template <
      typename U,
      std::enable_if_t<
          std::conjunction<
              std::negation<std::is_same<T, U>>,
              std::is_constructible<T, U&&>,
              std::is_convertible<U&&, T>,
              std::negation<internal_result::
                                IsConstructibleOrConvertibleFromResult<T, U>>>::
              value,
          int> = 0>
  constexpr Result(Result<U>&& other)  // NOLINT
      : Base(static_cast<typename Result<U>::Base&&>(other)) {}
  template <
      typename U,
      std::enable_if_t<
          std::conjunction<
              std::negation<std::is_same<T, U>>,
              std::is_constructible<T, U&&>,
              std::negation<std::is_convertible<U&&, T>>,
              std::negation<internal_result::
                                IsConstructibleOrConvertibleFromResult<T, U>>>::
              value,
          int> = 0>
  explicit constexpr Result(Result<U>&& other)
      : Base(static_cast<typename Result<U>::Base&&>(other)) {}

  // Converting Assignment Operators

  /// Creates a `Result<T>` through assignment from a `Result<U>`.
  ///
  /// @pre These overloads only apply if `Result<T>` is constructible and
  /// assignable from `Result<U>` and `Result<T>` cannot be directly
  /// assigned from `Result<U>`.
  /// @pre Both `Result<T>` and `pw::Result<U>` are OK by assigning `U` to `T`
  /// directly.
  /// @pre `Result<T>` is OK and `pw::Result<U>` contains an error code by
  /// destroying the value of `Result<T>` and assigning from `Result<U>`.
  /// @pre `Result<T>` contains an error code and `pw::Result<U>` is OK by
  /// directly initializing `T` from `U`.
  /// @pre Both `Result<T>` and `pw::Result<U>` contain an error code by
  /// assigning the `Status` in `Result<U>` to `Result<T>`.
  template <typename U,
            std::enable_if_t<
                std::conjunction<
                    std::negation<std::is_same<T, U>>,
                    std::is_constructible<T, const U&>,
                    std::is_assignable<T, const U&>,
                    std::negation<
                        internal_result::
                            IsConstructibleOrConvertibleOrAssignableFromResult<
                                T,
                                U>>>::value,
                int> = 0>
  constexpr Result& operator=(const Result<U>& other) {
    this->Assign(other);
    return *this;
  }
  template <typename U,
            std::enable_if_t<
                std::conjunction<
                    std::negation<std::is_same<T, U>>,
                    std::is_constructible<T, U&&>,
                    std::is_assignable<T, U&&>,
                    std::negation<
                        internal_result::
                            IsConstructibleOrConvertibleOrAssignableFromResult<
                                T,
                                U>>>::value,
                int> = 0>
  constexpr Result& operator=(Result<U>&& other) {
    this->Assign(std::move(other));
    return *this;
  }

  /// Constructs a new `Result<T>` with a non-OK status.
  ///
  /// After calling this constructor, `this->ok()` will be `false` and calls to
  /// `value()` will crash, or produce an exception if exceptions are enabled.
  ///
  /// The constructor also takes any type `U` that is convertible to `Status`.
  /// This constructor is explicit if an only if `U` is not of type `Status` and
  /// the conversion from `U` to `Status` is explicit.
  ///
  /// @pre `!Status(std::forward<U>(v)).ok()`. This requirement is DCHECKed. In
  /// optimized builds, passing @OK here will have the effect of passing
  /// @INTERNAL as a fallback.
  template <
      typename U = Status,
      std::enable_if_t<
          std::conjunction<
              std::is_convertible<U&&, Status>,
              std::is_constructible<Status, U&&>,
              std::negation<std::is_same<std::decay_t<U>, Result<T>>>,
              std::negation<std::is_same<std::decay_t<U>, T>>,
              std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
              std::negation<internal_result::
                                HasConversionOperatorToResult<T, U&&>>>::value,
          int> = 0>
  constexpr Result(U&& v) : Base(std::forward<U>(v)) {}

  template <
      typename U = Status,
      std::enable_if_t<
          std::conjunction<
              std::negation<std::is_convertible<U&&, Status>>,
              std::is_constructible<Status, U&&>,
              std::negation<std::is_same<std::decay_t<U>, Result<T>>>,
              std::negation<std::is_same<std::decay_t<U>, T>>,
              std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
              std::negation<internal_result::
                                HasConversionOperatorToResult<T, U&&>>>::value,
          int> = 0>
  constexpr explicit Result(U&& v) : Base(std::forward<U>(v)) {}

  template <
      typename U = Status,
      std::enable_if_t<
          std::conjunction<
              std::is_convertible<U&&, Status>,
              std::is_constructible<Status, U&&>,
              std::negation<std::is_same<std::decay_t<U>, Result<T>>>,
              std::negation<std::is_same<std::decay_t<U>, T>>,
              std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
              std::negation<internal_result::
                                HasConversionOperatorToResult<T, U&&>>>::value,
          int> = 0>
  constexpr Result& operator=(U&& v) {
    this->AssignStatus(std::forward<U>(v));
    return *this;
  }

  /// Perfect-forwarding value assignment operator
  ///
  /// If `*this` contains a `T` value before the call, the contained value is
  /// assigned from `std::forward<U>(v)`. Otherwise, it is directly initialized
  /// from `std::forward<U>(v)`.
  ///
  /// @pre `std::is_constructible_v<T, U>` is true.
  /// @pre `std::is_assignable_v<T&, U>` is true.
  /// @pre `std::is_same_v<Result<T>, std::remove_cvref_t<U>>` is false.
  /// @pre Assigning `U` to `T` is not ambiguous. If `U` is `Result<V>` and `T`
  /// is constructible and assignable from both `Result<V>` and `V`, the
  /// assignment is considered bug-prone and ambiguous and thus will fail to
  /// compile. For example:
  /// @code{.cpp}
  ///   Result<bool> s1 = true;  // s1.ok() && *s1 == true
  ///   Result<bool> s2 = false;  // s2.ok() && *s2 == false
  ///   s1 = s2;  // ambiguous, `s1 = *s2` or `s1 = bool(s2)`?
  /// @endcode
  template <
      typename U = T,
      typename = typename std::enable_if<std::conjunction<
          std::is_constructible<T, U&&>,
          std::is_assignable<T&, U&&>,
          std::disjunction<
              std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
              std::conjunction<
                  std::negation<std::is_convertible<U&&, Status>>,
                  std::negation<
                      internal_result::HasConversionOperatorToResult<T, U&&>>>>,
          internal_result::IsForwardingAssignmentValid<T, U&&>>::value>::type>
  constexpr Result& operator=(U&& v) {
    this->Assign(std::forward<U>(v));
    return *this;
  }

  /// Constructs the inner value `T` in-place using the provided args, using the
  /// `T(args...)` constructor.
  template <typename... Args>
  explicit constexpr Result(std::in_place_t, Args&&... args);
  template <typename U, typename... Args>
  explicit constexpr Result(std::in_place_t,
                            std::initializer_list<U> ilist,
                            Args&&... args);

  /// Constructs the inner value `T` in-place using the provided args, using the
  /// `T(U)` (direct-initialization) constructor.
  ///
  /// @pre This constructor is only valid if `T` can be constructed from a `U`.
  /// Can accept move or copy constructors.
  ///
  /// This constructor is explicit if `U` is not convertible to `T`. To avoid
  /// ambiguity, this constructor is disabled if `U` is a `Result<J>`, where
  /// `J` is convertible to `T`.
  template <
      typename U = T,
      std::enable_if_t<
          std::conjunction<
              internal_result::IsDirectInitializationValid<T, U&&>,
              std::is_constructible<T, U&&>,
              std::is_convertible<U&&, T>,
              std::disjunction<
                  std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
                  std::conjunction<
                      std::negation<std::is_convertible<U&&, Status>>,
                      std::negation<
                          internal_result::
                              HasConversionOperatorToResult<T, U&&>>>>>::value,
          int> = 0>
  constexpr Result(U&& u)  // NOLINT
      : Result(std::in_place, std::forward<U>(u)) {}

  template <
      typename U = T,
      std::enable_if_t<
          std::conjunction<
              internal_result::IsDirectInitializationValid<T, U&&>,
              std::disjunction<
                  std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
                  std::conjunction<
                      std::negation<std::is_constructible<Status, U&&>>,
                      std::negation<
                          internal_result::
                              HasConversionOperatorToResult<T, U&&>>>>,
              std::is_constructible<T, U&&>,
              std::negation<std::is_convertible<U&&, T>>>::value,
          int> = 0>
  explicit constexpr Result(U&& u)  // NOLINT
      : Result(std::in_place, std::forward<U>(u)) {}

  /// @returns Whether or not this `Result<T>` holds a `T` value. This
  /// member function is analogous to @OK and should be used similarly to check
  /// the status of return values.
  ///
  /// Example:
  ///
  /// @code{.cpp}
  ///   Result<Foo> result = DoBigCalculationThatCouldFail();
  ///   if (result.ok()) {
  ///      // Handle result
  ///   else {
  ///      // Handle error
  ///   }
  /// @endcode
  [[nodiscard]] constexpr bool ok() const { return this->status_.ok(); }

  /// @returns The current `pw::Status` code contained within the `Result<T>`.
  /// If `pw::Result<T>` contains a `T`, then this function returns @OK.
  constexpr Status status() const;

  /// @returns A reference to the held value if `this->ok()`. Otherwise,
  /// terminates the process.
  ///
  /// If you have already checked the status using `this->ok()`, you probably
  /// want to use `operator*()` or `operator->()` to access the value instead of
  /// `value`.
  ///
  /// For value types that are cheap to copy, prefer simple code:
  ///
  /// @code{.cpp}
  ///   T value = result.value();
  /// @endcode
  ///
  /// Otherwise, if the value type is expensive to copy, but can be left
  /// in the `Result<T>`, simply assign to a reference:
  ///
  /// @code{.cpp}
  ///   T& value = result.value();  // or `const T&`
  /// @endcode
  ///
  /// Otherwise, if the value type supports an efficient move, it can be
  /// used as follows:
  ///
  /// @code{.cpp}
  ///   T value = std::move(result).value();
  /// @endcode
  ///
  /// The `std::move` on result instead of on the whole expression enables
  /// warnings about possible uses of the result object after the move.
  constexpr const T& value() const& PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr T& value() & PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr const T&& value() const&& PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr T&& value() && PW_ATTRIBUTE_LIFETIME_BOUND;

  /// @returns A reference to the current value.
  ///
  /// @pre `this->ok() == true`, otherwise the behavior is undefined.
  ///
  /// Use `this->ok()` to verify that there is a current value within the
  /// `Result<T>`. Alternatively, see the `value()` member function for a
  /// similar API that guarantees crashing or throwing an exception if there is
  /// no current value.
  constexpr const T& operator*() const& PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr T& operator*() & PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr const T&& operator*() const&& PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr T&& operator*() && PW_ATTRIBUTE_LIFETIME_BOUND;

  /// @returns A pointer to the current value.
  ///
  /// @pre `this->ok() == true`, otherwise the behavior is undefined.
  ///
  /// Use `this->ok()` to verify that there is a current value.
  constexpr const T* operator->() const PW_ATTRIBUTE_LIFETIME_BOUND;
  constexpr T* operator->() PW_ATTRIBUTE_LIFETIME_BOUND;

  /// @returns The current value if `this->ok() == true`. Otherwise constructs a
  /// value using the provided `default_value`.
  ///
  /// Unlike `value`, this function returns by value, copying the current value
  /// if necessary. If the value type supports an efficient move, it can be used
  /// as follows:
  ///
  /// @code{.cpp}
  ///   T value = std::move(result).value_or(def);
  /// @endcode
  ///
  /// Unlike with `value`, calling `std::move()` on the result of `value_or`
  /// will still trigger a copy.
  template <typename U>
  constexpr T value_or(U&& default_value) const&;
  template <typename U>
  constexpr T value_or(U&& default_value) &&;

  /// Ignores any errors. This method does nothing except potentially suppress
  /// complaints from any tools that are checking that errors are not dropped.
  constexpr void IgnoreError() const;

  /// Reconstructs the inner value `T` in-place using the provided args, using
  /// the `T(args...)` constructor.
  ///
  /// @returns A reference to the reconstructed `T`.
  template <typename... Args>
  T& emplace(Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(std::forward<Args>(args)...);
    } else {
      this->MakeValue(std::forward<Args>(args)...);
      this->status_ = OkStatus();
    }
    return this->data_;
  }

  template <
      typename U,
      typename... Args,
      std::enable_if_t<
          std::is_constructible<T, std::initializer_list<U>&, Args&&...>::value,
          int> = 0>
  T& emplace(std::initializer_list<U> ilist, Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(ilist, std::forward<Args>(args)...);
    } else {
      this->MakeValue(ilist, std::forward<Args>(args)...);
      this->status_ = OkStatus();
    }
    return this->data_;
  }

  /// @returns The `Result<T>` from the invocation of the function on the
  /// contained value if it exists. Otherwise, returns the contained status in
  /// the `Result<T>`.
  ///
  /// General template logic:
  ///
  /// @code{.cpp}
  ///   template <typename U>
  ///   Result<U> and_then(Function<Result<U>(T)> func);
  /// @endcode
  ///
  /// Example:
  ///
  /// @code{.cpp}
  ///   Result<Foo> CreateFoo();
  ///   Result<Bar> CreateBarFromFoo(const Foo& foo);
  ///
  ///   Result<Bar> bar = CreateFoo().and_then(CreateBarFromFoo);
  /// @endcode
  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&>,
            std::enable_if_t<std::is_copy_constructible_v<Ret>, int> = 0>
  constexpr Ret and_then(Fn&& function) & {
    static_assert(internal_result::IsResult<Ret>,
                  "Fn must return a pw::Result");
    return ok() ? std::invoke(std::forward<Fn>(function), value())
                : Ret(status());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&&>,
            std::enable_if_t<std::is_move_constructible_v<Ret>, int> = 0>
  constexpr auto and_then(Fn&& function) && {
    static_assert(internal_result::IsResult<Ret>,
                  "Fn must return a pw::Result");
    return ok() ? std::invoke(std::forward<Fn>(function), std::move(value()))
                : Ret(status());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, const T&>,
            std::enable_if_t<std::is_copy_constructible_v<Ret>, int> = 0>
  constexpr auto and_then(Fn&& function) const& {
    static_assert(internal_result::IsResult<Ret>,
                  "Fn must return a pw::Result");
    return ok() ? std::invoke(std::forward<Fn>(function), value())
                : Ret(status());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, const T&&>,
            std::enable_if_t<std::is_move_constructible_v<Ret>, int> = 0>
  constexpr auto and_then(Fn&& function) const&& {
    static_assert(internal_result::IsResult<Ret>,
                  "Fn must return a pw::Result");
    return ok() ? std::invoke(std::forward<Fn>(function), std::move(value()))
                : Ret(status());
  }

  /// @returns A `Result<T>` if it has a value, otherwise it invokes the given
  /// function. The function must return a type convertible to a `Result<T>` or
  /// a `void`.
  ///
  /// General template logic:
  ///
  /// @code{.cpp}
  ///   template <typename U>
  ///     requires std::is_convertible_v<U, Result<T>>
  ///   Result<T> or_else(Function<U(Status)> func);
  ///
  ///   Result<T> or_else(Function<void(Status)> func);
  /// @endcode
  ///
  /// Example:
  ///
  /// @code{.cpp}
  ///   Result<Foo> CreateFoo();
  ///
  ///   Result<Foo> foo = CreateFoo().or_else(
  ///       [](Status s) { PW_LOG_ERROR("Status: %d", s.code()); });
  /// @endcode
  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, const Status&>,
            std::enable_if_t<!std::is_void_v<Ret>, int> = 0>
  constexpr Result<T> or_else(Fn&& function) const& {
    static_assert(std::is_convertible_v<Ret, Result<T>>,
                  "Fn must be convertible to a pw::Result");
    return ok() ? *this : std::invoke(std::forward<Fn>(function), status());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, const Status&>,
            std::enable_if_t<std::is_void_v<Ret>, int> = 0>
  constexpr Result<T> or_else(Fn&& function) const& {
    if (ok()) {
      return *this;
    }
    std::invoke(std::forward<Fn>(function), status());
    return *this;
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, Status&&>,
            std::enable_if_t<!std::is_void_v<Ret>, int> = 0>
  constexpr Result<T> or_else(Fn&& function) && {
    static_assert(std::is_convertible_v<Ret, Result<T>>,
                  "Fn must be convertible to a pw::Result");
    return ok() ? std::move(*this)
                : std::invoke(std::forward<Fn>(function), std::move(status()));
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, Status&&>,
            std::enable_if_t<std::is_void_v<Ret>, int> = 0>
  constexpr Result<T> or_else(Fn&& function) && {
    if (ok()) {
      return *this;
    }
    std::invoke(std::forward<Fn>(function), status());
    return std::move(*this);
  }

  /// @returns A `Result<U>` which contains the result of the invocation of the
  /// given function if `*this` contains a value. Otherwise, returns a
  /// `Result<U>` with the same Status as `*this`.
  ///
  /// General template logic:
  ///
  /// @code{.cpp}
  ///   template <typename U>
  ///   Result<U> transform(Function<U(T)> func);
  /// @endcode
  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&>,
            std::enable_if_t<std::is_copy_constructible_v<Ret>, int> = 0>
  constexpr Result<Ret> transform(Fn&& function) & {
    if (!ok()) {
      return status();
    }
    return std::invoke(std::forward<Fn>(function), value());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&&>,
            std::enable_if_t<std::is_move_constructible_v<Ret>, int> = 0>
  constexpr Result<Ret> transform(Fn&& function) && {
    if (!ok()) {
      return std::move(status());
    }
    return std::invoke(std::forward<Fn>(function), std::move(value()));
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&>,
            std::enable_if_t<std::is_copy_constructible_v<Ret>, int> = 0>
  constexpr Result<Ret> transform(Fn&& function) const& {
    if (!ok()) {
      return status();
    }
    return std::invoke(std::forward<Fn>(function), value());
  }

  template <typename Fn,
            typename Ret = internal_result::InvokeResultType<Fn, T&&>,
            std::enable_if_t<std::is_move_constructible_v<Ret>, int> = 0>
  constexpr Result<Ret> transform(Fn&& function) const&& {
    if (!ok()) {
      return std::move(status());
    }
    return std::invoke(std::forward<Fn>(function), std::move(value()));
  }

 private:
  using Base::Assign;
  template <typename U>
  constexpr void Assign(const Result<U>& other);
  template <typename U>
  constexpr void Assign(Result<U>&& other);
};

/// Deduction guide to allow `Result(v)` rather than `Result<T>(v)`.
template <typename T>
Result(T value) -> Result<T>;

/// Checks the equality of two `Result<T>` objects.
template <typename T>
constexpr bool operator==(const Result<T>& lhs, const Result<T>& rhs) {
  if (lhs.ok() && rhs.ok()) {
    return *lhs == *rhs;
  }
  return lhs.status() == rhs.status();
}

/// Checks the inequality of two `Result<T>` objects.
template <typename T>
constexpr bool operator!=(const Result<T>& lhs, const Result<T>& rhs) {
  return !(lhs == rhs);
}

/// @}

//------------------------------------------------------------------------------
// Implementation details for Result<T>
//------------------------------------------------------------------------------

template <typename T>
constexpr Result<T>::Result() : Base(Status::Unknown()) {}

template <typename T>
template <typename U>
constexpr inline void Result<T>::Assign(const Result<U>& other) {
  if (other.ok()) {
    this->Assign(*other);
  } else {
    this->AssignStatus(other.status());
  }
}

template <typename T>
template <typename U>
constexpr inline void Result<T>::Assign(Result<U>&& other) {
  if (other.ok()) {
    this->Assign(*std::move(other));
  } else {
    this->AssignStatus(std::move(other).status());
  }
}
template <typename T>
template <typename... Args>
constexpr Result<T>::Result(std::in_place_t, Args&&... args)
    : Base(std::in_place, std::forward<Args>(args)...) {}

template <typename T>
template <typename U, typename... Args>
constexpr Result<T>::Result(std::in_place_t,
                            std::initializer_list<U> ilist,
                            Args&&... args)
    : Base(std::in_place, ilist, std::forward<Args>(args)...) {}

template <typename T>
constexpr Status Result<T>::status() const {
  return this->status_;
}

template <typename T>
constexpr const T& Result<T>::value() const& {
  PW_ASSERT(this->status_.ok());
  return this->data_;
}

template <typename T>
constexpr T& Result<T>::value() & {
  PW_ASSERT(this->status_.ok());
  return this->data_;
}

template <typename T>
constexpr const T&& Result<T>::value() const&& {
  PW_ASSERT(this->status_.ok());
  return std::move(this->data_);
}

template <typename T>
constexpr T&& Result<T>::value() && {
  PW_ASSERT(this->status_.ok());
  return std::move(this->data_);
}

template <typename T>
constexpr const T& Result<T>::operator*() const& {
  PW_ASSERT(this->status_.ok());
  return this->data_;
}

template <typename T>
constexpr T& Result<T>::operator*() & {
  PW_ASSERT(this->status_.ok());
  return this->data_;
}

template <typename T>
constexpr const T&& Result<T>::operator*() const&& {
  PW_ASSERT(this->status_.ok());
  return std::move(this->data_);
}

template <typename T>
constexpr T&& Result<T>::operator*() && {
  PW_ASSERT(this->status_.ok());
  return std::move(this->data_);
}

template <typename T>
constexpr const T* Result<T>::operator->() const {
  PW_ASSERT(this->status_.ok());
  return &this->data_;
}

template <typename T>
constexpr T* Result<T>::operator->() {
  PW_ASSERT(this->status_.ok());
  return &this->data_;
}

template <typename T>
template <typename U>
constexpr T Result<T>::value_or(U&& default_value) const& {
  if (ok()) {
    return this->data_;
  }
  return std::forward<U>(default_value);
}

template <typename T>
template <typename U>
constexpr T Result<T>::value_or(U&& default_value) && {
  if (ok()) {
    return std::move(this->data_);
  }
  return std::forward<U>(default_value);
}

template <typename T>
constexpr void Result<T>::IgnoreError() const {
  // no-op
}

namespace internal {

template <typename T>
constexpr Status ConvertToStatus(const Result<T>& result) {
  return result.status();
}

template <typename T>
constexpr T&& ConvertToValue(Result<T>& result) {
  return std::move(result).value();
}

}  // namespace internal
}  // namespace pw
