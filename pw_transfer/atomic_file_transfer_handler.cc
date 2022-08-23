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
#include "pw_transfer/atomic_file_transfer_handler.h"

#include <filesystem>
#include <system_error>

#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_stream/std_file_stream.h"
#include "pw_transfer_private/filename_generator.h"

namespace pw::transfer {

Status AtomicFileTransferHandler::PrepareRead() {
  auto file_path = path_.c_str();
  PW_LOG_DEBUG("Preparing read for file %s", file_path);
  if (!std::filesystem::exists(file_path)) {
    PW_LOG_ERROR("File does not exist, path: %s", file_path);
    return Status::NotFound();
  }
  set_reader(stream_.emplace<stream::StdFileReader>(file_path));
  return OkStatus();
}

void AtomicFileTransferHandler::FinalizeRead(Status) {
  std::get<stream::StdFileReader>(stream_).Close();
}

Status AtomicFileTransferHandler::PrepareWrite() {
  const std::string tmp_file = GetTempFilePath(path_);
  PW_LOG_DEBUG("Preparing write for file %s", tmp_file.c_str());
  set_writer(stream_.emplace<stream::StdFileWriter>(tmp_file.c_str()));
  return OkStatus();
}

Status AtomicFileTransferHandler::FinalizeWrite(Status status) {
  std::get<stream::StdFileWriter>(stream_).Close();
  const auto tmp_file = GetTempFilePath(path_);
  auto temp_file_path = tmp_file.c_str();
  auto file_path = path_.c_str();
  if (!status.ok() || !std::filesystem::exists(temp_file_path) ||
      std::filesystem::is_empty(temp_file_path)) {
    PW_LOG_ERROR("Status not ok, remove file %s", temp_file_path);
    // Remove temp file if transfer fails.
    return std::filesystem::remove(tmp_file) ? status : Status::Aborted();
  }
  PW_LOG_DEBUG("Renaming file from: %s, to: %s", temp_file_path, file_path);
  std::error_code err;
  std::filesystem::rename(temp_file_path, file_path, err);
  if (err) {
    PW_LOG_ERROR("Error during renaming of file %s", temp_file_path);
    return std::filesystem::remove(tmp_file) ? Status::Internal()
                                             : Status::Aborted();
  }
  PW_LOG_INFO("File transfer was successful.");
  return OkStatus();
}

}  // namespace pw::transfer
