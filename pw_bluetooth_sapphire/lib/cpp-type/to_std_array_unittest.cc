// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cpp-type/to_std_array.h>
#include <gtest/gtest.h>

#include <type_traits>

namespace bt_lib_cpp_type {
namespace {

TEST(TypeTest, ToStdArray) {
  struct Foo;
  static_assert(std::is_same_v<Foo, ToStdArrayT<Foo>>);
  static_assert(std::is_same_v<int, ToStdArrayT<int>>);
  static_assert(std::is_same_v<std::array<int, 0>, ToStdArrayT<int[]>>);
  static_assert(std::is_same_v<std::array<int, 2>, ToStdArrayT<int[2]>>);
  static_assert(std::is_same_v<std::array<int*, 2>, ToStdArrayT<int* [2]>>);
  static_assert(std::is_same_v<std::array<std::array<int, 3>, 2>,
                               ToStdArrayT<int[2][3]>>);

  constexpr const char kStr[] = "foo";
  static_assert(std::is_same_v<std::array<const char, sizeof(kStr)>,
                               ToStdArrayT<decltype(kStr)>>);
}

}  // namespace
}  // namespace bt_lib_cpp_type
