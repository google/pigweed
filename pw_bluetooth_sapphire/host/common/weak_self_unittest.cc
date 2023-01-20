// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weak_self.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <lib/async/dispatcher.h>

#include <gtest/gtest.h>

#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt {
namespace {

using WeakSelfTest = ::gtest::TestLoopFixture;

class FunctionTester : public WeakSelf<FunctionTester> {
 public:
  explicit FunctionTester(uint8_t testval) : WeakSelf(this), value_(testval) {}

  void callback_later_with_weak(fit::function<void(FunctionTester::WeakPtr)> cb) {
    auto weak = GetWeakPtr();
    auto timed = [self = std::move(weak), cb = std::move(cb)]() { cb(self); };
    async::PostTask(async_get_default_dispatcher(), std::move(timed));
  }

  uint8_t value() const { return value_; }

 private:
  uint8_t value_;
};

TEST_F(WeakSelfTest, InvalidatingSelf) {
  bool called = false;
  FunctionTester::WeakPtr ptr;

  // Default-constructed weak pointers are not alive.
  EXPECT_FALSE(ptr.is_alive());

  auto cb = [&ptr, &called](auto weakptr) {
    called = true;
    ptr = weakptr;
  };

  {
    FunctionTester test(0xBA);

    test.callback_later_with_weak(cb);

    // Run the loop until we're called back.
    RunLoopUntilIdle();

    EXPECT_TRUE(called);
    EXPECT_TRUE(ptr.is_alive());
    EXPECT_EQ(&test, &ptr.get());
    EXPECT_EQ(0xBA, ptr->value());

    called = false;
    test.callback_later_with_weak(cb);

    // Now out of scope.
  }

  // Run the loop until we're called back.
  RunLoopUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_FALSE(ptr.is_alive());
  EXPECT_DEATH_IF_SUPPORTED(ptr.get(), "destroyed");
}

TEST_F(WeakSelfTest, InvalidatePtrs) {
  bool called = false;
  FunctionTester::WeakPtr ptr;

  // Default-constructed weak pointers are not alive.
  EXPECT_FALSE(ptr.is_alive());

  auto cb = [&ptr, &called](auto weakptr) {
    called = true;
    ptr = weakptr;
  };

  FunctionTester test(0xBA);

  test.callback_later_with_weak(cb);

  // Run the loop until we're called back.
  RunLoopUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_TRUE(ptr.is_alive());
  EXPECT_EQ(&test, &ptr.get());
  EXPECT_EQ(0xBA, ptr->value());

  called = false;
  test.callback_later_with_weak(cb);

  // Now invalidate the pointers.
  test.InvalidatePtrs();

  // Run the loop until we're called back.
  RunLoopUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_FALSE(ptr.is_alive());
  EXPECT_DEATH_IF_SUPPORTED(ptr.get(), "destroyed");
}

class StaticTester;

class OnlyTwoStaticManager {
 public:
  explicit OnlyTwoStaticManager(StaticTester *self_ptr) : obj_ptr_(self_ptr) {}
  ~OnlyTwoStaticManager() { InvalidateAll(); }

  using RefType = RecyclingWeakRef;

  std::optional<fbl::RefPtr<RefType>> GetWeakRef() {
    for (auto &ptr : OnlyTwoStaticManager::pointers_) {
      if (ptr.is_alive() && ptr.get() == obj_ptr_) {
        // Already adopted, add another refptr pointing to it.
        return fbl::RefPtr(&ptr);
      }
    }
    for (auto &ptr : OnlyTwoStaticManager::pointers_) {
      if (!ptr.is_in_use()) {
        return ptr.alloc(obj_ptr_);
      }
    }
    return std::nullopt;
  }

  void InvalidateAll() {
    OnlyTwoStaticManager::pointers_[0].maybe_unset(obj_ptr_);
    OnlyTwoStaticManager::pointers_[1].maybe_unset(obj_ptr_);
  }

 private:
  StaticTester *obj_ptr_;
  inline static RecyclingWeakRef pointers_[2];
};

class StaticTester : public WeakSelf<StaticTester, OnlyTwoStaticManager> {
 public:
  explicit StaticTester(uint8_t testval) : WeakSelf(this), value_(testval) {}

  uint8_t value() const { return value_; }

 private:
  uint8_t value_;
};

TEST_F(WeakSelfTest, StaticRecyclingPointers) {
  // We can create more objects than we have weak space for.
  StaticTester test1(1);
  StaticTester test2(2);
  StaticTester test3(3);

  // And create as many weak pointers of one of them as we want.
  auto ptr = test1.GetWeakPtr();
  auto ptr2 = test1.GetWeakPtr();
  auto ptr3 = test1.GetWeakPtr();
  auto ptr4 = ptr;

  // Make the second one have some ptrs too.
  {
    {
      StaticTester test4(4);
      auto second_ptr = test4.GetWeakPtr();
      auto second_ptr2 = test4.GetWeakPtr();
      EXPECT_TRUE(ptr4.is_alive());
      StaticTester *ptr4_old = &ptr4.get();
      ptr4 = second_ptr;
      EXPECT_TRUE(ptr4.is_alive());
      // It's moved to the new one though.
      EXPECT_NE(&ptr4.get(), ptr4_old);
      EXPECT_EQ(&ptr4.get(), &test4);
    }
    // ptr4 outlived it's target.
    EXPECT_FALSE(ptr4.is_alive());
    // Now let's make the second weak pointer unused, recycling it.
    ptr4 = ptr3;
  }

  // Now I can get a second weak ptr still, from our third object.
  auto still_okay = test3.GetWeakPtr();
  auto still_copy = still_okay;
  EXPECT_TRUE(still_copy.is_alive());
}

TEST_F(WeakSelfTest, StaticDeathWhenExhausted) {
  StaticTester test1(1);
  StaticTester test3(3);

  auto ptr1 = test1.GetWeakPtr();
  auto ptr2 = ptr1;
  {
    StaticTester test2(2);

    ptr2 = test2.GetWeakPtr();

    EXPECT_TRUE(ptr2.is_alive());
    EXPECT_TRUE(ptr1.is_alive());
  }

  EXPECT_FALSE(ptr2.is_alive());

  EXPECT_DEATH_IF_SUPPORTED(test3.GetWeakPtr(), ".*");
}

class BaseClass {
 public:
  BaseClass() = default;
  virtual ~BaseClass() = default;

  void set_value(int value) { value_ = value; }

  int value() const { return value_; }

 private:
  int value_ = 0;
};

class ChildClass : public BaseClass, public WeakSelf<ChildClass> {
 public:
  ChildClass() : BaseClass(), WeakSelf<ChildClass>(this) {}
};

TEST_F(WeakSelfTest, Upcast) {
  ChildClass obj;

  WeakPtr<ChildClass> child_weak = obj.GetWeakPtr();
  child_weak->set_value(1);
  EXPECT_EQ(child_weak->value(), 1);

  WeakPtr<BaseClass> base_weak_copy(child_weak);
  EXPECT_TRUE(child_weak.is_alive());
  base_weak_copy->set_value(2);
  EXPECT_EQ(base_weak_copy->value(), 2);

  WeakPtr<BaseClass> base_weak_move(std::move(child_weak));
  EXPECT_FALSE(child_weak.is_alive());
  base_weak_move->set_value(3);
  EXPECT_EQ(base_weak_move->value(), 3);
}

}  // namespace
}  // namespace bt
