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
#include "pw_rpc/raw/fake_channel_output.h"
#include "pw_rpc/service.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {

class TestServiceImpl final
    : public test::generated::TestService<TestServiceImpl> {
 public:
  static StatusWithSize TestUnaryRpc(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  void TestAnotherUnaryRpc(ServerContext&, ConstByteSpan, RawServerResponder&) {
  }

  void TestServerStreamRpc(ServerContext&, ConstByteSpan, RawServerWriter&) {}

  void TestClientStreamRpc(ServerContext&, RawServerReader&) {}

  void TestBidirectionalStreamRpc(ServerContext&, RawServerReaderWriter&) {}
};

struct ReaderWriterTestContext {
  ReaderWriterTestContext(MethodType type)
      : output(type),
        channel(Channel::Create<1>(&output)),
        server(std::span(&channel, 1)) {}

  TestServiceImpl service;
  RawFakeChannelOutput<128, 4> output;
  Channel channel;
  Server server;
};

using test::pw_rpc::raw::TestService;

TEST(RawServerResponder, Open_ReturnsUsableResponder) {
  ReaderWriterTestContext ctx(MethodType::kUnary);
  RawServerResponder call = RawServerResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Finish(std::as_bytes(std::span("hello from pw_rpc")));

  EXPECT_STREQ(reinterpret_cast<const char*>(ctx.output.last_response().data()),
               "hello from pw_rpc");
}

TEST(RawServerResponder, Open_MultipleTimes_CancelsPrevious) {
  ReaderWriterTestContext ctx(MethodType::kUnary);

  RawServerResponder one = RawServerResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  ASSERT_TRUE(one.active());

  RawServerResponder two = RawServerResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  ASSERT_FALSE(one.active());
  ASSERT_TRUE(two.active());
}

TEST(RawServerWriter, Open_ReturnsUsableWriter) {
  ReaderWriterTestContext ctx(MethodType::kServerStreaming);
  RawServerWriter call =
      RawServerWriter::Open<TestService::TestServerStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Write(std::as_bytes(std::span("321")));

  EXPECT_STREQ(reinterpret_cast<const char*>(ctx.output.last_response().data()),
               "321");
}

TEST(RawServerReader, Open_ReturnsUsableReader) {
  ReaderWriterTestContext ctx(MethodType::kClientStreaming);
  RawServerReader call =
      RawServerReader::Open<TestService::TestClientStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Finish(std::as_bytes(std::span("This is a message")));

  EXPECT_STREQ(reinterpret_cast<const char*>(ctx.output.last_response().data()),
               "This is a message");
}

TEST(RawServerReaderWriter, Open_ReturnsUsableReaderWriter) {
  ReaderWriterTestContext ctx(MethodType::kBidirectionalStreaming);
  RawServerReaderWriter call =
      RawServerReaderWriter::Open<TestService::TestBidirectionalStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Write(std::as_bytes(std::span("321")));

  EXPECT_STREQ(reinterpret_cast<const char*>(ctx.output.last_response().data()),
               "321");
}

}  // namespace pw::rpc
