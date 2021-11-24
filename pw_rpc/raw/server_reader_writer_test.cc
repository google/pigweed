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
#include "pw_rpc/writer.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {

class TestServiceImpl final
    : public test::pw_rpc::raw::TestService::Service<TestServiceImpl> {
 public:
  static StatusWithSize TestUnaryRpc(ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  void TestAnotherUnaryRpc(ConstByteSpan, RawUnaryResponder&) {}

  void TestServerStreamRpc(ConstByteSpan, RawServerWriter&) {}

  void TestClientStreamRpc(RawServerReader&) {}

  void TestBidirectionalStreamRpc(RawServerReaderWriter&) {}
};

struct ReaderWriterTestContext {
  static constexpr uint32_t kChannelId = 1;

  ReaderWriterTestContext()
      : channel(Channel::Create<kChannelId>(&output)),
        server(std::span(&channel, 1)) {}

  TestServiceImpl service;
  RawFakeChannelOutput<4, 128> output;
  Channel channel;
  Server server;
};

using test::pw_rpc::raw::TestService;

TEST(RawUnaryResponder, Open_ReturnsUsableResponder) {
  ReaderWriterTestContext ctx;
  RawUnaryResponder call = RawUnaryResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Finish(std::as_bytes(std::span("hello from pw_rpc")));

  EXPECT_STREQ(
      reinterpret_cast<const char*>(
          ctx.output.payloads<TestService::TestUnaryRpc>().back().data()),
      "hello from pw_rpc");
}

TEST(RawUnaryResponder, Open_MultipleTimes_CancelsPrevious) {
  ReaderWriterTestContext ctx;

  RawUnaryResponder one = RawUnaryResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  ASSERT_TRUE(one.active());

  RawUnaryResponder two = RawUnaryResponder::Open<TestService::TestUnaryRpc>(
      ctx.server, ctx.channel.id(), ctx.service);

  ASSERT_FALSE(one.active());
  ASSERT_TRUE(two.active());
}

TEST(RawServerWriter, Open_ReturnsUsableWriter) {
  ReaderWriterTestContext ctx;
  RawServerWriter call =
      RawServerWriter::Open<TestService::TestServerStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Write(std::as_bytes(std::span("321")));

  EXPECT_STREQ(reinterpret_cast<const char*>(
                   ctx.output.payloads<TestService::TestServerStreamRpc>()
                       .back()
                       .data()),
               "321");
}

TEST(RawServerReader, Open_ReturnsUsableReader) {
  ReaderWriterTestContext ctx;
  RawServerReader call =
      RawServerReader::Open<TestService::TestClientStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Finish(std::as_bytes(std::span("This is a message")));

  EXPECT_STREQ(reinterpret_cast<const char*>(
                   ctx.output.payloads<TestService::TestClientStreamRpc>()
                       .back()
                       .data()),
               "This is a message");
}

TEST(RawServerReaderWriter, Open_ReturnsUsableReaderWriter) {
  ReaderWriterTestContext ctx;
  RawServerReaderWriter call =
      RawServerReaderWriter::Open<TestService::TestBidirectionalStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  EXPECT_EQ(call.channel_id(), ctx.channel.id());
  call.Write(std::as_bytes(std::span("321")));

  EXPECT_STREQ(
      reinterpret_cast<const char*>(
          ctx.output.payloads<TestService::TestBidirectionalStreamRpc>()
              .back()
              .data()),
      "321");
}

TEST(RawUnaryResponder, Move_FinishesActiveCall) {
  ReaderWriterTestContext ctx;
  RawUnaryResponder active_call =
      RawUnaryResponder::Open<TestService::TestUnaryRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  RawUnaryResponder inactive_call;

  active_call = std::move(inactive_call);

  const auto completions = ctx.output.completions<TestService::TestUnaryRpc>();
  ASSERT_EQ(completions.size(), 1u);
  EXPECT_EQ(completions.back(), OkStatus());
}

TEST(RawUnaryResponder, OutOfScope_FinishesActiveCall) {
  ReaderWriterTestContext ctx;

  {
    RawUnaryResponder call = RawUnaryResponder::Open<TestService::TestUnaryRpc>(
        ctx.server, ctx.channel.id(), ctx.service);
    ASSERT_TRUE(ctx.output.completions<TestService::TestUnaryRpc>().empty());
  }

  const auto completions = ctx.output.completions<TestService::TestUnaryRpc>();
  ASSERT_EQ(completions.size(), 1u);
  EXPECT_EQ(completions.back(), OkStatus());
}

constexpr const char kWriterData[] = "20X6";

void WriteAsWriter(Writer& writer) {
  ASSERT_TRUE(writer.active());
  ASSERT_EQ(writer.channel_id(), ReaderWriterTestContext::kChannelId);

  EXPECT_EQ(OkStatus(), writer.Write(std::as_bytes(std::span(kWriterData))));
}

TEST(RawServerWriter, UsableAsWriter) {
  ReaderWriterTestContext ctx;
  RawServerWriter call =
      RawServerWriter::Open<TestService::TestServerStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  WriteAsWriter(call);

  EXPECT_STREQ(reinterpret_cast<const char*>(
                   ctx.output.payloads<TestService::TestServerStreamRpc>()
                       .back()
                       .data()),
               kWriterData);
}

TEST(RawServerReaderWriter, UsableAsWriter) {
  ReaderWriterTestContext ctx;
  RawServerReaderWriter call =
      RawServerReaderWriter::Open<TestService::TestBidirectionalStreamRpc>(
          ctx.server, ctx.channel.id(), ctx.service);

  WriteAsWriter(call);

  EXPECT_STREQ(
      reinterpret_cast<const char*>(
          ctx.output.payloads<TestService::TestBidirectionalStreamRpc>()
              .back()
              .data()),
      kWriterData);
}

}  // namespace pw::rpc
