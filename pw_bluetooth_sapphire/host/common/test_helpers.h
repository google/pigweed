// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TEST_HELPERS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TEST_HELPERS_H_

#include <algorithm>
#include <iostream>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "gtest/gtest.h"
#include "lib/fxl/strings/string_printf.h"

// Run |statement| and return if a fatal test error occurred. Include the file
// name and line number in the output.
//
// This is useful for running test helpers in subroutines. For example, if a
// test helper posted ASSERTs checks inside of a dispatcher task:
//
//   RETURN_IF_FATAL(RunLoopUntilIdle());
//
// would return if any of the tasks had an ASSERT failure.
//
// Fatal failures in Google Test such as ASSERT_* failures and calls to FAIL()
// set a global flag for the failure (see testing::Test::HasFatalFailure) and
// return from the function where they occur. However, the change in flow
// control is limited to that subroutine scope. Test code higher up the stack
// must propagate the failure in order to exit the test.
//
// Note in the above example, if a task in the test loop has a fatal failure, it
// does not prevent the remaining due tasks in the loop from running. The test
// case would not exit until RunLoopFromIdle returns.
#define RETURN_IF_FATAL(statement)      \
  do {                                  \
    SCOPED_TRACE("");                   \
    ASSERT_NO_FATAL_FAILURE(statement); \
  } while (false)

namespace btlib {
namespace common {

template <class InputIt>
void PrintByteContainer(InputIt begin, InputIt end) {
  for (InputIt iter = begin; iter != end; ++iter) {
    std::cout << fxl::StringPrintf("0x%.2x ", *iter);
  }
}

// Prints the contents of a container as a string.
template <class Container>
void PrintByteContainer(const Container& c) {
  PrintByteContainer(c.begin(), c.end());
}

// Function-template for comparing contents of two iterable byte containers for
// equality. If the contents are not equal, this logs a GTEST-style error
// message to stdout. Meant to be used from unit tests.
template <class InputIt1, class InputIt2>
bool ContainersEqual(InputIt1 expected_begin, InputIt1 expected_end,
                     InputIt2 actual_begin, InputIt2 actual_end) {
  if (std::equal(expected_begin, expected_end, actual_begin, actual_end))
    return true;
  std::cout << "Expected: { ";
  PrintByteContainer(expected_begin, expected_end);
  std::cout << "}\n   Found: { ";
  PrintByteContainer(actual_begin, actual_end);
  std::cout << "}" << std::endl;
  return false;
}

template <class Container1, class Container2>
bool ContainersEqual(const Container1& expected, const Container2& actual) {
  return ContainersEqual(expected.begin(), expected.end(), actual.begin(),
                         actual.end());
}

template <class Container1>
bool ContainersEqual(const Container1& expected, const uint8_t* actual_bytes,
                     size_t actual_num_bytes) {
  return ContainersEqual(expected.begin(), expected.end(), actual_bytes,
                         actual_bytes + actual_num_bytes);
}

// Returns a managed pointer to a heap allocated MutableByteBuffer.
template <typename... T>
common::MutableByteBufferPtr NewBuffer(T... bytes) {
  return std::make_unique<common::StaticByteBuffer<sizeof...(T)>>(
      std::forward<T>(bytes)...);
}

// Returns the Upper/Lower bits of a uint16_t
constexpr uint8_t UpperBits(const uint16_t x) { return x >> 8; }
constexpr uint8_t LowerBits(const uint16_t x) { return x & 0x00FF; }

}  // namespace common
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TEST_HELPERS_H_
