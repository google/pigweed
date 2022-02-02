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

#include "pw_rpc/internal/call.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"
#include "pw_rpc/internal/test_method.h"
#include "pw_rpc/internal/test_utils.h"
#include "pw_rpc/service.h"
#include "pw_rpc_private/fake_server_reader_writer.h"

namespace pw::rpc {

class TestService : public Service {
 public:
  constexpr TestService(uint32_t id) : Service(id, method) {}

  static constexpr internal::TestMethodUnion method = internal::TestMethod(8);
};

namespace internal {
namespace {

constexpr Packet kPacket(PacketType::REQUEST, 99, 16, 8);

using pw::rpc::internal::test::FakeServerWriter;
using std::byte;

TEST(ServerWriter, ConstructWithContext_StartsOpen) {
  ServerContextForTest<TestService> context(TestService::method.method());

  FakeServerWriter writer(context.get());

  EXPECT_TRUE(writer.active());
}

TEST(ServerWriter, Move_ClosesOriginal) {
  ServerContextForTest<TestService> context(TestService::method.method());

  FakeServerWriter moved(context.get());
  FakeServerWriter writer(std::move(moved));

#ifndef __clang_analyzer__
  EXPECT_FALSE(moved.active());
#endif  // ignore use-after-move
  EXPECT_TRUE(writer.active());
}

TEST(ServerWriter, DefaultConstruct_Closed) {
  FakeServerWriter writer;

  EXPECT_FALSE(writer.active());
}

TEST(ServerWriter, Construct_RegistersWithServer) PW_NO_LOCK_SAFETY_ANALYSIS {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  Call* call = context.server().FindCall(kPacket);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(static_cast<void*>(call), static_cast<void*>(&writer));
}

TEST(ServerWriter, Destruct_RemovesFromServer) PW_NO_LOCK_SAFETY_ANALYSIS {
  ServerContextForTest<TestService> context(TestService::method.method());
  { FakeServerWriter writer(context.get()); }

  EXPECT_EQ(context.server().FindCall(kPacket), nullptr);
}

TEST(ServerWriter, Finish_RemovesFromServer) PW_NO_LOCK_SAFETY_ANALYSIS {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  EXPECT_EQ(OkStatus(), writer.Finish());

  EXPECT_EQ(context.server().FindCall(kPacket), nullptr);
}

TEST(ServerWriter, Finish_SendsResponse) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  EXPECT_EQ(OkStatus(), writer.Finish());

  ASSERT_EQ(context.output().total_packets(), 1u);
  const Packet& packet = context.output().last_packet();
  EXPECT_EQ(packet.type(), PacketType::RESPONSE);
  EXPECT_EQ(packet.channel_id(), context.channel_id());
  EXPECT_EQ(packet.service_id(), context.service_id());
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_TRUE(packet.payload().empty());
  EXPECT_EQ(packet.status(), OkStatus());
}

TEST(ServerWriter, Finish_ReturnsStatusFromChannelSend) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());
  context.output().set_send_status(Status::Unauthenticated());

  // All non-OK statuses are remapped to UNKNOWN.
  EXPECT_EQ(Status::Unknown(), writer.Finish());
}

TEST(ServerWriter, Finish) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  ASSERT_TRUE(writer.active());
  EXPECT_EQ(OkStatus(), writer.Finish());
  EXPECT_FALSE(writer.active());
  EXPECT_EQ(Status::FailedPrecondition(), writer.Finish());
}

TEST(ServerWriter, Open_SendsPacketWithPayload) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  constexpr byte data[] = {byte{0xf0}, byte{0x0d}};
  ASSERT_EQ(OkStatus(), writer.Write(data));

  byte encoded[64];
  auto result = context.server_stream(data).Encode(encoded);
  ASSERT_EQ(OkStatus(), result.status());

  ConstByteSpan payload = context.output().last_packet().payload();
  EXPECT_EQ(sizeof(data), payload.size());
  EXPECT_EQ(0, std::memcmp(data, payload.data(), sizeof(data)));
}

TEST(ServerWriter, Closed_IgnoresFinish) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  EXPECT_EQ(OkStatus(), writer.Finish());
  EXPECT_EQ(Status::FailedPrecondition(), writer.Finish());
}

TEST(ServerWriter, DefaultConstructor_NoClientStream) {
  FakeServerWriter writer;
  LockGuard lock(rpc_lock());
  EXPECT_FALSE(writer.as_server_call().has_client_stream());
  EXPECT_FALSE(writer.as_server_call().client_stream_open());
}

TEST(ServerWriter, Open_NoClientStream) {
  ServerContextForTest<TestService> context(TestService::method.method());
  FakeServerWriter writer(context.get());

  LockGuard lock(rpc_lock());
  EXPECT_FALSE(writer.as_server_call().has_client_stream());
  EXPECT_FALSE(writer.as_server_call().client_stream_open());
}

TEST(ServerReader, DefaultConstructor_ClientStreamClosed) {
  test::FakeServerReader reader;
  EXPECT_FALSE(reader.as_server_call().active());
  LockGuard lock(rpc_lock());
  EXPECT_FALSE(reader.as_server_call().client_stream_open());
}

TEST(ServerReader, Open_ClientStreamStartsOpen) {
  ServerContextForTest<TestService> context(TestService::method.method());
  test::FakeServerReader reader(context.get());

  LockGuard lock(rpc_lock());
  EXPECT_TRUE(reader.as_server_call().has_client_stream());
  EXPECT_TRUE(reader.as_server_call().client_stream_open());
}

TEST(ServerReader, Close_ClosesClientStream) {
  ServerContextForTest<TestService> context(TestService::method.method());
  test::FakeServerReader reader(context.get());

  EXPECT_TRUE(reader.as_server_call().active());
  rpc_lock().lock();
  EXPECT_TRUE(reader.as_server_call().client_stream_open());
  rpc_lock().unlock();
  EXPECT_EQ(OkStatus(),
            reader.as_server_call().CloseAndSendResponse(OkStatus()));

  EXPECT_FALSE(reader.as_server_call().active());
  LockGuard lock(rpc_lock());
  EXPECT_FALSE(reader.as_server_call().client_stream_open());
}

TEST(ServerReader, EndClientStream_OnlyClosesClientStream) {
  ServerContextForTest<TestService> context(TestService::method.method());
  test::FakeServerReader reader(context.get());

  EXPECT_TRUE(reader.active());
  rpc_lock().lock();
  EXPECT_TRUE(reader.as_server_call().client_stream_open());
  reader.as_server_call().HandleClientStreamEnd();

  EXPECT_TRUE(reader.active());
  LockGuard lock(rpc_lock());
  EXPECT_FALSE(reader.as_server_call().client_stream_open());
}

TEST(ServerReaderWriter, Move_MaintainsClientStream) {
  ServerContextForTest<TestService> context(TestService::method.method());
  test::FakeServerReaderWriter reader_writer(context.get());
  test::FakeServerReaderWriter destination;

  rpc_lock().lock();
  EXPECT_FALSE(destination.as_server_call().client_stream_open());
  rpc_lock().unlock();

  destination = std::move(reader_writer);
  LockGuard lock(rpc_lock());
  EXPECT_TRUE(destination.as_server_call().has_client_stream());
  EXPECT_TRUE(destination.as_server_call().client_stream_open());
}

TEST(ServerReaderWriter, Move_MovesCallbacks) {
  ServerContextForTest<TestService> context(TestService::method.method());
  test::FakeServerReaderWriter reader_writer(context.get());

  int calls = 0;
  reader_writer.set_on_error([&calls](Status) { calls += 1; });
  reader_writer.set_on_next([&calls](ConstByteSpan) { calls += 1; });

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  reader_writer.set_on_client_stream_end([&calls]() { calls += 1; });
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK

  test::FakeServerReaderWriter destination(std::move(reader_writer));
  rpc_lock().lock();
  destination.as_server_call().HandlePayload({});
  rpc_lock().lock();
  destination.as_server_call().HandleClientStreamEnd();
  rpc_lock().lock();
  destination.as_server_call().HandleError(Status::Unknown());

  EXPECT_EQ(calls, 2 + PW_RPC_CLIENT_STREAM_END_CALLBACK);
}

}  // namespace
}  // namespace internal
}  // namespace pw::rpc
