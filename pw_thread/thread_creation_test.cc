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

pw::ThreadContext<1024> context_1024;

TEST_F(ThreadCreationTest, ThreadContext) {
  pw::Thread(context_1024, kThread1024, TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThread1023>(context_1024), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(context_1024, kThread1023), TestThread())
      .join();

  pw::Thread(pw::GetThreadOptions<kThread1024>(context_1024), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(context_1024, kThread1024), TestThread())
      .join();

#if PW_NC_TEST(StackTooSmall_TemplateArgument)
  PW_NC_EXPECT(
      "ThreadContext stack is too small for the requested ThreadAttrs");
  pw::Thread(pw::GetThreadOptions<kThread1025>(context_1024), TestThread())
      .join();
#elif PW_NC_TEST(StackTooSmall_FunctionArgument)
  PW_NC_EXPECT(
      "PW_ASSERT\(kContextStackSizeBytes >= "
      "attributes.stack_size_bytes\(\)\);");
  [[maybe_unused]] constexpr auto options =
      pw::GetThreadOptions(context_1024, kThread1025);
#endif  // PW_NC_TEST
}

pw::DefaultThreadContext default_context;

TEST_F(ThreadCreationTest, DefaultThreadContext) {
  pw::Thread(default_context, pw::ThreadAttrs(), TestThread()).join();

  static constexpr pw::ThreadAttrs kDefault;
  pw::Thread(pw::GetThreadOptions<kDefault>(default_context), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(default_context, pw::ThreadAttrs()),
             TestThread())
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
  // This ThreadContext allocates space for a stack, but it is not used.
  pw::ThreadContext<128> context;

  pw::Thread(pw::GetThreadOptions<kThreadExternal>(context), TestThread())
      .join();

  pw::Thread(pw::GetThreadOptions(context, kThreadExternal), TestThread())
      .join();
}

TEST_F(ThreadCreationTest, ThreadContextMinimumSizedExternalStack) {
  pw::ThreadContext<> context;  // external stack

  pw::Thread(context, kThreadExternalMin, TestThread()).join();

  pw::Thread(pw::GetThreadOptions<kThreadExternalMin>(context), TestThread())
      .join();
  pw::Thread(pw::GetThreadOptions(context, kThreadExternalMin), TestThread())
      .join();
}

pw::ThreadContextFor<kThread1024> context_for;

TEST_F(ThreadCreationTest, ThreadContextFor) {
  pw::Thread(context_for, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context_for), TestThread()).join();
}

TEST_F(ThreadCreationTest, ThreadContextForExternalStack) {
  pw::ThreadContextFor<kThreadExternal> context;

  pw::Thread(context, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context), TestThread()).join();
}

pw::ThreadContextFor<kThread0> context_for_min_stack;

TEST_F(ThreadCreationTest, ThreadContextForMinimumSizedStack) {
  pw::Thread(context_for_min_stack, TestThread()).join();

  pw::Thread(pw::GetThreadOptions(context_for_min_stack), TestThread()).join();
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
