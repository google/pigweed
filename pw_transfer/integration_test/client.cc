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

// Client binary for the cross-language integration test.
//
// Usage:
//  bazel-bin/pw_transfer/integration_test_client 3300 <<< "resource_id: 12
//  file: '/tmp/myfile.txt'"
//
// WORK IN PROGRESS, SEE b/228516801
#include "pw_transfer/client.h"

#include <sys/socket.h>

#include <cstddef>
#include <cstdio>

#include "google/protobuf/text_format.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_rpc/integration_testing.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/std_file_stream.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/integration_test/config.pb.h"
#include "pw_transfer/transfer_thread.h"

namespace pw::transfer::integration_test {
namespace {

// This is the maximum size of the socket send buffers. Ideally, this is set
// to the lowest allowed value to minimize buffering between the proxy and
// clients so rate limiting causes the client to block and wait for the
// integration test proxy to drain rather than allowing OS buffers to backlog
// large quantities of data.
//
// Note that the OS may chose to not strictly follow this requested buffer size.
// Still, setting this value to be as small as possible does reduce bufer sizes
// significantly enough to better reflect typical inter-device communication.
//
// For this to be effective, servers should also configure their sockets to a
// smaller receive buffer size.
constexpr int kMaxSocketSendBufferSize = 1;

// This client configures a socket read timeout to allow the RPC dispatch thread
// to exit gracefully.
constexpr timeval kSocketReadTimeout = {.tv_sec = 1, .tv_usec = 0};

thread::Options& TransferThreadOptions() {
  static thread::stl::Options options;
  return options;
}

// Transfer status, valid only after semaphore is acquired.
//
// We need to bundle the status and semaphore together because a pw_function
// callback can at most capture the reference to one variable (and we need to
// both set the status and release the semaphore).
struct WriteResult {
  Status status = Status::Unknown();
  sync::BinarySemaphore completed;
};

// Create a pw_transfer client, read data from path_to_data, and write it to the
// client using the given resource_id.
pw::Status SendData(const pw::transfer::ClientConfig& config) {
  std::byte chunk_buffer[512];
  std::byte encode_buffer[512];
  transfer::Thread<2, 2> transfer_thread(chunk_buffer, encode_buffer);
  thread::Thread system_thread(TransferThreadOptions(), transfer_thread);

  pw::transfer::Client client(rpc::integration_test::client(),
                              rpc::integration_test::kChannelId,
                              transfer_thread,
                              /*max_bytes_to_receive=*/256);

  pw::stream::StdFileReader input(config.file().c_str());

  WriteResult result;

  client.Write(config.resource_id(), input, [&result](Status status) {
    result.status = status;
    result.completed.release();
  });

  // Waits for the transfer to complete and returns the status.
  // PW_CHECK(completed.try_acquire_for(3s));  How to get this syntax to work?
  result.completed.acquire();

  transfer_thread.Terminate();

  system_thread.join();

  // The RPC thread must join before destroying transfer objects as the transfer
  // service may still reference the transfer thread or transfer client objects.
  pw::rpc::integration_test::TerminateClient();
  return result.status;
}

}  // namespace
}  // namespace pw::transfer::integration_test

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PW_LOG_INFO("Usage: %s PORT <<< config textproto", argv[0]);
    return 1;
  }

  const int port = std::atoi(argv[1]);

  std::string config_string;
  std::string line;
  while (std::getline(std::cin, line)) {
    config_string = config_string + line + '\n';
  }
  pw::transfer::ClientConfig config;

  bool ok =
      google::protobuf::TextFormat::ParseFromString(config_string, &config);
  if (!ok) {
    PW_LOG_INFO("Failed to parse config: %s", config_string.c_str());
    PW_LOG_INFO("Usage: %s PORT <<< config textproto", argv[0]);
    return 1;
  } else {
    PW_LOG_INFO("Client loaded config:\n%s", config.DebugString().c_str());
  }

  if (!pw::rpc::integration_test::InitializeClient(port).ok()) {
    return 1;
  }

  int retval = setsockopt(
      pw::rpc::integration_test::GetClientSocketFd(),
      SOL_SOCKET,
      SO_SNDBUF,
      &pw::transfer::integration_test::kMaxSocketSendBufferSize,
      sizeof(pw::transfer::integration_test::kMaxSocketSendBufferSize));
  PW_CHECK_INT_EQ(retval,
                  0,
                  "Failed to configure socket send buffer size with errno=%d",
                  errno);

  retval =
      setsockopt(pw::rpc::integration_test::GetClientSocketFd(),
                 SOL_SOCKET,
                 SO_RCVTIMEO,
                 &pw::transfer::integration_test::kSocketReadTimeout,
                 sizeof(pw::transfer::integration_test::kSocketReadTimeout));
  PW_CHECK_INT_EQ(retval,
                  0,
                  "Failed to configure socket receive timeout with errno=%d",
                  errno);

  if (!pw::transfer::integration_test::SendData(config).ok()) {
    PW_LOG_INFO("Failed to transfer!");
    return 1;
  }
  return 0;
}
