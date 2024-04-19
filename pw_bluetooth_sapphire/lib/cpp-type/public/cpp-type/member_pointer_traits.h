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

#include <cstddef>
#include <memory>
#include <type_traits>

namespace bt_lib_cpp_type {

// Given a pointer-to-data-member, declare public types ClassType and MemberType
// deduced from the type of the pointer. Note that the template argument is a
// pointer, not a type. Example:
//   using MemberType = typename MemberPointerTraits<&Foo::bar>::MemberType; //
//   OK using MemberType = typename
//   MemberPointerTraits<decltype(&Foo::bar)>::MemberType;  // Wrong
template <auto>
class MemberPointerTraits;  // undefined base case for
                            // non-pointers-to-data-member

template <typename ClassT, typename MemberT, MemberT ClassT::* PointerToMember>
class MemberPointerTraits<PointerToMember> {
 public:
  // For class InnerClass declared within OuterClass, this will be deduced as
  // InnerClass.
  using ClassType = std::remove_cv_t<ClassT>;
  using MemberType = MemberT;

  // Returns the offset of MemberT within the layout of ClassT.
  // This is necessary because pointers-to-data-member can not be used with the
  // offsetof macro (see http://wg21.link/p0908r0 for a description and proposed
  // standard fix). Note this constructs and value-initializes an object of
  // ClassT, so care should be taken if that has side effects.
  static size_t constexpr offset() {
    // Pointer subtraction is only defined for array objects per ISO/IEC
    // 14882:2017(E) § 8.7 [expr.add] ¶ 2.5, so we have to actually construct an
    // object rather than use a nullptr pointer or std::declval (whose rules
    // disallow odr-use).
    static_assert(std::is_standard_layout_v<ClassType>);

    // This assumes that the layout of the ClassT temporary construct is the
    // same as that of ClassT objects elsewhere, an assumption that is likely UB
    // as only the first member of ClassT is guaranteed (by [class.mem] ¶ 17) to
    // be pointer-interconvertible with ClassT. Subsequent members are only
    // guaranteed to have increasing addresses ([class.mem] ¶ 24), so it's not
    // impossible that a compiler may use different padding around members that
    // are not accessed in this function than it uses elsewhere. For now, it's
    // safest to use this _only_ with
    // [[gnu::packed]] classes.
    const ClassType
        temporary{};  // Value initialization in case of const members
    return size_t(&(temporary.*PointerToMember)) - size_t(&temporary);
  }
};

}  // namespace bt_lib_cpp_type
