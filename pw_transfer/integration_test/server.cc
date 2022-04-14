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
// with RPC packets through a socket. This server has a single resource ID that
// is available, and data must be written to the server before data can be read
// from the resource ID.
//
// Usage:
//
//   integration_test_server 3300 <<< "resource_id: 12 file: '/tmp/gotbytes'"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "google/protobuf/text_format.h"
#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_rpc_system_server/rpc_server.h"
#include "pw_rpc_system_server/socket.h"
#include "pw_stream/std_file_stream.h"
#include "pw_thread/detached_thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/integration_test/config.pb.h"
#include "pw_transfer/transfer.h"

namespace pw::transfer {
namespace {

using stream::MemoryReader;
using stream::MemoryWriter;

// TODO(tpudlik): This is copy-pasted from test_rpc_server.cc, break it out into
// a shared library.
class FileTransferHandler final : public ReadWriteHandler {
 public:
  FileTransferHandler(TransferService& service,
                      uint32_t resource_id,
                      const char* path)
      : ReadWriteHandler(resource_id), service_(service), path_(path) {
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

void RunServer(int socket_port, ServerConfig config) {
  std::vector<std::byte> chunk_buffer(config.chunk_size_bytes());
  std::vector<std::byte> encode_buffer(config.chunk_size_bytes());
  transfer::Thread<4, 4> transfer_thread(chunk_buffer, encode_buffer);
  TransferService transfer_service(
      transfer_thread,
      config.pending_bytes(),
      std::chrono::seconds(config.chunk_timeout_seconds()),
      config.transfer_service_retries(),
      config.extend_window_divisor());

  rpc::system_server::set_socket_port(socket_port);

  rpc::system_server::Init();
  rpc::system_server::Server().RegisterService(transfer_service);

  thread::DetachedThread(thread::stl::Options(), transfer_thread);

  // It's fine to allocate this on the stack since this thread doesn't return
  // until this process is killed.
  FileTransferHandler transfer_handler(
      transfer_service, config.resource_id(), config.file().c_str());

  PW_LOG_INFO("Starting pw_rpc server");
  PW_CHECK_OK(rpc::system_server::Start());

  // Force server to exit as a temporary work around for b/229142175.
  _Exit(0);
}

}  // namespace
}  // namespace pw::transfer

int main(int argc, char* argv[]) {
  if (argc != 2) {
    PW_LOG_INFO("Usage: %s PORT <<< config textproto", argv[0]);
    return 1;
  }

  int port = std::atoi(argv[1]);
  PW_CHECK_UINT_GT(port, 0, "Invalid port!");

  std::string config_string;
  std::string line;
  while (std::getline(std::cin, line)) {
    config_string = config_string + line + '\n';
  }
  pw::transfer::ServerConfig config;

  bool ok =
      google::protobuf::TextFormat::ParseFromString(config_string, &config);
  if (!ok) {
    PW_LOG_INFO("Failed to parse config: %s", config_string.c_str());
    PW_LOG_INFO("Usage: %s PORT <<< config textproto", argv[0]);
    return 1;
  } else {
    PW_LOG_INFO("Server loaded config:\n%s", config.DebugString().c_str());
  }

  pw::transfer::RunServer(port, config);
  return 0;
}
