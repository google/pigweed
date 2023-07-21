// Copyright 2023 The Pigweed Authors
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

/// This file provide trait types that can be used to check C++ lock-related
/// named requirements: BasicLockable, Lockable, and TimedLockable.

namespace pw::sync {

/// Checks if a type is a basic lock.
///
/// If `Lock` satisfies \em BasicLockable, provides the member constant value
/// equal to true. Otherwise value is false.
///
/// @{
template <typename Lock, typename = void>
struct is_basic_lockable : std::false_type {};

template <typename Lock>
struct is_basic_lockable<Lock,
                         std::void_t<decltype(std::declval<Lock>().lock()),
                                     decltype(std::declval<Lock>().unlock())>>
    : std::true_type {};
/// @}

/// Helper variable template for `is_basic_lockable<Lock>::value`.
template <typename Lock>
inline constexpr bool is_basic_lockable_v = is_basic_lockable<Lock>::value;

/// Checks if a type is a lock.
///
/// If `Lock` satisfies C++'s \em Lockable named requirement, provides the
/// member constant value equal to true. Otherwise value is false.
///
/// @{
template <typename Lock, typename = void>
struct is_lockable : std::false_type {};

template <typename Lock>
struct is_lockable<Lock, std::void_t<decltype(std::declval<Lock>().try_lock())>>
    : is_basic_lockable<Lock> {};
/// @}

/// Helper variable template for `is_lockable<Lock>::value`.
template <typename Lock>
inline constexpr bool is_lockable_v = is_lockable<Lock>::value;

/// Checks if a type is can be locked within a set time.
///
/// If `Lock` has a valid `try_lock_for` method, as described by C++'s
/// \em TimedLockable named requirement, provides the member constant value
/// equal to true. Otherwise value is false.
///
/// @{
template <typename Lock, typename Duration, typename = void>
struct is_lockable_for : std::false_type {};

template <typename Lock, typename Duration>
struct is_lockable_for<Lock,
                       Duration,
                       std::void_t<decltype(std::declval<Lock>().try_lock_for(
                           std::declval<Duration>()))>> : is_lockable<Lock> {};
/// @}

/// Helper variable template for `is_lockable_for<Lock, Duration>::value`.
template <typename Lock, typename Duration>
inline constexpr bool is_lockable_for_v =
    is_lockable_for<Lock, Duration>::value;

/// Checks if a type is can be locked by a set time.
///
/// If `Lock` has a valid `try_lock_until` method, as described by C++'s
/// \em TimedLockable named requirement, provides the member constant value
/// equal to true. Otherwise value is false.
///
/// @{
template <typename Lock, typename TimePoint, typename = void>
struct is_lockable_until : std::false_type {};

template <typename Lock, typename TimePoint>
struct is_lockable_until<
    Lock,
    TimePoint,
    std::void_t<decltype(std::declval<Lock>().try_lock_until(
        std::declval<TimePoint>()))>> : is_lockable<Lock> {};
/// @}

/// Helper variable template for `is_lockable_until<Lock, TimePoint>::value`.
template <typename Lock, typename TimePoint>
inline constexpr bool is_lockable_until_v =
    is_lockable_until<Lock, TimePoint>::value;

/// Checks if a lock type is timed-lockable.
///
/// If `Lock` satisfies C++'s \em TimedLockable named requirement, provides the
/// member constant value equal to true. Otherwise value is false.
template <typename Lock, typename Clock>
struct is_timed_lockable
    : std::integral_constant<
          bool,
          is_lockable_for_v<Lock, typename Clock::duration> &&
              is_lockable_until_v<Lock, typename Clock::time_point>> {};

/// Helper variable template for `is_timed_lockable<Lock, Clock>::value`.
template <typename Lock, typename Clock>
inline constexpr bool is_timed_lockable_v =
    is_timed_lockable<Lock, Clock>::value;

}  // namespace pw::sync
