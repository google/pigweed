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

#include "metrics.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace pw::fuzzer::examples {

// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_unittest]
TEST(MetricsTest, SerializeAndDeserialize) {
  std::array<std::byte, Metrics::kMaxSerializedSize> buffer;

  // Add and copy the names only.
  Metrics src, dst;
  EXPECT_TRUE(src.SetValue("one", 0).ok());
  EXPECT_TRUE(src.SetValue("two", 0).ok());
  EXPECT_TRUE(src.SetValue("three", 0).ok());
  EXPECT_TRUE(dst.SetMetrics(src.GetMetrics()).ok());

  // Modify the values.
  EXPECT_TRUE(src.SetValue("one", 1).ok());
  EXPECT_TRUE(src.SetValue("two", 2).ok());
  EXPECT_TRUE(src.SetValue("three", 3).ok());

  // Transfer the data and check.
  EXPECT_TRUE(src.Serialize(buffer).ok());
  EXPECT_TRUE(dst.Deserialize(buffer).ok());
  EXPECT_EQ(dst.GetValue("one").value_or(0), 1U);
  EXPECT_EQ(dst.GetValue("two").value_or(0), 2U);
  EXPECT_EQ(dst.GetValue("three").value_or(0), 3U);
}

TEST(MetricsTest, DeserializeDoesNotCrash) {
  std::array<std::byte, Metrics::kMaxSerializedSize> buffer;
  std::fill(buffer.begin(), buffer.end(), std::byte(0x5C));

  // Just make sure this does not crash.
  Metrics dst;
  dst.Deserialize(buffer).IgnoreError();
}
// DOCSTAG: [pwfuzzer_examples_fuzztest-metrics_unittest]

}  // namespace pw::fuzzer::examples
