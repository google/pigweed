// Copyright 2025 The Pigweed Authors
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

#include <ranges>

#include "pw_rpc/benchmark.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

bool DataEqual(pw::ConstByteSpan s1, pw::ConstByteSpan s2) {
  return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end());
}

TEST(BenchmarkService, Benchmark_UnaryEchoRequestMessage) {
  PW_RAW_TEST_METHOD_CONTEXT(BenchmarkService, UnaryEcho) context;
  auto msg = pw::bytes::Array<0x12, 0x34, 0x56, 0x78>();

  context.call(msg);
  EXPECT_EQ(OkStatus(), context.status());
  // std::ranges::equal is only supported in C++20 and later.
  ASSERT_TRUE(DataEqual(context.response(), msg));
}

TEST(BenchmarkService, Benchmark_EmptyRequest) {
  PW_RAW_TEST_METHOD_CONTEXT(BenchmarkService, UnaryEcho) context;
  std::vector<std::byte> msg = {};

  context.call(msg);
  EXPECT_EQ(OkStatus(), context.status());
  // std::ranges::equal is only supported in C++20 and later.
  ASSERT_TRUE(DataEqual(context.response(), msg));
}

}  // namespace
}  // namespace pw::rpc
