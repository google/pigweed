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

#include <algorithm>

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
#include "metrics.h"

namespace pw_fuzzer::examples {

using namespace fuzztest;
using ValueMap = std::unordered_map<std::string, Metrics::Value>;

void ArbitraryMarshalAndUnmarshal(const ValueMap& values) {
  std::array<std::byte, 100> buffer;

  Metrics src;
  for (const auto& [name, value] : values) {
    src.SetValue(name, value);
  }
  size_t num = src.Marshal(buffer);
  if (num > buffer.size()) {
    return;
  }

  Metrics dst;
  dst.SetKeys(src.GetKeys());
  EXPECT_TRUE(dst.Unmarshal(buffer));
  for (const auto& [name, expected] : values) {
    auto actual = dst.GetValue(name);
    EXPECT_TRUE(actual);
    EXPECT_EQ(*actual, expected);
  }
}

// This unit test will run on host and may run on target devices (if supported).
TEST(MetricsTest, MarshalAndUnmarshal) {
  ValueMap values;
  values["one"] = 1;
  values["two"] = 2;
  values["three"] = 3;
  ArbitraryMarshalAndUnmarshal(values);
}

// This fuzz test will only run on host.
FUZZ_TEST(MetricsTest, ArbitraryMarshalAndUnmarshal)
    .WithDomains(UnorderedMapOf(PrintableAsciiString(),
                                Arbitrary<Metrics::Value>())
                     .WithMaxSize(15));

void ArbitraryUnmarshal(pw::ConstByteSpan buffer) {
  // Just make sure this does not crash.
  Metrics dst;
  dst.Unmarshal(buffer);
}

// This unit test will run on host and may run on target devices (if supported).
TEST(MetricsTest, UnmarshalDoesNotCrash) {
  ArbitraryUnmarshal(std::vector<std::byte>(100, std::byte(0x5C)));
}

// This fuzz test will only run on host.
FUZZ_TEST(MetricsTest, ArbitraryUnmarshal)
    .WithDomains(VectorOf(Arbitrary<std::byte>()));

}  // namespace pw_fuzzer::examples
