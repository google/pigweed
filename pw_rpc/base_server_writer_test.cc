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

#include "pw_rpc/internal/base_server_writer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"
#include "pw_rpc/internal/service.h"
#include "pw_rpc/server_context.h"
#include "pw_rpc_private/test_utils.h"

namespace pw::rpc {

class TestService : public internal::Service {
 public:
  constexpr TestService(uint32_t id)
      : Service(id, span(&method, 1)), method(8) {}

  internal::Method method;
};

namespace internal {
namespace {

using std::byte;

TEST(BaseServerWriter, ConstructWithContext_StartsOpen) {
  ServerContextForTest<TestService> context;

  BaseServerWriter writer(context.get());

  EXPECT_TRUE(writer.open());
}

TEST(BaseServerWriter, Move_ClosesOriginal) {
  ServerContextForTest<TestService> context;

  BaseServerWriter moved(context.get());
  BaseServerWriter writer(std::move(moved));

  EXPECT_FALSE(moved.open());
  EXPECT_TRUE(writer.open());
}

class FakeServerWriter : public BaseServerWriter {
 public:
  constexpr FakeServerWriter(ServerCall& context) : BaseServerWriter(context) {}

  constexpr FakeServerWriter() = default;

  Status Write(span<const byte> response) {
    span buffer = AcquireBuffer();
    std::memcpy(buffer.data(),
                response.data(),
                std::min(buffer.size(), response.size()));
    return SendAndReleaseBuffer(buffer.first(response.size()));
  }
};

TEST(ServerWriter, DefaultConstruct_Closed) {
  FakeServerWriter writer;

  EXPECT_FALSE(writer.open());
}

TEST(ServerWriter, Close) {
  ServerContextForTest<TestService> context;
  FakeServerWriter writer(context.get());

  ASSERT_TRUE(writer.open());
  writer.close();
  EXPECT_FALSE(writer.open());
}

TEST(ServerWriter, Open_SendsPacketWithPayload) {
  ServerContextForTest<TestService> context;
  FakeServerWriter writer(context.get());

  constexpr byte data[] = {byte{0xf0}, byte{0x0d}};
  ASSERT_EQ(Status::OK, writer.Write(data));

  byte encoded[64];
  auto sws = context.packet(data).Encode(encoded);
  ASSERT_EQ(Status::OK, sws.status());

  EXPECT_EQ(sws.size(), context.output().sent_packet().size());
  EXPECT_EQ(
      0,
      std::memcmp(encoded, context.output().sent_packet().data(), sws.size()));
}

TEST(ServerWriter, Closed_IgnoresPacket) {
  ServerContextForTest<TestService> context;
  FakeServerWriter writer(context.get());

  writer.close();

  constexpr byte data[] = {byte{0xf0}, byte{0x0d}};
  EXPECT_EQ(Status::FAILED_PRECONDITION, writer.Write(data));
}

}  // namespace
}  // namespace internal
}  // namespace pw::rpc
