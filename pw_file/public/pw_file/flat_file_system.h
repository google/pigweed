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
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "pw_bytes/span.h"
#include "pw_file/file.pwpb.h"
#include "pw_file/file.raw_rpc.pb.h"
#include "pw_result/result.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::file {

// This implements the pw.file.FileSystem RPC service. This implementation
// has a strict limitation that everything is treated as if the file system
// was "flat" (i.e. no directories). This means there's no concept of logical
// directories, despite any "path like" naming that may be employed by a user.
class FlatFileSystemService
    : public pw_rpc::raw::FileSystem::Service<FlatFileSystemService> {
 public:
  class Entry {
   public:
    using FilePermissions = pw::file::Path::Permissions;
    using Id = uint32_t;

    Entry() = default;
    virtual ~Entry() = default;

    // All readable files MUST be named, and names must be globally unique to
    // prevent ambiguity. Unnamed file entries will NOT be enumerated by a
    // FlatFileSystemService. The returned status must indicate the length
    // of the string written to `dest`, and should NOT include any null
    // terminator that may have been written.
    //
    // Note: The bounded string written to `dest` is not expected to be
    // null-terminated, and should be treated like a std::string_view.
    //
    // Returns:
    //   OK - Successfully read file name to `dest`.
    //   NOT_FOUND - No file to enumerate for this entry.
    //   RESOURCE_EXHAUSTED - `dest` buffer too small to fit the full file name.
    virtual StatusWithSize Name(std::span<char> dest) = 0;

    virtual size_t SizeBytes() = 0;
    virtual FilePermissions Permissions() const = 0;

    // Deleting a file, if allowed, should cause the backing data store to be
    // cleared. Read-only files should also no longer enumerate (i.e. Name()
    // should return NOT_FOUND). Write-only and read/write files may still
    // enumerate but with SizeBytes() reporting zero.
    virtual Status Delete() = 0;

    // File IDs must be globally unique, and map to a pw_transfer
    // TransferService read/write handler.
    virtual Id FileId() const = 0;
  };

  // Constructs a flat file system from a static list of file entries.
  //
  // Args:
  //   entry_list - A list of pointers to all Entry objects that may
  //     contain files. These pointers may not be null. The span's underlying
  //     buffer must outlive this object.
  //   file_name_buffer - Used internally by this class to find and enumerate
  //     files. Should be large enough to hold the longest expected file name.
  //     The span's underlying buffer must outlive this object.
  FlatFileSystemService(std::span<Entry*> entry_list,
                        std::span<char> file_name_buffer)
      : file_name_buffer_(file_name_buffer), entries_(entry_list) {}

  // Method definitions for pw.file.FileSystem.
  void List(ConstByteSpan request, RawServerWriter& writer);

  // Returns:
  //   OK - File successfully deleted.
  //   NOT_FOUND - Could not find
  StatusWithSize Delete(ConstByteSpan request, ByteSpan);

 private:
  Result<Entry*> FindFile(std::string_view file_name);
  Status FindAndDeleteFile(std::string_view file_name);

  Status EnumerateFile(Entry& entry,
                       pw::file::ListResponse::StreamEncoder& output_encoder);
  void EnumerateAllFiles(RawServerWriter& writer);

  std::span<char> file_name_buffer_;
  std::span<Entry*> entries_;
};

}  // namespace pw::file
