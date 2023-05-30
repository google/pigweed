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

#include <array>
#include <cstddef>
#include <new>
#include <type_traits>

namespace pw::containers::internal {

// Used as max_size in generic-sized interfaces using RawStorage<T>.
inline constexpr size_t kGenericSized = static_cast<size_t>(-1);

template <typename InputIterator>
using EnableIfInputIterator = std::enable_if_t<std::is_convertible<
    typename std::iterator_traits<InputIterator>::iterator_category,
    std::input_iterator_tag>::value>;

template <typename T>
using EnableIfIterable =
    std::enable_if_t<true, decltype(T().begin(), T().end())>;

// The DestructorHelper is used to make a Container<T> trivially destructible if
// T is. This could be replaced with a C++20 constraint.
template <typename Container, bool kIsTriviallyDestructible>
class DestructorHelper;

template <typename Container>
class DestructorHelper<Container, true> {
 public:
  ~DestructorHelper() = default;
};

template <typename Container>
class DestructorHelper<Container, false> {
 public:
  ~DestructorHelper() { static_cast<Container*>(this)->clear(); }
};

// Container similar to std::array that provides an array of Elements which are
// stored as uninitialized memory blocks aligned correctly for the type.
//
// The caller is responsible for constructing, accessing, and destructing
// elements. In addition, the caller is responsible for element access and all
// associated bounds checking.
template <typename ValueType, size_t kCapacity>
class RawStorage {
 public:
  using value_type = ValueType;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  // Construct
  constexpr RawStorage() noexcept : null_bits_() {}

  // Do not permit copying and move for now.
  RawStorage(const RawStorage&) = delete;
  RawStorage& operator=(const RawStorage&) = delete;
  RawStorage(RawStorage&&) = delete;
  RawStorage&& operator=(RawStorage&&) = delete;

  pointer data() noexcept {
    return std::launder(reinterpret_cast<pointer>(&bytes_));
  }
  const_pointer data() const noexcept {
    return std::launder(reinterpret_cast<const_pointer>(&bytes_));
  }

  constexpr size_type size() const noexcept { return max_size(); }
  constexpr size_type max_size() const noexcept { return kCapacity; }

 private:
  struct Empty {};

  union {
    // Elements must initialized on demand with placement new, this array
    // provides the right amount of storage needed for the elements, however it
    // is never constructed nor destructed by the RawStorage container, as
    // null_bits_ is used instead.
    //
    // This uses std::array instead of a C array to support zero-length arrays.
    // Zero-length C arrays are non-standard, but std::array<T, 0> is valid.
    // The alignas specifier is required ensure that a zero-length array is
    // aligned the same as an array with elements.
    alignas(value_type)
        std::array<std::byte, sizeof(value_type) * kCapacity> bytes_;

    // Empty struct used when initializing the storage in the constexpr
    // constructor.
    Empty null_bits_;
  };
};

}  // namespace pw::containers::internal
