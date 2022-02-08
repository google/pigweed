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

#include "pw_log_rpc_private/test_utils.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_protobuf/bytes_utils.h"
#include "pw_protobuf/decoder.h"

namespace pw::log_rpc {

// Unpacks a `LogEntry` proto buffer to compare it with the expected data and
// updates the total drop count found.
void VerifyLogEntry(protobuf::Decoder& entry_decoder,
                    const TestLogEntry& expected_entry,
                    uint32_t& drop_count_out) {
  ConstByteSpan tokenized_data;
  if (!expected_entry.tokenized_data.empty()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(),
              static_cast<uint32_t>(log::LogEntry::Fields::MESSAGE));
    ASSERT_TRUE(entry_decoder.ReadBytes(&tokenized_data).ok());
    if (tokenized_data.size() != expected_entry.tokenized_data.size()) {
      PW_LOG_ERROR(
          "actual: '%s', expected: '%s'",
          reinterpret_cast<const char*>(tokenized_data.begin()),
          reinterpret_cast<const char*>(expected_entry.tokenized_data.begin()));
    }
    EXPECT_EQ(tokenized_data.size(), expected_entry.tokenized_data.size());
    EXPECT_EQ(std::memcmp(tokenized_data.begin(),
                          expected_entry.tokenized_data.begin(),
                          expected_entry.tokenized_data.size()),
              0);
  }
  if (expected_entry.metadata.level()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(),
              static_cast<uint32_t>(log::LogEntry::Fields::LINE_LEVEL));
    uint32_t line_level;
    ASSERT_TRUE(entry_decoder.ReadUint32(&line_level).ok());
    EXPECT_EQ(expected_entry.metadata.level(),
              line_level & PW_LOG_LEVEL_BITMASK);
    EXPECT_EQ(expected_entry.metadata.line_number(),
              (line_level & ~PW_LOG_LEVEL_BITMASK) >> PW_LOG_LEVEL_BITS);
  }
  if (expected_entry.metadata.flags()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(),
              static_cast<uint32_t>(log::LogEntry::Fields::FLAGS));
    uint32_t flags;
    ASSERT_TRUE(entry_decoder.ReadUint32(&flags).ok());
    EXPECT_EQ(expected_entry.metadata.flags(), flags);
  }
  if (expected_entry.timestamp) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_TRUE(entry_decoder.FieldNumber() ==
                    static_cast<uint32_t>(log::LogEntry::Fields::TIMESTAMP) ||
                entry_decoder.FieldNumber() ==
                    static_cast<uint32_t>(
                        log::LogEntry::Fields::TIME_SINCE_LAST_ENTRY));
    int64_t timestamp;
    ASSERT_TRUE(entry_decoder.ReadInt64(&timestamp).ok());
    EXPECT_EQ(expected_entry.timestamp, timestamp);
  }
  if (expected_entry.dropped) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(),
              static_cast<uint32_t>(log::LogEntry::Fields::DROPPED));
    uint32_t dropped = 0;
    ASSERT_TRUE(entry_decoder.ReadUint32(&dropped).ok());
    EXPECT_EQ(expected_entry.dropped, dropped);
    drop_count_out += dropped;
  }
  if (expected_entry.metadata.module()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(),
              static_cast<uint32_t>(log::LogEntry::Fields::MODULE));
    const Result<uint32_t> module =
        protobuf::DecodeBytesToUint32(entry_decoder);
    ASSERT_EQ(module.status(), OkStatus());
    EXPECT_EQ(expected_entry.metadata.module(), module.value());
  }
}

// Verifies a stream of log entries and updates the total drop count found.
size_t VerifyLogEntries(protobuf::Decoder& entries_decoder,
                        Vector<TestLogEntry>& expected_entries_stack,
                        uint32_t expected_first_entry_sequence_id,
                        uint32_t& drop_count_out) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    if (static_cast<pw::log::LogEntries::Fields>(
            entries_decoder.FieldNumber()) ==
        log::LogEntries::Fields::ENTRIES) {
      ConstByteSpan entry;
      EXPECT_EQ(entries_decoder.ReadBytes(&entry), OkStatus());
      protobuf::Decoder entry_decoder(entry);
      if (expected_entries_stack.empty()) {
        break;
      }
      // Keep track of entries and drops respective counts.
      uint32_t current_drop_count = 0;
      VerifyLogEntry(
          entry_decoder, expected_entries_stack.back(), current_drop_count);
      drop_count_out += current_drop_count;
      if (current_drop_count == 0) {
        ++entries_found;
      }
      expected_entries_stack.pop_back();
    } else if (static_cast<pw::log::LogEntries::Fields>(
                   entries_decoder.FieldNumber()) ==
               log::LogEntries::Fields::FIRST_ENTRY_SEQUENCE_ID) {
      uint32_t first_entry_sequence_id = 0;
      EXPECT_EQ(entries_decoder.ReadUint32(&first_entry_sequence_id),
                OkStatus());
      EXPECT_EQ(expected_first_entry_sequence_id, first_entry_sequence_id);
    }
  }
  return entries_found;
}

size_t CountLogEntries(protobuf::Decoder& entries_decoder) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    if (static_cast<pw::log::LogEntries::Fields>(
            entries_decoder.FieldNumber()) ==
        log::LogEntries::Fields::ENTRIES) {
      ++entries_found;
    }
  }
  return entries_found;
}

}  // namespace pw::log_rpc
