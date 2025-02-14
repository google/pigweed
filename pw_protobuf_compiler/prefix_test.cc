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

#include <string>

#if !defined(PW_PROTOBUF_COMPILER_IMPORT)
#error "This test requires specifying a value for PW_PROTOBUF_COMPILER_IMPORT!"
#endif

#define DEFAULT 1
#define STRIP_IMPORT_PREFIX 2
#define IMPORT_PREFIX 3

#if PW_PROTOBUF_COMPILER_IMPORT == DEFAULT
#include "pw_protobuf_compiler/nanopb_test_protos/prefix_tests.pb.h"
#endif

#if PW_PROTOBUF_COMPILER_IMPORT == STRIP_IMPORT_PREFIX
#include "prefix_tests.pb.h"
#endif

#if PW_PROTOBUF_COMPILER_IMPORT == IMPORT_PREFIX
#include "some_prefix/prefix_tests.pb.h"
#endif

#include "pw_status/status.h"
#include "pw_string/util.h"
#include "pw_unit_test/framework.h"

namespace pw::protobuf_compiler {
namespace {

TEST(PrefixTest, Compiles) {
  std::string str = "abcd";
  prefixtest_Message proto = prefixtest_Message_init_default;
  EXPECT_EQ(
      pw::string::Copy(str.c_str(), proto.name, sizeof(proto.name)).status(),
      pw::OkStatus());
}

}  // namespace
}  // namespace pw::protobuf_compiler
