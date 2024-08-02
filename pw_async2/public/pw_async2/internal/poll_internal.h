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

#include <type_traits>
#include <utility>

#include "lib/stdcompat/type_traits.h"

namespace pw::async2 {

template <typename T>
class [[nodiscard]] Poll;

struct [[nodiscard]] PendingType;

namespace internal_poll {

using ::cpp20::remove_cvref_t;

// Detects whether `U` has conversion operator to `Poll<T>`, i.e. `operator
// Poll<T>()`.
template <typename T, typename U, typename = void>
struct HasConversionOperatorToPoll : std::false_type {};

template <typename T, typename U>
void test(char (*)[sizeof(std::declval<U>().operator Poll<T>())]);

template <typename T, typename U>
struct HasConversionOperatorToPoll<T, U, decltype(test<T, U>(0))>
    : std::true_type {};

// Detects whether `T` is constructible or convertible from `Poll<U>`.
template <typename T, typename U>
using IsConstructibleOrConvertibleFromPoll =
    std::disjunction<std::is_constructible<T, Poll<U>&>,
                     std::is_constructible<T, const Poll<U>&>,
                     std::is_constructible<T, Poll<U>&&>,
                     std::is_constructible<T, const Poll<U>&&>,
                     std::is_convertible<Poll<U>&, T>,
                     std::is_convertible<const Poll<U>&, T>,
                     std::is_convertible<Poll<U>&&, T>,
                     std::is_convertible<const Poll<U>&&, T>>;

// `UQualified` here may be a type like `const U&` or `U&&`.
// That is, the qualified type of ``U``, not the type "unqualified" ;)
template <typename T, typename UQualified>
using EnableIfImplicitlyConvertible = std::enable_if_t<
    std::conjunction<
        std::negation<std::is_same<T, remove_cvref_t<UQualified>>>,
        std::is_constructible<T, UQualified>,
        std::is_convertible<UQualified, T>,
        std::negation<internal_poll::IsConstructibleOrConvertibleFromPoll<
            T,
            remove_cvref_t<UQualified>>>>::value,
    int>;

// `UQualified` here may be a type like `const U&` or `U&&`.
// That is, the qualified type of ``U``, not the type "unqualified" ;)
template <typename T, typename UQualified>
using EnableIfExplicitlyConvertible = std::enable_if_t<
    std::conjunction<
        std::negation<std::is_same<T, remove_cvref_t<UQualified>>>,
        std::is_constructible<T, UQualified>,
        std::negation<std::is_convertible<UQualified, T>>,
        std::negation<internal_poll::IsConstructibleOrConvertibleFromPoll<
            T,
            remove_cvref_t<UQualified>>>>::value,
    int>;

// Detects whether `T` is constructible or convertible or assignable from
// `Poll<U>`.
template <typename T, typename U>
using IsConstructibleOrConvertibleOrAssignableFromPoll =
    std::disjunction<IsConstructibleOrConvertibleFromPoll<T, U>,
                     std::is_assignable<T&, Poll<U>&>,
                     std::is_assignable<T&, const Poll<U>&>,
                     std::is_assignable<T&, Poll<U>&&>,
                     std::is_assignable<T&, const Poll<U>&&>>;

// Detects whether direct initializing `Poll<T>` from `U` is ambiguous, i.e.
// when `U` is `Poll<V>` and `T` is constructible or convertible from `V`.
template <typename T, typename U>
struct IsDirectInitializationAmbiguous
    : public std::conditional_t<
          std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, U>::value,
          std::false_type,
          IsDirectInitializationAmbiguous<
              T,
              std::remove_cv_t<std::remove_reference_t<U>>>> {};

template <typename T, typename V>
struct IsDirectInitializationAmbiguous<T, Poll<V>>
    : public IsConstructibleOrConvertibleFromPoll<T, V> {};

// Checks against the constraints of the direction initialization, i.e. when
// `Poll<T>::Poll(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsDirectInitializationValid = std::disjunction<
    // Short circuits if T is basically U.
    std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
    std::negation<std::disjunction<
        std::is_same<Poll<T>, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<PendingType, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<std::in_place_t,
                     std::remove_cv_t<std::remove_reference_t<U>>>,
        IsDirectInitializationAmbiguous<T, U>>>>;

template <typename T, typename U>
using EnableIfImplicitlyInitializable = std::enable_if_t<
    std::conjunction<
        internal_poll::IsDirectInitializationValid<T, U&&>,
        std::is_constructible<T, U&&>,
        std::is_convertible<U&&, T>,
        std::disjunction<
            std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
            std::conjunction<
                std::negation<std::is_convertible<U&&, PendingType>>,
                std::negation<
                    internal_poll::HasConversionOperatorToPoll<T, U&&>>>>>::
        value,
    int>;

template <typename T, typename U>
using EnableIfExplicitlyInitializable = std::enable_if_t<
    std::conjunction<
        internal_poll::IsDirectInitializationValid<T, U&&>,
        std::disjunction<
            std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
            std::conjunction<
                std::negation<std::is_constructible<PendingType, U&&>>,
                std::negation<
                    internal_poll::HasConversionOperatorToPoll<T, U&&>>>>,
        std::is_constructible<T, U&&>,
        std::negation<std::is_convertible<U&&, T>>>::value,
    int>;

// This trait detects whether `Poll<T>::operator=(U&&)` is ambiguous, which
// is equivalent to whether all the following conditions are met:
// 1. `U` is `Poll<V>`.
// 2. `T` is constructible and assignable from `V`.
// 3. `T` is constructible and assignable from `U` (i.e. `Poll<V>`).
template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous
    : public std::conditional_t<
          std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, U>::value,
          std::false_type,
          IsForwardingAssignmentAmbiguous<
              T,
              std::remove_cv_t<std::remove_reference_t<U>>>> {};

template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous<T, Poll<U>>
    : public IsConstructibleOrConvertibleOrAssignableFromPoll<T, U> {};

// Checks against the constraints of the forwarding assignment, i.e. whether
// `Poll<T>::operator(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsForwardingAssignmentValid = std::disjunction<
    // Short circuits if T is basically U.
    std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
    std::negation<std::disjunction<
        std::is_same<Poll<T>, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<PendingType, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<std::in_place_t,
                     std::remove_cv_t<std::remove_reference_t<U>>>,
        IsForwardingAssignmentAmbiguous<T, U>>>>;

// This trait is for determining if a given type is a Poll.
template <typename T>
constexpr bool IsPoll = false;
template <typename T>
constexpr bool IsPoll<Poll<T>> = true;

}  // namespace internal_poll
}  // namespace pw::async2
