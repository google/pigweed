// Copyright 2022 The Pigweed Authors
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

#include "pw_string/string.h"
#include "pw_unit_test/framework.h"
#include "pwpb_test_no_prefix.pwpb.h"

namespace pw::protobuf_compiler {
namespace {

TEST(Pwpb, InlineOptionsAppliedAndOverridden) {
  pw::protobuf_compiler::pwpb::InlineOptionsExample::Message
      inline_options_example;

  static_assert(
      std::is_same_v<decltype(inline_options_example.ten_chars_inline),
                     pw::InlineString<10>>,
      "Field `ten_chars_inline` should be a `pw::InlineString<10>`.");
}

}  // namespace
}  // namespace pw::protobuf_compiler
