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

// The Pigweed unit test framework requires C++17 to use its full functionality.
// In C++11, only the TEST, TEST_F, EXPECT_TRUE, EXPECT_FALSE, ASSERT_TRUE,
// ASSERT_FALSE, FAIL, and ADD_FAILURE macros may be used.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>

#include "pw_polyfill/standard.h"
#include "pw_preprocessor/concat.h"
#include "pw_preprocessor/util.h"
#include "pw_unit_test/event_handler.h"

#if PW_CXX_STANDARD_IS_SUPPORTED(17)
#include "pw_string/string_builder.h"
#endif  // PW_CXX_STANDARD_IS_SUPPORTED(17)

#define PW_TEST(test_suite_name, test_name) \
  _PW_TEST(test_suite_name, test_name, ::pw::unit_test::Test)

// TEST() is a pretty generic macro name which could conflict with other code.
// If GTEST_DONT_DEFINE_TEST is set, don't alias PW_TEST to TEST.
#if !(defined(GTEST_DONT_DEFINE_TEST) && GTEST_DONT_DEFINE_TEST)
#define TEST PW_TEST
#endif  // !GTEST_DONT_DEFINE_TEST

#define TEST_F(test_fixture, test_name) \
  _PW_TEST(test_fixture, test_name, test_fixture)

#define EXPECT_TRUE(expr) _PW_EXPECT_BOOL(expr, true)
#define EXPECT_FALSE(expr) _PW_EXPECT_BOOL(expr, false)
#define EXPECT_EQ(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, ==)
#define EXPECT_NE(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, !=)
#define EXPECT_GT(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, >)
#define EXPECT_GE(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, >=)
#define EXPECT_LT(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, <)
#define EXPECT_LE(lhs, rhs) _PW_TEST_OP(_PW_TEST_EXPECT, lhs, rhs, <=)
#define EXPECT_STREQ(lhs, rhs) _PW_TEST_STREQ(_PW_TEST_EXPECT, lhs, rhs)
#define EXPECT_STRNE(lhs, rhs) _PW_TEST_STRNE(_PW_TEST_EXPECT, lhs, rhs)

#define ASSERT_TRUE(expr) _PW_ASSERT_BOOL(expr, true)
#define ASSERT_FALSE(expr) _PW_ASSERT_BOOL(expr, false)
#define ASSERT_EQ(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, ==)
#define ASSERT_NE(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, !=)
#define ASSERT_GT(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, >)
#define ASSERT_GE(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, >=)
#define ASSERT_LT(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, <)
#define ASSERT_LE(lhs, rhs) _PW_TEST_OP(_PW_TEST_ASSERT, lhs, rhs, <=)
#define ASSERT_STREQ(lhs, rhs) _PW_TEST_STREQ(_PW_TEST_ASSERT, lhs, rhs)
#define ASSERT_STRNE(lhs, rhs) _PW_TEST_STRNE(_PW_TEST_ASSERT, lhs, rhs)

// Generates a non-fatal failure with a generic message.
#define ADD_FAILURE() \
  _PW_TEST_MESSAGE("(line is not executed)", "(line was executed)", false)

// Generates a fatal failure with a generic message.
#define GTEST_FAIL() return ADD_FAILURE()

// Define either macro to 1 to omit the definition of FAIL(), which is a
// generic name and clashes with some other libraries.
#if !(defined(GTEST_DONT_DEFINE_FAIL) && GTEST_DONT_DEFINE_FAIL)
#define FAIL() GTEST_FAIL()
#endif  // !GTEST_DONT_DEFINE_FAIL

// Generates a success with a generic message.
#define GTEST_SUCCEED() _PW_TEST_MESSAGE("(success)", "(success)", true)

// Define either macro to 1 to omit the definition of SUCCEED(), which
// is a generic name and clashes with some other libraries.
#if !(defined(GTEST_DONT_DEFINE_SUCCEED) && GTEST_DONT_DEFINE_SUCCEED)
#define SUCCEED() GTEST_SUCCEED()
#endif  // !GTEST_DONT_DEFINE_SUCCEED

// pw_unit_test framework entry point. Runs every registered test case and
// dispatches the results through the event handler. Returns a status of zero
// if all tests passed, or nonzero if there were any failures.
// This is compatible with Googletest.
//
// In order to receive test output, an event handler must be registered before
// this is called:
//
//   int main() {
//     MyEventHandler handler;
//     pw::unit_test::RegisterEventHandler(&handler);
//     return RUN_ALL_TESTS();
//   }
//
#define RUN_ALL_TESTS() \
  ::pw::unit_test::internal::Framework::Get().RunAllTests()

namespace pw {

#if PW_CXX_STANDARD_IS_SUPPORTED(17)

namespace string {

// This function is used to print unknown types that are used in EXPECT or
// ASSERT statements in tests.
//
// You can add support for displaying custom types by defining a ToString
// template specialization. For example:
//
//   namespace pw {
//
//   template <>
//   StatusWithSize ToString<MyType>(const MyType& value,
//                                   std::span<char> buffer) {
//     return string::Format("<MyType|%d>", value.id);
//   }
//
//   }  // namespace pw
//
// See the documentation in pw_string/string_builder.h for more information.
template <typename T>
StatusWithSize UnknownTypeToString(const T& value, std::span<char> buffer) {
  StringBuilder sb(buffer);
  sb << '<' << sizeof(value) << "-byte object at 0x" << &value << '>';
  return sb.status_with_size();
}

}  // namespace string

#endif  // PW_CXX_STANDARD_IS_SUPPORTED(17)

namespace unit_test {

class Test;

namespace internal {

class TestInfo;

// Singleton test framework class responsible for managing and running test
// cases. This implementation is internal to Pigweed test; free functions
// wrapping its functionality are exposed as the public interface.
class Framework {
 public:
  constexpr Framework()
      : current_test_(nullptr),
        current_result_(TestResult::kSuccess),
        run_tests_summary_{.passed_tests = 0, .failed_tests = 0},
        exit_status_(0),
        event_handler_(nullptr),
        memory_pool_() {}

  static Framework& Get() { return framework_; }

  // Registers a single test case with the framework. The framework owns the
  // registered unit test. Called during static initialization.
  void RegisterTest(TestInfo* test);

  // Sets the handler to which the framework dispatches test events. During a
  // test run, the framework owns the event handler.
  void RegisterEventHandler(EventHandler* event_handler) {
    event_handler_ = event_handler;
  }

  // Runs all registered test cases, returning a status of 0 if all succeeded or
  // nonzero if there were any failures. Test events that occur during the run
  // are sent to the registered event handler, if any.
  int RunAllTests();

  // Constructs an instance of a unit test class and runs the test.
  //
  // Tests are constructed within a static memory pool at run time instead of
  // being statically allocated to avoid blowing up the size of the test binary
  // in cases where users have large test fixtures (e.g. containing buffers)
  // reused many times. Instead, only a small, fixed-size TestInfo struct is
  // statically allocated per test case, with a run() function that references
  // this method instantiated for its test class.
  template <typename TestInstance>
  static void CreateAndRunTest(const TestInfo& test_info) {
    // TODO(frolv): Update the assert message with the name of the config option
    // for memory pool size once it is configurable.
    static_assert(
        sizeof(TestInstance) <= sizeof(memory_pool_),
        "The test memory pool is too small for this test. Either increase "
        "kTestMemoryPoolSizeBytes or decrease the size of your test fixture.");

    Framework& framework = Get();
    framework.StartTest(test_info);

    // Reset the memory pool to a marker value to help detect use of
    // uninitialized memory.
    std::memset(&framework.memory_pool_, 0xa5, sizeof(framework.memory_pool_));

    // Construct the test object within the static memory pool. The StartTest
    // function has already been called by the TestInfo at this point.
    TestInstance* test_instance = new (&framework.memory_pool_) TestInstance;
    test_instance->PigweedTestRun();

    // Manually call the destructor as it is not called automatically for
    // objects constructed using placement new.
    test_instance->~TestInstance();

    framework.EndCurrentTest();
  }

  // Runs an expectation function for the currently active test case.
  template <typename Expectation, typename Lhs, typename Rhs>
  bool CurrentTestExpect(Expectation expectation,
                         const Lhs& lhs,
                         const Rhs& rhs,
                         const char* expectation_string,
                         const char* expression,
                         int line) {
    // Size of the buffer into which to write the string with the evaluated
    // version of the arguments. This buffer is allocated on the unit test's
    // stack, so it shouldn't be too large.
    // TODO(hepler): Make this configurable.
    constexpr size_t kExpectationBufferSizeBytes = 128;

    bool result = expectation(lhs, rhs);
    ExpectationResult(expression,
#if PW_CXX_STANDARD_IS_SUPPORTED(17)
                      MakeString<kExpectationBufferSizeBytes>(
                          lhs, ' ', expectation_string, ' ', rhs)
                          .c_str(),
#else
                      "(evaluation requires C++17)",
#endif  // PW_CXX_STANDARD_IS_SUPPORTED(17)
                      line,
                      result);

    static_cast<void>(expectation_string);
    static_cast<void>(kExpectationBufferSizeBytes);

    return result;
  }

  // Dispatches an event indicating the result of an expectation.
  void ExpectationResult(const char* expression,
                         const char* evaluated_expression,
                         int line,
                         bool success);

 private:
  // Sets current_test_ and dispatches an event indicating that a test started.
  void StartTest(const TestInfo& test);

  // Dispatches event indicating that a test finished and clears current_test_.
  void EndCurrentTest();

  // Singleton instance of the framework class.
  static Framework framework_;

  // Linked list of all registered test cases. This is static as it tests are
  // registered using static initialization.
  static TestInfo* tests_;

  // The current test case which is running.
  const TestInfo* current_test_;

  // Overall result of the current test case (pass/fail).
  TestResult current_result_;

  // Overall result of the ongoing test run, which covers multiple tests.
  RunTestsSummary run_tests_summary_;

  // Program exit status returned by RunAllTests for Googletest compatibility.
  int exit_status_;

  // Handler to which to dispatch test events.
  EventHandler* event_handler_;

  // Memory region in which to construct test case classes as they are run.
  // TODO(frolv): Make the memory pool size configurable.
  static constexpr size_t kTestMemoryPoolSizeBytes = 16384;
  std::aligned_storage_t<kTestMemoryPoolSizeBytes, alignof(std::max_align_t)>
      memory_pool_;
};

// Information about a single test case, including a pointer to a function which
// constructs and runs the test class. These are statically allocated instead of
// the test classes, as test classes can be very large.
class TestInfo {
 public:
  TestInfo(const char* const test_suite_name,
           const char* const test_name,
           const char* const file_name,
           void (*run_func)(const TestInfo&))
      : test_case_{
        .suite_name = test_suite_name,
        .test_name = test_name,
        .file_name = file_name,
       }, run_(run_func) {
    Framework::Get().RegisterTest(this);
  }

  // The name of the suite to which the test case belongs, the name of the test
  // case itself, and the path to the file in which the test case is located.
  const TestCase& test_case() const { return test_case_; }

  bool enabled() const;

  void run() const { run_(*this); }

  TestInfo* next() const { return next_; }
  void set_next(TestInfo* next) { next_ = next; }

 private:
  TestCase test_case_;

  // Function which runs the test case. Refers to Framework::CreateAndRunTest
  // instantiated for the test case's class.
  void (*run_)(const TestInfo&);

  // TestInfo structs are registered with the test framework and stored as a
  // linked list.
  TestInfo* next_ = nullptr;
};

}  // namespace internal

// Base class for all test cases or custom test fixtures.
// Every unit test created using the TEST or TEST_F macro defines a class that
// inherits from this (or a subclass of this).
//
// For example, given the following test definition:
//
//   TEST(MyTest, SaysHello) {
//     ASSERT_STREQ(SayHello(), "Hello, world!");
//   }
//
// A new class is defined for the test, e.g. MyTest_SaysHello_Test. This class
// inherits from the Test class and implements its PigweedTestBody function with
// the block provided to the TEST macro.
class Test {
 public:
  Test(const Test&) = delete;
  Test& operator=(const Test&) = delete;

  virtual ~Test() = default;

  // Runs the unit test.
  void PigweedTestRun() {
    SetUp();
    PigweedTestBody();
    TearDown();
  }

 protected:
  Test() = default;

  // Called immediately before executing the test body.
  //
  // Setup and cleanup can typically be done in the test fixture's constructor
  // and destructor, but there are cases where SetUp/TearDown must be used
  // instead. See the Google Test documentation for more information.
  virtual void SetUp() {}

  // Called immediately after executing the test body.
  virtual void TearDown() {}

 private:
  friend class internal::Framework;

  // The user-provided body of the test case. Populated by the TEST macro.
  virtual void PigweedTestBody() = 0;
};

}  // namespace unit_test
}  // namespace pw

#define _PW_TEST_CLASS_NAME(test_suite_name, test_name) \
  PW_CONCAT(test_suite_name, _, test_name, _Test)

#define _PW_TEST(test_suite_name, test_name, parent_class)              \
  static_assert(sizeof(#test_suite_name) > 1,                           \
                "test_suite_name must not be empty");                   \
  static_assert(sizeof(#test_name) > 1, "test_name must not be empty"); \
                                                                        \
  class _PW_TEST_CLASS_NAME(test_suite_name, test_name) final           \
      : public parent_class {                                           \
   private:                                                             \
    void PigweedTestBody() override;                                    \
                                                                        \
    static ::pw::unit_test::internal::TestInfo test_info_;              \
  };                                                                    \
                                                                        \
  ::pw::unit_test::internal::TestInfo                                   \
      _PW_TEST_CLASS_NAME(test_suite_name, test_name)::test_info_(      \
          #test_suite_name,                                             \
          #test_name,                                                   \
          __FILE__,                                                     \
          ::pw::unit_test::internal::Framework::CreateAndRunTest<       \
              _PW_TEST_CLASS_NAME(test_suite_name, test_name)>);        \
                                                                        \
  void _PW_TEST_CLASS_NAME(test_suite_name, test_name)::PigweedTestBody()

#define _PW_TEST_EXPECT(lhs, rhs, expectation, expectation_string) \
  ::pw::unit_test::internal::Framework::Get().CurrentTestExpect(   \
      expectation,                                                 \
      (lhs),                                                       \
      (rhs),                                                       \
      expectation_string,                                          \
      #lhs " " expectation_string " " #rhs,                        \
      __LINE__)

#define _PW_TEST_ASSERT(lhs, rhs, expectation, expectation_string)     \
  do {                                                                 \
    if (!_PW_TEST_EXPECT(lhs, rhs, expectation, expectation_string)) { \
      return;                                                          \
    }                                                                  \
  } while (0)

#define _PW_TEST_MESSAGE(expected, actual, success)              \
  ::pw::unit_test::internal::Framework::Get().ExpectationResult( \
      expected, actual, __LINE__, success)

#define _PW_TEST_OP(expect_or_assert, lhs, rhs, op)  \
  expect_or_assert(                                  \
      lhs,                                           \
      rhs,                                           \
      [](const auto& _pw_lhs, const auto& _pw_rhs) { \
        return _pw_lhs op _pw_rhs;                   \
      },                                             \
      #op)

// Implement boolean expectations in a C++11-compatible way.
#define _PW_EXPECT_BOOL(expr, value)                             \
  ::pw::unit_test::internal::Framework::Get().CurrentTestExpect( \
      [](bool lhs, bool rhs) { return lhs == rhs; },             \
      static_cast<bool>(expr),                                   \
      value,                                                     \
      "is",                                                      \
      #expr " is " #value,                                       \
      __LINE__)

#define _PW_ASSERT_BOOL(expr, value)     \
  do {                                   \
    if (!_PW_EXPECT_BOOL(expr, value)) { \
      return;                            \
    }                                    \
  } while (0)

#define _PW_TEST_STREQ(expect_or_assert, lhs, rhs)   \
  expect_or_assert(                                  \
      lhs,                                           \
      rhs,                                           \
      [](const auto& _pw_lhs, const auto& _pw_rhs) { \
        return std::strcmp(_pw_lhs, _pw_rhs) == 0;   \
      },                                             \
      "equals")

#define _PW_TEST_STRNE(expect_or_assert, lhs, rhs)   \
  expect_or_assert(                                  \
      lhs,                                           \
      rhs,                                           \
      [](const auto& _pw_lhs, const auto& _pw_rhs) { \
        return std::strcmp(_pw_lhs, _pw_rhs) != 0;   \
      },                                             \
      "does not equal")

// Alias Test as ::testing::Test for Googletest compatibility.
namespace testing {
using Test = ::pw::unit_test::Test;
}  // namespace testing
