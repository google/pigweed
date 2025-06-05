// Copyright 2024 The Pigweed Authors
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

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_polyfill/language_feature_macros.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace {

#if PW_THREAD_GENERIC_CREATION_IS_SUPPORTED
#if PW_THREAD_JOINING_ENABLED

constexpr pw::ThreadAttrs kThread0 = pw::ThreadAttrs().set_stack_size_bytes(0);
constexpr pw::ThreadAttrs kThread1023 =
    pw::ThreadAttrs().set_name("hello world").set_stack_size_bytes(1023);
constexpr pw::ThreadAttrs kThread1024 =
    pw::ThreadAttrs(kThread1023).set_stack_size_bytes(1024);
[[maybe_unused]] constexpr pw::ThreadAttrs kThread1025 =
    pw::ThreadAttrs(kThread1024).set_stack_size_bytes(1025);

PW_CONSTINIT pw::ThreadStack<0> stack_0;
PW_CONSTINIT pw::ThreadStack<1024> stack_1024;

[[maybe_unused]] PW_CONSTINIT pw::ThreadContext<> must_be_constinit_1;
[[maybe_unused]] PW_CONSTINIT pw::ThreadContext<1024> must_be_constinit_2;

constexpr pw::ThreadAttrs kThreadExternalMin =
    pw::ThreadAttrs().set_stack(stack_0);
constexpr pw::ThreadAttrs kThreadExternal =
    pw::ThreadAttrs().set_stack(stack_1024);

class ThreadCreationTest : public ::testing::Test {
 public:
  ~ThreadCreationTest() override {
    EXPECT_EQ(expected_thread_runs_, thread_runs_);
  }

  auto TestThread() {
    expected_thread_runs_ += 1;
    return [this] { thread_runs_ += 1; };
  }

 private:
  int expected_thread_runs_ = 0;
  int thread_runs_ = 0;
};

TEST_F(ThreadCreationTest, ThreadContext) {
  pw::ThreadContext<1024> context;

  pw::Thread(context, kThread1024, TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThread1023>(context), TestThread()).join();
  pw::Thread(pw::GetThreadOptions(context, kThread1023), TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThread1024>(context), TestThread()).join();
  pw::Thread(pw::GetThreadOptions(context, kThread1024), TestThread()).join();

#if PW_NC_TEST(StackTooSmall_TemplateArgument)
  PW_NC_EXPECT(
      "ThreadContext stack is too small for the requested ThreadAttrs");
  pw::Thread(pw::GetThreadOptions<kThread1025>(context), TestThread()).join();
#elif PW_NC_TEST(StackTooSmall_FunctionArgument)
  PW_NC_EXPECT(
      "PW_ASSERT\(kContextStackSizeBytes >= "
      "attributes.stack_size_bytes\(\)\);");
  [[maybe_unused]] constexpr auto options =
      pw::GetThreadOptions(context, kThread1025);
#endif  // PW_NC_TEST
}

TEST_F(ThreadCreationTest, DefaultThreadContext) {
  pw::DefaultThreadContext context;

  pw::Thread(context, pw::ThreadAttrs(), TestThread()).join();

  static constexpr pw::ThreadAttrs kDefault;
  pw::Thread(pw::GetThreadOptions<kDefault>(context), TestThread()).join();
  pw::Thread(pw::GetThreadOptions(context, pw::ThreadAttrs()), TestThread())
      .join();
}

TEST_F(ThreadCreationTest, ThreadContextExternalStack) {
  pw::ThreadContext<> context;

  pw::Thread(context, kThreadExternal, TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThreadExternal>(context), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(context, kThreadExternal), TestThread())
      .join();

#if PW_NC_TEST(NoStack)
  PW_NC_EXPECT("No stack was provided!");
  std::ignore = pw::GetThreadOptions<kThread1024>(context);
#endif  // PW_NC_TEST
}

TEST_F(ThreadCreationTest,
       ThreadContextWithStackButAttrsWithExternallyAllocatedStack) {
  pw::ThreadContext<1024> context;

  pw::Thread(pw::GetThreadOptions<kThreadExternal>(context), TestThread())
      .join();

  pw::Thread(pw::GetThreadOptions(context, kThreadExternal), TestThread())
      .join();
}

TEST_F(ThreadCreationTest, ThreadContextMinimumSizedExternalStack) {
  pw::ThreadContext<> context;

  pw::Thread(context, kThreadExternalMin, TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThreadExternalMin>(context), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(context, kThreadExternalMin), TestThread())
      .join();
}

TEST_F(ThreadCreationTest, ThreadContextFor) {
  pw::ThreadContextFor<kThread1024> context;

  pw::Thread(context, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context), TestThread()).join();
}

TEST_F(ThreadCreationTest, ThreadContextForExternalStack) {
  pw::ThreadContextFor<kThreadExternal> context;

  pw::Thread(context, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context), TestThread()).join();
}

TEST_F(ThreadCreationTest, ThreadContextForMinimumSizedStack) {
  pw::ThreadContextFor<kThread0> context;

  pw::Thread(context, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context), TestThread()).join();
}

#endif  // PW_THREAD_JOINING_ENABLED

#else  // thread creation is not supported

[[maybe_unused]] constexpr pw::ThreadAttrs kAttrs;

#if PW_NC_TEST(CreationUnsupported_ThreadContext)
PW_NC_EXPECT("Generic thread creation is not yet supported");
pw::ThreadContext<> context;
#elif PW_NC_TEST(CreationUnsupported_ThreadContextFor)
PW_NC_EXPECT("Generic thread creation is not yet supported");
pw::ThreadContextFor<kAttrs> context_for_attrs;
#elif PW_NC_TEST(CreationUnsupported_ThreadStack)
PW_NC_EXPECT("Generic thread creation is not yet supported");
pw::ThreadStack<1000> stack;
#endif  // PW_NC_TEST

#endif  // PW_THREAD_GENERIC_CREATION_IS_SUPPORTED

}  // namespace
