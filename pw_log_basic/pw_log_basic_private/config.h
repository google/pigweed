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
#pragma once

// Replaces log levels and flag presence indicator with emoji.
#ifndef PW_EMOJI
#define PW_EMOJI 0
#endif  // PW_EMOJI

// With all the following flags enabled, log messages look like this:
//
// clang-format off
//  my_file.cc                    :  42 |                Foo | TST | INF  Hello, world!
//  buggy.cc                      :2145 |    ReadBuggyBuffer |     * ERR  No, BAD!
//
// With emoji:
//  my_file.cc                    :  42 |                Foo | TST    ℹ️  Hello, world!
//  buggy.cc                      :2145 |    ReadBuggyBuffer |     🚩 ❌  No, BAD!
// clang-format on

// Prints the name of the file that emitted the log message.
#ifndef PW_LOG_SHOW_FILENAME
#define PW_LOG_SHOW_FILENAME 0
#endif  // PW_LOG_SHOW_FILENAME

// Prints the name of the function that emitted the log message.
#ifndef PW_LOG_SHOW_FUNCTION
#define PW_LOG_SHOW_FUNCTION 0
#endif  // PW_LOG_SHOW_FUNCTION

// Prints an indicator for whether or not there are any active flags for a given
// log statement.
#ifndef PW_LOG_SHOW_FLAG
#define PW_LOG_SHOW_FLAG 0
#endif  // PW_LOG_SHOW_FLAG

// Prints the module name associated with a log statement. This is provided by
// defining PW_LOG_MODULE_NAME inside module source files, it is not implied by
// module structure or file path magic.
#ifndef PW_LOG_SHOW_MODULE
#define PW_LOG_SHOW_MODULE 0
#endif  // PW_LOG_SHOW_MODULE
