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
#include "pw_persistent_ram/persistent_buffer.h"

#include <cstddef>
#include <type_traits>

#include "pw_bytes/span.h"
#include "pw_random/xor_shift.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::persistent_ram {
namespace {

class PersistentTest : public ::testing::Test {
 protected:
  static constexpr size_t kBufferSize = 256;
  PersistentTest() { ZeroPersistentMemory(); }

  // Emulate invalidation of persistent section(s).
  void ZeroPersistentMemory() { memset(buffer_, 0, sizeof(buffer_)); }
  void RandomFillMemory() {
    random::XorShiftStarRng64 rng(0x9ad75);
    rng.Get(buffer_);
  }

  PersistentBuffer<kBufferSize>& GetPersistentBuffer() {
    return *(new (buffer_) PersistentBuffer<kBufferSize>());
  }

  // Allocate a chunk of aligned storage that can be independently controlled.
  alignas(PersistentBuffer<kBufferSize>)
      std::byte buffer_[sizeof(PersistentBuffer<kBufferSize>)];
};

TEST_F(PersistentTest, DefaultConstructionAndDestruction) {
  constexpr uint32_t kExpectedNumber = 0x6C2C6582;
  {
    // Emulate a boot where the persistent sections were invalidated.
    // Although the fixture always does this, we do this an extra time to be
    // 100% confident that an integrity check cannot be accidentally selected
    // which results in reporting there is valid data when zero'd.
    ZeroPersistentMemory();
    auto& persistent = GetPersistentBuffer();
    auto writer = persistent.GetWriter();
    EXPECT_EQ(persistent.size(), 0u);

    ASSERT_EQ(OkStatus(), writer.Write(as_bytes(span(&kExpectedNumber, 1))));
    ASSERT_TRUE(persistent.has_value());

    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Emulate a boot where persistent memory was kept as is.
    auto& persistent = GetPersistentBuffer();
    ASSERT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), sizeof(kExpectedNumber));

    uint32_t temp = 0;
    memcpy(&temp, persistent.data(), sizeof(temp));
    EXPECT_EQ(temp, kExpectedNumber);
  }
}

TEST_F(PersistentTest, LongData) {
  constexpr std::string_view kTestString(
      "A nice string should remain valid even if written incrementally!");
  constexpr size_t kWriteSize = 5;

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    ASSERT_FALSE(persistent.has_value());

    auto writer = persistent.GetWriter();
    for (size_t i = 0; i < kTestString.length(); i += kWriteSize) {
      ASSERT_EQ(OkStatus(),
                writer.Write(kTestString.data() + i,
                             std::min(kWriteSize, kTestString.length() - i)));
    }
    // Need to manually write a null terminator since std::string_view doesn't
    // include one in the string length.
    ASSERT_EQ(OkStatus(), writer.Write(std::byte(0)));

    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Ensure data is valid.
    auto& persistent = GetPersistentBuffer();
    ASSERT_TRUE(persistent.has_value());
    ASSERT_STREQ(kTestString.data(),
                 reinterpret_cast<const char*>(persistent.data()));
  }
}

TEST_F(PersistentTest, MostlyFilled) {
  std::array<std::byte, kBufferSize - 3> test_data;
  constexpr size_t kWriteSize = 11;
  random::XorShiftStarRng64 test_data_generator(0xDA960FD9);
  test_data_generator.Get(test_data);

  static_assert(test_data.size() < kBufferSize);

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    EXPECT_FALSE(persistent.has_value());

    auto writer = persistent.GetWriter();
    for (size_t i = 0; i < test_data.size(); i += kWriteSize) {
      EXPECT_EQ(OkStatus(),
                writer.Write(test_data.data() + i,
                             std::min(kWriteSize, test_data.size() - i)));
    }
    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Ensure data is valid.
    auto& persistent = GetPersistentBuffer();
    EXPECT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), test_data.size());
    EXPECT_EQ(
        std::memcmp(test_data.data(), persistent.data(), persistent.size()), 0);
  }
}

TEST_F(PersistentTest, AttemptOversizedWrite) {
  std::array<std::byte, kBufferSize - 3> test_data;
  constexpr size_t kWriteSize = 11;
  random::XorShiftStarRng64 test_data_generator(0xDA960FD9);
  test_data_generator.Get(test_data);

  static_assert(test_data.size() < kBufferSize);

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    EXPECT_FALSE(persistent.has_value());

    auto writer = persistent.GetWriter();
    for (size_t i = 0; i < test_data.size(); i += kWriteSize) {
      EXPECT_EQ(OkStatus(),
                writer.Write(test_data.data() + i,
                             std::min(kWriteSize, test_data.size() - i)));
    }

    // This final write is guaranteed to be too big, but shouldn't corrupt the
    // final contents of the buffer.
    constexpr size_t kFinalWriteSize = 21;
    EXPECT_GT(writer.ConservativeWriteLimit(), 0u);
    EXPECT_GT(kFinalWriteSize, writer.ConservativeWriteLimit());
    EXPECT_EQ(Status::ResourceExhausted(),
              writer.Write(test_data.data(), kFinalWriteSize));

    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Ensure data is valid.
    auto& persistent = GetPersistentBuffer();
    EXPECT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), test_data.size());
    EXPECT_EQ(
        std::memcmp(test_data.data(), persistent.data(), persistent.size()), 0);
  }
}

TEST_F(PersistentTest, Filled) {
  std::array<std::byte, kBufferSize> test_data;
  constexpr size_t kWriteSize = 5;
  random::XorShiftStarRng64 test_data_generator(0x4BEDED8F);
  test_data_generator.Get(test_data);

  static_assert(test_data.size() == kBufferSize);

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    EXPECT_FALSE(persistent.has_value());

    auto writer = persistent.GetWriter();
    for (size_t i = 0; i < test_data.size(); i += kWriteSize) {
      EXPECT_EQ(OkStatus(),
                writer.Write(test_data.data() + i,
                             std::min(kWriteSize, test_data.size() - i)));
    }
    EXPECT_EQ(writer.ConservativeWriteLimit(), 0u);
    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Ensure data is valid.
    auto& persistent = GetPersistentBuffer();
    EXPECT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), kBufferSize);
    EXPECT_EQ(
        std::memcmp(test_data.data(), persistent.data(), test_data.size()), 0);
  }
}

TEST_F(PersistentTest, VariableSizedWrites) {
  std::array<std::byte, kBufferSize> test_data;
  constexpr size_t kMaxWriteSize = 11;
  random::XorShiftStarRng64 test_data_generator(0x63CAA44A);
  test_data_generator.Get(test_data);

  static_assert(test_data.size() == kBufferSize);

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    EXPECT_FALSE(persistent.has_value());

    auto writer = persistent.GetWriter();

    size_t count = 0;
    size_t write_size = 1;
    while (count < kBufferSize) {
      const size_t remaining_space = writer.ConservativeWriteLimit();

      EXPECT_EQ(OkStatus(),
                writer.Write(test_data.data() + count,
                             std::min(write_size, remaining_space)));

      count += write_size;
      write_size = (write_size % kMaxWriteSize) + 1;
      ASSERT_NE(write_size, 12u);
      ASSERT_NE(write_size, 0u);
    }
    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Ensure data is valid.
    auto& persistent = GetPersistentBuffer();
    EXPECT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), test_data.size());
    EXPECT_EQ(
        std::memcmp(test_data.data(), persistent.data(), persistent.size()), 0);
  }
}

TEST_F(PersistentTest, ZeroDataIsNoValue) {
  ZeroPersistentMemory();
  auto& persistent = GetPersistentBuffer();
  EXPECT_FALSE(persistent.has_value());
}

TEST_F(PersistentTest, RandomDataIsInvalid) {
  RandomFillMemory();
  auto& persistent = GetPersistentBuffer();
  ASSERT_FALSE(persistent.has_value());
}

TEST_F(PersistentTest, AppendingData) {
  constexpr std::string_view kTestString("Test string one!");
  constexpr uint32_t kTestNumber = 42;

  {  // Initialize the buffer.
    RandomFillMemory();
    auto& persistent = GetPersistentBuffer();
    auto writer = persistent.GetWriter();
    EXPECT_EQ(persistent.size(), 0u);

    // Write an integer.
    ASSERT_EQ(OkStatus(), writer.Write(as_bytes(span(&kTestNumber, 1))));
    ASSERT_TRUE(persistent.has_value());

    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }

  {  // Get a pointer to the buffer and validate the contents.
    auto& persistent = GetPersistentBuffer();
    ASSERT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), sizeof(kTestNumber));

    // Write more data.
    auto writer = persistent.GetWriter();
    EXPECT_EQ(persistent.size(), sizeof(kTestNumber));
    ASSERT_EQ(OkStatus(),
              writer.Write(as_bytes(span<const char>(kTestString))));

    persistent.~PersistentBuffer();  // Emulate shutdown / global destructors.
  }
  {  // Ensure data was appended.
    auto& persistent = GetPersistentBuffer();
    ASSERT_TRUE(persistent.has_value());
    EXPECT_EQ(persistent.size(), sizeof(kTestNumber) + kTestString.length());
  }
}

}  // namespace
}  // namespace pw::persistent_ram
