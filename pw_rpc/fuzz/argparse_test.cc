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

#include "pw_rpc/fuzz/argparse.h"

#include <cstdint>
#include <limits>

#include "gtest/gtest.h"
#include "pw_string/string_builder.h"

namespace pw::rpc::fuzz {
namespace {

TEST(ArgsParseTest, ParseBoolFlag) {
  auto parser1 = BoolParser("-t", "--true").set_default(true);
  auto parser2 = BoolParser("-f").set_default(false);
  EXPECT_TRUE(parser1.value());
  EXPECT_FALSE(parser2.value());

  EXPECT_EQ(parser1.Parse("-t"), ParseStatus::kParsedOne);
  EXPECT_EQ(parser2.Parse("-t"), ParseStatus::kParseMismatch);
  EXPECT_TRUE(parser1.value());
  EXPECT_FALSE(parser2.value());

  EXPECT_EQ(parser1.Parse("--true"), ParseStatus::kParsedOne);
  EXPECT_EQ(parser2.Parse("--true"), ParseStatus::kParseMismatch);
  EXPECT_TRUE(parser1.value());
  EXPECT_FALSE(parser2.value());

  EXPECT_EQ(parser1.Parse("--no-true"), ParseStatus::kParsedOne);
  EXPECT_EQ(parser2.Parse("--no-true"), ParseStatus::kParseMismatch);
  EXPECT_FALSE(parser1.value());
  EXPECT_FALSE(parser2.value());

  EXPECT_EQ(parser1.Parse("-f"), ParseStatus::kParseMismatch);
  EXPECT_EQ(parser2.Parse("-f"), ParseStatus::kParsedOne);
  EXPECT_FALSE(parser1.value());
  EXPECT_TRUE(parser2.value());
}

template <typename T>
void ParseUnsignedFlag() {
  auto parser = UnsignedParser<T>("-u", "--unsigned").set_default(137);
  EXPECT_EQ(parser.value(), 137u);

  // Wrong name.
  EXPECT_EQ(parser.Parse("-s"), ParseStatus::kParseMismatch);
  EXPECT_EQ(parser.Parse("--signed"), ParseStatus::kParseMismatch);
  EXPECT_EQ(parser.value(), 137u);

  // Missing values.
  EXPECT_EQ(parser.Parse("-u"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.Parse("--unsigned"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.value(), 137u);

  // Non-numeric values.
  EXPECT_EQ(parser.Parse("-u", "foo"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.Parse("--unsigned", "bar"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.value(), 137u);

  // Minimum values.
  EXPECT_EQ(parser.Parse("-u", "0"), ParseStatus::kParsedTwo);
  EXPECT_EQ(parser.Parse("--unsigned", "0"), ParseStatus::kParsedTwo);
  EXPECT_EQ(parser.value(), 0u);

  // Maximum values.
  T max = std::numeric_limits<T>::max();
  StringBuffer<32> buf;
  buf << max;
  EXPECT_EQ(parser.Parse("-u", buf.c_str()), ParseStatus::kParsedTwo);
  EXPECT_EQ(parser.value(), max);
  EXPECT_EQ(parser.Parse("--unsigned", buf.c_str()), ParseStatus::kParsedTwo);
  EXPECT_EQ(parser.value(), max);

  // Out of-range value.
  if (max < std::numeric_limits<uint64_t>::max()) {
    buf.clear();
    buf << (max + 1ULL);
    EXPECT_EQ(parser.Parse("-u", buf.c_str()), ParseStatus::kParseFailure);
    EXPECT_EQ(parser.Parse("--unsigned", buf.c_str()),
              ParseStatus::kParseFailure);
    EXPECT_EQ(parser.value(), max);
  }
}

TEST(ArgsParseTest, ParseUnsignedFlags) {
  ParseUnsignedFlag<uint8_t>();
  ParseUnsignedFlag<uint16_t>();
  ParseUnsignedFlag<uint32_t>();
  ParseUnsignedFlag<uint64_t>();
}

TEST(ArgsParseTest, ParsePositional) {
  auto parser = UnsignedParser<size_t>("positional").set_default(1);
  EXPECT_EQ(parser.Parse("-p", "2"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.value(), 1u);

  EXPECT_EQ(parser.Parse("--positional", "2"), ParseStatus::kParseFailure);
  EXPECT_EQ(parser.value(), 1u);

  // Second arg is ignored..
  EXPECT_EQ(parser.Parse("2", "3"), ParseStatus::kParsedOne);
  EXPECT_EQ(parser.value(), 2u);

  // Positional only matches once.
  EXPECT_EQ(parser.Parse("3"), ParseStatus::kParseMismatch);
  EXPECT_EQ(parser.value(), 2u);
}

TEST(ArgsParseTest, PrintUsage) {
  // Just verify it compiles and runs.
  Vector<ArgParserVariant, 3> parsers = {
      BoolParser("-v", "--verbose").set_default(false),
      UnsignedParser<size_t>("-r", "--runs").set_default(1000),
      UnsignedParser<size_t>("port").set_default(11111),
  };
  PrintUsage(parsers, "test-bin");
}

void CheckArgs(Vector<ArgParserVariant>& parsers,
               bool verbose,
               size_t runs,
               uint16_t port) {
  bool actual_verbose = false;
  EXPECT_EQ(GetArg(parsers, "--verbose", &actual_verbose), OkStatus());
  EXPECT_EQ(verbose, actual_verbose);
  EXPECT_EQ(ResetArg(parsers, "--verbose"), OkStatus());

  size_t actual_runs = 0u;
  EXPECT_EQ(GetArg(parsers, "--runs", &actual_runs), OkStatus());
  EXPECT_EQ(runs, actual_runs);
  EXPECT_EQ(ResetArg(parsers, "--runs"), OkStatus());

  uint16_t actual_port = 0u;
  EXPECT_EQ(GetArg(parsers, "port", &actual_port), OkStatus());
  EXPECT_EQ(port, actual_port);
  EXPECT_EQ(ResetArg(parsers, "port"), OkStatus());
}

TEST(ArgsParseTest, ParseArgs) {
  Vector<ArgParserVariant, 3> parsers{
      BoolParser("-v", "--verbose").set_default(false),
      UnsignedParser<size_t>("-r", "--runs").set_default(1000),
      UnsignedParser<uint16_t>("port").set_default(11111),
  };

  char const* argv1[] = {"test-bin"};
  EXPECT_EQ(ParseArgs(parsers, 1, const_cast<char**>(argv1)), OkStatus());
  CheckArgs(parsers, false, 1000, 11111);

  char const* argv2[] = {"test-bin", "22222"};
  EXPECT_EQ(ParseArgs(parsers, 2, const_cast<char**>(argv2)), OkStatus());
  CheckArgs(parsers, false, 1000, 22222);

  // Out of range argument.
  char const* argv3[] = {"test-bin", "65536"};
  EXPECT_EQ(ParseArgs(parsers, 2, const_cast<char**>(argv3)),
            Status::InvalidArgument());

  // Extra argument.
  char const* argv4[] = {"test-bin", "1", "2"};
  EXPECT_EQ(ParseArgs(parsers, 3, const_cast<char**>(argv4)),
            Status::InvalidArgument());
  EXPECT_EQ(ResetArg(parsers, "port"), OkStatus());

  // Flag missing value.
  char const* argv5[] = {"test-bin", "--runs"};
  EXPECT_EQ(ParseArgs(parsers, 2, const_cast<char**>(argv5)),
            Status::InvalidArgument());

  char const* argv6[] = {"test-bin", "-v", "33333", "--runs", "300"};
  EXPECT_EQ(ParseArgs(parsers, 5, const_cast<char**>(argv6)), OkStatus());
  CheckArgs(parsers, true, 300, 33333);

  char const* argv7[] = {"test-bin", "-r", "400", "--verbose"};
  EXPECT_EQ(ParseArgs(parsers, 4, const_cast<char**>(argv7)), OkStatus());
  CheckArgs(parsers, true, 400, 11111);

  char const* argv8[] = {"test-bin", "--no-verbose", "-r", "5000", "55555"};
  EXPECT_EQ(ParseArgs(parsers, 5, const_cast<char**>(argv8)), OkStatus());
  CheckArgs(parsers, false, 5000, 55555);
}

}  // namespace
}  // namespace pw::rpc::fuzz
