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

#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"

#include <cstddef>
#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_containers/vector.h"
#include "pw_unit_test/framework.h"

using std::byte;

namespace pw {
namespace ring_buffer {
namespace {

TEST(PrefixedEntryRingBuffer, NoBuffer) {
  PrefixedEntryRingBuffer ring(false);

  byte buf[32];
  size_t count;

  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.SetBuffer(std::span<byte>(nullptr, 10u)),
            Status::InvalidArgument());
  EXPECT_EQ(ring.SetBuffer(std::span(buf, 0u)), Status::InvalidArgument());
  EXPECT_EQ(ring.FrontEntryDataSizeBytes(), 0u);

  EXPECT_EQ(ring.PushBack(buf), Status::FailedPrecondition());
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PeekFront(buf, &count), Status::FailedPrecondition());
  EXPECT_EQ(count, 0u);
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PeekFrontWithPreamble(buf, &count),
            Status::FailedPrecondition());
  EXPECT_EQ(count, 0u);
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PopFront(), Status::FailedPrecondition());
  EXPECT_EQ(ring.EntryCount(), 0u);
}

// Single entry to write/read/pop over and over again.
constexpr byte single_entry_data[] = {byte(1),
                                      byte(2),
                                      byte(3),
                                      byte(4),
                                      byte(5),
                                      byte(6),
                                      byte(7),
                                      byte(8),
                                      byte(9)};
constexpr size_t single_entry_total_size = sizeof(single_entry_data) + 1;
constexpr size_t single_entry_test_buffer_size =
    (single_entry_total_size * 7) / 2;

// Make sure the single_entry_size is even so single_entry_buffer_Size gets the
// proper wrap/even behavior when getting to the end of the buffer.
static_assert((single_entry_total_size % 2) == 0u);
constexpr size_t kSingleEntryCycles = 300u;

// Repeatedly write the same data, read it, and pop it, done over and over
// again.
void SingleEntryWriteReadTest(bool user_data) {
  PrefixedEntryRingBuffer ring(user_data);
  byte test_buffer[single_entry_test_buffer_size];

  byte read_buffer[single_entry_total_size];

  // Set read_size to an unexpected value to make sure result checks don't luck
  // out and happen to see a previous value.
  size_t read_size = 500U;

  EXPECT_EQ(ring.SetBuffer(test_buffer), Status::Ok());

  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PopFront(), Status::OutOfRange());
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PushBack(std::span(single_entry_data, 0u)),
            Status::InvalidArgument());
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(
      ring.PushBack(std::span(single_entry_data, sizeof(test_buffer) + 5)),
      Status::OutOfRange());
  EXPECT_EQ(ring.EntryCount(), 0u);
  EXPECT_EQ(ring.PeekFront(read_buffer, &read_size), Status::OutOfRange());
  EXPECT_EQ(read_size, 0u);
  read_size = 500U;
  EXPECT_EQ(ring.PeekFrontWithPreamble(read_buffer, &read_size),
            Status::OutOfRange());
  EXPECT_EQ(read_size, 0u);

  size_t user_preamble_bytes = (user_data ? 1 : 0);
  size_t data_size = sizeof(single_entry_data) - user_preamble_bytes;
  size_t data_offset = single_entry_total_size - data_size;

  byte expect_buffer[single_entry_total_size] = {};
  expect_buffer[user_preamble_bytes] = byte(data_size);
  memcpy(expect_buffer + data_offset, single_entry_data, data_size);

  for (size_t i = 0; i < kSingleEntryCycles; i++) {
    ASSERT_EQ(ring.FrontEntryDataSizeBytes(), 0u);
    ASSERT_EQ(ring.FrontEntryTotalSizeBytes(), 0u);

    ASSERT_EQ(ring.PushBack(std::span(single_entry_data, data_size), byte(i)),
              Status::Ok());
    ASSERT_EQ(ring.FrontEntryDataSizeBytes(), data_size);
    ASSERT_EQ(ring.FrontEntryTotalSizeBytes(), single_entry_total_size);

    read_size = 500U;
    ASSERT_EQ(ring.PeekFront(read_buffer, &read_size), Status::Ok());
    ASSERT_EQ(read_size, data_size);

    // ASSERT_THAT(std::span(expect_buffer).last(data_size),
    //            testing::ElementsAreArray(std::span(read_buffer, data_size)));
    ASSERT_EQ(memcmp(std::span(expect_buffer).last(data_size).data(),
                     read_buffer,
                     data_size),
              0);

    read_size = 500U;
    ASSERT_EQ(ring.PeekFrontWithPreamble(read_buffer, &read_size),
              Status::Ok());
    ASSERT_EQ(read_size, single_entry_total_size);
    ASSERT_EQ(ring.PopFront(), Status::Ok());

    if (user_data) {
      expect_buffer[0] = byte(i);
    }

    // ASSERT_THAT(std::span(expect_buffer),
    //            testing::ElementsAreArray(std::span(read_buffer)));
    ASSERT_EQ(memcmp(expect_buffer, read_buffer, single_entry_total_size), 0);
  }
}

TEST(PrefixedEntryRingBuffer, SingleEntryWriteReadNoUserData) {
  SingleEntryWriteReadTest(false);
}

TEST(PrefixedEntryRingBuffer, SingleEntryWriteReadYesUserData) {
  SingleEntryWriteReadTest(true);
}

// TODO(pwbug/196): Increase this to 5000 once we have a way to detect targets
// with more computation and memory oomph.
constexpr size_t kOuterCycles = 50u;
constexpr size_t kCountingUpMaxExpectedEntries =
    single_entry_test_buffer_size / single_entry_total_size;

// Write data that is filled with a byte value that increments each write. Write
// many times without read/pop and then check to make sure correct contents are
// in the ring buffer.
template <bool user_data>
void CountingUpWriteReadTest() {
  PrefixedEntryRingBuffer ring(user_data);
  byte test_buffer[single_entry_test_buffer_size];

  EXPECT_EQ(ring.SetBuffer(test_buffer), Status::Ok());
  EXPECT_EQ(ring.EntryCount(), 0u);

  constexpr size_t data_size = sizeof(single_entry_data) - (user_data ? 1 : 0);

  for (size_t i = 0; i < kOuterCycles; i++) {
    size_t seed = i;

    byte write_buffer[data_size];

    size_t j;
    for (j = 0; j < kSingleEntryCycles; j++) {
      memset(write_buffer, j + seed, sizeof(write_buffer));

      ASSERT_EQ(ring.PushBack(write_buffer), Status::Ok());

      size_t expected_count = (j < kCountingUpMaxExpectedEntries)
                                  ? j + 1
                                  : kCountingUpMaxExpectedEntries;
      ASSERT_EQ(ring.EntryCount(), expected_count);
    }
    size_t final_write_j = j;
    size_t fill_val = seed + final_write_j - kCountingUpMaxExpectedEntries;

    for (j = 0; j < kCountingUpMaxExpectedEntries; j++) {
      byte read_buffer[sizeof(write_buffer)];
      size_t read_size;
      memset(write_buffer, fill_val + j, sizeof(write_buffer));
      ASSERT_EQ(ring.PeekFront(read_buffer, &read_size), Status::Ok());

      ASSERT_EQ(memcmp(write_buffer, read_buffer, data_size), 0);

      ASSERT_EQ(ring.PopFront(), Status::Ok());
    }
  }
}

TEST(PrefixedEntryRingBuffer, CountingUpWriteReadNoUserData) {
  CountingUpWriteReadTest<false>();
}

TEST(PrefixedEntryRingBuffer, CountingUpWriteReadYesUserData) {
  CountingUpWriteReadTest<true>();
}

// Create statically to prevent allocating a capture in the lambda below.
static pw::Vector<byte, single_entry_total_size> read_buffer;

// Repeatedly write the same data, read it, and pop it, done over and over
// again.
void SingleEntryWriteReadWithSectionWriterTest(bool user_data) {
  PrefixedEntryRingBuffer ring(user_data);
  byte test_buffer[single_entry_test_buffer_size];

  EXPECT_EQ(ring.SetBuffer(test_buffer), Status::Ok());

  auto output = [](std::span<const byte> src) -> Status {
    for (byte b : src) {
      read_buffer.push_back(b);
    }
    return Status::Ok();
  };

  size_t user_preamble_bytes = (user_data ? 1 : 0);
  size_t data_size = sizeof(single_entry_data) - user_preamble_bytes;
  size_t data_offset = single_entry_total_size - data_size;

  byte expect_buffer[single_entry_total_size] = {};
  expect_buffer[user_preamble_bytes] = byte(data_size);
  memcpy(expect_buffer + data_offset, single_entry_data, data_size);

  for (size_t i = 0; i < kSingleEntryCycles; i++) {
    ASSERT_EQ(ring.FrontEntryDataSizeBytes(), 0u);
    ASSERT_EQ(ring.FrontEntryTotalSizeBytes(), 0u);

    ASSERT_EQ(ring.PushBack(std::span(single_entry_data, data_size), byte(i)),
              Status::Ok());
    ASSERT_EQ(ring.FrontEntryDataSizeBytes(), data_size);
    ASSERT_EQ(ring.FrontEntryTotalSizeBytes(), single_entry_total_size);

    read_buffer.clear();
    ASSERT_EQ(ring.PeekFront(output), Status::Ok());
    ASSERT_EQ(read_buffer.size(), data_size);

    ASSERT_EQ(memcmp(std::span(expect_buffer).last(data_size).data(),
                     read_buffer.data(),
                     data_size),
              0);

    read_buffer.clear();
    ASSERT_EQ(ring.PeekFrontWithPreamble(output), Status::Ok());
    ASSERT_EQ(read_buffer.size(), single_entry_total_size);
    ASSERT_EQ(ring.PopFront(), Status::Ok());

    if (user_data) {
      expect_buffer[0] = byte(i);
    }

    ASSERT_EQ(
        memcmp(expect_buffer, read_buffer.data(), single_entry_total_size), 0);
  }
}

TEST(PrefixedEntryRingBuffer, SingleEntryWriteReadWithSectionWriterNoUserData) {
  SingleEntryWriteReadWithSectionWriterTest(false);
}

TEST(PrefixedEntryRingBuffer,
     SingleEntryWriteReadWithSectionWriterYesUserData) {
  SingleEntryWriteReadWithSectionWriterTest(true);
}

constexpr size_t kEntrySizeBytes = 8u;
constexpr size_t kTotalEntryCount = 20u;
constexpr size_t kBufferExtraBytes = 5u;
constexpr size_t kTestBufferSize =
    (kEntrySizeBytes * kTotalEntryCount) + kBufferExtraBytes;

// Create statically to prevent allocating a capture in the lambda below.
static pw::Vector<byte, kTestBufferSize> actual_result;

void DeringTest(bool preload) {
  PrefixedEntryRingBuffer ring;

  byte test_buffer[kTestBufferSize];
  EXPECT_EQ(ring.SetBuffer(test_buffer), Status::Ok());

  // Entry data is entry size - preamble (single byte in this case).
  byte single_entry_buffer[kEntrySizeBytes - 1u];
  auto entry_data = std::span(single_entry_buffer);
  size_t i;

  // TODO(pwbug/196): Increase this to 500 once we have a way to detect targets
  // with more computation and memory oomph.
  size_t loop_goal = preload ? 50 : 1;

  for (size_t main_loop_count = 0; main_loop_count < loop_goal;
       main_loop_count++) {
    if (preload) {
      // Prime the ringbuffer with some junk data to get the buffer
      // wrapped.
      for (i = 0; i < (kTotalEntryCount * (main_loop_count % 64u)); i++) {
        memset(single_entry_buffer, i, sizeof(single_entry_buffer));
        ring.PushBack(single_entry_buffer);
      }
    }

    // Build up the expected buffer and fill the ring buffer with the test data.
    pw::Vector<byte, kTestBufferSize> expected_result;
    for (i = 0; i < kTotalEntryCount; i++) {
      // First component of the entry: the varint size.
      static_assert(sizeof(single_entry_buffer) < 127);
      expected_result.push_back(byte(sizeof(single_entry_buffer)));

      // Second component of the entry: the raw data.
      memset(single_entry_buffer, 'a' + i, sizeof(single_entry_buffer));
      for (byte b : entry_data) {
        expected_result.push_back(b);
      }

      // The ring buffer internally pushes the varint size byte.
      ring.PushBack(single_entry_buffer);
    }

    // Check values before doing the dering.
    EXPECT_EQ(ring.EntryCount(), kTotalEntryCount);
    EXPECT_EQ(expected_result.size(), ring.TotalUsedBytes());

    ASSERT_EQ(ring.Dering(), Status::Ok());

    // Check values after doing the dering.
    EXPECT_EQ(ring.EntryCount(), kTotalEntryCount);
    EXPECT_EQ(expected_result.size(), ring.TotalUsedBytes());

    // Read out the entries of the ring buffer.
    actual_result.clear();
    auto output = [](std::span<const byte> src) -> Status {
      for (byte b : src) {
        actual_result.push_back(b);
      }
      return Status::Ok();
    };
    while (ring.EntryCount()) {
      ASSERT_EQ(ring.PeekFrontWithPreamble(output), Status::Ok());
      ASSERT_EQ(ring.PopFront(), Status::Ok());
    }

    // Ensure the actual result out of the ring buffer matches our manually
    // computed result.
    EXPECT_EQ(expected_result.size(), actual_result.size());
    ASSERT_EQ(memcmp(test_buffer, actual_result.data(), actual_result.size()),
              0);
    ASSERT_EQ(
        memcmp(
            expected_result.data(), actual_result.data(), actual_result.size()),
        0);
  }
}

TEST(PrefixedEntryRingBuffer, Dering) { DeringTest(true); }
TEST(PrefixedEntryRingBuffer, DeringNoPreload) { DeringTest(false); }

template <typename T>
Status PushBack(PrefixedEntryRingBuffer& ring, T element) {
  union {
    std::array<byte, sizeof(element)> buffer;
    T item;
  } aliased;
  aliased.item = element;
  return ring.PushBack(aliased.buffer);
}

template <typename T>
Status TryPushBack(PrefixedEntryRingBuffer& ring, T element) {
  union {
    std::array<byte, sizeof(element)> buffer;
    T item;
  } aliased;
  aliased.item = element;
  return ring.TryPushBack(aliased.buffer);
}

template <typename T>
T PeekFront(PrefixedEntryRingBuffer& ring) {
  union {
    std::array<byte, sizeof(T)> buffer;
    T item;
  } aliased;
  size_t bytes_read = 0;
  PW_CHECK_OK(ring.PeekFront(aliased.buffer, &bytes_read));
  PW_CHECK_INT_EQ(bytes_read, sizeof(T));
  return aliased.item;
}

TEST(PrefixedEntryRingBuffer, TryPushBack) {
  PrefixedEntryRingBuffer ring;
  byte test_buffer[kTestBufferSize];
  EXPECT_EQ(ring.SetBuffer(test_buffer), Status::Ok());

  // Fill up the ring buffer with a constant.
  int total_items = 0;
  while (true) {
    Status status = TryPushBack<int>(ring, 5);
    if (status.ok()) {
      total_items++;
    } else {
      EXPECT_EQ(status, Status::ResourceExhausted());
      break;
    }
  }
  EXPECT_EQ(PeekFront<int>(ring), 5);

  // Should be unable to push more items.
  for (int i = 0; i < total_items; ++i) {
    EXPECT_EQ(TryPushBack<int>(ring, 100), Status::ResourceExhausted());
    EXPECT_EQ(PeekFront<int>(ring), 5);
  }

  // Fill up the ring buffer with a constant.
  for (int i = 0; i < total_items; ++i) {
    EXPECT_EQ(PushBack<int>(ring, 100), Status::Ok());
  }
  EXPECT_EQ(PeekFront<int>(ring), 100);
}

}  // namespace
}  // namespace ring_buffer
}  // namespace pw
