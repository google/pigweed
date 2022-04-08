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

#include "pw_stream/std_file_stream.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string_view>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_string/util.h"

namespace pw::stream {
namespace {

constexpr std::string_view kSmallTestData(
    "This is a test string used to verify correctness!");

class StdFileStreamTest : public ::testing::Test {
 protected:
  StdFileStreamTest() = default;

  void SetUp() override {
    ASSERT_EQ(std::tmpnam(temp_filename_buffer_.data()),
              temp_filename_buffer_.data());
    // Verify the string was null terminated in bounds.
    ASSERT_EQ(string::NullTerminatedLength(temp_filename_buffer_).status(),
              OkStatus());
    temp_filename_ = temp_filename_buffer_.data();
  }
  void TearDown() override {
    // Only clean up the file if a name was successfully generated.
    if (temp_filename() != nullptr) {
      EXPECT_EQ(std::remove(temp_filename()), 0);
    }
  }

  const char* temp_filename() { return temp_filename_; }

 private:
  std::array<char, L_tmpnam> temp_filename_buffer_;
  const char* temp_filename_;
};

TEST_F(StdFileStreamTest, SeekAtEnd) {
  // Write some data to the temporary file.
  const std::string_view kTestData = kSmallTestData;
  StdFileWriter writer(temp_filename());
  ASSERT_EQ(writer.Write(std::as_bytes(std::span(kTestData))), OkStatus());
  writer.Close();

  StdFileReader reader(temp_filename());
  std::array<char, 3> read_buffer;
  size_t read_offset = 0;
  while (read_offset < kTestData.size()) {
    Result<ConstByteSpan> result =
        reader.Read(std::as_writable_bytes(std::span(read_buffer)));
    ASSERT_EQ(result.status(), OkStatus());
    ASSERT_GT(result.value().size(), 0u);
    ASSERT_LE(result.value().size(), read_buffer.size());
    ASSERT_LE(result.value().size(), kTestData.size() - read_offset);
    ConstByteSpan expect_window =
        std::as_bytes(std::span(kTestData))
            .subspan(read_offset, result.value().size());
    EXPECT_TRUE(std::equal(result.value().begin(),
                           result.value().end(),
                           expect_window.begin(),
                           expect_window.end()));
    read_offset += result.value().size();
  }
  // After data has been read, do a final read to trigger EOF.
  Result<ConstByteSpan> result =
      reader.Read(std::as_writable_bytes(std::span(read_buffer)));
  EXPECT_EQ(result.status(), Status::OutOfRange());

  EXPECT_EQ(read_offset, kTestData.size());

  // Seek backwards and read again to ensure seek at EOF works.
  ASSERT_EQ(reader.Seek(-1 * read_buffer.size(), Stream::Whence::kEnd),
            OkStatus());
  result = reader.Read(std::as_writable_bytes(std::span(read_buffer)));
  EXPECT_EQ(result.status(), OkStatus());

  reader.Close();
}

}  // namespace
}  // namespace pw::stream
