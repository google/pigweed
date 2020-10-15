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

// This is a very basic direct output log implementation with no buffering.

//#define PW_LOG_MODULE_NAME "ASRT"
//#include "pw_log/log.h"

#include "pw_assert_basic/assert_basic.h"

#include <cstring>

#include "pw_assert/options.h"
#include "pw_preprocessor/util.h"
#include "pw_string/string_builder.h"
#include "pw_sys_io/sys_io.h"

// If 1, call C's standard abort() function on assert failure.
#ifndef PW_ASSERT_BASIC_ABORT
#define PW_ASSERT_BASIC_ABORT 1
#endif  // PW_ASSERT_BASIC_ABORT

// TODO(pwbug/17): Expose these through the config system.
#define PW_ASSERT_BASIC_SHOW_BANNER 1
#define PW_ASSERT_BASIC_USE_COLORS 1

// ANSI color constants to control the terminal. Not Windows compatible.
// clang-format off
#if PW_ASSERT_BASIC_USE_COLORS
#define MAGENTA   "\033[35m"
#define YELLOW    "\033[33m"
#define RED       "\033[31m"
#define GREEN     "\033[32m"
#define BLUE      "\033[96m"
#define BLACK     "\033[30m"
#define YELLOW_BG "\033[43m"
#define WHITE_BG  "\033[47m"
#define RED_BG    "\033[41m"
#define BOLD      "\033[1m"
#define RESET     "\033[0m"
#else
#define MAGENTA   ""
#define YELLOW    ""
#define RED       ""
#define GREEN     ""
#define BLUE      ""
#define BLACK     ""
#define YELLOW_BG ""
#define WHITE_BG  ""
#define RED_BG    ""
#define BOLD      ""
#define RESET     ""
#endif  // PW_ASSERT_BASIC_USE_COLORS
// clang-format on

static const char* kCrashBanner[] = {
    " ",
    "   ▄████▄      ██▀███      ▄▄▄           ██████     ██░ ██    ",
    "  ▒██▀ ▀█     ▓██ ▒ ██▒   ▒████▄       ▒██    ▒    ▓██░ ██▒   ",
    "  ▒▓█ 💥 ▄    ▓██ ░▄█ ▒   ▒██  ▀█▄     ░ ▓██▄      ▒██▀▀██░   ",
    "  ▒▓▓▄ ▄██▒   ▒██▀▀█▄     ░██▄▄▄▄██      ▒   ██▒   ░▓█ ░██    ",
    "  ▒ ▓███▀ ░   ░██▓ ▒██▒    ▓█   ▓██▒   ▒██████▒▒   ░▓█▒░██▓   ",
    "  ░ ░▒ ▒  ░   ░ ▒▓ ░▒▓░    ▒▒   ▓▒█░   ▒ ▒▓▒ ▒ ░    ▒ ░░▒░▒   ",
    "    ░  ▒        ░▒ ░ ▒░     ▒   ▒▒ ░   ░ ░▒  ░ ░    ▒ ░▒░ ░   ",
    "  ░             ░░   ░      ░   ▒      ░  ░  ░      ░  ░░ ░   ",
    "  ░ ░            ░              ░  ░         ░      ░  ░  ░   ",
    "  ░",
    " ",
};

using pw::sys_io::WriteLine;

typedef pw::StringBuffer<150> Buffer;

extern "C" void pw_Crash(const char* file_name,
                         int line_number,
                         const char* function_name,
                         const char* message,
                         ...) {
  // As a matter of usability, crashes should be visible; make it so.
#if PW_ASSERT_BASIC_SHOW_BANNER
  WriteLine(RED);
  for (const char* line : kCrashBanner) {
    WriteLine(line);
  }
  WriteLine(RESET);
#endif  // PW_ASSERT_BASIC_SHOW_BANNER

  WriteLine(
      "  Welp, that didn't go as planned. "
      "It seems we crashed. Terribly sorry!");
  WriteLine("");
  WriteLine(YELLOW "  CRASH MESSAGE" RESET);
  WriteLine("");
  {
    Buffer buffer;
    buffer << "     ";
    va_list args;
    va_start(args, message);
    buffer.FormatVaList(message, args);
    va_end(args);
    WriteLine(buffer.view());
  }

  WriteLine("");
  WriteLine(YELLOW "  CRASH FILE & LINE" RESET);
  WriteLine("");
  {
    Buffer buffer;
    buffer.Format("     %s:%d", file_name, line_number);
    WriteLine(buffer.view());
  }
  WriteLine("");
  WriteLine(YELLOW "  CRASH FUNCTION" RESET);
  WriteLine("");
  {
    Buffer buffer;
    buffer.Format("     %s", function_name);
    WriteLine(buffer.view());
  }
  WriteLine("");

  // TODO(pwbug/95): Perhaps surprisingly, this doesn't actually crash the
  // device. At some point we'll have a reboot BSP function or similar, but for
  // now this is acceptable since no one is using this basic backend.
  if (!PW_ASSERT_BASIC_DISABLE_NORETURN) {
    if (PW_ASSERT_BASIC_ABORT) {
      abort();
    } else {
      WriteLine(MAGENTA "  HANG TIME" RESET);
      WriteLine("");
      WriteLine(
          "     ... until a debugger joins. System is waiting in a while(1)");
      while (1) {
      }
    }
    PW_UNREACHABLE;
  } else {
    WriteLine(MAGENTA "  NOTE: YOU ARE IN ASSERT BASIC TEST MODE" RESET);
    WriteLine("");
    WriteLine("     This build returns from the crash handler for testing.");
    WriteLine("     If you see this message in production, your build is ");
    WriteLine("     incorrectly configured. Search for");
    WriteLine("     PW_ASSERT_BASIC_DISABLE_NORETURN to fix it.");
    WriteLine("");
  }
}

extern "C" void pw_assert_HandleFailure(void) {
#if PW_ASSERT_ENABLE_DEBUG
  pw_Crash("", 0, "", "Crash: PW_ASSERT() or PW_DASSERT() failure");
#else
  pw_Crash("", 0, "", "Crash: PW_ASSERT() failure. Note: PW_DASSERT disabled");
#endif  // PW_ASSERT_ENABLE_DEBUG
}
