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

#ifdef PW_BLUETOOTH_SAPPHIRE_INSPECT_ENABLED

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <lib/inspect/cpp/reader.h>

namespace inspect {

// Printers for inspect types.
void PrintTo(const PropertyValue& property, std::ostream* os);
void PrintTo(const NodeValue& node, std::ostream* os);

// Printer for Hierarchy wrapper.
void PrintTo(const Hierarchy& hierarchy, std::ostream* os);

namespace testing {

// Type for a matcher matching a Node.
using NodeMatcher = ::testing::Matcher<const NodeValue&>;

// Type for a matcher matching a vector of properties.
using PropertiesMatcher = ::testing::Matcher<const std::vector<PropertyValue>&>;

// Type for a matcher that matches a base path on an |Hierarchy|.
using PrefixPathMatcher = ::testing::Matcher<const std::vector<std::string>&>;

// Type for a matcher that matches a vector of |Hierarchy| children.
using ChildrenMatcher = ::testing::Matcher<const std::vector<Hierarchy>&>;

namespace internal {

// Matcher interface to check the name of an inspect Nodes.
class NameMatchesMatcher
    : public ::testing::MatcherInterface<const NodeValue&> {
 public:
  NameMatchesMatcher(std::string name);

  bool MatchAndExplain(const NodeValue& obj,
                       ::testing::MatchResultListener* listener) const override;

  void DescribeTo(::std::ostream* os) const override;

  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  std::string name_;
};

// Matcher interface to check the list of Node properties.
class PropertyListMatcher
    : public ::testing::MatcherInterface<const NodeValue&> {
 public:
  PropertyListMatcher(PropertiesMatcher matcher);

  bool MatchAndExplain(const NodeValue& obj,
                       ::testing::MatchResultListener* listener) const override;

  void DescribeTo(::std::ostream* os) const override;

  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  PropertiesMatcher matcher_;
};

}  // namespace internal

// Matches against the name of an Inspect Node.
// Example:
//  EXPECT_THAT(node, NameMatches("objects"));
::testing::Matcher<const NodeValue&> NameMatches(std::string name);

// Matches against the property list of an Inspect Node.
// Example:
//  EXPECT_THAT(node, AllOf(PropertyList(::testing::IsEmpty())));
::testing::Matcher<const NodeValue&> PropertyList(PropertiesMatcher matcher);

// Matches a particular StringProperty with the given name using the given
// matcher.
::testing::Matcher<const PropertyValue&> StringIs(
    const std::string& name, ::testing::Matcher<std::string> matcher);

// Matches a particular ByteVectorProperty with the given name using the given
// matcher.
::testing::Matcher<const PropertyValue&> ByteVectorIs(
    const std::string& name, ::testing::Matcher<std::vector<uint8_t>> matcher);

// Matches a particular IntProperty with the given name using the given matcher.
::testing::Matcher<const PropertyValue&> IntIs(
    const std::string& name, ::testing::Matcher<int64_t> matcher);

// Matches a particular UintProperty with the given name using the given
// matcher.
::testing::Matcher<const PropertyValue&> UintIs(
    const std::string& name, ::testing::Matcher<uint64_t> matcher);

// Matches a particular DoubleProperty with the given name using the given
// matcher.
::testing::Matcher<const PropertyValue&> DoubleIs(
    const std::string& name, ::testing::Matcher<double> matcher);

// Matches a particular BoolProperty with the given name using the given
// matcher.
::testing::Matcher<const PropertyValue&> BoolIs(
    const std::string& name, ::testing::Matcher<bool> matcher);

// Matches the values of an integer array.
::testing::Matcher<const PropertyValue&> IntArrayIs(
    const std::string& name, ::testing::Matcher<std::vector<int64_t>>);

// Matches the values of an unsigned integer array.
::testing::Matcher<const PropertyValue&> UintArrayIs(
    const std::string& name, ::testing::Matcher<std::vector<uint64_t>>);

// Matches the values of a double width floating point number array.
::testing::Matcher<const PropertyValue&> DoubleArrayIs(
    const std::string& name, ::testing::Matcher<std::vector<double>>);

// Matches the display format of a numeric array value.
::testing::Matcher<const PropertyValue&> ArrayDisplayFormatIs(
    ArrayDisplayFormat format);

// Matcher for the object inside an Hierarchy.
::testing::Matcher<const Hierarchy&> NodeMatches(NodeMatcher matcher);

// Matcher for the base path inside an Hierarchy.
::testing::Matcher<const Hierarchy&> PrefixPathMatches(
    PrefixPathMatcher matcher);

// Matcher for the children of the object in an Hierarchy.
::testing::Matcher<const Hierarchy&> ChildrenMatch(ChildrenMatcher matcher);

}  // namespace testing
}  // namespace inspect

#endif  // PW_BLUETOOTH_SAPPHIRE_INSPECT_ENABLED
