// Copyright 2021 The Pigweed Authors
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
#include "pw_persistent_ram/persistent.h"

#include <type_traits>

#include "gtest/gtest.h"

namespace pw::persistent_ram {
namespace {

class PersistentTest : public ::testing::Test {
 protected:
  PersistentTest() { ZeroPersistentMemory(); }

  // Emulate invalidation of persistent section(s).
  void ZeroPersistentMemory() { memset(&buffer_, 0, sizeof(buffer_)); }

  // Allocate a chunk of aligned storage that can be independently controlled.
  std::aligned_storage_t<sizeof(Persistent<uint32_t>),
                         alignof(Persistent<uint32_t>)>
      buffer_;
};

TEST_F(PersistentTest, DefaultConstructionAndDestruction) {
  {  // Emulate a boot where the persistent sections were invalidated.
    // Although the fixture always does this, we do this an extra time to be
    // 100% confident that an integrity check cannot be accidentally selected
    // which results in reporting there is valid data when zero'd.
    ZeroPersistentMemory();
    auto& persistent = *(new (&buffer_) Persistent<uint32_t>());
    EXPECT_FALSE(persistent.has_value());

    persistent = 42;
    ASSERT_TRUE(persistent.has_value());
    EXPECT_EQ(42u, persistent.value());

    persistent.~Persistent();  // Emulate shutdown / global destructors.
  }

  {  // Emulate a boot where persistent memory was kept as is.
    auto& persistent = *(new (&buffer_) Persistent<uint32_t>());
    ASSERT_TRUE(persistent.has_value());
    EXPECT_EQ(42u, persistent.value());
  }
}

TEST_F(PersistentTest, Reset) {
  {  // Emulate a boot where the persistent sections were invalidated.
    auto& persistent = *(new (&buffer_) Persistent<uint32_t>());
    persistent = 42u;
    EXPECT_TRUE(persistent.has_value());
    persistent.reset();

    persistent.~Persistent();  // Emulate shutdown / global destructors.
  }

  {  // Emulate a boot where persistent memory was kept as is.
    auto& persistent = *(new (&buffer_) Persistent<uint32_t>());
    EXPECT_FALSE(persistent.has_value());
  }
}

TEST_F(PersistentTest, Emplace) {
  auto& persistent = *(new (&buffer_) Persistent<uint32_t>());
  EXPECT_FALSE(persistent.has_value());

  persistent.emplace(42u);
  ASSERT_TRUE(persistent.has_value());
  EXPECT_EQ(42u, persistent.value());
}

}  // namespace
}  // namespace pw::persistent_ram
