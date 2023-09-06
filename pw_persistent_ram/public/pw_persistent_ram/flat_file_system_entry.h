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

#pragma once

#include "pw_file/flat_file_system.h"
#include "pw_persistent_ram/persistent_buffer.h"

namespace pw::persistent_ram {

template <size_t kMaxSizeBytes>
class FlatFileSystemPersistentBufferEntry final
    : public file::FlatFileSystemService::Entry {
 public:
  FlatFileSystemPersistentBufferEntry(
      std::string_view file_name,
      file::FlatFileSystemService::Entry::Id file_id,
      file::FlatFileSystemService::Entry::FilePermissions permissions,
      PersistentBuffer<kMaxSizeBytes>& persistent_buffer)
      : file_name_(file_name),
        file_id_(file_id),
        permissions_(permissions),
        persistent_buffer_(persistent_buffer) {}

  StatusWithSize Name(span<char> dest) final {
    if (file_name_.empty() || !persistent_buffer_.has_value()) {
      return StatusWithSize(Status::NotFound(), 0);
    }

    size_t bytes_to_copy = std::min(dest.size_bytes(), file_name_.size());
    std::memcpy(dest.data(), file_name_.data(), bytes_to_copy);
    if (bytes_to_copy != file_name_.size()) {
      return StatusWithSize(Status::ResourceExhausted(), bytes_to_copy);
    }

    return StatusWithSize(OkStatus(), bytes_to_copy);
  }

  size_t SizeBytes() final { return persistent_buffer_.size(); }

  Status Delete() final {
    persistent_buffer_.clear();
    return pw::OkStatus();
  }

  file::FlatFileSystemService::Entry::FilePermissions Permissions()
      const final {
    return permissions_;
  }

  file::FlatFileSystemService::Entry::Id FileId() const final {
    return file_id_;
  }

 private:
  const std::string_view file_name_;
  const file::FlatFileSystemService::Entry::Id file_id_;
  const file::FlatFileSystemService::Entry::FilePermissions permissions_;
  PersistentBuffer<kMaxSizeBytes>& persistent_buffer_;
};

}  // namespace pw::persistent_ram
