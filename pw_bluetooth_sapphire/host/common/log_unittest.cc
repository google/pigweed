// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "log.h"

#include <string_view>

#include <gtest/gtest.h>

namespace bt {
namespace {

// Check that BaseName strips the directory portion of a string literal path at compile time.
static_assert(internal::BaseName(nullptr) == nullptr);
static_assert(std::string_view(internal::BaseName("")).empty());
static_assert(internal::BaseName("main.cc") == std::string_view("main.cc"));
static_assert(internal::BaseName("/main.cc") == std::string_view("main.cc"));
static_assert(internal::BaseName("../foo/bar//main.cc") == std::string_view("main.cc"));

TEST(LogScopeTest, LogScopes) {
  LogContext context0;
  LogContext context1;
  LogContext context2;
  {
    bt_log_scope("A");
    {
      bt_log_scope("%d", 1);
      context0 = capture_log_context();
    }
    context1 = capture_log_context();
  }
  context2 = capture_log_context();

  EXPECT_EQ(std::string("[A][1]"), context0.context);
  EXPECT_EQ(std::string("[A]"), context1.context);
  EXPECT_EQ(std::string(""), context2.context);
}

TEST(LogScopeTest, SaveAndRestoreContext) {
  LogContext context0;
  {
    bt_log_scope("A");
    bt_log_scope("B");
    context0 = capture_log_context();
  }
  EXPECT_EQ(std::string("[A][B]"), context0.context);

  LogContext context1;
  {
    add_parent_context(context0);
    context1 = capture_log_context();
  }
  EXPECT_EQ(std::string("{[A][B]}"), context1.context);

  LogContext context2;
  {
    bt_log_scope("C");
    add_parent_context(context0);
    context2 = capture_log_context();
  }
  EXPECT_EQ(std::string("{[A][B]}[C]"), context2.context);

  LogContext context3;
  {
    bt_log_scope("C");
    add_parent_context(context0);
    bt_log_scope("D");
    context3 = capture_log_context();
  }
  EXPECT_EQ(std::string("{[A][B]}[C][D]"), context3.context);

  LogContext context4;
  {
    bt_log_scope("E");
    add_parent_context(context3);
    context4 = capture_log_context();
  }
  EXPECT_EQ(std::string("{{[A][B]}[C][D]}[E]"), context4.context);

  LogContext context5;
  {
    add_parent_context(context0);
    add_parent_context(context0);
    add_parent_context(context0);
    context5 = capture_log_context();
  }
  EXPECT_EQ(std::string("{[A][B],[A][B],[A][B]}"), context5.context);
}

TEST(LogScopeTest, DoubleContextBraces) {
  LogContext ctx;
  {
    bt_log_scope("A");
    ctx = capture_log_context();
  }
  {
    add_parent_context(ctx);
    ctx = capture_log_context();
  }
  {
    add_parent_context(ctx);
    ctx = capture_log_context();
  }
  EXPECT_EQ(std::string("{{[A]}}"), ctx.context);
}

TEST(LogScopeTest, SaveAndRestoreEmptyContext) {
  LogContext ctx = capture_log_context();
  add_parent_context(ctx);
  EXPECT_EQ(std::string(""), capture_log_context().context);
}

}  // namespace
}  // namespace bt
