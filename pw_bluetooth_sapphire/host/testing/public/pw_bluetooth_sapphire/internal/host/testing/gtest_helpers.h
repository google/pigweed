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
#pragma once

#include "gmock/gmock.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace bt {

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

// Wraps ContainerEq, which doesn't support comparing different ByteBuffer types
MATCHER_P(BufferEq, b, "") {
  return ::testing::ExplainMatchResult(
      ::testing::ContainerEq(bt::BufferView(b)),
      bt::BufferView(arg),
      result_listener);
}

}  // namespace bt
