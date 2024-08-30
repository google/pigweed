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

#ifdef PW_BLUETOOTH_SAPPHIRE_INSPECT_ENABLED

#include "lib/inspect/testing/cpp/inspect.h"

using inspect::NodeValue;
using inspect::PropertyValue;

namespace {}  // namespace

namespace inspect {

void PrintTo(const PropertyValue& property, std::ostream* os) {
  if (property.format() == PropertyFormat::kInt)
    *os << "Int";
  if (property.format() == PropertyFormat::kUint)
    *os << "Uint";
  if (property.format() == PropertyFormat::kDouble)
    *os << "Double";
  if (property.format() == PropertyFormat::kBool)
    *os << "Bool";
  if (property.format() == PropertyFormat::kIntArray)
    *os << "IntArray";
  if (property.format() == PropertyFormat::kUintArray)
    *os << "UintArray";
  if (property.format() == PropertyFormat::kDoubleArray)
    *os << "DoubleArray";
  if (property.format() == PropertyFormat::kString)
    *os << "String";
  if (property.format() == PropertyFormat::kBytes)
    *os << "ByteVector";
  *os << "Property(" << ::testing::PrintToString(property.name()) << ", ";
  if (property.format() == PropertyFormat::kInt)
    *os << ::testing::PrintToString(property.Get<IntPropertyValue>().value());
  if (property.format() == PropertyFormat::kUint)
    *os << ::testing::PrintToString(property.Get<UintPropertyValue>().value());
  if (property.format() == PropertyFormat::kDouble)
    *os << ::testing::PrintToString(
        property.Get<DoublePropertyValue>().value());
  if (property.format() == PropertyFormat::kBool)
    *os << ::testing::PrintToString(property.Get<BoolPropertyValue>().value());
  if (property.format() == PropertyFormat::kIntArray)
    *os << ::testing::PrintToString(property.Get<IntArrayValue>().value());
  if (property.format() == PropertyFormat::kUintArray)
    *os << ::testing::PrintToString(property.Get<UintArrayValue>().value());
  if (property.format() == PropertyFormat::kDoubleArray)
    *os << ::testing::PrintToString(property.Get<DoubleArrayValue>().value());
  if (property.format() == PropertyFormat::kString)
    *os << ::testing::PrintToString(
        property.Get<StringPropertyValue>().value());
  if (property.format() == PropertyFormat::kBytes)
    *os << ::testing::PrintToString(
        property.Get<ByteVectorPropertyValue>().value());
  *os << ")";
}

void PrintTo(const NodeValue& node, std::ostream* os) {
  *os << "Node(" << ::testing::PrintToString(node.name()) << ", "
      << ::testing::PrintToString(node.properties().size()) << " properties)";
}

void PrintTo(const ::inspect::Hierarchy& hierarchy, std::ostream* os) {
  *os << "Hierarchy(" << ::testing::PrintToString(hierarchy.node()) << ", "
      << ::testing::PrintToString(hierarchy.children().size()) << " children)";
}

namespace testing {

internal::NameMatchesMatcher::NameMatchesMatcher(std::string name)
    : name_(std::move(name)) {}

bool internal::NameMatchesMatcher::MatchAndExplain(
    const NodeValue& node, ::testing::MatchResultListener* listener) const {
  if (node.name() != name_) {
    *listener << "expected name \"" << name_ << "\" but found \"" << node.name()
              << "\"";
    return false;
  }
  return true;
}

void internal::NameMatchesMatcher::DescribeTo(::std::ostream* os) const {
  *os << "name matches \"" << name_ << "\"";
}

void internal::NameMatchesMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << "name does not match \"" << name_ << "\"";
}

internal::PropertyListMatcher::PropertyListMatcher(PropertiesMatcher matcher)
    : matcher_(std::move(matcher)) {}

bool internal::PropertyListMatcher::MatchAndExplain(
    const NodeValue& node, ::testing::MatchResultListener* listener) const {
  return ::testing::ExplainMatchResult(matcher_, node.properties(), listener);
}

void internal::PropertyListMatcher::DescribeTo(::std::ostream* os) const {
  *os << "property list ";
  matcher_.DescribeTo(os);
}

void internal::PropertyListMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << "property list ";
  matcher_.DescribeNegationTo(os);
}

::testing::Matcher<const NodeValue&> NameMatches(std::string name) {
  return ::testing::MakeMatcher(
      new internal::NameMatchesMatcher(std::move(name)));
}

::testing::Matcher<const NodeValue&> PropertyList(PropertiesMatcher matcher) {
  return ::testing::MakeMatcher(
      new internal::PropertyListMatcher(std::move(matcher)));
}

::testing::Matcher<const PropertyValue&> StringIs(
    const std::string& name, ::testing::Matcher<std::string> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kString),
      ::testing::Property(&PropertyValue::Get<StringPropertyValue>,
                          ::testing::Property(&StringPropertyValue::value,
                                              std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> ByteVectorIs(
    const std::string& name, ::testing::Matcher<std::vector<uint8_t>> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kBytes),
      ::testing::Property(&PropertyValue::Get<ByteVectorPropertyValue>,
                          ::testing::Property(&ByteVectorPropertyValue::value,
                                              std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> IntIs(
    const std::string& name, ::testing::Matcher<int64_t> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kInt),
      ::testing::Property(
          &PropertyValue::Get<IntPropertyValue>,
          ::testing::Property(&IntPropertyValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> UintIs(
    const std::string& name, ::testing::Matcher<uint64_t> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kUint),
      ::testing::Property(
          &PropertyValue::Get<UintPropertyValue>,
          ::testing::Property(&UintPropertyValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> DoubleIs(
    const std::string& name, ::testing::Matcher<double> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kDouble),
      ::testing::Property(&PropertyValue::Get<DoublePropertyValue>,
                          ::testing::Property(&DoublePropertyValue::value,
                                              std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> BoolIs(
    const std::string& name, ::testing::Matcher<bool> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kBool),
      ::testing::Property(
          &PropertyValue::Get<BoolPropertyValue>,
          ::testing::Property(&BoolPropertyValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> IntArrayIs(
    const std::string& name, ::testing::Matcher<std::vector<int64_t>> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kIntArray),
      ::testing::Property(
          &PropertyValue::Get<IntArrayValue>,
          ::testing::Property(&IntArrayValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> UintArrayIs(
    const std::string& name,
    ::testing::Matcher<std::vector<uint64_t>> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kUintArray),
      ::testing::Property(
          &PropertyValue::Get<UintArrayValue>,
          ::testing::Property(&UintArrayValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> DoubleArrayIs(
    const std::string& name, ::testing::Matcher<std::vector<double>> matcher) {
  return ::testing::AllOf(
      ::testing::Property(&PropertyValue::name, ::testing::StrEq(name)),
      ::testing::Property(&PropertyValue::format, PropertyFormat::kDoubleArray),
      ::testing::Property(
          &PropertyValue::Get<DoubleArrayValue>,
          ::testing::Property(&DoubleArrayValue::value, std::move(matcher))));
}

::testing::Matcher<const PropertyValue&> ArrayDisplayFormatIs(
    ArrayDisplayFormat format) {
  return ::testing::AnyOf(
      ::testing::AllOf(
          ::testing::Property(&PropertyValue::format,
                              PropertyFormat::kIntArray),
          ::testing::Property(
              &PropertyValue::Get<IntArrayValue>,
              ::testing::Property(&IntArrayValue::GetDisplayFormat, format))),
      ::testing::AllOf(
          ::testing::Property(&PropertyValue::format,
                              PropertyFormat::kUintArray),
          ::testing::Property(
              &PropertyValue::Get<UintArrayValue>,
              ::testing::Property(&UintArrayValue::GetDisplayFormat, format))),
      ::testing::AllOf(::testing::Property(&PropertyValue::format,
                                           PropertyFormat::kDoubleArray),
                       ::testing::Property(
                           &PropertyValue::Get<DoubleArrayValue>,
                           ::testing::Property(
                               &DoubleArrayValue::GetDisplayFormat, format))));
}

::testing::Matcher<const Hierarchy&> NodeMatches(NodeMatcher matcher) {
  return ::testing::Property(&Hierarchy::node, std::move(matcher));
}

::testing::Matcher<const Hierarchy&> ChildrenMatch(ChildrenMatcher matcher) {
  return ::testing::Property(&Hierarchy::children, std::move(matcher));
}

}  // namespace testing
}  // namespace inspect
#endif  // PW_BLUETOOTH_SAPPHIRE_INSPECT_ENABLED
