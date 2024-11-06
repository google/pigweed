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

#include "pw_stream/stream.h"

#include <limits>

#include "pw_assert/check.h"
#include "pw_bytes/array.h"
#include "pw_bytes/span.h"
#include "pw_containers/to_array.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::stream {
namespace {

static_assert(sizeof(Stream) <= 2 * sizeof(void*),
              "Stream should be no larger than two pointers (vtable pointer & "
              "packed members)");

class TestNonSeekableReader : public NonSeekableReader {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
};

class TestRelativeSeekableReader : public RelativeSeekableReader {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
  Status DoSeek(ptrdiff_t, Whence) override { return Status(); }
};

class TestSeekableReader : public SeekableReader {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
  Status DoSeek(ptrdiff_t, Whence) override { return Status(); }
};

class TestNonSeekableWriter : public NonSeekableWriter {
 private:
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
};

class TestRelativeSeekableWriter : public RelativeSeekableWriter {
 private:
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
  Status DoSeek(ptrdiff_t, Whence) override { return OkStatus(); }
};

class TestSeekableWriter : public SeekableWriter {
 private:
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
  Status DoSeek(ptrdiff_t, Whence) override { return OkStatus(); }
};

class TestNonSeekableReaderWriter : public NonSeekableReaderWriter {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
};

class TestRelativeSeekableReaderWriter : public RelativeSeekableReaderWriter {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
  Status DoSeek(ptrdiff_t, Whence) override { return OkStatus(); }
};

class TestSeekableReaderWriter : public SeekableReaderWriter {
 private:
  StatusWithSize DoRead(ByteSpan) override { return StatusWithSize(0); }
  Status DoWrite(ConstByteSpan) override { return OkStatus(); }
  Status DoSeek(ptrdiff_t, Whence) override { return OkStatus(); }
};

// Test ReaderWriter conversions to Reader/Writer.
// clang-format off
static_assert(std::is_convertible<TestNonSeekableReaderWriter, Reader&>());
static_assert(std::is_convertible<TestNonSeekableReaderWriter, Writer&>());
static_assert(!std::is_convertible<TestNonSeekableReaderWriter, RelativeSeekableReader&>());
static_assert(!std::is_convertible<TestNonSeekableReaderWriter, RelativeSeekableWriter&>());
static_assert(!std::is_convertible<TestNonSeekableReaderWriter, SeekableWriter&>());
static_assert(!std::is_convertible<TestNonSeekableReaderWriter, SeekableReader&>());

static_assert(std::is_convertible<TestRelativeSeekableReaderWriter, Reader&>());
static_assert(std::is_convertible<TestRelativeSeekableReaderWriter, Writer&>());
static_assert(std::is_convertible<TestRelativeSeekableReaderWriter, RelativeSeekableReader&>());
static_assert(std::is_convertible<TestRelativeSeekableReaderWriter, RelativeSeekableWriter&>());
static_assert(!std::is_convertible<TestRelativeSeekableReaderWriter, SeekableWriter&>());
static_assert(!std::is_convertible<TestRelativeSeekableReaderWriter, SeekableReader&>());

static_assert(std::is_convertible<TestSeekableReaderWriter, Reader&>());
static_assert(std::is_convertible<TestSeekableReaderWriter, Writer&>());
static_assert(std::is_convertible<TestSeekableReaderWriter, RelativeSeekableReader&>());
static_assert(std::is_convertible<TestSeekableReaderWriter, RelativeSeekableWriter&>());
static_assert(std::is_convertible<TestSeekableReaderWriter, SeekableWriter&>());
static_assert(std::is_convertible<TestSeekableReaderWriter, SeekableReader&>());
// clang-format on

constexpr uint8_t kSeekable =
    Stream::kBeginning | Stream::kCurrent | Stream::kEnd;
constexpr uint8_t kRelativeSeekable = Stream::kCurrent;
constexpr uint8_t kNonSeekable = 0;

enum Readable : bool { kNonReadable = false, kReadable = true };
enum Writable : bool { kNonWritable = false, kWritable = true };

template <typename T, Readable readable, Writable writable, uint8_t seekable>
void TestStreamImpl() {
  T derived_stream;
  Stream& stream = derived_stream;

  // Check stream properties
  ASSERT_EQ(writable, stream.writable());
  ASSERT_EQ(readable, stream.readable());

  ASSERT_EQ((seekable & Stream::kBeginning) != 0,
            stream.seekable(Stream::kBeginning));
  ASSERT_EQ((seekable & Stream::kCurrent) != 0,
            stream.seekable(Stream::kCurrent));
  ASSERT_EQ((seekable & Stream::kEnd) != 0, stream.seekable(Stream::kEnd));

  ASSERT_EQ(seekable != kNonSeekable, stream.seekable());

  // Check Read()/Write()/Seek()
  ASSERT_EQ(readable ? OkStatus() : Status::Unimplemented(),
            stream.Read({}).status());
  ASSERT_EQ(writable ? OkStatus() : Status::Unimplemented(), stream.Write({}));
  ASSERT_EQ(seekable ? OkStatus() : Status::Unimplemented(), stream.Seek(0));

  // Check ConservativeLimits()
  ASSERT_EQ(readable ? Stream::kUnlimited : 0, stream.ConservativeReadLimit());
  ASSERT_EQ(writable ? Stream::kUnlimited : 0, stream.ConservativeWriteLimit());
}

TEST(Stream, NonSeekableReader) {
  TestStreamImpl<TestNonSeekableReader,
                 kReadable,
                 kNonWritable,
                 kNonSeekable>();
}

TEST(Stream, RelativeSeekableReader) {
  TestStreamImpl<TestRelativeSeekableReader,
                 kReadable,
                 kNonWritable,
                 kRelativeSeekable>();
}

TEST(Stream, SeekableReader) {
  TestStreamImpl<TestSeekableReader, kReadable, kNonWritable, kSeekable>();
}

TEST(Stream, NonSeekableWriter) {
  TestStreamImpl<TestNonSeekableWriter,
                 kNonReadable,
                 kWritable,
                 kNonSeekable>();
}

TEST(Stream, RelativeSeekableWriter) {
  TestStreamImpl<TestRelativeSeekableWriter,
                 kNonReadable,
                 kWritable,
                 kRelativeSeekable>();
}

TEST(Stream, SeekableWriter) {
  TestStreamImpl<TestSeekableWriter, kNonReadable, kWritable, kSeekable>();
}

TEST(Stream, NonSeekableReaderWriter) {
  TestStreamImpl<TestNonSeekableReaderWriter,
                 kReadable,
                 kWritable,
                 kNonSeekable>();
}

TEST(Stream, RelativeSeekableReaderWriter) {
  TestStreamImpl<TestRelativeSeekableReaderWriter,
                 kReadable,
                 kWritable,
                 kRelativeSeekable>();
}

TEST(Stream, SeekableReaderWriter) {
  TestStreamImpl<TestSeekableReaderWriter, kReadable, kWritable, kSeekable>();
}

class TestFragmentedReader : public NonSeekableReader {
 public:
  TestFragmentedReader(ConstByteSpan data, span<StatusWithSize> frags)
      : data_(data), frags_(frags), current_frag_(frags.begin()) {
    size_t frags_sum = 0;
    for (const auto& frag : frags) {
      frags_sum += frag.size();
    }
    PW_CHECK_UINT_LE(frags_sum, data.size());
  }

 private:
  StatusWithSize DoRead(ByteSpan dest) override {
    // Each fragment is consumed entirely on each read.
    PW_CHECK_UINT_GE(dest.size(), current_frag_->size());
    PW_CHECK(current_frag_ != frags_.end());

    auto frag = current_frag_++;
    auto source = data_.subspan(data_offset_, frag->size());
    data_offset_ += frag->size();

    std::copy(source.begin(), source.end(), dest.begin());
    return *frag;
  }

  ConstByteSpan data_;
  span<StatusWithSize> frags_;
  decltype(frags_)::iterator current_frag_;
  size_t data_offset_ = 0;
};

TEST(Stream, ReadExact_Works) {
  constexpr auto kData = bytes::
      Array<0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A>();

  auto frags = containers::to_array<StatusWithSize>({
      StatusWithSize(3),  // 0x00, 0x01, 0x02
      StatusWithSize(5),  // 0x03, 0x04, 0x05, 0x06, 0x07
      StatusWithSize(1),  // 0x08
      StatusWithSize(2),  // 0x09, 0x0A
  });

  TestFragmentedReader reader(kData, frags);

  std::array<std::byte, kData.size()> dest;
  auto result = reader.ReadExact(dest);

  PW_TEST_ASSERT_OK(result);
  EXPECT_EQ(result->data(), dest.data());
  EXPECT_EQ(result->size(), dest.size());
  EXPECT_TRUE(std::equal(result->begin(), result->end(), kData.begin()));
}

TEST(Stream, ReadExact_HandlesError) {
  constexpr auto kData = bytes::
      Array<0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A>();

  auto frags = containers::to_array<StatusWithSize>({
      StatusWithSize(3),            // 0x00, 0x01, 0x02
      StatusWithSize(5),            // 0x03, 0x04, 0x05, 0x06, 0x07
      StatusWithSize::Internal(1),  // 0x08
      StatusWithSize(2),            // 0x09, 0x0A
  });

  TestFragmentedReader reader(kData, frags);

  std::array<std::byte, kData.size()> dest;
  auto result = reader.ReadExact(dest);

  EXPECT_EQ(result.status(), Status::Internal());
}

}  // namespace
}  // namespace pw::stream
