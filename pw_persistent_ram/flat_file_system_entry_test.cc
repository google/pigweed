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

#include "pw_persistent_ram/flat_file_system_entry.h"

#include "pw_unit_test/framework.h"

namespace pw::persistent_ram {
namespace {

class FlatFileSystemPersistentBufferEntryTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kBufferSize = 16;
  static constexpr size_t kMaxFileNameLength = 32;

  FlatFileSystemPersistentBufferEntryTest() {}

  // Emulate invalidation of persistent section(s).
  void ZeroPersistentMemory() { memset(buffer_, 0, sizeof(buffer_)); }

  PersistentBuffer<kBufferSize>& GetPersistentBuffer() {
    return *(new (buffer_) PersistentBuffer<kBufferSize>());
  }

  // Allocate a chunk of aligned storage that can be independently controlled.
  alignas(PersistentBuffer<kBufferSize>)
      std::byte buffer_[sizeof(PersistentBuffer<kBufferSize>)];
};

TEST_F(FlatFileSystemPersistentBufferEntryTest, BasicProperties) {
  constexpr std::string_view kExpectedFileName("file_1.bin");
  constexpr file::FlatFileSystemService::Entry::Id kExpectedFileId = 7;
  constexpr file::FlatFileSystemService::Entry::FilePermissions
      kExpectedPermissions =
          file::FlatFileSystemService::Entry::FilePermissions::READ;

  ZeroPersistentMemory();
  auto& persistent = GetPersistentBuffer();

  // write some data to create the file
  constexpr uint32_t kExpectedNumber = 0x6C2C6582;
  auto writer = persistent.GetWriter();
  ASSERT_EQ(OkStatus(), writer.Write(as_bytes(span(&kExpectedNumber, 1))));

  FlatFileSystemPersistentBufferEntry persistent_file(
      kExpectedFileName, kExpectedFileId, kExpectedPermissions, persistent);

  std::array<char, kMaxFileNameLength> tmp_buffer = {};
  static_assert(kExpectedFileName.size() <= tmp_buffer.size());
  StatusWithSize sws = persistent_file.Name(tmp_buffer);
  ASSERT_EQ(OkStatus(), sws.status());

  EXPECT_EQ(
      0, std::memcmp(tmp_buffer.data(), kExpectedFileName.data(), sws.size()));
  EXPECT_EQ(sizeof(kExpectedNumber), persistent_file.SizeBytes());
  EXPECT_EQ(kExpectedPermissions, persistent_file.Permissions());
  EXPECT_EQ(kExpectedFileId, persistent_file.FileId());
}

TEST_F(FlatFileSystemPersistentBufferEntryTest, Delete) {
  constexpr std::string_view kExpectedFileName("file_2.bin");
  constexpr file::FlatFileSystemService::Entry::Id kExpectedFileId = 8;
  constexpr file::FlatFileSystemService::Entry::FilePermissions
      kExpectedPermissions =
          file::FlatFileSystemService::Entry::FilePermissions::WRITE;

  ZeroPersistentMemory();
  auto& persistent = GetPersistentBuffer();

  // write some data to create the file
  constexpr uint32_t kExpectedNumber = 0x6C2C6582;
  auto writer = persistent.GetWriter();
  ASSERT_EQ(OkStatus(), writer.Write(as_bytes(span(&kExpectedNumber, 1))));

  FlatFileSystemPersistentBufferEntry persistent_file(
      kExpectedFileName, kExpectedFileId, kExpectedPermissions, persistent);

  std::array<char, kMaxFileNameLength> tmp_buffer = {};
  static_assert(kExpectedFileName.size() <= tmp_buffer.size());
  StatusWithSize sws = persistent_file.Name(tmp_buffer);
  ASSERT_EQ(OkStatus(), sws.status());

  EXPECT_EQ(
      0, std::memcmp(tmp_buffer.data(), kExpectedFileName.data(), sws.size()));
  EXPECT_EQ(sizeof(kExpectedNumber), persistent_file.SizeBytes());

  ASSERT_EQ(OkStatus(), persistent_file.Delete());

  sws = persistent_file.Name(tmp_buffer);
  ASSERT_EQ(Status::NotFound(), sws.status());
  EXPECT_EQ(0u, persistent_file.SizeBytes());
}

TEST_F(FlatFileSystemPersistentBufferEntryTest, NoData) {
  constexpr std::string_view kExpectedFileName("file_2.bin");
  constexpr file::FlatFileSystemService::Entry::Id kExpectedFileId = 9;
  constexpr file::FlatFileSystemService::Entry::FilePermissions
      kExpectedPermissions =
          file::FlatFileSystemService::Entry::FilePermissions::READ_AND_WRITE;

  ZeroPersistentMemory();
  auto& persistent = GetPersistentBuffer();

  FlatFileSystemPersistentBufferEntry persistent_file(
      kExpectedFileName, kExpectedFileId, kExpectedPermissions, persistent);

  std::array<char, kMaxFileNameLength> tmp_buffer = {};
  static_assert(kExpectedFileName.size() <= tmp_buffer.size());

  StatusWithSize sws = persistent_file.Name(tmp_buffer);
  ASSERT_EQ(Status::NotFound(), sws.status());
  EXPECT_EQ(0u, persistent_file.SizeBytes());
}

}  // namespace
}  // namespace pw::persistent_ram
