// Copyright 2020 The Pigweed Authors
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

#include "pw_stream/stream.h"

#include <limits>

#include "gtest/gtest.h"
#include "pw_stream/null_stream.h"

namespace pw::stream {
namespace {

TEST(Stream, DefaultConservativeWriteLimit) {
  NullWriter stream;
  EXPECT_EQ(stream.ConservativeWriteLimit(),
            std::numeric_limits<size_t>::max());
}

TEST(Stream, DefaultConservativeReadLimit) {
  NullReader stream;
  EXPECT_EQ(stream.ConservativeReadLimit(), std::numeric_limits<size_t>::max());
}

}  // namespace
}  // namespace pw::stream
