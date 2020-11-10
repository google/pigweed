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

#include "pw_log_multisink/log_queue.h"

#include "gtest/gtest.h"
#include "pw_log/levels.h"
#include "pw_log_proto/log.pwpb.h"
#include "pw_protobuf/decoder.h"

namespace pw::log_rpc {
namespace {

constexpr size_t kEncodeBufferSize = 512;

constexpr const char kTokenizedMessage[] = "msg_token";
constexpr uint32_t kFlags = 0xF;
constexpr uint32_t kLevel = 0b010;
constexpr uint32_t kLine = 0b101011000;
constexpr uint32_t kTokenizedThread = 0xF;
constexpr int64_t kTimestamp = 0;

constexpr size_t kLogBufferSize = kEncodeBufferSize * 3;

void VerifyLogEntry(pw::protobuf::Decoder& log_decoder,
                    const char* expected_tokenized_message,
                    const uint32_t expected_flags,
                    const uint32_t expected_level,
                    const uint32_t expected_line,
                    const uint32_t expected_tokenized_thread,
                    const int64_t expected_timestamp) {
  ConstByteSpan log_entry_message;
  EXPECT_TRUE(log_decoder.Next().ok());  // preamble
  EXPECT_EQ(1U, log_decoder.FieldNumber());
  EXPECT_TRUE(log_decoder.ReadBytes(&log_entry_message).ok());

  pw::protobuf::Decoder entry_decoder(log_entry_message);
  ConstByteSpan tokenized_message;
  EXPECT_TRUE(entry_decoder.Next().ok());  // tokenized_message
  EXPECT_EQ(1U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadBytes(&tokenized_message).ok());
  EXPECT_TRUE(std::memcmp(tokenized_message.begin(),
                          (const void*)expected_tokenized_message,
                          tokenized_message.size()) == 0);

  uint32_t line_level;
  EXPECT_TRUE(entry_decoder.Next().ok());  // line_level
  EXPECT_EQ(2U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadUint32(&line_level).ok());
  EXPECT_EQ(expected_level, line_level & PW_LOG_LEVEL_BITMASK);
  EXPECT_EQ(expected_line,
            (line_level & ~PW_LOG_LEVEL_BITMASK) >> PW_LOG_LEVEL_BITWIDTH);

  uint32_t flags;
  EXPECT_TRUE(entry_decoder.Next().ok());  // flags
  EXPECT_EQ(3U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadUint32(&flags).ok());
  EXPECT_EQ(expected_flags, flags);

  uint32_t tokenized_thread;
  EXPECT_TRUE(entry_decoder.Next().ok());  // tokenized_thread
  EXPECT_EQ(4U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadUint32(&tokenized_thread).ok());
  EXPECT_EQ(expected_tokenized_thread, tokenized_thread);

  int64_t timestamp;
  EXPECT_TRUE(entry_decoder.Next().ok());  // timestamp
  EXPECT_EQ(5U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadInt64(&timestamp).ok());
  EXPECT_EQ(expected_timestamp, timestamp);
}

}  // namespace

TEST(LogQueue, SinglePushPopTokenizedMessage) {
  std::byte log_buffer[kLogBufferSize];
  LogQueueWithEncodeBuffer<kEncodeBufferSize> log_queue(log_buffer);

  EXPECT_EQ(Status::OK,
            log_queue.PushTokenizedMessage(
                std::as_bytes(std::span(kTokenizedMessage)),
                kFlags,
                kLevel,
                kLine,
                kTokenizedThread,
                kTimestamp));

  std::byte log_entry[kEncodeBufferSize];
  Result<LogEntries> pop_result = log_queue.Pop(std::span(log_entry));
  EXPECT_TRUE(pop_result.ok());

  pw::protobuf::Decoder log_decoder(pop_result.value().entries);
  EXPECT_EQ(pop_result.value().entry_count, 1U);
  VerifyLogEntry(log_decoder,
                 kTokenizedMessage,
                 kFlags,
                 kLevel,
                 kLine,
                 kTokenizedThread,
                 kTimestamp);
}

TEST(LogQueue, MultiplePushPopTokenizedMessage) {
  constexpr size_t kEntryCount = 3;

  std::byte log_buffer[1024];
  LogQueueWithEncodeBuffer<kEncodeBufferSize> log_queue(log_buffer);

  for (size_t i = 0; i < kEntryCount; i++) {
    EXPECT_EQ(Status::OK,
              log_queue.PushTokenizedMessage(
                  std::as_bytes(std::span(kTokenizedMessage)),
                  kFlags,
                  kLevel,
                  kLine + (i << 3),
                  kTokenizedThread,
                  kTimestamp + i));
  }

  std::byte log_entry[kEncodeBufferSize];
  for (size_t i = 0; i < kEntryCount; i++) {
    Result<LogEntries> pop_result = log_queue.Pop(std::span(log_entry));
    EXPECT_TRUE(pop_result.ok());

    pw::protobuf::Decoder log_decoder(pop_result.value().entries);
    EXPECT_EQ(pop_result.value().entry_count, 1U);
    VerifyLogEntry(log_decoder,
                   kTokenizedMessage,
                   kFlags,
                   kLevel,
                   kLine + (i << 3),
                   kTokenizedThread,
                   kTimestamp + i);
  }
}

TEST(LogQueue, PopMultiple) {
  constexpr size_t kEntryCount = 3;

  std::byte log_buffer[kLogBufferSize];
  LogQueueWithEncodeBuffer<kEncodeBufferSize> log_queue(log_buffer);

  for (size_t i = 0; i < kEntryCount; i++) {
    EXPECT_EQ(Status::OK,
              log_queue.PushTokenizedMessage(
                  std::as_bytes(std::span(kTokenizedMessage)),
                  kFlags,
                  kLevel,
                  kLine + (i << 3),
                  kTokenizedThread,
                  kTimestamp + i));
  }

  std::byte log_entries[kLogBufferSize];
  Result<LogEntries> pop_result = log_queue.PopMultiple(log_entries);
  EXPECT_TRUE(pop_result.ok());

  pw::protobuf::Decoder log_decoder(pop_result.value().entries);
  EXPECT_EQ(pop_result.value().entry_count, kEntryCount);
  for (size_t i = 0; i < kEntryCount; i++) {
    VerifyLogEntry(log_decoder,
                   kTokenizedMessage,
                   kFlags,
                   kLevel,
                   kLine + (i << 3),
                   kTokenizedThread,
                   kTimestamp + i);
  }
}

TEST(LogQueue, TooSmallEncodeBuffer) {
  constexpr size_t kSmallBuffer = 1;

  std::byte log_buffer[kLogBufferSize];
  LogQueueWithEncodeBuffer<kSmallBuffer> log_queue(log_buffer);
  EXPECT_EQ(Status::INTERNAL,
            log_queue.PushTokenizedMessage(
                std::as_bytes(std::span(kTokenizedMessage)),
                kFlags,
                kLevel,
                kLine,
                kTokenizedThread,
                kTimestamp));
}

TEST(LogQueue, TooSmallLogBuffer) {
  constexpr size_t kSmallerThanPreamble = 1;
  constexpr size_t kEntryCount = 100;

  // Expect OUT_OF_RANGE when the buffer is smaller than a preamble.
  std::byte log_buffer[kLogBufferSize];
  LogQueueWithEncodeBuffer<kEncodeBufferSize> log_queue_small(
      std::span(log_buffer, kSmallerThanPreamble));
  EXPECT_EQ(Status::OUT_OF_RANGE,
            log_queue_small.PushTokenizedMessage(
                std::as_bytes(std::span(kTokenizedMessage)),
                kFlags,
                kLevel,
                kLine,
                kTokenizedThread,
                kTimestamp));

  // Expect RESOURCE_EXHAUSTED when there's not enough space for the chunk.
  LogQueueWithEncodeBuffer<kEncodeBufferSize> log_queue_medium(log_buffer);
  for (size_t i = 0; i < kEntryCount; i++) {
    log_queue_medium.PushTokenizedMessage(
        std::as_bytes(std::span(kTokenizedMessage)),
        kFlags,
        kLevel,
        kLine,
        kTokenizedThread,
        kTimestamp);
  }
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED,
            log_queue_medium.PushTokenizedMessage(
                std::as_bytes(std::span(kTokenizedMessage)),
                kFlags,
                kLevel,
                kLine,
                kTokenizedThread,
                kTimestamp));
}

}  // namespace pw::log_rpc
