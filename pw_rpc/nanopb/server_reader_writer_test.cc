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

class TestServiceImpl final
    : public test::generated::TestService<TestServiceImpl> {
 public:
  Status TestUnaryRpc(ServerContext&,
                      const pw_rpc_test_TestRequest&,
                      pw_rpc_test_TestResponse&) {
    return OkStatus();
  }

  void TestAnotherUnaryRpc(ServerContext&,
                           const pw_rpc_test_TestRequest&,
                           NanopbServerResponder<pw_rpc_test_TestResponse>&) {}

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

template <auto kMethod>
struct ReaderWriterTestContext {
  using Info = internal::MethodInfo<kMethod>;

  ReaderWriterTestContext()
      : output(decltype(output)::template Create<
               TestServiceImpl,
               Info::template Function<TestServiceImpl>(),
               Info::kMethodId>()),
        channel(Channel::Create<1>(&output)),
        server(std::span(&channel, 1)) {}

  TestServiceImpl service;
  NanopbFakeChannelOutput<typename Info::Response, 4, 128> output;
  Channel channel;
  Server server;
};

using test::pw_rpc::nanopb::TestService;

TEST(NanopbServerResponder, Open_ReturnsUsableResponder) {
  ReaderWriterTestContext<TestService::TestUnaryRpc> ctx;
  NanopbServerResponder responder =
      NanopbServerResponder<pw_rpc_test_TestResponse>::Open<
          TestService::TestUnaryRpc>(ctx.server, ctx.channel.id(), ctx.service);

  responder.Finish({.value = 4321});

  EXPECT_EQ(ctx.output.last_response().value, 4321);
  EXPECT_EQ(ctx.output.last_status(), OkStatus());
}

TEST(NanopbServerWriter, Open_ReturnsUsableWriter) {
  ReaderWriterTestContext<TestService::TestServerStreamRpc> ctx;
  NanopbServerWriter responder =
      NanopbServerWriter<pw_rpc_test_TestStreamResponse>::Open<
          TestService::TestServerStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  responder.Write({.chunk = {}, .number = 321});
  responder.Finish();

  EXPECT_EQ(ctx.output.last_response().number, 321u);
  EXPECT_EQ(ctx.output.last_status(), OkStatus());
}

TEST(NanopbServerReader, Open_ReturnsUsableReader) {
  ReaderWriterTestContext<TestService::TestClientStreamRpc> ctx;
  NanopbServerReader responder =
      NanopbServerReader<pw_rpc_test_TestRequest,
                         pw_rpc_test_TestStreamResponse>::
          Open<TestService::TestClientStreamRpc>(
              ctx.server, ctx.channel.id(), ctx.service);

  responder.Finish({.chunk = {}, .number = 321});

  EXPECT_EQ(ctx.output.last_response().number, 321u);
}

TEST(NanopbServerReaderWriter, Open_ReturnsUsableReaderWriter) {
  ReaderWriterTestContext<TestService::TestBidirectionalStreamRpc> ctx;
  NanopbServerReaderWriter responder =
      NanopbServerReaderWriter<pw_rpc_test_TestRequest,
                               pw_rpc_test_TestStreamResponse>::
          Open<TestService::TestBidirectionalStreamRpc>(
              ctx.server, ctx.channel.id(), ctx.service);

  responder.Write({.chunk = {}, .number = 321});
  responder.Finish(Status::NotFound());

  EXPECT_EQ(ctx.output.last_response().number, 321u);
  EXPECT_EQ(ctx.output.last_status(), Status::NotFound());
}

}  // namespace pw::rpc
