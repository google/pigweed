// Copyright 2024 The Pigweed Authors
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

#include "pw_elf/reader.h"

#include "pw_bytes/array.h"
#include "pw_bytes/endian.h"
#include "pw_bytes/suffix.h"
#include "pw_stream/memory_stream.h"
#include "pw_stream/std_file_stream.h"
#include "pw_unit_test/framework.h"

namespace pw::elf {
namespace {

constexpr Status kEndOfFileStatus = Status::OutOfRange();

Status TestInitialize(ConstByteSpan data) {
  stream::MemoryReader stream(data);
  return ElfReader::FromStream(stream).status();
}

TEST(Reader, HandlesEmptyStream) {
  constexpr auto kData = bytes::Array<>();
  EXPECT_EQ(TestInitialize(kData), kEndOfFileStatus);
}

TEST(Reader, HandlesInvalidMagic) {
  constexpr std::array<std::byte, EI_NIDENT> kData = {
      std::byte{0x7F},
      std::byte{'B'},
      std::byte{'A'},
      std::byte{'D'},
  };
  EXPECT_EQ(TestInitialize(kData), Status::DataLoss());
}

TEST(Reader, HandlesTruncatedAfterMagic) {
  constexpr auto kData = bytes::Array<0x7F, 'E', 'L', 'F'>();
  EXPECT_EQ(TestInitialize(kData), kEndOfFileStatus);
}

TEST(Reader, HandlesInvalidClass) {
  constexpr std::array<std::byte, EI_NIDENT> kData = {
      std::byte{0x7F},
      std::byte{'E'},
      std::byte{'L'},
      std::byte{'F'},
      std::byte{0x66},  // EI_CLASS
      std::byte{pw::endian::native == pw::endian::little
                    ? ELFDATA2LSB
                    : ELFDATA2MSB},  // EI_DATA
  };
  EXPECT_EQ(TestInitialize(kData), Status::DataLoss());
}

TEST(Reader, HandlesUnsupportedEndian) {
  constexpr std::array<std::byte, EI_NIDENT> kData = {
      std::byte{0x7F},
      std::byte{'E'},
      std::byte{'L'},
      std::byte{'F'},
      std::byte{0x66},  // EI_CLASS
      std::byte{pw::endian::native == pw::endian::little
                    ? ELFDATA2MSB
                    : ELFDATA2LSB},  // EI_DATA (opposite)
  };
  EXPECT_EQ(TestInitialize(kData), Status::Unimplemented());
}

TEST(Reader, SeekToSectionWorksOnRealFile) {
  pw::stream::StdFileReader stream(TEST_ELF_FILE_PATH);
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto reader,
                               pw::elf::ElfReader::FromStream(stream));

  auto section_size = reader.SeekToSection(".test_section_1");
  PW_TEST_ASSERT_OK(section_size);

  std::vector<std::byte> section_data;
  section_data.resize(section_size.size());
  PW_TEST_ASSERT_OK(stream.ReadExact(section_data));

  constexpr auto kExpectedData = bytes::String("You cannot pass\0");
  EXPECT_EQ(section_data.size(), kExpectedData.size());
  EXPECT_TRUE(std::equal(
      section_data.begin(), section_data.end(), kExpectedData.begin()));
}

TEST(Reader, ReadSectionWorksOnRealFile) {
  pw::stream::StdFileReader stream(TEST_ELF_FILE_PATH);
  PW_TEST_ASSERT_OK_AND_ASSIGN(auto reader,
                               pw::elf::ElfReader::FromStream(stream));

  PW_TEST_ASSERT_OK_AND_ASSIGN(auto section_data,
                               reader.ReadSection(".test_section_2"));

  constexpr auto kExpectedData = bytes::Array<0xEF, 0xBE, 0xED, 0xFE>();
  EXPECT_EQ(section_data.size(), kExpectedData.size());
  EXPECT_TRUE(std::equal(
      section_data.begin(), section_data.end(), kExpectedData.begin()));
}

}  // namespace
}  // namespace pw::elf
