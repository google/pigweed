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

#include "pw_system/file_manager.h"

#if PW_SYSTEM_ENABLE_TRACE_SERVICE
#include "pw_system/trace_service.h"
#endif

namespace pw::system {

namespace {
FileManager file_manager;
}

FileManager& GetFileManager() { return file_manager; }

FileManager::FileManager()
#if PW_SYSTEM_ENABLE_TRACE_SERVICE
    : trace_data_handler_(kTraceTransferHandlerId, GetTraceData()),
      trace_data_filesystem_entry_(
          "/trace/0.bin",
          kTraceTransferHandlerId,
          file::FlatFileSystemService::Entry::FilePermissions::READ,
          GetTraceData())
#endif
{
  // Every handler & filesystem element must be added to the collections, using
  // the associated handler ID as the index.
#if PW_SYSTEM_ENABLE_TRACE_SERVICE
  transfer_handlers_[kTraceTransferHandlerId] = &trace_data_handler_;
  file_system_entries_[kTraceTransferHandlerId] = &trace_data_filesystem_entry_;
#endif
}

}  // namespace pw::system
