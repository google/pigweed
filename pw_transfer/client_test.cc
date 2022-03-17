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

#include "pw_transfer/client.h"

#include <cstring>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_bytes/array.h"
#include "pw_rpc/raw/client_testing.h"
#include "pw_rpc/thread_testing.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer_private/chunk_testing.h"

namespace pw::transfer::test {
namespace {

using internal::Chunk;
using pw_rpc::raw::Transfer;

using namespace std::chrono_literals;

PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC(ignored, "-Wmissing-field-initializers");

thread::Options& TransferThreadOptions() {
  static thread::stl::Options options;
  return options;
}

class ReadTransfer : public ::testing::Test {
 protected:
  ReadTransfer(size_t max_bytes_to_receive = 0)
      : transfer_thread_(chunk_buffer_, encode_buffer_),
        client_(context_.client(),
                context_.channel().id(),
                transfer_thread_,
                max_bytes_to_receive),
        system_thread_(TransferThreadOptions(), transfer_thread_) {}

  ~ReadTransfer() {
    transfer_thread_.Terminate();
    system_thread_.join();
  }

  rpc::RawClientTestContext<> context_;

  Thread<1, 1> transfer_thread_;
  Client client_;

  std::array<std::byte, 64> chunk_buffer_;
  std::array<std::byte, 64> encode_buffer_;

  thread::Thread system_thread_;
};

constexpr auto kData32 = bytes::Initialized<32>([](size_t i) { return i; });
constexpr auto kData64 = bytes::Initialized<64>([](size_t i) { return i; });

TEST_F(ReadTransfer, SingleChunk) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(3, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));

  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 3u, .offset = 0, .data = kData32, .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 3u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

TEST_F(ReadTransfer, MultiChunk) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(4, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));

  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 4u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData32);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 4u, .offset = 0, .data = data.first(16)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 1u);

  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 4u,
                   .offset = 16,
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 4u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

TEST_F(ReadTransfer, MultipleTransfers) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(3, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 3u, .offset = 0, .data = kData32, .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(transfer_status, OkStatus());
  transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(3, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 3u, .offset = 0, .data = kData32, .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(transfer_status, OkStatus());
}

class ReadTransferMaxBytes32 : public ReadTransfer {
 protected:
  ReadTransferMaxBytes32() : ReadTransfer(/*max_bytes_to_receive=*/32) {}
};

TEST_F(ReadTransferMaxBytes32, SetsPendingBytesFromConstructorArg) {
  stream::MemoryWriterBuffer<64> writer;
  EXPECT_EQ(OkStatus(), client_.Read(5, writer, [](Status) {}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);
}

TEST_F(ReadTransferMaxBytes32, SetsPendingBytesFromWriterLimit) {
  stream::MemoryWriterBuffer<16> small_writer;
  EXPECT_EQ(OkStatus(), client_.Read(5, small_writer, [](Status) {}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 16u);
}

TEST_F(ReadTransferMaxBytes32, MultiParameters) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(6, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 6u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);

  constexpr ConstByteSpan data(kData64);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 6u, .offset = 0, .data = data.first(32)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 2u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Second parameters chunk.
  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 6u);
  EXPECT_EQ(c1.offset, 32u);
  ASSERT_EQ(c1.pending_bytes.value(), 32u);

  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 6u,
                   .offset = 32,
                   .data = data.subspan(32),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 6u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), data.data(), writer.bytes_written()), 0);
}

TEST_F(ReadTransfer, UnexpectedOffset) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(7, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 7u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData32);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u, .offset = 0, .data = data.first(16)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send a chunk with an incorrect offset. The client should resend parameters.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u,
                   .offset = 8,  // wrong!
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 2u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 7u);
  EXPECT_EQ(c1.offset, 16u);
  EXPECT_EQ(c1.pending_bytes.value(), 48u);

  // Send the correct chunk, completing the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u,
                   .offset = 16,
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 7u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

TEST_F(ReadTransferMaxBytes32, TooMuchData) {
  stream::MemoryWriterBuffer<32> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(8, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 8u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);

  constexpr ConstByteSpan data(kData64);

  // pending_bytes == 32
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 8u, .offset = 0, .data = data.first(16)}));

  // pending_bytes == 16
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 8u, .offset = 16, .data = data.subspan(16, 8)}));

  // pending_bytes == 8, send 16 instead.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 8u, .offset = 24, .data = data.subspan(24, 16)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 4u);

  Chunk c1 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c1.transfer_id, 8u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::Internal());

  EXPECT_EQ(transfer_status, Status::Internal());
}

TEST_F(ReadTransfer, ServerError) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(9, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 9u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  // Server sends an error. Client should not respond and terminate the
  // transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 9u, .status = Status::NotFound()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::NotFound());
}

TEST_F(ReadTransfer, OnlySendsParametersOnceAfterDrop) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(10, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 10u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData64);

  // Send the first 8 bytes of the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 10u, .offset = 0, .data = data.first(8)}));

  // Skip offset 8, send the rest starting from 16.
  for (uint32_t offset = 16; offset < data.size(); offset += 8) {
    context_.server().SendServerStream<Transfer::Read>(
        EncodeChunk({.transfer_id = 10u,
                     .offset = offset,
                     .data = data.subspan(offset, 8)}));
  }
  transfer_thread_.WaitUntilEventIsProcessed();

  // Only one parameters update should be sent, with the offset of the initial
  // dropped packet.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 10u);
  EXPECT_EQ(c1.offset, 8u);
  ASSERT_EQ(c1.pending_bytes.value(), 56u);

  // Send the remaining data to complete the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 10u,
                   .offset = 8,
                   .data = data.subspan(8, 56),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 10u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(ReadTransfer, ResendsParametersIfSentRepeatedChunkDuringRecovery) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(11, writer, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 11u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData64);

  // Send the first 8 bytes of the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 11u, .offset = 0, .data = data.first(8)}));

  // Skip offset 8, send the rest starting from 16.
  for (uint32_t offset = 16; offset < data.size(); offset += 8) {
    context_.server().SendServerStream<Transfer::Read>(
        EncodeChunk({.transfer_id = 11u,
                     .offset = offset,
                     .data = data.subspan(offset, 8)}));
  }
  transfer_thread_.WaitUntilEventIsProcessed();

  // Only one parameters update should be sent, with the offset of the initial
  // dropped packet.
  ASSERT_EQ(payloads.size(), 2u);

  const Chunk last_chunk = {
      .transfer_id = 11u, .offset = 56, .data = data.subspan(56)};

  // Re-send the final chunk of the block.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(last_chunk));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The original drop parameters should be re-sent.
  ASSERT_EQ(payloads.size(), 3u);
  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 11u);
  EXPECT_EQ(c2.offset, 8u);
  ASSERT_EQ(c2.pending_bytes.value(), 56u);

  // Do it again.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(last_chunk));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 4u);
  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, 11u);
  EXPECT_EQ(c3.offset, 8u);
  ASSERT_EQ(c3.pending_bytes.value(), 56u);

  // Finish the transfer normally.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 11u,
                   .offset = 8,
                   .data = data.subspan(8, 56),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 5u);

  Chunk c4 = DecodeChunk(payloads[4]);
  EXPECT_EQ(c4.transfer_id, 11u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

constexpr chrono::SystemClock::duration kTestTimeout =
    std::chrono::milliseconds(50);
constexpr uint8_t kTestRetries = 3;

TEST_F(ReadTransfer, Timeout_ResendsCurrentParameters) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(
                12,
                writer,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 12u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  // Wait for the timeout to expire without doing anything. The client should
  // resend its parameters chunk.
  transfer_thread_.SimulateClientTimeout(12);
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c = DecodeChunk(payloads.back());
  EXPECT_EQ(c.transfer_id, 12u);
  EXPECT_EQ(c.offset, 0u);
  EXPECT_EQ(c.pending_bytes.value(), 64u);

  // Transfer has not yet completed.
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Finish the transfer following the timeout.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 12u,
                   .offset = 0,
                   .data = kData32,
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c4 = DecodeChunk(payloads.back());
  EXPECT_EQ(c4.transfer_id, 12u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(ReadTransfer, Timeout_ResendsUpdatedParameters) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(
                13,
                writer,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 13u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData32);

  // Send some data, but not everything.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 13u, .offset = 0, .data = data.first(16)}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 1u);

  // Wait for the timeout to expire without sending more data. The client should
  // send an updated parameters chunk, accounting for the data already received.
  transfer_thread_.SimulateClientTimeout(13);
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c = DecodeChunk(payloads.back());
  EXPECT_EQ(c.transfer_id, 13u);
  EXPECT_EQ(c.offset, 16u);
  EXPECT_EQ(c.pending_bytes.value(), 48u);

  // Transfer has not yet completed.
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the rest of the data, finishing the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 13u,
                   .offset = 16,
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c4 = DecodeChunk(payloads.back());
  EXPECT_EQ(c4.transfer_id, 13u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(ReadTransfer, Timeout_EndsTransferAfterMaxRetries) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Read(
                14,
                writer,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 14u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  for (unsigned retry = 1; retry <= kTestRetries; ++retry) {
    // Wait for the timeout to expire without doing anything. The client should
    // resend its parameters chunk.
    transfer_thread_.SimulateClientTimeout(14);
    ASSERT_EQ(payloads.size(), retry + 1);

    Chunk c = DecodeChunk(payloads.back());
    EXPECT_EQ(c.transfer_id, 14u);
    EXPECT_EQ(c.offset, 0u);
    EXPECT_EQ(c.pending_bytes.value(), 64u);

    // Transfer has not yet completed.
    EXPECT_EQ(transfer_status, Status::Unknown());
  }

  // Sleep one more time after the final retry. The client should cancel the
  // transfer at this point and send a DEADLINE_EXCEEDED chunk.
  transfer_thread_.SimulateClientTimeout(14);
  ASSERT_EQ(payloads.size(), 5u);

  Chunk c4 = DecodeChunk(payloads.back());
  EXPECT_EQ(c4.transfer_id, 14u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), Status::DeadlineExceeded());

  EXPECT_EQ(transfer_status, Status::DeadlineExceeded());

  // After finishing the transfer, nothing else should be sent. Verify this by
  // waiting for a bit.
  this_thread::sleep_for(kTestTimeout * 4);
  ASSERT_EQ(payloads.size(), 5u);
}

TEST_F(ReadTransfer, Timeout_ReceivingDataResetsRetryCount) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  constexpr ConstByteSpan data(kData32);

  ASSERT_EQ(OkStatus(),
            client_.Read(
                14,
                writer,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 14u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.window_end_offset, 64u);

  // Simulate one less timeout than the maximum amount of retries.
  for (unsigned retry = 1; retry <= kTestRetries - 1; ++retry) {
    transfer_thread_.SimulateClientTimeout(14);
    ASSERT_EQ(payloads.size(), retry + 1);

    Chunk c = DecodeChunk(payloads.back());
    EXPECT_EQ(c.transfer_id, 14u);
    EXPECT_EQ(c.offset, 0u);
    EXPECT_EQ(c.window_end_offset, 64u);

    // Transfer has not yet completed.
    EXPECT_EQ(transfer_status, Status::Unknown());
  }

  // Send some data.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 14u, .offset = 0, .data = data.first(16)}));
  transfer_thread_.WaitUntilEventIsProcessed();
  ASSERT_EQ(payloads.size(), 3u);

  // Time out a couple more times. The context's retry count should have been
  // reset, so it should go through the standard retry flow instead of
  // terminating the transfer.
  transfer_thread_.SimulateClientTimeout(14);
  ASSERT_EQ(payloads.size(), 4u);

  Chunk c = DecodeChunk(payloads.back());
  EXPECT_FALSE(c.status.has_value());
  EXPECT_EQ(c.transfer_id, 14u);
  EXPECT_EQ(c.offset, 16u);
  EXPECT_EQ(c.window_end_offset, 64u);

  transfer_thread_.SimulateClientTimeout(14);
  ASSERT_EQ(payloads.size(), 5u);

  c = DecodeChunk(payloads.back());
  EXPECT_FALSE(c.status.has_value());
  EXPECT_EQ(c.transfer_id, 14u);
  EXPECT_EQ(c.offset, 16u);
  EXPECT_EQ(c.window_end_offset, 64u);
}

TEST_F(ReadTransfer, InitialPacketFails_OnCompletedCalledWithDataLoss) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  context_.output().set_send_status(Status::Unauthenticated());

  ASSERT_EQ(OkStatus(),
            client_.Read(
                14,
                writer,
                [&transfer_status](Status status) {
                  ASSERT_EQ(transfer_status,
                            Status::Unknown());  // Must only call once
                  transfer_status = status;
                },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(transfer_status, Status::Internal());
}

class WriteTransfer : public ::testing::Test {
 protected:
  WriteTransfer()
      : transfer_thread_(chunk_buffer_, encode_buffer_),
        client_(context_.client(), context_.channel().id(), transfer_thread_),
        system_thread_(TransferThreadOptions(), transfer_thread_) {}

  ~WriteTransfer() {
    transfer_thread_.Terminate();
    system_thread_.join();
  }

  rpc::RawClientTestContext<> context_;

  Thread<1, 1> transfer_thread_;
  Client client_;

  std::array<std::byte, 64> chunk_buffer_;
  std::array<std::byte, 64> encode_buffer_;

  thread::Thread system_thread_;
};

TEST_F(WriteTransfer, SingleChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(3, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 3u);

  // Send transfer parameters. Client should send a data chunk and the final
  // chunk.
  rpc::test::WaitForPackets(context_.output(), 2, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 3,
                     .pending_bytes = 64,
                     .max_chunk_size_bytes = 32,
                     .offset = 0}));
  });

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 3u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(payloads.size(), 3u);
  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(WriteTransfer, MultiChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(4, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 4u);

  // Send transfer parameters with a chunk size smaller than the data.

  // Client should send two data chunks and the final chunk.
  rpc::test::WaitForPackets(context_.output(), 3, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 4,
                     .pending_bytes = 64,
                     .max_chunk_size_bytes = 16,
                     .offset = 0}));
  });

  ASSERT_EQ(payloads.size(), 4u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 4u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 4u);
  EXPECT_EQ(c2.offset, 16u);
  EXPECT_EQ(
      std::memcmp(c2.data.data(), kData32.data() + c2.offset, c2.data.size()),
      0);

  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, 4u);
  ASSERT_TRUE(c3.remaining_bytes.has_value());
  EXPECT_EQ(c3.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 4, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(payloads.size(), 4u);
  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(WriteTransfer, OutOfOrder_SeekSupported) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(5, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);

  // Send transfer parameters with a nonzero offset, requesting a seek.
  // Client should send a data chunk and the final chunk.
  rpc::test::WaitForPackets(context_.output(), 2, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 5,
                     .pending_bytes = 64,
                     .max_chunk_size_bytes = 32,
                     .offset = 16}));
  });

  ASSERT_EQ(payloads.size(), 3u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 5u);
  EXPECT_EQ(c1.offset, 16u);
  EXPECT_EQ(
      std::memcmp(c1.data.data(), kData32.data() + c1.offset, c1.data.size()),
      0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 5u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 5, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(payloads.size(), 3u);
  EXPECT_EQ(transfer_status, OkStatus());
}

class FakeNonSeekableReader final : public stream::NonSeekableReader {
 public:
  FakeNonSeekableReader(ConstByteSpan data) : data_(data), position_(0) {}

 private:
  StatusWithSize DoRead(ByteSpan out) final {
    if (position_ == data_.size()) {
      return StatusWithSize::OutOfRange();
    }

    size_t to_copy = std::min(out.size(), data_.size() - position_);
    std::memcpy(out.data(), data_.data() + position_, to_copy);
    position_ += to_copy;

    return StatusWithSize(to_copy);
  }

  ConstByteSpan data_;
  size_t position_;
};

TEST_F(WriteTransfer, OutOfOrder_SeekNotSupported) {
  FakeNonSeekableReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(6, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 6u);

  // Send transfer parameters with a nonzero offset, requesting a seek.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 6,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 32,
                   .offset = 16}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 6u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::Unimplemented());

  EXPECT_EQ(transfer_status, Status::Unimplemented());
}

TEST_F(WriteTransfer, ServerError) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(7, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 7u);

  // Send an error from the server.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 7, .status = Status::NotFound()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Client should not respond and terminate the transfer.
  EXPECT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::NotFound());
}

TEST_F(WriteTransfer, MalformedParametersChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(8, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 8u);

  // Send an invalid transfer parameters chunk without pending_bytes.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 8, .max_chunk_size_bytes = 32}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 8u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::InvalidArgument());

  EXPECT_EQ(transfer_status, Status::InvalidArgument());
}

TEST_F(WriteTransfer, AbortIfZeroBytesAreRequested) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(9, reader, [&transfer_status](Status status) {
              transfer_status = status;
            }));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 9u);

  // Send an invalid transfer parameters chunk with 0 pending_bytes.
  context_.server().SendServerStream<Transfer::Write>(EncodeChunk(
      {.transfer_id = 9, .pending_bytes = 0, .max_chunk_size_bytes = 32}));
  transfer_thread_.WaitUntilEventIsProcessed();

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 9u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::ResourceExhausted());

  EXPECT_EQ(transfer_status, Status::ResourceExhausted());
}

TEST_F(WriteTransfer, Timeout_RetriesWithInitialChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(
                10,
                reader,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 10u);

  // Wait for the timeout to expire without doing anything. The client should
  // resend the initial transmit chunk.
  transfer_thread_.SimulateClientTimeout(10);
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c = DecodeChunk(payloads.back());
  EXPECT_EQ(c.transfer_id, 10u);

  // Transfer has not yet completed.
  EXPECT_EQ(transfer_status, Status::Unknown());
}

TEST_F(WriteTransfer, Timeout_RetriesWithMostRecentChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(
                11,
                reader,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 11u);

  // Send the first parameters chunk.
  rpc::test::WaitForPackets(context_.output(), 2, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 11,
                     .pending_bytes = 16,
                     .max_chunk_size_bytes = 8,
                     .offset = 0}));
  });
  ASSERT_EQ(payloads.size(), 3u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 11u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 11u);
  EXPECT_EQ(c2.offset, 8u);
  EXPECT_EQ(c2.data.size(), 8u);
  EXPECT_EQ(
      std::memcmp(c2.data.data(), kData32.data() + c2.offset, c1.data.size()),
      0);

  // Wait for the timeout to expire without doing anything. The client should
  // resend the most recently sent chunk.
  transfer_thread_.SimulateClientTimeout(11);
  ASSERT_EQ(payloads.size(), 4u);

  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, c2.transfer_id);
  EXPECT_EQ(c3.offset, c2.offset);
  EXPECT_EQ(c3.data.size(), c2.data.size());
  EXPECT_EQ(std::memcmp(c3.data.data(), c2.data.data(), c3.data.size()), 0);

  // Transfer has not yet completed.
  EXPECT_EQ(transfer_status, Status::Unknown());
}

TEST_F(WriteTransfer, Timeout_RetriesWithSingleChunkTransfer) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(
                12,
                reader,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 12u);

  // Send the first parameters chunk, requesting all the data. The client should
  // respond with one data chunk and a remaining_bytes = 0 chunk.
  rpc::test::WaitForPackets(context_.output(), 2, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 12,
                     .pending_bytes = 64,
                     .max_chunk_size_bytes = 64,
                     .offset = 0}));
  });
  ASSERT_EQ(payloads.size(), 3u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 12u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(c1.data.size(), 32u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 12u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  // Wait for the timeout to expire without doing anything. The client should
  // resend the data chunk.
  transfer_thread_.SimulateClientTimeout(12);
  ASSERT_EQ(payloads.size(), 4u);

  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, c1.transfer_id);
  EXPECT_EQ(c3.offset, c1.offset);
  EXPECT_EQ(c3.data.size(), c1.data.size());
  EXPECT_EQ(std::memcmp(c3.data.data(), c1.data.data(), c3.data.size()), 0);

  // The remaining_bytes = 0 chunk should be resent on the next parameters.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 12,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 64,
                   .offset = 32}));
  transfer_thread_.WaitUntilEventIsProcessed();

  ASSERT_EQ(payloads.size(), 5u);

  Chunk c4 = DecodeChunk(payloads[4]);
  EXPECT_EQ(c4.transfer_id, 12u);
  ASSERT_TRUE(c4.remaining_bytes.has_value());
  EXPECT_EQ(c4.remaining_bytes.value(), 0u);

  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 12, .status = OkStatus()}));
  transfer_thread_.WaitUntilEventIsProcessed();

  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(WriteTransfer, Timeout_EndsTransferAfterMaxRetries) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(
                13,
                reader,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 13u);

  for (unsigned retry = 1; retry <= kTestRetries; ++retry) {
    // Wait for the timeout to expire without doing anything. The client should
    // resend the initial transmit chunk.
    transfer_thread_.SimulateClientTimeout(13);
    ASSERT_EQ(payloads.size(), retry + 1);

    Chunk c = DecodeChunk(payloads.back());
    EXPECT_EQ(c.transfer_id, 13u);

    // Transfer has not yet completed.
    EXPECT_EQ(transfer_status, Status::Unknown());
  }

  // Sleep one more time after the final retry. The client should cancel the
  // transfer at this point and send a DEADLINE_EXCEEDED chunk.
  transfer_thread_.SimulateClientTimeout(13);
  ASSERT_EQ(payloads.size(), 5u);

  Chunk c4 = DecodeChunk(payloads.back());
  EXPECT_EQ(c4.transfer_id, 13u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), Status::DeadlineExceeded());

  EXPECT_EQ(transfer_status, Status::DeadlineExceeded());

  // After finishing the transfer, nothing else should be sent. Verify this by
  // waiting for a bit.
  this_thread::sleep_for(kTestTimeout * 4);
  ASSERT_EQ(payloads.size(), 5u);
}

TEST_F(WriteTransfer, Timeout_NonSeekableReaderEndsTransfer) {
  FakeNonSeekableReader reader(kData32);
  Status transfer_status = Status::Unknown();

  ASSERT_EQ(OkStatus(),
            client_.Write(
                14,
                reader,
                [&transfer_status](Status status) { transfer_status = status; },
                kTestTimeout));
  transfer_thread_.WaitUntilEventIsProcessed();

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads.back());
  EXPECT_EQ(c0.transfer_id, 14u);

  // Send the first parameters chunk.
  rpc::test::WaitForPackets(context_.output(), 2, [this] {
    context_.server().SendServerStream<Transfer::Write>(
        EncodeChunk({.transfer_id = 14,
                     .pending_bytes = 16,
                     .max_chunk_size_bytes = 8,
                     .offset = 0}));
  });
  ASSERT_EQ(payloads.size(), 3u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 14u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(c1.data.size(), 8u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 14u);
  EXPECT_EQ(c2.offset, 8u);
  EXPECT_EQ(c2.data.size(), 8u);
  EXPECT_EQ(
      std::memcmp(c2.data.data(), kData32.data() + c2.offset, c1.data.size()),
      0);

  // Wait for the timeout to expire without doing anything. The client should
  // fail to seek back and end the transfer.
  transfer_thread_.SimulateClientTimeout(14);
  ASSERT_EQ(payloads.size(), 4u);

  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, 14u);
  ASSERT_TRUE(c3.status.has_value());
  EXPECT_EQ(c3.status.value(), Status::DeadlineExceeded());

  EXPECT_EQ(transfer_status, Status::DeadlineExceeded());
}

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace
}  // namespace pw::transfer::test
