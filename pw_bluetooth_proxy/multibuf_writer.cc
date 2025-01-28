// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_proxy/internal/multibuf_writer.h"

namespace pw::bluetooth::proxy {

pw::Result<MultiBufWriter> MultiBufWriter::Create(
    multibuf::MultiBufAllocator& multibuf_allocator, size_t size) {
  std::optional<multibuf::MultiBuf> buf =
      multibuf_allocator.AllocateContiguous(size);
  if (!buf) {
    return Status::ResourceExhausted();
  }
  return MultiBufWriter(std::move(*buf));
}

Status MultiBufWriter::Write(pw::span<const uint8_t> data) {
  ConstByteSpan byte_data(reinterpret_cast<const std::byte*>(data.data()),
                          data.size());
  StatusWithSize status = buf_.CopyFrom(byte_data, write_offset_);
  if (!status.ok()) {
    return status.status();
  }
  write_offset_ += byte_data.size();
  return OkStatus();
}

}  // namespace pw::bluetooth::proxy
