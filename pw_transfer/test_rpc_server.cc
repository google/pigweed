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
// with RPC packets through a socket. The transfer service reads and writes to
// files within a given directory. The name of a file is its transfer ID.

#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_rpc_system_server/rpc_server.h"
#include "pw_rpc_system_server/socket.h"
#include "pw_stream/std_file_stream.h"
#include "pw_thread/detached_thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/transfer.h"
#include "pw_transfer_test/test_server.raw_rpc.pb.h"

namespace pw::transfer {
namespace {

class FileTransferHandler final : public ReadWriteHandler {
 public:
  FileTransferHandler(TransferService& service,
                      uint32_t transfer_id,
                      const char* path)
      : ReadWriteHandler(transfer_id), service_(service), path_(path) {
    service_.RegisterHandler(*this);
  }

  ~FileTransferHandler() { service_.UnregisterHandler(*this); }

  Status PrepareRead() final {
    PW_LOG_DEBUG("Preparing read for file %s", path_.c_str());
    set_reader(stream_.emplace<stream::StdFileReader>(path_.c_str()));
    return OkStatus();
  }

  void FinalizeRead(Status) final {
    std::get<stream::StdFileReader>(stream_).Close();
  }

  Status PrepareWrite() final {
    PW_LOG_DEBUG("Preparing write for file %s", path_.c_str());
    set_writer(stream_.emplace<stream::StdFileWriter>(path_.c_str()));
    return OkStatus();
  }

  Status FinalizeWrite(Status) final {
    std::get<stream::StdFileWriter>(stream_).Close();
    return OkStatus();
  }

 private:
  TransferService& service_;
  std::string path_;
  std::variant<std::monostate, stream::StdFileReader, stream::StdFileWriter>
      stream_;
};

class TestServerService
    : public pw_rpc::raw::TestServer::Service<TestServerService> {
 public:
  TestServerService(TransferService& transfer_service)
      : transfer_service_(transfer_service) {}

  void set_directory(const char* directory) { directory_ = directory; }

  void ReloadTransferFiles(ConstByteSpan, rpc::RawUnaryResponder&) {
    LoadFileHandlers();
  }

  void LoadFileHandlers() {
    PW_LOG_INFO("Reloading file handlers from %s", directory_.c_str());
    file_transfer_handlers_.clear();

    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      int transfer_id = std::atoi(entry.path().filename().c_str());
      if (transfer_id > 0) {
        PW_LOG_DEBUG("Found transfer file %d", transfer_id);
        file_transfer_handlers_.emplace_back(
            std::make_shared<FileTransferHandler>(
                transfer_service_, transfer_id, entry.path().c_str()));
      }
    }
  }

 private:
  TransferService& transfer_service_;
  std::string directory_;
  std::vector<std::shared_ptr<FileTransferHandler>> file_transfer_handlers_;
};

constexpr size_t kChunkSizeBytes = 256;
constexpr size_t kMaxReceiveSizeBytes = 1024;

std::array<std::byte, kChunkSizeBytes> chunk_buffer;
std::array<std::byte, kChunkSizeBytes> encode_buffer;
transfer::Thread<4, 4> transfer_thread(chunk_buffer, encode_buffer);
TransferService transfer_service(transfer_thread, kMaxReceiveSizeBytes);
TestServerService test_server_service(transfer_service);

void RunServer(int socket_port, const char* directory) {
  rpc::system_server::set_socket_port(socket_port);

  test_server_service.set_directory(directory);
  test_server_service.LoadFileHandlers();

  rpc::system_server::Init();
  rpc::system_server::Server().RegisterService(test_server_service,
                                               transfer_service);

  thread::DetachedThread(thread::stl::Options(), transfer_thread);

  PW_LOG_INFO("Starting pw_rpc server");
  PW_CHECK_OK(rpc::system_server::Start());
}

}  // namespace
}  // namespace pw::transfer

int main(int argc, char* argv[]) {
  if (argc != 3) {
    PW_LOG_ERROR("Usage: %s PORT DIR", argv[0]);
    return 1;
  }

  pw::transfer::RunServer(std::atoi(argv[1]), argv[2]);
  return 0;
}
