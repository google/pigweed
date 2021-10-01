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

#include "pw_rpc/raw/server_reader_writer.h"

namespace pw::transfer::internal {

struct Chunk;

enum TransferType : bool { kRead, kWrite };

// Stores the read/write streams and transfer parameters for communicating with
// a pw_transfer client.
class ClientConnection {
 public:
  constexpr ClientConnection(size_t max_chunk_size_bytes,
                             size_t default_max_bytes_to_receive)
      : max_chunk_size_bytes_(max_chunk_size_bytes),
        default_max_bytes_to_receive_(default_max_bytes_to_receive) {}

  void InitializeRead(rpc::RawServerReaderWriter& reader_writer,
                      Function<void(ConstByteSpan)> callback) {
    read_stream_ = std::move(reader_writer);
    read_stream_.set_on_next(std::move(callback));
  }

  void InitializeWrite(rpc::RawServerReaderWriter& reader_writer,
                       Function<void(ConstByteSpan)> callback) {
    write_stream_ = std::move(reader_writer);
    write_stream_.set_on_next(std::move(callback));
  }

  size_t max_chunk_size_bytes() const { return max_chunk_size_bytes_; }

  size_t default_max_bytes_to_receive() const {
    return default_max_bytes_to_receive_;
  }

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

  size_t max_chunk_size_bytes_;
  size_t default_max_bytes_to_receive_;
};

}  // namespace pw::transfer::internal
