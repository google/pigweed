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

#include <limits>
#include <string>  // for std::char_traits
#include <string_view>
#include <type_traits>

#include "pw_assert/assert.h"

namespace pw::string_impl {

// pw::InlineString<>::size_type is unsigned short so the capacity and current
// size fit into a single word.
using size_type = unsigned short;

// Reserved capacity that is used to represent a generic-length
// pw::InlineString.
inline constexpr size_t kGeneric = size_type(-1);

template <typename T>
inline constexpr bool kUseStdCharTraits =
#if !defined(__cpp_lib_constexpr_string) || __cpp_lib_constexpr_string < 201907L
    false &&
#endif  // __cpp_lib_constexpr_string
    !std::is_same_v<T, std::byte>;

// Provide a minimal custom traits class for use by std::byte and if
// std::char_traits is not yet fully constexpr (__cpp_lib_constexpr_string).
template <typename T, bool = kUseStdCharTraits<T>>
struct char_traits {
  using char_type = T;
  using int_type = unsigned int;

  static constexpr void assign(T& dest, const T& source) noexcept {
    dest = source;
  }

  static constexpr T* assign(T* dest, size_t count, T value) {
    for (size_t i = 0; i < count; ++i) {
      dest[i] = value;
    }
    return dest;
  }

  static constexpr bool eq(T lhs, T rhs) { return lhs == rhs; }

  static constexpr T* move(T* dest, const T* source, size_t count) {
    if (dest < source) {
      char_traits<T>::copy(dest, source, count);
    } else if (source < dest) {
      for (size_t i = count; i != 0; --i) {
        char_traits<T>::assign(dest[i - 1], source[i - 1]);
      }
    }
    return dest;
  }

  static constexpr T* copy(T* dest, const T* source, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      char_traits<T>::assign(dest[i], source[i]);
    }
    return dest;
  }

  static constexpr int compare(const T* lhs, const T* rhs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      if (lhs[i] < rhs[i]) {
        return -1;
      }
      if (rhs[i] < lhs[i]) {
        return 1;
      }
    }
    return 0;
  }
};

// Use std::char_traits for character types when it fully supports constexpr.
template <typename T>
struct char_traits<T, true> : public std::char_traits<T> {};

// string_views for byte strings need to use Pigweed's custom char_traits since
// std::char_traits<std::byte> is not defined. (Alternately, could specialize
// std::char_traits, but this is simpler.) basic_string_view<byte> won't be
// commonly used (byte spans are more common), but support it for completeness.
template <typename T>
struct StringViewType {
  using type = std::basic_string_view<T>;
};

template <>
struct StringViewType<std::byte> {
  using type = std::basic_string_view<std::byte, char_traits<std::byte>>;
};

template <typename T>
using View = typename StringViewType<T>::type;

// Aliases for disabling overloads with SFINAE.
template <typename CharType, typename T>
using EnableIfNonArrayCharPointer = std::enable_if_t<
    std::is_pointer<T>::value && !std::is_array<T>::value &&
    std::is_same<CharType, std::remove_cv_t<std::remove_pointer_t<T>>>::value>;

template <typename T>
using EnableIfInputIterator = std::enable_if_t<
    std::is_convertible<typename std::iterator_traits<T>::iterator_category,
                        std::input_iterator_tag>::value>;

template <typename CharType, typename T>
using EnableIfStringViewLike =
    std::enable_if_t<std::is_convertible<const T&, View<CharType>>() &&
                     !std::is_convertible<const T&, const CharType*>()>;

template <typename CharType, typename T>
using EnableIfStringViewLikeButNotStringView =
    std::enable_if_t<!std::is_same<T, View<CharType>>() &&
                     std::is_convertible<const T&, View<CharType>>() &&
                     !std::is_convertible<const T&, const CharType*>()>;

// Used in static_asserts to check that a C array fits in an InlineString.
constexpr bool NullTerminatedArrayFitsInString(
    size_t null_terminated_array_size, size_t capacity) {
  return null_terminated_array_size > 0u &&
         null_terminated_array_size - 1 <= capacity &&
         null_terminated_array_size - 1 < kGeneric;
}

// Used to safely convert various numeric types to `size_type`.
template <typename T>
constexpr size_type CheckedCastToSize(T num) {
  static_assert(std::is_unsigned<T>::value,
                "Attempted to convert signed value to string length, but only "
                "unsigned types are allowed.");
  PW_ASSERT(num < std::numeric_limits<size_type>::max());
  return static_cast<size_type>(num);
}

// Constexpr utility functions for pw::InlineString. These are NOT intended for
// general use. These mostly map directly to general purpose standard library
// utilities that are not constexpr until C++20.

// Calculates the length of a C string up to the capacity. Returns capacity + 1
// if the string is longer than the capacity. This replaces
// std::char_traits<T>::length, which is unbounded. The string must contain at
// least one character.
template <typename T>
constexpr size_t BoundedStringLength(const T* string, size_t capacity) {
  size_t length = 0;
  for (; length <= capacity; ++length) {
    if (char_traits<T>::eq(string[length], T())) {
      break;
    }
  }
  return length;  // length is capacity + 1 if T() was not found.
}

// As with std::string, InlineString treats literals and character arrays as
// null-terminated strings. ArrayStringLength checks that the array size fits
// within size_t and asserts if no null terminator was found in the array.
template <typename T>
constexpr size_t ArrayStringLength(const T* array,
                                   size_t max_string_length,
                                   size_t capacity) {
  const size_t max_length = std::min(max_string_length, capacity);
  const size_t length = BoundedStringLength(array, max_length);
  PW_ASSERT(length <= max_string_length);  // The array is not null terminated
  return length;
}

template <typename T, size_t kCharArraySize>
constexpr size_t ArrayStringLength(const T (&array)[kCharArraySize],
                                   size_t capacity) {
  static_assert(kCharArraySize > 0u, "C arrays cannot have a length of 0");
  static_assert(kCharArraySize - 1 < kGeneric,
                "The size of this literal or character array is too large "
                "for pw::InlineString<>::size_t");
  return ArrayStringLength(
      array, static_cast<size_t>(kCharArraySize - 1), capacity);
}

// Constexpr version of std::copy that returns the number of copied characters.
// Does NOT null-terminate the string.
template <typename InputIterator, typename T>
constexpr size_t IteratorCopy(InputIterator begin,
                              InputIterator end,
                              T* const string_begin,
                              const T* const string_end) {
  T* current_position = string_begin;

  // If `InputIterator` is a `LegacyRandomAccessIterator`, the bounds check can
  // be done up front, allowing the compiler more flexibility in optimizing the
  // loop.
  using category =
      typename std::iterator_traits<InputIterator>::iterator_category;
  if constexpr (std::is_same_v<category, std::random_access_iterator_tag>) {
    PW_ASSERT(begin <= end);
    PW_ASSERT(end - begin <= string_end - string_begin);
    for (InputIterator it = begin; it != end; ++it) {
      char_traits<T>::assign(*current_position++, *it);
    }
  } else {
    for (InputIterator it = begin; it != end; ++it) {
      PW_ASSERT(current_position != string_end);
      char_traits<T>::assign(*current_position++, *it);
    }
  }
  return static_cast<size_t>(current_position - string_begin);
}

// Constexpr lexicographical comparison.
template <typename T>
constexpr int Compare(const T* lhs,
                      size_t lhs_size,
                      const T* rhs,
                      size_t rhs_size) noexcept {
  int result = char_traits<T>::compare(lhs, rhs, std::min(lhs_size, rhs_size));
  if (result != 0 || lhs_size == rhs_size) {
    return result;
  }
  return lhs_size < rhs_size ? -1 : 1;
}

}  // namespace pw::string_impl
