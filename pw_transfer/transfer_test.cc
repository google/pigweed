// Copyright 2022 The Pigweed Authors
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
#include "pw_rpc/thread_testing.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk_testing.h"

namespace pw::transfer::test {
namespace {

using namespace std::chrono_literals;

PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC(ignored, "-Wmissing-field-initializers");

// TODO(frolv): Have a generic way to obtain a thread for testing on any system.
thread::Options& TransferThreadOptions() {
  static thread::stl::Options options;
  return options;
}

using internal::Chunk;

class TestMemoryReader : public stream::SeekableReader {
 public:
  constexpr TestMemoryReader(std::span<const std::byte> data)
      : memory_reader_(data) {}

  Status DoSeek(ptrdiff_t offset, Whence origin) override {
    if (seek_status.ok()) {
      return memory_reader_.Seek(offset, origin);
    }
    return seek_status;
  }

  StatusWithSize DoRead(ByteSpan dest) final {
    if (!read_status.ok()) {
      return StatusWithSize(read_status, 0);
    }

    auto result = memory_reader_.Read(dest);
    return result.ok() ? StatusWithSize(result->size())
                       : StatusWithSize(result.status(), 0);
  }

  Status seek_status;
  Status read_status;

 private:
  stream::MemoryReader memory_reader_;
};

class SimpleReadTransfer final : public ReadOnlyHandler {
 public:
  SimpleReadTransfer(uint32_t transfer_id, ConstByteSpan data)
      : ReadOnlyHandler(transfer_id),
        prepare_read_called(false),
        finalize_read_called(false),
        finalize_read_status(Status::Unknown()),
        reader_(data) {}

  Status PrepareRead() final {
    prepare_read_called = true;

    if (!prepare_read_return_status.ok()) {
      return prepare_read_return_status;
    }

    EXPECT_EQ(reader_.seek_status, reader_.Seek(0));
    set_reader(reader_);
    return OkStatus();
  }

  void FinalizeRead(Status status) final {
    finalize_read_called = true;
    finalize_read_status = status;
  }

  void set_seek_status(Status status) { reader_.seek_status = status; }
  void set_read_status(Status status) { reader_.read_status = status; }

  bool prepare_read_called;
  bool finalize_read_called;
  Status prepare_read_return_status;
  Status finalize_read_status;

 private:
  TestMemoryReader reader_;
};

constexpr auto kData = bytes::Initialized<32>([](size_t i) { return i; });

class ReadTransfer : public ::testing::Test {
 protected:
  ReadTransfer(size_t max_chunk_size_bytes = 64)
      : handler_(3, kData),
        transfer_thread_(std::span(data_buffer_).first(max_chunk_size_bytes),
                         encode_buffer_),
        ctx_(transfer_thread_, 64),
        system_thread_(TransferThreadOptions(), transfer_thread_) {
    ctx_.service().RegisterHandler(handler_);

    ASSERT_FALSE(handler_.prepare_read_called);
    ASSERT_FALSE(handler_.finalize_read_called);

    ctx_.call();  // Open the read stream
    transfer_thread_.WaitUntilEventIsProcessed();
  }

  ~ReadTransfer() {
    transfer_thread_.Terminate();
    system_thread_.join();
  }

  SimpleReadTransfer handler_;
  Thread<1, 1> transfer_thread_;
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Read) ctx_;
  thread::Thread system_thread_;
  std::array<std::byte, 64> data_buffer_;
  std::array<std::byte, 64> encode_buffer_;
};

TEST_F(ReadTransfer, SingleChunk) {
  rpc::test::WaitForPackets(ctx_.output(), 2, [this] {
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                       .window_end_offset = 64,
                                       .pending_bytes = 64,
                                       .offset = 0,
                                       .type = Chunk::Type::kTransferStart}));

    transfer_thread_.WaitUntilEventIsProcessed();
  });

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  // First chunk should have all the read data.
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), kData.size());
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  // Second chunk should be empty and set remaining_bytes = 0.
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.data.size(), 0u);
  ASSERT_TRUE(c1.remaining_bytes.has_value());
  EXPECT_EQ(c1.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, PendingBytes_SingleChunk) {
  rpc::test::WaitForPackets(ctx_.output(), 2, [this] {
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                       .pending_bytes = 64,
                                       .offset = 0,
                                       .type = Chunk::Type::kTransferStart}));

    transfer_thread_.WaitUntilEventIsProcessed();
  });

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  // First chunk should have all the read data.
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), kData.size());
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  // Second chunk should be empty and set remaining_bytes = 0.
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.data.size(), 0u);
  ASSERT_TRUE(c1.remaining_bytes.has_value());
  EXPECT_EQ(c1.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, MultiChunk) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .window_end_offset = 16,
                                     .pending_bytes = 16,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));

  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3,
                   .window_end_offset = 32,
                   .pending_bytes = 16,
                   .offset = 16,
                   .type = Chunk::Type::kParametersContinue}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 16u);
  ASSERT_EQ(c1.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData.data() + 16, c1.data.size()), 0);

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3,
                   .window_end_offset = 48,
                   .pending_bytes = 16,
                   .offset = 32,
                   .type = Chunk::Type::kParametersContinue}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  Chunk c2 = DecodeChunk(ctx_.responses()[2]);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.data.size(), 0u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, MultiChunk_RepeatedContinuePackets) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .window_end_offset = 16,
                                     .pending_bytes = 16,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));

  transfer_thread_.WaitUntilEventIsProcessed();

  const auto continue_chunk =
      EncodeChunk({.transfer_id = 3,
                   .window_end_offset = 24,
                   .pending_bytes = 8,
                   .offset = 16,
                   .type = Chunk::Type::kParametersContinue});
  ctx_.SendClientStream(continue_chunk);

  transfer_thread_.WaitUntilEventIsProcessed();

  // Resend the CONTINUE packets that don't actually advance the window.
  for (int i = 0; i < 3; ++i) {
    ctx_.SendClientStream(continue_chunk);
    transfer_thread_.WaitUntilEventIsProcessed();
  }

  ASSERT_EQ(ctx_.total_responses(), 2u);  // Only sent one packet
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 16u);
  ASSERT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData.data() + 16, c1.data.size()), 0);
}

TEST_F(ReadTransfer, PendingBytes_MultiChunk) {
  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 0}));

  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 16}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 16u);
  ASSERT_EQ(c1.data.size(), 16u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData.data() + 16, c1.data.size()), 0);

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 32}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  Chunk c2 = DecodeChunk(ctx_.responses()[2]);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.data.size(), 0u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, OutOfOrder_SeekingSupported) {
  rpc::test::WaitForPackets(ctx_.output(), 4, [this] {
    ctx_.SendClientStream(
        EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 0}));

    transfer_thread_.WaitUntilEventIsProcessed();

    Chunk chunk = DecodeChunk(ctx_.responses().back());
    EXPECT_TRUE(std::equal(
        &kData[0], &kData[16], chunk.data.begin(), chunk.data.end()));

    ctx_.SendClientStream(
        EncodeChunk({.transfer_id = 3, .pending_bytes = 8, .offset = 2}));

    transfer_thread_.WaitUntilEventIsProcessed();

    chunk = DecodeChunk(ctx_.responses().back());
    EXPECT_TRUE(std::equal(
        &kData[2], &kData[10], chunk.data.begin(), chunk.data.end()));

    ctx_.SendClientStream(
        EncodeChunk({.transfer_id = 3, .pending_bytes = 64, .offset = 17}));
  });

  ASSERT_EQ(ctx_.total_responses(), 4u);
  Chunk chunk = DecodeChunk(ctx_.responses()[2]);
  EXPECT_TRUE(std::equal(
      &kData[17], kData.end(), chunk.data.begin(), chunk.data.end()));
}

TEST_F(ReadTransfer, OutOfOrder_SeekingNotSupported_EndsWithUnimplemented) {
  handler_.set_seek_status(Status::Unimplemented());

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 0}));
  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 8, .offset = 2}));

  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.status, Status::Unimplemented());
}

TEST_F(ReadTransfer, MaxChunkSize_Client) {
  rpc::test::WaitForPackets(ctx_.output(), 5, [this] {
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                       .pending_bytes = 64,
                                       .max_chunk_size_bytes = 8,
                                       .offset = 0,
                                       .type = Chunk::Type::kTransferStart}));
  });

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 5u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);
  Chunk c2 = DecodeChunk(ctx_.responses()[2]);
  Chunk c3 = DecodeChunk(ctx_.responses()[3]);
  Chunk c4 = DecodeChunk(ctx_.responses()[4]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 8u);
  ASSERT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData.data() + 8, c1.data.size()), 0);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.offset, 16u);
  ASSERT_EQ(c2.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c2.data.data(), kData.data() + 16, c2.data.size()), 0);

  EXPECT_EQ(c3.transfer_id, 3u);
  EXPECT_EQ(c3.offset, 24u);
  ASSERT_EQ(c3.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c3.data.data(), kData.data() + 24, c3.data.size()), 0);

  EXPECT_EQ(c4.transfer_id, 3u);
  EXPECT_EQ(c4.data.size(), 0u);
  ASSERT_TRUE(c4.remaining_bytes.has_value());
  EXPECT_EQ(c4.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, HandlerIsClearedAfterTransfer) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .window_end_offset = 64,
                                     .pending_bytes = 64,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  ASSERT_TRUE(handler_.prepare_read_called);
  ASSERT_TRUE(handler_.finalize_read_called);
  ASSERT_EQ(OkStatus(), handler_.finalize_read_status);

  // Now, clear state and start a second transfer
  handler_.prepare_read_return_status = Status::FailedPrecondition();
  handler_.prepare_read_called = false;
  handler_.finalize_read_called = false;

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .window_end_offset = 64,
                                     .pending_bytes = 64,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Prepare failed, so the handler should not have been stored in the context,
  // and finalize should not have been called.
  ASSERT_TRUE(handler_.prepare_read_called);
  ASSERT_FALSE(handler_.finalize_read_called);
}

class ReadTransferMaxChunkSize8 : public ReadTransfer {
 protected:
  ReadTransferMaxChunkSize8() : ReadTransfer(/*max_chunk_size_bytes=*/8) {}
};

TEST_F(ReadTransferMaxChunkSize8, MaxChunkSize_Server) {
  // Client asks for max 16-byte chunks, but service places a limit of 8 bytes.
  rpc::test::WaitForPackets(ctx_.output(), 5, [this] {
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                       .pending_bytes = 64,
                                       .max_chunk_size_bytes = 16,
                                       .offset = 0,
                                       .type = Chunk::Type::kTransferStart}));
  });

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 5u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);
  Chunk c2 = DecodeChunk(ctx_.responses()[2]);
  Chunk c3 = DecodeChunk(ctx_.responses()[3]);
  Chunk c4 = DecodeChunk(ctx_.responses()[4]);

  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 8u);
  ASSERT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData.data() + 8, c1.data.size()), 0);

  EXPECT_EQ(c2.transfer_id, 3u);
  EXPECT_EQ(c2.offset, 16u);
  ASSERT_EQ(c2.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c2.data.data(), kData.data() + 16, c2.data.size()), 0);

  EXPECT_EQ(c3.transfer_id, 3u);
  EXPECT_EQ(c3.offset, 24u);
  ASSERT_EQ(c3.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c3.data.data(), kData.data() + 24, c3.data.size()), 0);

  EXPECT_EQ(c4.transfer_id, 3u);
  EXPECT_EQ(c4.data.size(), 0u);
  ASSERT_TRUE(c4.remaining_bytes.has_value());
  EXPECT_EQ(c4.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, ClientError) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .pending_bytes = 16,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));

  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);
  ASSERT_EQ(ctx_.total_responses(), 1u);

  // Send client error.
  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .status = Status::OutOfRange()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, Status::OutOfRange());
}

TEST_F(ReadTransfer, MalformedParametersChunk) {
  // pending_bytes is required in a parameters chunk.
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, Status::InvalidArgument());

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 3u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::InvalidArgument());
}

TEST_F(ReadTransfer, UnregisteredHandler) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 11,
                                     .pending_bytes = 32,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 11u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::NotFound());
}

TEST_F(ReadTransfer, IgnoresNonPendingTransfers) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .offset = 3}));
  ctx_.SendClientStream(EncodeChunk(
      {.transfer_id = 3, .offset = 0, .data = std::span(kData).first(10)}));
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Only start transfer for initial packet.
  EXPECT_FALSE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);
}

TEST_F(ReadTransfer, AbortAndRestartIfInitialPacketIsReceived) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .pending_bytes = 16,
                                     .offset = 0,
                                     .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);
  handler_.prepare_read_called = false;  // Reset so can check if called again.

  ctx_.SendClientStream(  // Resend starting chunk
      EncodeChunk({.transfer_id = 3,
                   .pending_bytes = 16,
                   .offset = 0,
                   .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, Status::Aborted());
  handler_.finalize_read_called = false;  // Reset so can check later

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 16, .offset = 16}));
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, ZeroPendingBytesWithRemainingData_Aborts) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .pending_bytes = 0,
                                     .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  ASSERT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, Status::ResourceExhausted());

  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.status, Status::ResourceExhausted());
}

TEST_F(ReadTransfer, ZeroPendingBytesNoRemainingData_Completes) {
  // Make the next read appear to be the end of the stream.
  handler_.set_read_status(Status::OutOfRange());

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                     .pending_bytes = 0,
                                     .type = Chunk::Type::kTransferStart}));
  transfer_thread_.WaitUntilEventIsProcessed();

  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 3u);
  EXPECT_EQ(chunk.remaining_bytes, 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  ASSERT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());
}

TEST_F(ReadTransfer, SendsErrorIfChunkIsReceivedInCompletedState) {
  rpc::test::WaitForPackets(ctx_.output(), 2, [this] {
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 3,
                                       .pending_bytes = 64,
                                       .offset = 0,
                                       .type = Chunk::Type::kTransferStart}));
  });

  EXPECT_TRUE(handler_.prepare_read_called);
  EXPECT_FALSE(handler_.finalize_read_called);

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk c0 = DecodeChunk(ctx_.responses()[0]);
  Chunk c1 = DecodeChunk(ctx_.responses()[1]);

  // First chunk should have all the read data.
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.data.size(), kData.size());
  EXPECT_EQ(std::memcmp(c0.data.data(), kData.data(), c0.data.size()), 0);

  // Second chunk should be empty and set remaining_bytes = 0.
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.data.size(), 0u);
  ASSERT_TRUE(c1.remaining_bytes.has_value());
  EXPECT_EQ(c1.remaining_bytes.value(), 0u);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_read_called);
  EXPECT_EQ(handler_.finalize_read_status, OkStatus());

  // At this point the transfer should be in a completed state. Send a
  // non-initial chunk as a continuation of the transfer.
  handler_.finalize_read_called = false;

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 3, .pending_bytes = 48, .offset = 16}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);

  Chunk c2 = DecodeChunk(ctx_.responses()[2]);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), Status::FailedPrecondition());

  // FinalizeRead should not be called again.
  EXPECT_FALSE(handler_.finalize_read_called);
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
    EXPECT_EQ(OkStatus(), writer_.Seek(0));
    set_writer(writer_);
    prepare_write_called = true;
    return OkStatus();
  }

  Status FinalizeWrite(Status status) final {
    finalize_write_called = true;
    finalize_write_status = status;
    return finalize_write_return_status_;
  }

  void set_finalize_write_return(Status status) {
    finalize_write_return_status_ = status;
  }

  bool prepare_write_called;
  bool finalize_write_called;
  Status finalize_write_status;

 private:
  Status finalize_write_return_status_;
  stream::MemoryWriter writer_;
};

class WriteTransfer : public ::testing::Test {
 protected:
  WriteTransfer(size_t max_bytes_to_receive = 64)
      : buffer{},
        handler_(7, buffer),
        transfer_thread_(data_buffer_, encode_buffer_),
        system_thread_(TransferThreadOptions(), transfer_thread_),
        ctx_(transfer_thread_,
             max_bytes_to_receive,
             // Use a long timeout to avoid accidentally triggering timeouts.
             std::chrono::minutes(1)) {
    ctx_.service().RegisterHandler(handler_);

    ASSERT_FALSE(handler_.prepare_write_called);
    ASSERT_FALSE(handler_.finalize_write_called);

    ctx_.call();  // Open the write stream
    transfer_thread_.WaitUntilEventIsProcessed();
  }

  ~WriteTransfer() {
    transfer_thread_.Terminate();
    system_thread_.join();
  }

  std::array<std::byte, kData.size()> buffer;
  SimpleWriteTransfer handler_;

  Thread<1, 1> transfer_thread_;
  thread::Thread system_thread_;
  std::array<std::byte, 64> data_buffer_;
  std::array<std::byte, 64> encode_buffer_;
  PW_RAW_TEST_METHOD_CONTEXT(TransferService, Write) ctx_;
};

TEST_F(WriteTransfer, SingleChunk) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);
  ASSERT_TRUE(chunk.max_chunk_size_bytes.has_value());
  EXPECT_EQ(chunk.max_chunk_size_bytes.value(), 37u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
}

TEST_F(WriteTransfer, FinalizeFails) {
  // Return an error when FinalizeWrite is called.
  handler_.set_finalize_write_return(Status::FailedPrecondition());

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::DataLoss());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
}

TEST_F(WriteTransfer, SendingFinalPacketFails) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ctx_.output().set_send_status(Status::Unknown());

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Should only have sent the transfer parameters.
  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);
  ASSERT_TRUE(chunk.max_chunk_size_bytes.has_value());
  EXPECT_EQ(chunk.max_chunk_size_bytes.value(), 37u);

  // When FinalizeWrite() was called, the transfer was considered successful.
  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
}

TEST_F(WriteTransfer, MultiChunk) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 8,
                                         .data = std::span(kData).subspan(8),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
}

TEST_F(WriteTransfer, WriteFailsOnRetry) {
  // Skip one packet to fail on a retry.
  ctx_.output().set_send_status(Status::FailedPrecondition(), 1);

  // Wait for 3 packets: initial params, retry attempt, final error
  rpc::test::WaitForPackets(ctx_.output(), 3, [this] {
    // Send only one client packet so the service times out.
    ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
    transfer_thread_.SimulateServerTimeout(7);  // Time out to trigger retry
  });

  // Attempted to send 3 packets, but the 2nd packet was dropped.
  // Check that the last packet is an INTERNAL error from the RPC write failure.
  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::Internal());
}

TEST_F(WriteTransfer, TimeoutInRecoveryState) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 0u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  constexpr std::span data(kData);

  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7, .offset = 0, .data = data.first(8)}));

  // Skip offset 8 to enter a recovery state.
  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 12, .data = data.subspan(12, 4)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Recovery parameters should be sent for offset 8.
  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 8u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 24u);

  // Timeout while in the recovery state.
  transfer_thread_.SimulateServerTimeout(7);
  transfer_thread_.WaitUntilEventIsProcessed();

  // Same recovery parameters should be re-sent.
  ASSERT_EQ(ctx_.total_responses(), 3u);
  chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 8u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 24u);
}

TEST_F(WriteTransfer, ExtendWindow) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.window_end_offset, 32u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  // Window starts at 32 bytes and should extend when half of that is sent.
  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(4)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 4, .data = std::span(kData).subspan(4, 4)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 8, .data = std::span(kData).subspan(8, 4)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7,
                   .offset = 12,
                   .data = std::span(kData).subspan(12, 4)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 2u);

  // Extend parameters chunk.
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.window_end_offset, 32u);
  EXPECT_EQ(chunk.type, Chunk::Type::kParametersContinue);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 16,
                                         .data = std::span(kData).subspan(16),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  chunk = DecodeChunk(ctx_.responses()[2]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
}

class WriteTransferMaxBytes16 : public WriteTransfer {
 protected:
  WriteTransferMaxBytes16() : WriteTransfer(/*max_bytes_to_receive=*/16) {}
};

TEST_F(WriteTransfer, TransmitterReducesWindow) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.window_end_offset, 32u);

  // Send only 12 bytes and set that as the new end offset.
  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .window_end_offset = 12,
                                         .offset = 0,
                                         .data = std::span(kData).first(12)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 2u);

  // Receiver should respond immediately with a retransmit chunk as the end of
  // the window has been reached.
  chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 12u);
  EXPECT_EQ(chunk.window_end_offset, 32u);
  EXPECT_EQ(chunk.type, Chunk::Type::kParametersRetransmit);
}

TEST_F(WriteTransfer, TransmitterExtendsWindow_TerminatesWithInvalid) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.window_end_offset, 32u);

  // Send only 12 bytes and set that as the new end offset.
  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7,
                   // Larger window end offset than the receiver's.
                   .window_end_offset = 48,
                   .offset = 0,
                   .data = std::span(kData).first(16)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(ctx_.total_responses(), 2u);

  chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::Internal());
}

TEST_F(WriteTransferMaxBytes16, MultipleParameters) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 8u);
  EXPECT_EQ(chunk.window_end_offset, 24u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 8, .data = std::span(kData).subspan(8, 8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  chunk = DecodeChunk(ctx_.responses()[2]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 16u);
  EXPECT_EQ(chunk.window_end_offset, 32u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7,
                   .offset = 16,
                   .data = std::span(kData).subspan(16, 8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 4u);
  chunk = DecodeChunk(ctx_.responses()[3]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 24u);
  EXPECT_EQ(chunk.window_end_offset, 32u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 8u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 24,
                                         .data = std::span(kData).subspan(24),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 5u);
  chunk = DecodeChunk(ctx_.responses()[4]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
}

TEST_F(WriteTransferMaxBytes16, SetsDefaultPendingBytes) {
  // Default max bytes is smaller than buffer.
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);
}

TEST_F(WriteTransfer, SetsWriterPendingBytes) {
  // Buffer is smaller than constructor's default max bytes.
  std::array<std::byte, 8> small_buffer = {};

  SimpleWriteTransfer handler_(987, small_buffer);
  ctx_.service().RegisterHandler(handler_);

  ctx_.SendClientStream(EncodeChunk({.transfer_id = 987}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 987u);
  EXPECT_EQ(chunk.pending_bytes.value(), 8u);
}

TEST_F(WriteTransfer, UnexpectedOffset) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 0u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 4,  // incorrect
                                         .data = std::span(kData).subspan(16),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 8u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 24u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 8,  // correct
                                         .data = std::span(kData).subspan(8),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  chunk = DecodeChunk(ctx_.responses()[2]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
}

TEST_F(WriteTransferMaxBytes16, TooMuchData) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  // pending_bytes = 16 but send 24
  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(24)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::Internal());
}

TEST_F(WriteTransfer, UnregisteredHandler) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 999}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 999u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::NotFound());
}

TEST_F(WriteTransfer, ClientError) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 32u);

  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7, .status = Status::DataLoss()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(ctx_.total_responses(), 1u);

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, Status::DataLoss());
}

TEST_F(WriteTransfer, OnlySendParametersUpdateOnceAfterDrop) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  constexpr std::span data(kData);
  ctx_.SendClientStream<64>(
      EncodeChunk({.transfer_id = 7, .offset = 0, .data = data.first(1)}));

  // Drop offset 1, then send the rest of the data.
  for (uint32_t i = 2; i < kData.size(); ++i) {
    ctx_.SendClientStream<64>(EncodeChunk(
        {.transfer_id = 7, .offset = i, .data = data.subspan(i, 1)}));
  }

  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 1u);

  // Send the remaining data and the final status.
  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 1,
                                         .data = data.subspan(1, 31),
                                         .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
}

TEST_F(WriteTransfer, ResendParametersIfSentRepeatedChunkDuringRecovery) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  constexpr std::span data(kData);

  // Skip offset 0, then send the rest of the data.
  for (uint32_t i = 1; i < kData.size(); ++i) {
    ctx_.SendClientStream<64>(EncodeChunk(
        {.transfer_id = 7, .offset = i, .data = data.subspan(i, 1)}));
  }

  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);  // Resent transfer parameters once.

  const auto last_chunk = EncodeChunk(
      {.transfer_id = 7, .offset = kData.size() - 1, .data = data.last(1)});
  ctx_.SendClientStream<64>(last_chunk);
  transfer_thread_.WaitUntilEventIsProcessed();

  // Resent transfer parameters since the packet is repeated
  ASSERT_EQ(ctx_.total_responses(), 3u);

  ctx_.SendClientStream<64>(last_chunk);
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 4u);

  Chunk chunk = DecodeChunk(ctx_.responses().back());
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 0u);
  EXPECT_TRUE(chunk.pending_bytes.has_value());

  // Resumes normal operation when correct offset is sent.
  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = kData, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
}

TEST_F(WriteTransfer, ResendsStatusIfClientRetriesAfterStatusChunk) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 2u);
  Chunk chunk = DecodeChunk(ctx_.responses().back());
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);
  chunk = DecodeChunk(ctx_.responses().back());
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), OkStatus());
}

TEST_F(WriteTransfer, IgnoresNonPendingTransfers) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7, .offset = 3}));
  ctx_.SendClientStream(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(10)}));
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7, .status = OkStatus()}));

  transfer_thread_.WaitUntilEventIsProcessed();

  // Only start transfer for initial packet.
  EXPECT_FALSE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);
}

TEST_F(WriteTransfer, AbortAndRestartIfInitialPacketIsReceived) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);

  ASSERT_TRUE(handler_.prepare_write_called);
  ASSERT_FALSE(handler_.finalize_write_called);
  handler_.prepare_write_called = false;  // Reset to check it's called again.

  // Simulate client disappearing then restarting the transfer.
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, Status::Aborted());

  handler_.finalize_write_called = false;  // Reset to check it's called again.

  ASSERT_EQ(ctx_.total_responses(), 2u);

  ctx_.SendClientStream<64>(EncodeChunk({.transfer_id = 7,
                                         .offset = 0,
                                         .data = std::span(kData),
                                         .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 3u);

  EXPECT_TRUE(handler_.finalize_write_called);
  EXPECT_EQ(handler_.finalize_write_status, OkStatus());
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);
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

TEST_F(ReadTransfer, PrepareError) {
  SometimesUnavailableReadHandler unavailable_handler(88, kData);
  ctx_.service().RegisterHandler(unavailable_handler);

  ctx_.SendClientStream(
      EncodeChunk({.transfer_id = 88, .pending_bytes = 128, .offset = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 88u);
  ASSERT_TRUE(chunk.status.has_value());
  EXPECT_EQ(chunk.status.value(), Status::DataLoss());

  // Try starting the transfer again. It should work this time.
  // TODO(frolv): This won't work until completion ACKs are supported.
  if (false) {
    ctx_.SendClientStream(
        EncodeChunk({.transfer_id = 88, .pending_bytes = 128, .offset = 0}));
    transfer_thread_.WaitUntilEventIsProcessed();

    ASSERT_EQ(ctx_.total_responses(), 2u);
    chunk = DecodeChunk(ctx_.responses()[1]);
    EXPECT_EQ(chunk.transfer_id, 88u);
    ASSERT_EQ(chunk.data.size(), kData.size());
    EXPECT_EQ(std::memcmp(chunk.data.data(), kData.data(), chunk.data.size()),
              0);
  }
}

TEST_F(WriteTransferMaxBytes16, Service_SetMaxPendingBytes) {
  ctx_.SendClientStream(EncodeChunk({.transfer_id = 7}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_TRUE(handler_.prepare_write_called);
  EXPECT_FALSE(handler_.finalize_write_called);

  // First parameters chunk has default pending bytes of 16.
  ASSERT_EQ(ctx_.total_responses(), 1u);
  Chunk chunk = DecodeChunk(ctx_.responses()[0]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 16u);

  // Update the pending bytes value.
  ctx_.service().set_max_pending_bytes(12);

  ctx_.SendClientStream<64>(EncodeChunk(
      {.transfer_id = 7, .offset = 0, .data = std::span(kData).first(8)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Second parameters chunk should use the new max pending bytes.
  ASSERT_EQ(ctx_.total_responses(), 2u);
  chunk = DecodeChunk(ctx_.responses()[1]);
  EXPECT_EQ(chunk.transfer_id, 7u);
  EXPECT_EQ(chunk.offset, 8u);
  EXPECT_EQ(chunk.window_end_offset, 20u);
  ASSERT_TRUE(chunk.pending_bytes.has_value());
  EXPECT_EQ(chunk.pending_bytes.value(), 12u);
}

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace
}  // namespace pw::transfer::test
