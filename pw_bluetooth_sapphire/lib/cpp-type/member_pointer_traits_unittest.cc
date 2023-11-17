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

#include <cpp-type/member_pointer_traits.h>
#include <gtest/gtest.h>

#include <type_traits>

namespace bt_lib_cpp_type {
namespace {

TEST(TypeTest, MemberPointerTraits) {
  struct Foo {
    bool scalar_bool_member;
    double scalar_double_member;
    double array_double_member[1];
    struct InnerStruct {
      int nested_scalar_int_member;
    } struct_member;
  };

  static_assert(
      std::is_same_v<Foo,
                     MemberPointerTraits<&Foo::scalar_bool_member>::ClassType>);
  static_assert(std::is_same_v<
                Foo,
                MemberPointerTraits<&Foo::scalar_double_member>::ClassType>);
  static_assert(std::is_same_v<
                Foo,
                MemberPointerTraits<&Foo::array_double_member>::ClassType>);
  static_assert(
      std::is_same_v<Foo, MemberPointerTraits<&Foo::struct_member>::ClassType>);
  static_assert(std::is_same_v<
                Foo::InnerStruct,
                MemberPointerTraits<
                    &Foo::InnerStruct::nested_scalar_int_member>::ClassType>);

  static_assert(std::is_same_v<
                bool,
                MemberPointerTraits<&Foo::scalar_bool_member>::MemberType>);
  static_assert(std::is_same_v<
                double,
                MemberPointerTraits<&Foo::scalar_double_member>::MemberType>);
  static_assert(std::is_same_v<
                double[1],
                MemberPointerTraits<&Foo::array_double_member>::MemberType>);
  static_assert(
      std::is_same_v<Foo::InnerStruct,
                     MemberPointerTraits<&Foo::struct_member>::MemberType>);
  static_assert(std::is_same_v<
                int,
                MemberPointerTraits<
                    &Foo::InnerStruct::nested_scalar_int_member>::MemberType>);

  EXPECT_EQ(offsetof(Foo, scalar_bool_member),
            MemberPointerTraits<&Foo::scalar_bool_member>::offset());
  EXPECT_EQ(offsetof(Foo, scalar_double_member),
            MemberPointerTraits<&Foo::scalar_double_member>::offset());
  EXPECT_EQ(offsetof(Foo, array_double_member),
            MemberPointerTraits<&Foo::array_double_member>::offset());
  EXPECT_EQ(offsetof(Foo, struct_member),
            MemberPointerTraits<&Foo::struct_member>::offset());

  // offsetof can be used with non-anonymous nested members because it refers to
  // member identifiers, not types.
  EXPECT_LT(0U, offsetof(Foo, struct_member.nested_scalar_int_member));

  // |Foo::InnerStruct| is the qualified name of a type so
  // |nested_scalar_int_member|'s offset within it is 0.
  EXPECT_EQ(0U,
            MemberPointerTraits<
                &Foo::InnerStruct::nested_scalar_int_member>::offset());
}

}  // namespace
}  // namespace bt_lib_cpp_type
