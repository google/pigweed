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

#include "pw_assert/assert.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer::internal {

struct Chunk;

enum TransferType : bool { kRead, kWrite };

// Stores the read/write streams and transfer parameters for communicating with
// a pw_transfer client.
class ClientConnection {
 public:
  constexpr ClientConnection(uint32_t max_pending_bytes,
                             uint32_t max_chunk_size_bytes)
      : max_parameters_(max_pending_bytes, max_chunk_size_bytes) {}

  void InitializeRead(rpc::RawServerReaderWriter& reader_writer,
                      Function<void(ConstByteSpan)>&& callback) {
    read_stream_ = std::move(reader_writer);
    read_stream_.set_on_next(std::move(callback));
  }

  void InitializeWrite(rpc::RawServerReaderWriter& reader_writer,
                       Function<void(ConstByteSpan)>&& callback) {
    write_stream_ = std::move(reader_writer);
    write_stream_.set_on_next(std::move(callback));
  }

  const TransferParameters& max_parameters() const { return max_parameters_; }

  rpc::RawServerReaderWriter& read_stream() { return read_stream_; }
  rpc::RawServerReaderWriter& write_stream() { return write_stream_; }

  rpc::RawServerReaderWriter& stream(TransferType type) {
    return type == kRead ? read_stream_ : write_stream_;
  }

  void SendStatusChunk(TransferType type, uint32_t transfer_id, Status status);

 private:
  // Persistent streams for read and write transfers. The server never closes
  // these streams -- they remain open until the client ends them.
  rpc::RawServerReaderWriter read_stream_;
  rpc::RawServerReaderWriter write_stream_;

  // Cannot exceed these parameters, even if the client requests a larger
  // pending bytes or chunk size.
  TransferParameters max_parameters_;
};

}  // namespace pw::transfer::internal
