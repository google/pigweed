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

// Simple RPC server with the transfer service registered. Reads HDLC frames
// with RPC packets through a socket. The transfer service reads and writes to a
// fixed-size buffer. The buffer's contents can be set by writing
// null-terminated strings through stdin.

#include <cstddef>
#include <thread>
#include <variant>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_rpc_system_server/rpc_server.h"
#include "pw_rpc_system_server/socket.h"
#include "pw_stream/memory_stream.h"
#include "pw_transfer/transfer.h"

namespace pw::transfer {
namespace {

class BufferReaderWriter final : public ReadWriteHandler {
 public:
  BufferReaderWriter(uint32_t transfer_id, ByteSpan buffer)
      : ReadWriteHandler(transfer_id), buffer_(buffer), size_bytes_(0) {}

  Status PrepareRead() override {
    set_reader(
        stream_.emplace<stream::MemoryReader>(buffer_.first(size_bytes_)));
    return OkStatus();
  }

  Status PrepareWrite() override {
    set_writer(stream_.emplace<stream::MemoryWriter>(buffer_));
    return OkStatus();
  }

  Status FinalizeWrite(Status) override {
    size_bytes_ = std::get<stream::MemoryWriter>(stream_).bytes_written();
    return OkStatus();
  }

  void set_size(size_t new_size) { size_bytes_ = new_size; }

 private:
  // TODO(hepler): Use a seekable MemoryReaderWriter when available.
  std::variant<std::monostate, stream::MemoryReader, stream::MemoryWriter>
      stream_;
  ByteSpan buffer_;
  size_t size_bytes_;
};

constexpr size_t kChunkSizeBytes = 256;
constexpr size_t kMaxReceiveSizeBytes = 1024;

TransferService transfer_service(kChunkSizeBytes, kMaxReceiveSizeBytes);

std::byte buffer[512];

void RunServer() {
  rpc::system_server::set_socket_port(33001);

  BufferReaderWriter transfer(99, buffer);

  // Read characters from stdin directly into the buffer. Stop at 0 or overflow.
  std::thread([&transfer] {
    int next_char;
    size_t index = 0;

    while ((next_char = std::getchar()) != EOF) {
      if (next_char == '\0') {
        transfer.set_size(index);
        index = 0;
        continue;
      }

      PW_CHECK_UINT_LT(index, sizeof(buffer), "Sent too many characters!");
      buffer[index++] = static_cast<std::byte>(next_char);
    }
  }).detach();

  transfer_service.RegisterHandler(transfer);

  rpc::system_server::Init();
  rpc::system_server::Server().RegisterService(transfer_service);

  PW_LOG_INFO("Starting pw_rpc server");
  rpc::system_server::Start()
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
}

}  // namespace
}  // namespace pw::transfer

int main() {
  pw::transfer::RunServer();
  return 0;
}
