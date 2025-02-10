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

#include "pw_build/linker_symbol.h"

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

// These symbols are defined in linker_symbol_test.ld
extern "C" LinkerSymbol<int> FOO_SYM;
extern "C" LinkerSymbol BAR_SYM;  // Test template default (uintptr_t)
extern "C" LinkerSymbol<int> NEGATIVE_SYM;
extern "C" LinkerSymbol<char> CHAR_SYM;

enum class MyEnum {
  kValue7 = 7,
};
extern "C" LinkerSymbol<MyEnum> ENUM_SYM;

TEST(LinkerSymbolTest, ValueWorks) {
  // You can use value() to get the value as the specified type.
  auto value = FOO_SYM.value();
  static_assert(std::is_same_v<decltype(value), int>);
  EXPECT_EQ(value, 42);
}

TEST(LinkerSymbolTest, NegativeValueWorks) {
  // LinkerSymbol works with negative integers.
  EXPECT_EQ(NEGATIVE_SYM.value(), -567);
}

TEST(LinkerSymbolTest, CharValueWorks) {
  // LinkerSymbol works with characters.
  EXPECT_EQ(CHAR_SYM.value(), 'a');
}

TEST(LinkerSymbolTest, EnumValueWorks) {
  // LinkerSymbol works with enums.
  EXPECT_EQ(ENUM_SYM.value(), MyEnum::kValue7);
}

TEST(LinkerSymbolTest, ValueWorksDefaultType) {
  // You can use value() to get the value as the default type (uintptr_t).
  auto value = BAR_SYM.value();
  static_assert(std::is_same_v<decltype(value), uintptr_t>);
  EXPECT_EQ(value, 0xDEADBEEFu);
}

TEST(LinkerSymbolTest, CStyleCastWorks) {
  // You can use a C-style cast, if you insist.
  EXPECT_EQ((uintptr_t)&FOO_SYM, 42u);
}

}  // namespace
}  // namespace pw
