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

/// @file fuzztest.h
/// Pigweed interface to FuzzTest
///
/// @rst
/// This header exposes the portion of the FuzzTest interface that only depends
/// on permitted C++ standard library `headers`_, including `macros`_ and
/// `domains`_.
///
///
/// .. _domains:
///     https://github.com/google/fuzztest/blob/main/doc/domains-reference.md
/// .. _headers: https://pigweed.dev/docs/style_guide.html#permitted-headers
/// .. _macros:
///     https://github.com/google/fuzztest/blob/main/doc/fuzz-test-macro.md
/// @endrst

#include "pw_fuzzer/internal/fuzztest.h"

namespace pw::fuzzer {

/// Produces values for fuzz target function parameters.
///
/// This defines a new template rather than using the `fuzztest` one directly in
/// order to facilitate specializations.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#arbitrary-domains
template <typename T>
auto Arbitrary() {
  return fuzztest::Arbitrary<T>();
}

/// Produces values from a closed interval.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::InRange;

/// Like Arbitrary<T>(), but does not produce the zero value.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::NonZero;

/// Produces numbers greater than zero.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::Positive;

/// Produces the zero value and numbers greater than zero.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::NonNegative;

/// Produces numbers less than zero.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::Negative;

/// Produces the zero value and numbers less than zero.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::NonPositive;

/// Produces floating points numbers that are neither infinity nor NaN.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#numerical-domains
using fuzztest::Finite;

/// Produces any char except '\0'.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::NonZeroChar;

/// Alias for `InRange('0', '9')`.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::NumericChar;

/// Alias for `InRange('a', 'z')`.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::LowerChar;

/// Alias for `InRange('A', 'Z')`.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::UpperChar;

/// Alias for `OneOf(LowerChar(), UpperChar())`.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::AlphaChar;

/// Alias for `OneOf(AlphaChar(), NumericChar())`.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::AlphaNumericChar;

/// Produces printable characters (`InRange<char>(32, 126)`).
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::PrintableAsciiChar;

/// Produces ASCII characters (`InRange<char>(0, 127)`).
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#character-domains
using fuzztest::AsciiChar;

/// Produces values from an enumerated set.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#elementof-domains
using fuzztest::ElementOf;

/// Produces combinations of binary flags from a provided list.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#bitflagcombinationof-domains
using fuzztest::BitFlagCombinationOf;

/// Produces containers of elements provided by inner domains.
///
/// The container type is given by a template parameter.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#container-combinators
using fuzztest::ContainerOf;

/// Produces containers of at least one element provided by inner domains.
///
/// The container type is given by a template parameter.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#container-combinators
using fuzztest::NonEmpty;

/// Produces containers of unique elements provided by inner domains.
///
/// The container type is given by a template parameter.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#container-combinators
using fuzztest::UniqueElementsContainerOf;

/// Produces std::array<T>s of elements provided by inner domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#container-combinators
using fuzztest::ArrayOf;

/// Produces user-defined structs.
///
/// The struct type is given by a template parameter.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#structof
using fuzztest::StructOf;

/// Produces user-defined objects.
///
/// The class type is given by a template parameter.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#constructorof
using fuzztest::ConstructorOf;

/// Produces `std::pair<T1, T2>`s from provided inner domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#pairof
using fuzztest::PairOf;

/// Produces `std::tuple<T...>`s from provided inner domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#tupleof
using fuzztest::TupleOf;

/// Produces `std::variant<T...>` from provided inner domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#variantof
using fuzztest::VariantOf;

/// Produces `std::optional<T>` from provided inner domain.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#optionalof
using fuzztest::OptionalOf;

/// Produces `std::optional<T>` from provided inner domain that is null.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#optionalof
using fuzztest::NullOpt;

/// Produces `std::optional<T>` from provided inner domain that is not null.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#optionalof
using fuzztest::NonNull;

/// Produces values by choosing between provided inner domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#oneof
using fuzztest::OneOf;

/// Produces values equal to the provided value.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#oneof
using fuzztest::Just;

/// Produces values by applying a function to values from provided domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#map
using fuzztest::Map;

/// Creates a domain by applying a function to values from provided domains.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#flatmap
using fuzztest::FlatMap;

/// Produces values from a provided domain that will cause a provided predicate
/// to return true.
///
/// See
/// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md#filter
using fuzztest::Filter;

}  // namespace pw::fuzzer
