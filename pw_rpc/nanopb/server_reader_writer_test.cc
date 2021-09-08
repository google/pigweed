// Copyright 2021 The Pigweed Authors
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

#include "pw_rpc/raw/server_reader_writer.h"

#include "gtest/gtest.h"
#include "pw_rpc/nanopb/fake_channel_output.h"
#include "pw_rpc/service.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"

namespace pw::rpc {

class TestService final : public test::generated::TestService<TestService> {
 public:
  Status TestUnaryRpc(ServerContext&,
                      const pw_rpc_test_TestRequest&,
                      pw_rpc_test_TestResponse&) {
    return OkStatus();
  }

  void TestServerStreamRpc(
      ServerContext&,
      const pw_rpc_test_TestRequest&,
      NanopbServerWriter<pw_rpc_test_TestStreamResponse>&) {}

  void TestClientStreamRpc(
      ServerContext&,
      NanopbServerReader<pw_rpc_test_TestRequest,
                         pw_rpc_test_TestStreamResponse>&) {}

  void TestBidirectionalStreamRpc(
      ServerContext&,
      NanopbServerReaderWriter<pw_rpc_test_TestRequest,
                               pw_rpc_test_TestStreamResponse>&) {}
};

template <auto kMethod, uint32_t kMethodId>
struct ReaderWriterTestContext {
  ReaderWriterTestContext()
      : output(decltype(output)::
                   template Create<TestService, kMethod, kMethodId>()),
        channel(Channel::Create<1>(&output)),
        server(std::span(&channel, 1)) {}

  TestService service;
  NanopbFakeChannelOutput<internal::Response<kMethod>, 4, 128> output;
  Channel channel;
  Server server;
};

TEST(NanopbServerWriter, Open_ReturnsUsableWriter) {
  ReaderWriterTestContext<&TestService::TestServerStreamRpc,
                          CalculateMethodId("TestServerStreamRpc")>
      ctx;
  NanopbServerWriter responder =
      NanopbServerWriter<pw_rpc_test_TestStreamResponse>::Open<
          &TestService::TestServerStreamRpc,
          CalculateMethodId("TestServerStreamRpc")>(
          ctx.server, ctx.channel.id(), ctx.service);

  responder.Write({.chunk = {}, .number = 321});
  responder.Finish();

  EXPECT_EQ(ctx.output.last_response().number, 321u);
  EXPECT_EQ(ctx.output.last_status(), OkStatus());
}

TEST(NanopbServerReader, Open_ReturnsUsableReader) {
  ReaderWriterTestContext<&TestService::TestClientStreamRpc,
                          CalculateMethodId("TestClientStreamRpc")>
      ctx;
  NanopbServerReader responder =
      NanopbServerReader<pw_rpc_test_TestRequest,
                         pw_rpc_test_TestStreamResponse>::
          Open<&TestService::TestClientStreamRpc,
               CalculateMethodId("TestClientStreamRpc")>(
              ctx.server, ctx.channel.id(), ctx.service);

  responder.Finish({.chunk = {}, .number = 321});

  EXPECT_EQ(ctx.output.last_response().number, 321u);
}

TEST(NanopbServerReaderWriter, Open_ReturnsUsableReaderWriter) {
  ReaderWriterTestContext<&TestService::TestBidirectionalStreamRpc,
                          CalculateMethodId("TestBidirectionalStreamRpc")>
      ctx;
  NanopbServerReaderWriter responder =
      NanopbServerReaderWriter<pw_rpc_test_TestRequest,
                               pw_rpc_test_TestStreamResponse>::
          Open<&TestService::TestBidirectionalStreamRpc,
               CalculateMethodId("TestBidirectionalStreamRpc")>(
              ctx.server, ctx.channel.id(), ctx.service);

  responder.Write({.chunk = {}, .number = 321});
  responder.Finish(Status::NotFound());

  EXPECT_EQ(ctx.output.last_response().number, 321u);
  EXPECT_EQ(ctx.output.last_status(), Status::NotFound());
}

}  // namespace pw::rpc
