// Copyright 2023 The Pigweed Authors
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

#include "pw_stream/mpsc_stream.h"

#include "pw_containers/vector.h"
#include "pw_fuzzer/fuzztest.h"
#include "pw_random/xor_shift.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace pw::stream {
namespace {

using namespace std::chrono_literals;
using namespace pw::fuzzer;

////////////////////////////////////////////////////////////////////////////////
// Test fixtures.

/// Capacity in bytes for data buffers.
constexpr size_t kBufSize = 512;

/// Fills a byte span with random data.
void Fill(std::byte* buf, size_t len) {
  ByteSpan data(buf, len);
  random::XorShiftStarRng64 rng(1);
  rng.Get(data);
}

/// FNV-1a offset basis.
constexpr uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;

/// FNV-1a prime value.
constexpr uint64_t kPrimeValue = 0x100000001b3ULL;

/// Quick implementation of public-domain Fowler-Noll-Vo hashing algorithm.
///
/// This is used in the tests below to verify equality of two sequences of bytes
/// that are too large to compare directly.
///
/// See http://www.isthe.com/chongo/tech/comp/fnv/index.html
void fnv1a(ConstByteSpan bytes, uint64_t& hash) {
  for (const auto& b : bytes) {
    hash = (hash ^ static_cast<uint8_t>(b)) * kPrimeValue;
  }
}

/// MpscStream test context that uses a generic reader.
///
/// This struct associates a reader and writer with their parameters and return
/// values. This is useful for communicating with threads spawned to call a
/// blocking method.
struct MpscTestContext {
  MpscWriter writer;
  MpscReader reader;

  ConstByteSpan data;
  std::byte write_buffer[kBufSize];
  uint64_t write_hash = kOffsetBasis;
  Status write_status;

  ByteSpan destination;
  std::byte read_buffer[kBufSize];
  Result<ByteSpan> read_result;
  uint64_t read_hash = kOffsetBasis;
  size_t total_read = 0;

  MpscTestContext() {
    data = ConstByteSpan(write_buffer);
    destination = ByteSpan(read_buffer);
  }

  void Connect() { CreateMpscStream(reader, writer); }

  // Fills a byte span with random data.
  void Fill() { pw::stream::Fill(write_buffer, sizeof(write_buffer)); }

  // Writes data using the writer.
  void Write() {
    fnv1a(data, write_hash);
    write_status = writer.Write(data);
  }

  // Writes data repeatedly up to the writer's limit.
  void WriteAll() {
    size_t limit = writer.ConservativeWriteLimit();
    ASSERT_NE(limit, 0U);
    ASSERT_NE(limit, Stream::kUnlimited);
    while (limit != 0) {
      if (limit < kBufSize) {
        data = data.subspan(0, limit);
      }
      Fill();
      Write();
      if (!write_status.ok()) {
        break;
      }
      limit = writer.ConservativeWriteLimit();
    }
  }

  // Reads data using the reader.
  void Read() {
    read_result = reader.Read(destination);
    if (read_result.ok()) {
      fnv1a(*read_result, write_hash);
      total_read += read_result->size();
    }
  }

  // Run the given function on a dedicated thread.
  using ThreadBody = Function<void(MpscTestContext* ctx)>;
  void Spawn(ThreadBody func) {
    body_ = std::move(func);
    thread_ = thread::Thread(context_.options(), [this]() { body_(this); });
  }

  // Waits for the spawned thread to complete.
  void Join() { thread_.join(); }

 private:
  thread::Thread thread_;
  thread::test::TestThreadContext context_;
  ThreadBody body_;
};

////////////////////////////////////////////////////////////////////////////////
// Unit tests.

TEST(MpscStreamTest, CopyWriters) {
  MpscTestContext ctx;
  ctx.Connect();
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(ctx.writer.connected());

  MpscWriter writer2(ctx.writer);
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(ctx.writer.connected());
  EXPECT_TRUE(writer2.connected());

  MpscWriter writer3 = writer2;
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(ctx.writer.connected());
  EXPECT_TRUE(writer2.connected());
  EXPECT_TRUE(writer3.connected());

  ctx.writer.Close();
  writer2.Close();
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_FALSE(ctx.writer.connected());
  EXPECT_FALSE(writer2.connected());
  EXPECT_TRUE(writer3.connected());
}

TEST(MpscStreamTest, MoveWriters) {
  MpscTestContext ctx;
  ctx.Connect();
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(ctx.writer.connected());

  MpscWriter writer2(std::move(ctx.writer));
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(writer2.connected());

  MpscWriter writer3 = std::move(writer2);
  EXPECT_TRUE(ctx.reader.connected());
  EXPECT_TRUE(writer3.connected());

  // Only writer3 should be connected.
  writer3.Close();
  EXPECT_FALSE(writer3.connected());
  EXPECT_FALSE(ctx.reader.connected());
}

TEST(MpscStreamTest, ReadFailsIfDisconnected) {
  MpscTestContext ctx;
  ctx.Connect();

  ctx.writer.Close();
  ctx.Read();
  EXPECT_EQ(ctx.read_result.status(), Status::OutOfRange());
}

TEST(MpscStreamTest, ReadBlocksWhenEmpty) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.reader.SetTimeout(10ms);

  auto start = chrono::SystemClock::now();
  ctx.Read();
  auto elapsed = chrono::SystemClock::now() - start;

  EXPECT_EQ(ctx.read_result.status(), Status::ResourceExhausted());
  EXPECT_GE(elapsed, 10ms);
}

TEST(MpscStreamTest, ReadReturnsAfterReaderClose) {
  MpscTestContext ctx;
  ctx.Connect();

  ctx.Spawn([](MpscTestContext* inner) { inner->Read(); });
  ctx.reader.Close();
  ctx.Join();

  EXPECT_EQ(ctx.read_result.status(), Status::OutOfRange());
}

TEST(MpscStreamTest, WriteBlocksUntilTimeout) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.writer.SetTimeout(10ms);
  ctx.Fill();

  auto start = chrono::SystemClock::now();
  ctx.Write();
  auto elapsed = chrono::SystemClock::now() - start;

  EXPECT_EQ(ctx.write_status, Status::ResourceExhausted());
  EXPECT_GE(elapsed, 10ms);
}

TEST(MpscStreamTest, WriteReturnsAfterClose) {
  MpscTestContext ctx;
  ctx.Connect();

  ctx.Fill();
  ctx.Spawn([](MpscTestContext* inner) { inner->Write(); });
  ctx.reader.Close();
  ctx.Join();

  EXPECT_EQ(ctx.write_status, Status::OutOfRange());
}

void VerifyRoundtripImpl(const Vector<std::byte>& data, ByteSpan buffer) {
  MpscTestContext ctx;
  ctx.Connect();

  ctx.reader.SetBuffer(buffer);
  ctx.data = ConstByteSpan(data.data(), data.size());
  ctx.Spawn([](MpscTestContext* inner) { inner->Write(); });
  size_t offset = 0;
  while (offset < data.size()) {
    ctx.Read();
    ASSERT_EQ(ctx.read_result.status(), OkStatus());
    size_t num_read = ctx.read_result->size();
    EXPECT_EQ(memcmp(ctx.read_buffer, &data[offset], num_read), 0);
    offset += num_read;
  }
  ctx.Join();
}

template <size_t kCapacity>
void FillAndVerifyRoundtripImpl(ByteSpan buffer) {
  Vector<std::byte, kCapacity> data;
  Fill(data.data(), data.size());
  VerifyRoundtripImpl(data, buffer);
}

TEST(MpscStreamTest, VerifyRoundtripWithoutBufferSmall) {
  FillAndVerifyRoundtripImpl<kBufSize / 2>(ByteSpan());
}

TEST(MpscStreamTest, VerifyRoundtripWithoutBufferLarge) {
  FillAndVerifyRoundtripImpl<kBufSize * 2>(ByteSpan());
}

void VerifyRoundtripWithoutBuffer(const Vector<std::byte>& data) {
  VerifyRoundtripImpl(data, ByteSpan());
}
FUZZ_TEST(MpscStreamTest, VerifyRoundtripWithoutBuffer)
    .WithDomains(VectorOf<kBufSize * 2>(Arbitrary<std::byte>()).WithMinSize(1));

TEST(MpscStreamTest, VerifyRoundtripWithBufferSmall) {
  std::byte buffer[kBufSize];
  FillAndVerifyRoundtripImpl<kBufSize / 2>(buffer);
}

TEST(MpscStreamTest, VerifyRoundtripWithBufferLarge) {
  std::byte buffer[kBufSize];
  FillAndVerifyRoundtripImpl<kBufSize * 2>(buffer);
}

void VerifyRoundtripWithBuffer(const Vector<std::byte>& data) {
  std::byte buffer[kBufSize];
  VerifyRoundtripImpl(data, buffer);
}
FUZZ_TEST(MpscStreamTest, VerifyRoundtripWithBuffer)
    .WithDomains(VectorOf<kBufSize * 2>(Arbitrary<std::byte>()).WithMinSize(1));

TEST(MpscStreamTest, CanRetryAfterPartialWrite) {
  constexpr size_t kChunk = kBufSize - 4;
  MpscTestContext ctx;
  ctx.Connect();
  ctx.writer.SetTimeout(10ms);
  ByteSpan destination = ctx.destination;

  ctx.Spawn([](MpscTestContext* inner) {
    inner->Fill();
    inner->Write();
  });
  ctx.destination = destination.subspan(0, kChunk);
  ctx.Read();
  ctx.Join();
  EXPECT_EQ(ctx.read_result.status(), OkStatus());
  EXPECT_EQ(ctx.read_result->size(), kChunk);
  EXPECT_EQ(ctx.write_status, Status::ResourceExhausted());
  EXPECT_EQ(ctx.writer.last_write(), kChunk);

  ctx.Spawn([](MpscTestContext* inner) {
    inner->data = inner->data.subspan(kChunk);
    inner->Write();
  });
  ctx.destination = destination.subspan(kChunk);
  ctx.Read();
  ctx.Join();
  EXPECT_EQ(ctx.read_result.status(), OkStatus());
  EXPECT_EQ(ctx.read_result->size(), 4U);
  EXPECT_EQ(ctx.write_status, OkStatus());
  EXPECT_EQ(ctx.writer.last_write(), 4U);

  EXPECT_EQ(memcmp(ctx.write_buffer, ctx.read_buffer, kBufSize), 0);
}

TEST(MpscStreamTest, CannotReadAfterReaderClose) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.reader.Close();
  ctx.Read();
  EXPECT_EQ(ctx.read_result.status(), Status::OutOfRange());
}

TEST(MpscStreamTest, CanReadAfterWriterCloses) {
  MpscTestContext ctx;
  ctx.Connect();
  std::byte buffer[kBufSize];
  ctx.reader.SetBuffer(buffer);
  ctx.Fill();
  ctx.Write();
  EXPECT_EQ(ctx.write_status, OkStatus());
  ctx.writer.Close();

  ctx.Read();
  ASSERT_EQ(ctx.read_result.status(), OkStatus());
  ASSERT_EQ(ctx.read_result->size(), kBufSize);
  EXPECT_EQ(memcmp(ctx.write_buffer, ctx.read_buffer, kBufSize), 0);
}

TEST(MpscStreamTest, CannotWriteAfterWriterClose) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.Fill();
  ctx.writer.Close();
  ctx.Write();
  EXPECT_EQ(ctx.write_status, Status::OutOfRange());
}

TEST(MpscStreamTest, CannotWriteAfterReaderClose) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.Fill();
  ctx.reader.Close();
  ctx.Write();
  EXPECT_EQ(ctx.write_status, Status::OutOfRange());
}

TEST(MpscStreamTest, MultipleWriters) {
  MpscTestContext ctx1;
  ctx1.Connect();
  Vector<std::byte, kBufSize + 1> data1(kBufSize + 1, std::byte(1));
  ctx1.data = ByteSpan(data1.data(), data1.size());

  MpscTestContext ctx2;
  ctx2.writer = ctx1.writer;
  Vector<std::byte, kBufSize / 2> data2(kBufSize / 2, std::byte(2));
  ctx2.data = ByteSpan(data2.data(), data2.size());

  MpscTestContext ctx3;
  ctx3.writer = ctx1.writer;
  Vector<std::byte, kBufSize * 3> data3(kBufSize * 3, std::byte(3));
  ctx3.data = ByteSpan(data3.data(), data3.size());

  // Start all threads.
  ctx1.Spawn([](MpscTestContext* ctx) { ctx->Write(); });
  ctx2.Spawn([](MpscTestContext* ctx) { ctx->Write(); });
  ctx3.Spawn([](MpscTestContext* ctx) { ctx->Write(); });

  // The loop below keeps track of how many contiguous values are read, in order
  // to verify that writes are not split or interleaved.
  size_t expected[4] = {0, data1.size(), data2.size(), data3.size()};
  size_t actual[4] = {0};

  size_t total_read = 0;
  auto current = std::byte(0);
  size_t num_current = 0;
  while (total_read < data1.size() + data2.size() + data3.size()) {
    ctx1.Read();
    if (!ctx1.read_result.ok()) {
      break;
    }
    size_t num_read = ctx1.read_result->size();
    for (size_t i = 0; i < num_read; ++i) {
      if (current == ctx1.read_buffer[i]) {
        ++num_current;
        continue;
      }
      actual[size_t(current)] = num_current;
      current = ctx1.read_buffer[i];
      num_current = 1;
    }
    actual[size_t(current)] = num_current;
    total_read += num_read;
  }
  ctx1.reader.Close();
  ctx1.Join();
  ctx2.Join();
  ctx3.Join();
  ASSERT_EQ(ctx1.read_result.status(), OkStatus());
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(actual[i], expected[i]);
  }
}

TEST(MpscStreamTest, GetAndSetLimits) {
  MpscReader reader;
  EXPECT_EQ(reader.ConservativeReadLimit(), 0U);

  MpscWriter writer;
  EXPECT_EQ(writer.ConservativeWriteLimit(), 0U);

  CreateMpscStream(reader, writer);
  EXPECT_EQ(reader.ConservativeReadLimit(), Stream::kUnlimited);
  EXPECT_EQ(writer.ConservativeWriteLimit(), Stream::kUnlimited);

  writer.SetLimit(10);
  EXPECT_EQ(reader.ConservativeReadLimit(), 10U);
  EXPECT_EQ(writer.ConservativeWriteLimit(), 10U);

  writer.Close();
  EXPECT_EQ(reader.ConservativeReadLimit(), 0U);
  EXPECT_EQ(writer.ConservativeWriteLimit(), 0U);
}

TEST(MpscStreamTest, ReaderAggregatesLimit) {
  MpscTestContext ctx;
  ctx.Connect();
  ctx.writer.SetLimit(10);

  MpscWriter writer2 = ctx.writer;
  writer2.SetLimit(20);

  EXPECT_EQ(ctx.reader.ConservativeReadLimit(), 30U);

  ctx.writer.SetLimit(Stream::kUnlimited);
  EXPECT_EQ(ctx.reader.ConservativeReadLimit(), Stream::kUnlimited);

  writer2.SetLimit(40);
  EXPECT_EQ(ctx.reader.ConservativeReadLimit(), Stream::kUnlimited);

  ctx.writer.SetLimit(0);
  EXPECT_EQ(ctx.reader.ConservativeReadLimit(), 40U);
}

TEST(MpscStreamTest, ReadingUpdatesLimit) {
  MpscTestContext ctx;
  ctx.Connect();

  constexpr size_t kChunk = kBufSize - 4;
  std::byte buffer[kBufSize];
  ctx.reader.SetBuffer(buffer);
  ctx.Fill();
  ctx.writer.SetLimit(kBufSize);
  ctx.Write();
  EXPECT_EQ(ctx.write_status, OkStatus());

  ctx.destination = ByteSpan(ctx.read_buffer, kChunk);
  ctx.Read();
  EXPECT_EQ(ctx.read_result.status(), OkStatus());
  EXPECT_EQ(ctx.read_result->size(), kChunk);
  EXPECT_EQ(ctx.reader.ConservativeReadLimit(), kBufSize - kChunk);
}

TEST(MpscStreamTest, CannotWriteMoreThanLimit) {
  MpscTestContext ctx;
  ctx.Connect();

  std::byte buffer[kBufSize];
  ctx.reader.SetBuffer(buffer);
  ctx.writer.SetLimit(kBufSize - 1);
  ctx.Fill();
  ctx.Write();
  EXPECT_EQ(ctx.write_status, Status::ResourceExhausted());
}

TEST(MpscStreamTest, WritersCanCloseAutomatically) {
  MpscTestContext ctx1;
  ctx1.Connect();
  Vector<std::byte, kBufSize + 1> data1(kBufSize + 1, std::byte(1));
  ctx1.writer.SetLimit(data1.size());
  ctx1.data = ByteSpan(data1.data(), data1.size());

  MpscTestContext ctx2;
  ctx2.writer = ctx1.writer;
  Vector<std::byte, kBufSize / 2> data2(kBufSize / 2, std::byte(2));
  ctx2.writer.SetLimit(data2.size());
  ctx2.data = ByteSpan(data2.data(), data2.size());

  // Start all threads.
  EXPECT_TRUE(ctx1.reader.connected());
  EXPECT_TRUE(ctx1.writer.connected());
  EXPECT_TRUE(ctx2.writer.connected());

  ctx1.Spawn([](MpscTestContext* ctx) { ctx->Write(); });
  ctx2.Spawn([](MpscTestContext* ctx) { ctx->Write(); });

  size_t total = 0;
  while (ctx1.reader.ConservativeReadLimit() != 0) {
    ctx1.Read();
    EXPECT_EQ(ctx1.read_result.status(), OkStatus());
    if (!ctx1.read_result.ok()) {
      ctx1.reader.Close();
      break;
    }
    total += ctx1.read_result->size();
  }
  EXPECT_EQ(total, data1.size() + data2.size());
  ctx1.Join();
  ctx2.Join();
  EXPECT_FALSE(ctx1.reader.connected());
  EXPECT_FALSE(ctx1.writer.connected());
  EXPECT_FALSE(ctx2.writer.connected());
}

TEST(MpscStreamTest, ReadAllWithoutBuffer) {
  MpscTestContext ctx;
  Status status = ctx.reader.ReadAll([](ConstByteSpan) { return OkStatus(); });
  EXPECT_EQ(status, Status::FailedPrecondition());
}

TEST(MpscStreamTest, ReadAll) {
  MpscTestContext ctx;
  ctx.Connect();

  std::byte buffer[kBufSize];
  ctx.reader.SetBuffer(buffer);
  ctx.writer.SetLimit(kBufSize * 100);
  ctx.Spawn([](MpscTestContext* inner) { inner->WriteAll(); });

  Status status = ctx.reader.ReadAll([&ctx](ConstByteSpan data) {
    ctx.total_read += data.size();
    fnv1a(data, ctx.read_hash);
    return OkStatus();
  });
  ctx.Join();

  EXPECT_EQ(status, OkStatus());
  EXPECT_FALSE(ctx.reader.connected());
  EXPECT_EQ(ctx.total_read, kBufSize * 100);
  EXPECT_EQ(ctx.read_hash, ctx.write_hash);
}

TEST(MpscStreamTest, BufferedMpscReader) {
  BufferedMpscReader<kBufSize> reader;
  MpscWriter writer;
  CreateMpscStream(reader, writer);

  // `kBufSize` writes of 1 byte each should fit without blocking.
  for (size_t i = 0; i < kBufSize; ++i) {
    std::byte b{static_cast<uint8_t>(i)};
    EXPECT_EQ(writer.Write(ConstByteSpan(&b, 1)), OkStatus());
  }

  std::byte rx_buffer[kBufSize];
  auto result = reader.Read(ByteSpan(rx_buffer));
  ASSERT_EQ(result.status(), OkStatus());
  ASSERT_EQ(result->size(), kBufSize);
  for (size_t i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(rx_buffer[i], std::byte(i));
  }
}

}  // namespace
}  // namespace pw::stream
