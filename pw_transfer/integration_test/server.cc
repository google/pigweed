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

// Simple RPC server with the transfer service registered. Reads HDLC frames
// with RPC packets through a socket. This server has a single transfer ID that
// is available, and data must be written to the server before data can be read
// from the transfer ID.

#include <cstddef>
#include <string>
#include <thread>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_rpc_system_server/rpc_server.h"
#include "pw_rpc_system_server/socket.h"
#include "pw_stream/memory_stream.h"
#include "pw_thread/detached_thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/transfer.h"

namespace pw::transfer {
namespace {

using stream::MemoryReader;
using stream::MemoryWriter;

// TODO(amontanez): These should be configurable.
constexpr size_t kChunkSizeBytes = 256;
constexpr size_t kMaxReceiveSizeBytes = 1024;

std::array<std::byte, kChunkSizeBytes> chunk_buffer;
std::array<std::byte, kChunkSizeBytes> encode_buffer;
transfer::Thread<4, 4> transfer_thread(chunk_buffer, encode_buffer);
TransferService transfer_service(transfer_thread, kMaxReceiveSizeBytes);

class DynamicallyAllocatedRamHandler final : public ReadWriteHandler {
 public:
  DynamicallyAllocatedRamHandler(TransferService& service,
                                 uint32_t transfer_id,
                                 size_t max_size)
      : ReadWriteHandler(transfer_id),
        max_size_(max_size),
        size_(0),
        writer_open_(false),
        readers_open_(0),
        service_(service),
        buffer_(nullptr),
        memory_reader_(ConstByteSpan()),
        memory_writer_(ByteSpan()) {
    buffer_ = new std::byte[max_size];
    service_.RegisterHandler(*this);
  }

  ~DynamicallyAllocatedRamHandler() {
    service_.UnregisterHandler(*this);
    delete[] buffer_;
  }

  Status PrepareRead() final {
    if (writer_open_) {
      PW_LOG_ERROR("Failed to open for reading; writer still open");
      return Status::Unavailable();
    }
    if (readers_open_ == 0) {
      PW_LOG_DEBUG("Creating new MemoryReader");
      memory_reader_ = MemoryReader(ByteSpan(buffer_, size_));
      set_reader(memory_reader_);
    }
    readers_open_++;
    PW_LOG_DEBUG("%d readers now open", static_cast<int>(readers_open_));
    return OkStatus();
  }

  void FinalizeRead(Status) final {
    PW_CHECK_UINT_GT(readers_open_, 0);
    readers_open_--;
    PW_LOG_DEBUG("%d readers now open", static_cast<int>(readers_open_));
  }

  Status PrepareWrite() final {
    if (writer_open_) {
      PW_LOG_ERROR("Failed to open for writing; writer still open");
      return Status::Unavailable();
    }
    if (readers_open_ > 0) {
      PW_LOG_ERROR("Failed to open for writing; %d readers still open",
                   static_cast<int>(readers_open_));
      return Status::Unavailable();
    }

    memory_writer_ = MemoryWriter(ByteSpan(buffer_, max_size_));
    set_writer(memory_writer_);
    writer_open_ = true;
    return OkStatus();
  }

  Status FinalizeWrite(Status) final {
    PW_CHECK(writer_open_);
    size_ = memory_writer_.size();
    writer_open_ = false;
    return OkStatus();
  }

 private:
  const size_t max_size_;
  size_t size_;
  bool writer_open_;
  size_t readers_open_;
  TransferService& service_;
  std::byte* buffer_;
  MemoryReader memory_reader_;
  MemoryWriter memory_writer_;
};

void RunServer(int socket_port, uint32_t transfer_id, size_t max_file_size) {
  rpc::system_server::set_socket_port(socket_port);

  rpc::system_server::Init();
  rpc::system_server::Server().RegisterService(transfer_service);

  thread::DetachedThread(thread::stl::Options(), transfer_thread);

  // It's fine to allocate this on the stack since this thread doesn't return
  // until this process is killed.
  DynamicallyAllocatedRamHandler transfer_handler(
      transfer_service, transfer_id, max_file_size);

  PW_LOG_INFO("Starting pw_rpc server");
  PW_CHECK_OK(rpc::system_server::Start());
}

}  // namespace
}  // namespace pw::transfer

int main(int argc, char* argv[]) {
  if (argc != 4) {
    PW_LOG_ERROR("Usage: %s PORT TRANSFER_ID MAX_FILE_SIZE", argv[0]);
    return 1;
  }

  int port = std::atoi(argv[1]);
  PW_CHECK_UINT_GT(port, 0, "Invalid port!");

  int transfer_id = std::atoi(argv[2]);
  PW_CHECK_UINT_GT(transfer_id, 0, "Invalid transfer ID!");

  long long max_file_size = std::atoll(argv[3]);
  PW_CHECK_UINT_GT(max_file_size, 0, "Invalid maximum file size!");

  pw::transfer::RunServer(port, transfer_id, max_file_size);
  return 0;
}
