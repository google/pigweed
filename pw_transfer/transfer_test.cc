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

#include "pw_transfer/transfer.h"

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk.h"

namespace pw::transfer {
namespace {

PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC(ignored, "-Wmissing-field-initializers");

using internal::Chunk;

Vector<std::byte, 64> EncodeChunk(const Chunk& chunk) {
  Vector<std::byte, 64> buffer(64);
  auto result = internal::EncodeChunk(chunk, buffer);
  EXPECT_EQ(result.status(), OkStatus());
  buffer.resize(result.value().size());
  return buffer;
}

Chunk DecodeChunk(ConstByteSpan buffer) {
  Chunk chunk = {};
  EXPECT_EQ(internal::DecodeChunk(buffer, chunk), OkStatus());
  return chunk;
}

class SimpleReadTransfer final : public ReadOnlyHandler {
 public:
  SimpleReadTransfer(uint32_t transfer_id, ConstByteSpan data)
      : ReadOnlyHandler(transfer_id),
        prepare_read_called(false),
        finalize_read_called(false),
        finalize_read_status(Status::Unknown()),
        reader_(data) {}

  Status PrepareRead() final {
    // reader_.Seek(0);
    set_reader(reader_);
    prepare_read_called = true;
    return OkStatus();
  }

  void FinalizeRead(Status status) final {
    finalize_read_called = true;
    finalize_read_status = status;
  }

  bool prepare_read_called;
  bool finalize_read_called;
  Status finalize_read_status;

 private:
  stream::MemoryReader reader_;
};

TEST(Tranfser, Read_SingleChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx;
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 64, .offset = 0}));
  EXPECT_TRUE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ASSERT_EQ(ctx.total_responses(), 2u);
  Chunk c0 = DecodeChunk(ctx.responses()[0]);
  Chunk c1 = DecodeChunk(ctx.responses()[1]);

  // First chunk should have all the read data.
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), data.size());
  EXPECT_EQ(std::memcmp(c0.data.data(), data.data(), c0.data.size()), 0);

  // Second chunk should be empty and set remaining_bytes = 0.
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.data.size(), 0u);
  ASSERT_TRUE(c1.remaining_bytes.has_value());
  EXPECT_EQ(c1.remaining_bytes.value(), 0u);

  ctx.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  EXPECT_TRUE(handler.finalize_read_called);
  EXPECT_EQ(handler.finalize_read_status, OkStatus());
}

TEST(Tranfser, Read_MultiChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx;
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 0}));
  EXPECT_TRUE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk c0 = DecodeChunk(ctx.responses()[0]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c0.data.data(), data.data(), c0.data.size()), 0);

  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 16}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  Chunk c1 = DecodeChunk(ctx.responses()[1]);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 16u);
  ASSERT_EQ(c1.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c1.data.data(), data.data() + 16, c1.data.size()), 0);

  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 32}));
  ASSERT_EQ(ctx.total_responses(), 3u);
  Chunk c2 = DecodeChunk(ctx.responses()[2]);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.data.size(), 0u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  ctx.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  EXPECT_TRUE(handler.finalize_read_called);
  EXPECT_EQ(handler.finalize_read_status, OkStatus());
}

TEST(Tranfser, Read_MaxChunkSize) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read, 5, 64) ctx;
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ctx.SendClientStream(EncodeChunk({.transfer_id = 3,
                                    .pending_bytes = 64,
                                    .max_chunk_size_bytes = 8,
                                    .offset = 0}));
  EXPECT_TRUE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ASSERT_EQ(ctx.total_responses(), 5u);
  Chunk c0 = DecodeChunk(ctx.responses()[0]);
  Chunk c1 = DecodeChunk(ctx.responses()[1]);
  Chunk c2 = DecodeChunk(ctx.responses()[2]);
  Chunk c3 = DecodeChunk(ctx.responses()[3]);
  Chunk c4 = DecodeChunk(ctx.responses()[4]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c0.data.data(), data.data(), c0.data.size()), 0);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 8u);
  ASSERT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), data.data() + 8, c1.data.size()), 0);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.offset, 16u);
  ASSERT_EQ(c2.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c2.data.data(), data.data() + 16, c2.data.size()), 0);

  EXPECT_EQ(c3.transfer_id, 3u);
  EXPECT_EQ(c3.offset, 24u);
  ASSERT_EQ(c3.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c3.data.data(), data.data() + 24, c3.data.size()), 0);

  EXPECT_EQ(c4.transfer_id, 3u);
  EXPECT_EQ(c4.data.size(), 0u);
  ASSERT_TRUE(c4.remaining_bytes.has_value());
  EXPECT_EQ(c4.remaining_bytes.value(), 0u);

  ctx.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  EXPECT_TRUE(handler.finalize_read_called);
  EXPECT_EQ(handler.finalize_read_status, OkStatus());
}

TEST(Tranfser, Read_ClientError) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx;
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 0}));
  EXPECT_TRUE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);
  ASSERT_EQ(ctx.total_responses(), 1u);

  // Send client error.
  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .status = Status::OutOfRange()}));
  ASSERT_EQ(ctx.total_responses(), 1u);
  EXPECT_TRUE(handler.finalize_read_called);
  EXPECT_EQ(handler.finalize_read_status, Status::OutOfRange());
}

TEST(Tranfser, Read_MalformedParametersChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx;
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  // pending_bytes is required in a parameters chunk.
  ctx.SendClientStream(EncodeChunk({.transfer_id = 3}));
  EXPECT_TRUE(handler.prepare_read_called);
  EXPECT_TRUE(handler.finalize_read_called);
  EXPECT_EQ(handler.finalize_read_status, Status::InvalidArgument());

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 3u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::InvalidArgument());
}

TEST(Tranfser, Read_UnregisteredHandler) {
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx;

  ctx.call();
  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 11, .pending_bytes = 32, .offset = 0}));

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 11u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::NotFound());
}

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace
}  // namespace pw::transfer
