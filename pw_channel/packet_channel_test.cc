// Copyright 2025 The Pigweed Authors
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

#include "pw_channel/packet_channel.h"

#include <utility>

#include "lib/stdcompat/utility.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/task.h"
#include "pw_async2/try.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_containers/inline_deque.h"
#include "pw_containers/vector.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::Result;
using pw::Status;
using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::Task;
using pw::async2::Waker;
using pw::channel::AnyPacketChannel;
using pw::channel::kReadable;
using pw::channel::kWritable;
using pw::channel::PacketReader;
using pw::channel::PacketReaderWriter;
using pw::channel::PacketWriter;

class TestPacket {
 public:
  explicit constexpr TestPacket(int value) : value_(value) {}

  constexpr TestPacket(TestPacket&& other)
      : value_(cpp20::exchange(other.value_, kEmpty)) {}

  constexpr TestPacket& operator=(TestPacket&& other) {
    value_ = cpp20::exchange(other.value_, kEmpty);
    return *this;
  }

  int value() const { return value_; }

  bool operator==(const TestPacket& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const TestPacket& other) const {
    return value_ != other.value_;
  }

 private:
  static constexpr int kEmpty = -1;

  int value_;
};

class PacketReaderWriterStub
    : public pw::channel::Implement<PacketReaderWriter<TestPacket>> {
 public:
  constexpr PacketReaderWriterStub() = default;

 private:
  Poll<Result<TestPacket>> DoPendRead(Context&) override { return Pending(); }

  Poll<Status> DoPendReadyToWrite(Context&, size_t) override {
    return Pending();
  }

  void DoStageWrite(TestPacket&&) override {}

  Poll<> DoPendWrite(Context&) override { return Pending(); }

  pw::async2::Poll<pw::Status> DoPendClose(pw::async2::Context&) override {
    return Pending();
  }
};

class ReadOnlyPacketStub
    : public pw::channel::Implement<PacketReader<TestPacket>> {
 public:
  constexpr ReadOnlyPacketStub() = default;

 private:
  Poll<Result<TestPacket>> DoPendRead(Context&) override { return Pending(); }

  pw::async2::Poll<pw::Status> DoPendClose(pw::async2::Context&) override {
    return Pending();
  }
};

template <typename Packet = TestPacket>
class TestPacketWriterImpl
    : public pw::channel::Implement<pw::channel::PacketWriter<Packet>> {
 public:
  pw::span<const Packet> packets() const { return packets_; }

  void set_ready_to_write(bool ready) {
    bool old_ready_to_write = ready_to_write_;
    ready_to_write_ = ready;
    if (!old_ready_to_write && ready_to_write_ && !waker_.IsEmpty()) {
      std::move(waker_).Wake();
    }
  }
  void ClearPackets() { packets_.clear(); }

  // Expose for testing
  using pw::channel::Implement<
      pw::channel::PacketWriter<Packet>>::GetAvailableWrites;

 private:
  Poll<Status> DoPendReadyToWrite(pw::async2::Context& context,
                                  size_t /*num*/) override {
    if (ready_to_write_) {
      return pw::async2::Ready(pw::OkStatus());
    }
    PW_ASYNC_STORE_WAKER(context, waker_, "waiting for set_ready_to_write");
    return pw::async2::Pending();
  }

  void DoStageWrite(Packet&& packet) override {
    packets_.push_back(std::move(packet));
  }

  Poll<> DoPendWrite(pw::async2::Context& /*context*/) override {
    // This basic stub considers writes flushed immediately after staging.
    return pw::async2::Ready();
  }

  pw::async2::Poll<pw::Status> DoPendClose(pw::async2::Context&) override {
    return pw::async2::Ready(pw::OkStatus());
  }

  bool ready_to_write_ = true;
  pw::async2::Waker waker_;
  pw::Vector<Packet, 5> packets_;
};

// Test reader implementation that allows pushing packets for reading.
class TestPacketReader
    : public pw::channel::Implement<PacketReader<TestPacket>> {
 public:
  TestPacketReader() = default;

  void PushPacket(TestPacket packet) {
    bool was_empty = packet_queue_.empty();
    packet_queue_.push_back(std::move(packet));
    if (was_empty && !read_waker_.IsEmpty()) {
      std::move(read_waker_).Wake();
    }
  }

  void SimulateEndOfStream() {
    end_of_stream_ = true;
    if (!read_waker_.IsEmpty()) {
      std::move(read_waker_).Wake();
    }
  }

  size_t queue_size() const { return packet_queue_.size(); }

 private:
  Poll<Result<TestPacket>> DoPendRead(Context& cx) override {
    if (!packet_queue_.empty()) {
      TestPacket packet = std::move(packet_queue_.front());
      packet_queue_.pop_front();
      return Result<TestPacket>(std::move(packet));
    }
    if (end_of_stream_) {
      return Result<TestPacket>(Status::OutOfRange());
    }
    PW_ASYNC_STORE_WAKER(
        cx, read_waker_, "TestPacketReader waiting for PushPacket");
    return Pending();
  }

  pw::async2::Poll<pw::Status> DoPendClose(pw::async2::Context&) override {
    return pw::async2::Ready(pw::OkStatus());
  }

  Waker read_waker_;
  pw::InlineDeque<TestPacket, 5> packet_queue_;
  bool end_of_stream_ = false;
};

// Test task for reading packets
class ReadPacketsTask : public Task {
 public:
  ReadPacketsTask(PacketReader<TestPacket>& reader, size_t packets_to_read)
      : Task(PW_ASYNC_TASK_NAME("ReadPacketsTask")),
        reader_(reader),
        packets_to_read_(packets_to_read) {}

  int ran_to_completion = 0;
  pw::Vector<TestPacket, 5> received_packets;
  Status final_status = Status::Unknown();

 private:
  Poll<> DoPend(Context& cx) override {
    while (received_packets.size() < packets_to_read_) {
      Poll<Result<TestPacket>> poll_result = reader_.PendRead(cx);
      if (poll_result.IsPending()) {
        return Pending();
      }
      if (!poll_result->ok()) {
        final_status = poll_result->status();
        ran_to_completion_with_error_ += 1;
        return Ready();
      }
      received_packets.push_back(std::move(poll_result->value()));
    }
    final_status = pw::OkStatus();
    ran_to_completion += 1;
    return Ready();
  }

  PacketReader<TestPacket>& reader_;
  size_t packets_to_read_;
  int ran_to_completion_with_error_ = 0;
};

TEST(PacketChannel, ReadOnlyChannelProperties) {
  ReadOnlyPacketStub stub_impl;
  auto& channel = stub_impl.channel();

  EXPECT_TRUE(channel.readable());
  EXPECT_FALSE(channel.writable());
  EXPECT_TRUE(channel.is_read_open());
  EXPECT_FALSE(channel.is_write_open());
}

TEST(PacketChannel, WriteOnlyChannelProperties) {
  TestPacketWriterImpl<> writer_impl;
  auto& channel = writer_impl.channel();

  EXPECT_FALSE(channel.readable());
  EXPECT_TRUE(channel.writable());
  EXPECT_FALSE(channel.is_read_open());
  EXPECT_TRUE(channel.is_write_open());
  EXPECT_EQ(writer_impl.GetAvailableWrites(), pw::channel::kNoFlowControl);
}

TEST(PacketChannel, TestPacketReader_ReadPackets) {
  Dispatcher dispatcher;
  TestPacketReader reader_impl;
  auto& channel = reader_impl.channel();

  ReadPacketsTask read_task(channel, 3);
  dispatcher.Post(read_task);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(read_task.ran_to_completion, 0);
  EXPECT_EQ(reader_impl.queue_size(), 0u);

  reader_impl.PushPacket(TestPacket(10));
  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(read_task.received_packets.size(), 1u);
  EXPECT_EQ(read_task.received_packets[0].value(), 10);

  reader_impl.PushPacket(TestPacket(20));
  reader_impl.PushPacket(TestPacket(30));
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(read_task.ran_to_completion, 1);
  ASSERT_EQ(read_task.received_packets.size(), 3u);
  EXPECT_EQ(read_task.received_packets[0].value(), 10);
  EXPECT_EQ(read_task.received_packets[1].value(), 20);
  EXPECT_EQ(read_task.received_packets[2].value(), 30);
  EXPECT_EQ(read_task.final_status, pw::OkStatus());
}

TEST(PacketChannel, TestPacketReader_ReadUntilEndOfStream) {
  Dispatcher dispatcher;
  TestPacketReader reader_impl;
  auto& channel = reader_impl.channel();

  ReadPacketsTask read_task(channel, 5);
  dispatcher.Post(read_task);

  reader_impl.PushPacket(TestPacket(1));
  reader_impl.PushPacket(TestPacket(2));
  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  ASSERT_EQ(read_task.received_packets.size(), 2u);

  reader_impl.SimulateEndOfStream();
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(read_task.final_status, Status::OutOfRange());
  ASSERT_EQ(read_task.received_packets.size(), 2u);
  EXPECT_EQ(read_task.received_packets[0].value(), 1);
  EXPECT_EQ(read_task.received_packets[1].value(), 2);
}

// Helper functions for conversion tests
void TakesAnyPacketChannel(const AnyPacketChannel<TestPacket>&) {}
void TakesPacketReader(const PacketReader<TestPacket>&) {}
void TakesPacketWriter(const PacketWriter<TestPacket>&) {}
const PacketReaderWriter<TestPacket>& TakesPacketReaderWriter(
    const PacketReaderWriter<TestPacket>& channel) {
  return channel;
}

[[maybe_unused]] void TakesPacketReader(const int&) {}
[[maybe_unused]] void TakesPacketReader(const PacketReader<int>&) {}
[[maybe_unused]] void TakesPacketReader(PacketReader<int>&) {}

TEST(PacketChannel, Conversions) {
  PacketReaderWriterStub rws;
  ReadOnlyPacketStub rs;
  TestPacketWriterImpl<> ws;

  // PacketReaderWriter
  TakesAnyPacketChannel(rws);
  TakesAnyPacketChannel(rws.channel());
  TakesPacketReader(rws.channel());
  TakesPacketWriter(rws.channel());
  TakesPacketReaderWriter(rws.channel());

  TakesAnyPacketChannel(rws.channel().as<AnyPacketChannel<TestPacket>>());
  TakesPacketReader(rws.channel().as<kReadable>());
  TakesPacketWriter(rws.channel().as<kWritable>());
  TakesPacketReaderWriter(rws.channel().as<kReadable, kWritable>());

  // ReadOnlyPacketStub
  TakesAnyPacketChannel(rs);
  TakesAnyPacketChannel(rs.channel());
  TakesPacketReader(rs.channel());
  TakesPacketReader(rs.channel().as<kReadable>());
#if PW_NC_TEST(CannotConvertReadOnlyToWriter)
  PW_NC_EXPECT("private");
  TakesPacketWriter(rs.channel());
#endif  // PW_NC_TEST

  // TestPacketWriterImpl
  TakesAnyPacketChannel(ws);
  TakesAnyPacketChannel(ws.channel());
  TakesPacketWriter(ws.channel());
  TakesPacketWriter(ws.channel().as<kWritable>());
#if PW_NC_TEST(CannotConvertWriteOnlyToReader)
  PW_NC_EXPECT("private");
  TakesPacketReader(ws.channel());
#endif  // PW_NC_TEST
}

class WriteFivePackets : public pw::async2::Task {
 public:
  WriteFivePackets(pw::channel::PacketWriter<TestPacket>& writer)
      : Task(PW_ASYNC_TASK_NAME("WriteFivePackets test task")),
        writer_(writer) {}

  int ran_to_completion = 0;

 private:
  Poll<> DoPend(pw::async2::Context& cx) override {
    for (; next_packet_ <= 5; ++next_packet_) {
      PW_TRY_READY_ASSIGN(
          pw::Result<pw::channel::PendingWrite<TestPacket>> pending,
          writer_.PendReadyToWrite(cx));

      PW_TEST_EXPECT_OK(pending);

      TestPacket packet(next_packet_);
      pending->Stage(std::move(packet));

      PW_TRY_READY(writer_.PendWrite(cx));
    }

    ran_to_completion += 1;
    return pw::async2::Ready();
  }

  pw::channel::PacketWriter<TestPacket>& writer_;
  int next_packet_ = 1;
};

TEST(PacketWriter, Write) {
  TestPacketWriterImpl<> writer_impl;
  auto& writer = writer_impl.channel();

  pw::async2::Dispatcher dispatcher;
  WriteFivePackets task(writer);
  dispatcher.Post(task);

  writer_impl.set_ready_to_write(false);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());

  writer_impl.set_ready_to_write(true);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(task.ran_to_completion, 1);

  ASSERT_EQ(writer_impl.packets().size(), 5u);
  int i = 1;
  for (const TestPacket& packet : writer_impl.packets()) {
    ASSERT_EQ(i++, packet.value());
  }
}

TEST(PacketWriter, WriteWithFlowControl) {
  TestPacketWriterImpl<> writer_impl;
  auto& writer = writer_impl.channel();

  pw::async2::Dispatcher dispatcher;
  WriteFivePackets task(writer);
  dispatcher.Post(task);

  ASSERT_EQ(writer_impl.GetAvailableWrites(), pw::channel::kNoFlowControl);

  writer.SetAvailableWrites(0);
  ASSERT_FALSE(dispatcher.RunUntilStalled().IsReady());

  EXPECT_TRUE(writer_impl.packets().empty());

  writer.SetAvailableWrites(2);
  ASSERT_FALSE(dispatcher.RunUntilStalled().IsReady());

  EXPECT_EQ(writer_impl.GetAvailableWrites(), 0u);
  ASSERT_EQ(writer_impl.packets().size(), 2u);
  int i = 1;
  for (const TestPacket& packet : writer_impl.packets()) {
    ASSERT_EQ(i++, packet.value());
  }

  writer.AcknowledgeWrites(4);
  ASSERT_TRUE(dispatcher.RunUntilStalled().IsReady());

  EXPECT_EQ(writer_impl.GetAvailableWrites(), 1u);
  ASSERT_EQ(writer_impl.packets().size(), 5u);
  i = 1;
  for (const TestPacket& packet : writer_impl.packets()) {
    ASSERT_EQ(i++, packet.value());
  }
  EXPECT_EQ(task.ran_to_completion, 1);
}

#if PW_NC_TEST(PacketChannel_CannotCallReadOnWriter)
PW_NC_EXPECT("PendRead may only be called on readable channels");
void CallReadOnWriter() {
  TestPacketWriterImpl<> ws;
  Dispatcher d;
  Context cx = d.MakeContext(ws.channel(), d);
  ws.channel().PendRead(cx);
}
#elif PW_NC_TEST(PacketChannel_CannotCallWriteOnReader_PendReadyToWrite)
PW_NC_EXPECT("PendReadyToWrite may only be called on writable channels");
void CallWriteOnReader_PendReadyToWrite() {
  ReadOnlyPacketStub rs;
  Dispatcher d;
  Context cx = d.MakeContext(rs.channel(), d);
  rs.channel().PendReadyToWrite(cx);
}
#elif PW_NC_TEST(PacketChannel_CannotCallWriteOnReader_PendWrite)
PW_NC_EXPECT("PendWrite may only be called on writable channels");
void CallWriteOnReader_PendWrite() {
  ReadOnlyPacketStub rs;
  Dispatcher d;
  Context cx = d.MakeContext(rs.channel(), d);
  rs.channel().PendWrite(cx);
}
#elif PW_NC_TEST(PacketChannel_CannotConvertReaderToWriter)
PW_NC_EXPECT("Cannot use a non-writable channel as a writable channel");
void ConvertReaderToWriter() {
  ReadOnlyPacketStub rs;
  TakesPacketWriter(rs.channel().template as<PacketWriter<TestPacket>>());
}
#elif PW_NC_TEST(PacketChannel_CannotConvertWriterToReader)
PW_NC_EXPECT("Cannot use a non-readable channel as a readable channel");
void ConvertWriterToReader() {
  TestPacketWriterImpl<> ws;
  TakesPacketReader(ws.channel().template as<PacketReader<TestPacket>>());
}
#endif  // PW_NC_TEST

}  // namespace
