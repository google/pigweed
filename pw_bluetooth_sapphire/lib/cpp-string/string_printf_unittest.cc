// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cpp-string/string_printf.h>
#include <errno.h>
#include <gtest/gtest.h>
#include <stdarg.h>

#include <string>

namespace bt_lib_cpp_string {
namespace {

constexpr size_t kStackBufferSize = 1024u;

// Note: |runnable| can't be a reference since that'd make the behavior of
// |va_start()| undefined.
template <typename Runnable>
std::string VAListHelper(Runnable runnable, ...) {
  va_list ap;
  va_start(ap, runnable);
  std::string rv = runnable(ap);
  va_end(ap);
  return rv;
}

TEST(StringPrintfTest, StringPrintfBasic) {
  EXPECT_EQ("", StringPrintf(""));
  EXPECT_EQ("hello", StringPrintf("hello"));
  EXPECT_EQ("hello-123", StringPrintf("hello%d", -123));
  EXPECT_EQ("hello0123FACE", StringPrintf("%s%04d%X", "hello", 123, 0xfaceU));
}

TEST(StringPrintfTest, StringVPrintf_Basic) {
  EXPECT_EQ("", VAListHelper([](va_list ap) -> std::string {
              return StringVPrintf("", ap);
            }));
  EXPECT_EQ("hello", VAListHelper([](va_list ap) -> std::string {
              return StringVPrintf("hello", ap);
            }));
  EXPECT_EQ("hello-123",
            VAListHelper(
                [](va_list ap) -> std::string {
                  return StringVPrintf("hello%d", ap);
                },
                -123));
  EXPECT_EQ("hello0123FACE",
            VAListHelper(
                [](va_list ap) -> std::string {
                  return StringVPrintf("%s%04d%X", ap);
                },
                "hello",
                123,
                0xfaceU));
}

TEST(StringPrintfTest, StringAppendfBasic) {
  {
    std::string s = "existing";
    StringAppendf(&s, "");
    EXPECT_EQ("existing", s);
  }
  {
    std::string s = "existing";
    StringAppendf(&s, "hello");
    EXPECT_EQ("existinghello", s);
  }
  {
    std::string s = "existing";
    StringAppendf(&s, "hello%d", -123);
    EXPECT_EQ("existinghello-123", s);
  }
  {
    std::string s = "existing";
    StringAppendf(&s, "%s%04d%X", "hello", 123, 0xfaceU);
    EXPECT_EQ("existinghello0123FACE", s);
  }
}

TEST(StringPrintfTest, StringVAppendfBasic) {
  EXPECT_EQ("existing", VAListHelper([](va_list ap) -> std::string {
              std::string s = "existing";
              StringVAppendf(&s, "", ap);
              return s;
            }));
  EXPECT_EQ("existinghello", VAListHelper([](va_list ap) -> std::string {
              std::string s = "existing";
              StringVAppendf(&s, "hello", ap);
              return s;
            }));
  EXPECT_EQ("existinghello-123",
            VAListHelper(
                [](va_list ap) -> std::string {
                  std::string s = "existing";
                  StringVAppendf(&s, "hello%d", ap);
                  return s;
                },
                -123));
  EXPECT_EQ("existinghello0123FACE",
            VAListHelper(
                [](va_list ap) -> std::string {
                  std::string s = "existing";
                  StringVAppendf(&s, "%s%04d%X", ap);
                  return s;
                },
                "hello",
                123,
                0xfaceU));
}

// Generally, we assume that everything forwards to |StringVAppendf()|, so
// testing |StringPrintf()| more carefully suffices.

TEST(StringPrintfTest, StringPrintfMaxSize) {
  std::string stuff(kStackBufferSize - 1, 'x');
  std::string format = "%s";
  EXPECT_EQ(stuff, StringPrintf(format.c_str(), stuff.c_str()));
}

TEST(StringPrintfTest, StringPrintfTruncated) {
  std::string stuff(kStackBufferSize, 'x');
  std::string format = "%s";
  // One char in kStackBufferSize will be used for null terminator, so one char
  // in |stuff| will be truncated.
  std::string expected(kStackBufferSize - 1, 'x');
  EXPECT_EQ(expected, StringPrintf(format.c_str(), stuff.c_str()));
}

}  // namespace
}  // namespace bt_lib_cpp_string
