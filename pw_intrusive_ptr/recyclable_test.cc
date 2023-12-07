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

#include "pw_intrusive_ptr/recyclable.h"

#include <stdint.h>

#include <utility>

#include "pw_intrusive_ptr/intrusive_ptr.h"
#include "pw_unit_test/framework.h"

namespace pw {
namespace {

class TestItem : public RefCounted<TestItem>, public Recyclable<TestItem> {
 public:
  TestItem() = default;

  virtual ~TestItem() {}

  inline static int32_t recycle_counter = 0;

 private:
  friend class Recyclable<TestItem>;
  void pw_recycle() {
    recycle_counter++;
    delete this;
  }
};

// Class that thas the pw_recyclable method, but does not derive from
// Recyclable.
class TestItemNonRecyclable : public RefCounted<TestItemNonRecyclable> {
 public:
  TestItemNonRecyclable() = default;

  virtual ~TestItemNonRecyclable() {}

  inline static int32_t recycle_counter = 0;

  void pw_recycle() { recycle_counter++; }
};

class RecyclableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    TestItem::recycle_counter = 0;
    TestItemNonRecyclable::recycle_counter = 0;
  }
};

TEST_F(RecyclableTest, DeletingLastPtrRecyclesTheObject) {
  {
    IntrusivePtr<TestItem> ptr(new TestItem());
    EXPECT_EQ(TestItem::recycle_counter, 0);
  }
  EXPECT_EQ(TestItem::recycle_counter, 1);
}

TEST_F(RecyclableTest, ConstRecycle) {
  {
    IntrusivePtr<const TestItem> ptr(new TestItem());
    EXPECT_EQ(TestItem::recycle_counter, 0);
  }
  EXPECT_EQ(TestItem::recycle_counter, 1);
}

TEST_F(RecyclableTest, NonRecyclableWithPwRecycleMethod) {
  {
    IntrusivePtr<TestItemNonRecyclable> ptr(new TestItemNonRecyclable());
    EXPECT_EQ(TestItemNonRecyclable::recycle_counter, 0);
  }
  EXPECT_EQ(TestItemNonRecyclable::recycle_counter, 0);
}

}  // namespace
}  // namespace pw
