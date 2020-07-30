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

#include "gtest/gtest.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/test_method_context.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"

namespace pw::rpc {
namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  Status TestRpc(ServerContext&,
                 const pw_rpc_test_TestRequest& request,
                 pw_rpc_test_TestResponse& response) {
    response.value = request.integer + 1;
    return static_cast<Status::Code>(request.status_code);
  }

  void TestStreamRpc(ServerContext&,
                     const pw_rpc_test_TestRequest& request,
                     ServerWriter<pw_rpc_test_TestStreamResponse>& writer) {
    for (int i = 0; i < request.integer; ++i) {
      writer.Write({.number = static_cast<uint32_t>(i)});
    }

    writer.Finish(static_cast<Status::Code>(request.status_code));
  }
};

}  // namespace test

namespace {

TEST(NanopbCodegen, CompilesProperly) {
  test::TestService service;
  EXPECT_EQ(service.id(), internal::Hash("pw.rpc.test.TestService"));
  EXPECT_STREQ(service.name(), "TestService");
}

TEST(NanopbCodegen, InvokeUnaryRpc) {
  TestMethodContext<&test::TestService::TestRpc> context;

  EXPECT_EQ(Status::OK,
            context.call({.integer = 123, .status_code = Status::OK}));

  EXPECT_EQ(124, context.response().value);

  EXPECT_EQ(
      Status::INVALID_ARGUMENT,
      context.call({.integer = 999, .status_code = Status::INVALID_ARGUMENT}));
  EXPECT_EQ(1000, context.response().value);
}

TEST(NanopbCodegen, InvokeStreamingRpc) {
  TestMethodContext<&test::TestService::TestStreamRpc> context;

  context.call({.integer = 0, .status_code = Status::ABORTED});

  EXPECT_EQ(Status::ABORTED, context.status());
  EXPECT_TRUE(context.done());
  EXPECT_TRUE(context.responses().empty());
  EXPECT_EQ(0u, context.total_responses());

  context.call({.integer = 4, .status_code = Status::OK});

  ASSERT_EQ(4u, context.responses().size());
  ASSERT_EQ(4u, context.total_responses());

  for (size_t i = 0; i < context.responses().size(); ++i) {
    EXPECT_EQ(context.responses()[i].number, i);
  }

  EXPECT_EQ(Status::OK, context.status());
}

TEST(NanopbCodegen, InvokeStreamingRpc_ContextKeepsFixedNumberOfResponses) {
  TestMethodContext<&test::TestService::TestStreamRpc, 3> context;

  ASSERT_EQ(3u, context.responses().max_size());

  context.call({.integer = 5, .status_code = Status::NOT_FOUND});

  ASSERT_EQ(3u, context.responses().size());
  ASSERT_EQ(5u, context.total_responses());

  EXPECT_EQ(context.responses()[0].number, 0u);
  EXPECT_EQ(context.responses()[1].number, 1u);
  EXPECT_EQ(context.responses()[2].number, 4u);
}

TEST(NanopbCodegen, InvokeStreamingRpc_ManualWriting) {
  TestMethodContext<&test::TestService::TestStreamRpc, 3> context;

  ASSERT_EQ(3u, context.responses().max_size());

  auto writer = context.writer();

  writer.Write({.number = 3});
  writer.Write({.number = 6});
  writer.Write({.number = 9});

  EXPECT_FALSE(context.done());

  writer.Finish(Status::CANCELLED);
  ASSERT_TRUE(context.done());
  EXPECT_EQ(Status::CANCELLED, context.status());

  ASSERT_EQ(3u, context.responses().size());
  ASSERT_EQ(3u, context.total_responses());

  EXPECT_EQ(context.responses()[0].number, 3u);
  EXPECT_EQ(context.responses()[1].number, 6u);
  EXPECT_EQ(context.responses()[2].number, 9u);
}

}  // namespace
}  // namespace pw::rpc
