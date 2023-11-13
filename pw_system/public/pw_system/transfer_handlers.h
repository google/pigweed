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

#include "pw_persistent_ram/persistent_buffer.h"
#include "pw_trace_tokenized/config.h"
#include "pw_transfer/transfer.h"

namespace pw::system {

using TracePersistentBuffer =
    persistent_ram::PersistentBuffer<PW_TRACE_BUFFER_SIZE_BYTES>;

class TracePersistentBufferTransfer : public transfer::ReadOnlyHandler {
 public:
  TracePersistentBufferTransfer(uint32_t id,
                                TracePersistentBuffer& persistent_buffer);

  Status PrepareRead() final;

 private:
  TracePersistentBuffer& persistent_buffer_;
  stream::MemoryReader reader_;
};

}  // namespace pw::system
