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
    reader_.Seek(0);
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

TEST(Transfer, Read_SingleChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);
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

TEST(Transfer, Read_MultiChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);
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

TEST(Transfer, Read_MaxChunkSize_Client) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read, 5, 64) ctx(64, 64);
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

TEST(Transfer, Read_MaxChunkSize_Server) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read, 5, 64)
  ctx(/*max_chunk_size_bytes=*/8, 64);
  ctx.service().RegisterHandler(handler);

  ctx.call();
  EXPECT_FALSE(handler.prepare_read_called);
  EXPECT_FALSE(handler.finalize_read_called);

  // Client asks for max 16-byte chunks, but service places a limit of 8 bytes.
  ctx.SendClientStream(EncodeChunk({.transfer_id = 3,
                                    .pending_bytes = 64,
                                    .max_chunk_size_bytes = 16,
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

TEST(Transfer, Read_ClientError) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);
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

TEST(Transfer, Read_MalformedParametersChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SimpleReadTransfer handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);
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

TEST(Transfer, Read_UnregisteredHandler) {
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);

  ctx.call();
  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 11, .pending_bytes = 32, .offset = 0}));

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 11u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::NotFound());
}

class SimpleWriteTransfer final : public WriteOnlyHandler {
 public:
  SimpleWriteTransfer(uint32_t transfer_id, ByteSpan data)
      : WriteOnlyHandler(transfer_id),
        prepare_write_called(false),
        finalize_write_called(false),
        finalize_write_status(Status::Unknown()),
        writer_(data) {}

  Status PrepareWrite() final {
    writer_.Seek(0);
    set_writer(writer_);
    prepare_write_called = true;
    return OkStatus();
  }

  Status FinalizeWrite(Status status) final {
    finalize_write_called = true;
    finalize_write_status = status;
    return OkStatus();
  }

  bool prepare_write_called;
  bool finalize_write_called;
  Status finalize_write_status;

 private:
  stream::MemoryWriter writer_;
};

TEST(Transfer, Write_SingleChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);
  ASSERT_TRUE(chunk.max_chunk_size_bytes.has_value());
  EXPECT_EQ(chunk.max_chunk_size_bytes.value(), 42u);

  ctx.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                        .offset = 0,
                                        .data = std::span(data),
                                        .remaining_bytes = 0}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler.finalize_write_called);
  EXPECT_EQ(handler.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), data.data(), data.size()), 0);
}

TEST(Transfer, Write_MultiChunk) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(data).first(16)}));
  ASSERT_EQ(ctx.total_responses(), 1u);

  ctx.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                        .offset = 16,
                                        .data = std::span(data).subspan(16),
                                        .remaining_bytes = 0}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler.finalize_write_called);
  EXPECT_EQ(handler.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), data.data(), data.size()), 0);
}

TEST(Transfer, Write_MultipleParameters) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 16);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(data).first(8)}));
  ASSERT_EQ(ctx.total_responses(), 1u);

  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 8, .data = std::span(data).subspan(8, 8)}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7,
                   .offset = 16,
                   .data = std::span(data).subspan(16, 8)}));
  ASSERT_EQ(ctx.total_responses(), 2u);

  ctx.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                        .offset = 24,
                                        .data = std::span(data).subspan(24),
                                        .remaining_bytes = 0}));
  ASSERT_EQ(ctx.total_responses(), 3u);
  chunk = DecodeChunk(ctx.responses()[2]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler.finalize_write_called);
  EXPECT_EQ(handler.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), data.data(), data.size()), 0);
}

TEST(Transfer, Write_SetsDefaultPendingBytes) {
  // Constructor's default max bytes is smaller than buffer.
  std::array<std::byte, 32> buffer = {};
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 16);

  SimpleWriteTransfer handler(7, buffer);
  ctx.service().RegisterHandler(handler);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);
}

TEST(Transfer, Write_SetsWriterPendingBytes) {
  // Buffer is smaller than constructor's default max bytes.
  std::array<std::byte, 8> buffer = {};
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);

  SimpleWriteTransfer handler(7, buffer);
  ctx.service().RegisterHandler(handler);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.pending_bytes.value(), 8u);
}

TEST(Transfer, Write_UnexpectedOffset) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 0u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(data).first(16)}));
  ASSERT_EQ(ctx.total_responses(), 1u);

  ctx.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                        .offset = 8,  // incorrect
                                        .data = std::span(data).subspan(16),
                                        .remaining_bytes = 0}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 16u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                        .offset = 16,  // incorrect
                                        .data = std::span(data).subspan(16),
                                        .remaining_bytes = 0}));
  ASSERT_EQ(ctx.total_responses(), 3u);
  chunk = DecodeChunk(ctx.responses()[2]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler.finalize_write_called);
  EXPECT_EQ(handler.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), data.data(), data.size()), 0);
}

TEST(Transfer, Write_TooMuchData) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 16);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  // pending_bytes = 16
  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(data).first(8)}));
  ASSERT_EQ(ctx.total_responses(), 1u);

  // pending_bytes = 8
  ctx.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 8, .data = std::span(data).subspan(8, 4)}));
  ASSERT_EQ(ctx.total_responses(), 1u);

  // pending_bytes = 4 but send 8 instead
  ctx.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7,
                   .offset = 12,
                   .data = std::span(data).subspan(12, 8)}));
  ASSERT_EQ(ctx.total_responses(), 2u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::Internal());
}

TEST(Transfer, Write_UnregisteredHandler) {
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::NotFound());
}

TEST(Transfer, Write_ClientError) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  std::array<std::byte, sizeof(data)> buffer = {};
  SimpleWriteTransfer handler(7, buffer);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx(64, 64);
  ctx.service().RegisterHandler(handler);

  EXPECT_FALSE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ctx.call();
  ctx.SendClientStream(EncodeChunk({.transfer_id = 7}));

  EXPECT_TRUE(handler.prepare_write_called);
  EXPECT_FALSE(handler.finalize_write_called);

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7, .status = Status::DataLoss()}));
  EXPECT_EQ(ctx.total_responses(), 1u);

  EXPECT_TRUE(handler.finalize_write_called);
  EXPECT_EQ(handler.finalize_write_status, Status::DataLoss());
}

class SometimesUnavailableReadHandler final : public ReadOnlyHandler {
 public:
  SometimesUnavailableReadHandler(uint32_t transfer_id, ConstByteSpan data)
      : ReadOnlyHandler(transfer_id), reader_(data), call_count_(0) {}

  Status PrepareRead() final {
    if ((call_count_++ % 2) == 0) {
      return Status::Unavailable();
    }

    set_reader(reader_);
    return OkStatus();
  }

 private:
  stream::MemoryReader reader_;
  int call_count_;
};

TEST(Transfer, PrepareError) {
  constexpr auto data = bytes::Initialized<32>([](size_t i) { return i; });
  SometimesUnavailableReadHandler handler(3, data);

  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx(64, 64);
  ctx.service().RegisterHandler(handler);

  ctx.call();
  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 128, .offset = 0}));

  ASSERT_EQ(ctx.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 3u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::Unavailable());

  // Try starting the transfer again. It should work this time.
  ctx.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 128, .offset = 0}));
  ASSERT_EQ(ctx.total_responses(), 3u);
  chunk = DecodeChunk(ctx.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 3u);
  ASSERT_EQ(chunk.data.size(), data.size());
  EXPECT_EQ(std::memcmp(chunk.data.data(), data.data(), chunk.data.size()), 0);
}

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace
}  // namespace pw::transfer
