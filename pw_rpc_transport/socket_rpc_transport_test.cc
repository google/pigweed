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

#include "pw_rpc_transport/socket_rpc_transport.h"

#include <algorithm>
#include <random>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_rpc_transport/socket_rpc_transport.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_stream/socket_stream.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace pw::rpc {
namespace {

using namespace std::chrono_literals;

constexpr size_t kMaxWriteSize = 64;
constexpr size_t kReadBufferSize = 64;
// Let the kernel pick the port number.
constexpr uint16_t kServerPort = 0;

class TestIngress : public RpcIngressHandler {
 public:
  explicit TestIngress(size_t num_bytes_expected)
      : num_bytes_expected_(num_bytes_expected) {}

  Status ProcessIncomingData(ConstByteSpan buffer) override {
    if (num_bytes_expected_ > 0) {
      std::copy(buffer.begin(), buffer.end(), std::back_inserter(received_));
      num_bytes_expected_ -= std::min(num_bytes_expected_, buffer.size());
    }
    if (num_bytes_expected_ == 0) {
      done_.release();
    }
    return OkStatus();
  }

  std::vector<std::byte> received() const { return received_; }
  void Wait() { done_.acquire(); }

 private:
  size_t num_bytes_expected_ = 0;
  sync::ThreadNotification done_;
  std::vector<std::byte> received_;
};

class SocketSender {
 public:
  SocketSender(SocketRpcTransport<kReadBufferSize>& transport)
      : transport_(transport) {
    unsigned char c = 0;
    for (auto& i : data_) {
      i = std::byte{c++};
    }
    std::mt19937 rg{0x12345678};
    std::shuffle(data_.begin(), data_.end(), rg);
  }

  std::vector<std::byte> sent() { return sent_; }

  RpcFrame MakeFrame(size_t max_size) {
    std::mt19937 rg{0x12345678};
    size_t offset = offset_dist_(rg);
    size_t message_size = std::min(size_dist_(rg), max_size);
    size_t header_size = message_size > 4 ? 4 : message_size;
    size_t payload_size = message_size > 4 ? message_size - 4 : 0;

    return RpcFrame{.header = span(data_).subspan(offset, header_size),
                    .payload = span(data_).subspan(offset, payload_size)};
  }

  void Send(size_t num_bytes) {
    size_t bytes_written = 0;
    while (bytes_written < num_bytes) {
      auto frame = MakeFrame(num_bytes - bytes_written);
      std::copy(
          frame.header.begin(), frame.header.end(), std::back_inserter(sent_));
      std::copy(frame.payload.begin(),
                frame.payload.end(),
                std::back_inserter(sent_));

      // Tests below expect to see all data written to the socket to be received
      // by the other end, so we keep retrying on any errors that could happen
      // during reconnection: in reality it would be up to the higher level
      // abstractions to do this depending on how they manage buffers etc. For
      // the tests we just keep retrying indefinitely: if there is a
      // non-transient problem then the test will eventually time out.
      while (true) {
        const auto send_status = transport_.Send(frame);
        if (send_status.ok()) {
          break;
        }
      }

      bytes_written += frame.header.size() + frame.payload.size();
    }
  }

 private:
  SocketRpcTransport<kReadBufferSize>& transport_;
  std::vector<std::byte> sent_;
  std::array<std::byte, 256> data_{};
  std::uniform_int_distribution<size_t> offset_dist_{0, 255};
  std::uniform_int_distribution<size_t> size_dist_{1, kMaxWriteSize};
};

class SocketSenderThreadCore : public SocketSender, public thread::ThreadCore {
 public:
  SocketSenderThreadCore(SocketRpcTransport<kReadBufferSize>& transport,
                         size_t write_size)
      : SocketSender(transport), write_size_(write_size) {}

 private:
  void Run() override { Send(write_size_); }
  size_t write_size_;
};

TEST(SocketRpcTransportTest, SendAndReceiveFramesOverSocketConnection) {
  constexpr size_t kWriteSize = 8192;

  TestIngress server_ingress(kWriteSize);
  TestIngress client_ingress(kWriteSize);

  auto server = SocketRpcTransport<kReadBufferSize>(
      SocketRpcTransport<kReadBufferSize>::kAsServer,
      kServerPort,
      server_ingress);
  auto server_thread = thread::Thread(thread::stl::Options(), server);

  server.WaitUntilReady();
  auto server_port = server.port();

  auto client = SocketRpcTransport<kReadBufferSize>(
      SocketRpcTransport<kReadBufferSize>::kAsClient,
      "localhost",
      server_port,
      client_ingress);
  auto client_thread = thread::Thread(thread::stl::Options(), client);

  client.WaitUntilConnected();
  server.WaitUntilConnected();

  SocketSenderThreadCore client_sender(client, kWriteSize);
  SocketSenderThreadCore server_sender(server, kWriteSize);

  auto client_sender_thread =
      thread::Thread(thread::stl::Options(), client_sender);
  auto server_sender_thread =
      thread::Thread(thread::stl::Options(), server_sender);

  client_sender_thread.join();
  server_sender_thread.join();

  server_ingress.Wait();
  client_ingress.Wait();

  server.Stop();
  client.Stop();

  server_thread.join();
  client_thread.join();

  auto received_by_server = server_ingress.received();
  EXPECT_EQ(received_by_server.size(), kWriteSize);
  EXPECT_TRUE(std::equal(received_by_server.begin(),
                         received_by_server.end(),
                         client_sender.sent().begin()));

  auto received_by_client = client_ingress.received();
  EXPECT_EQ(received_by_client.size(), kWriteSize);
  EXPECT_TRUE(std::equal(received_by_client.begin(),
                         received_by_client.end(),
                         server_sender.sent().begin()));
}

TEST(SocketRpcTransportTest, ServerReconnects) {
  // Set up a server and a client that reconnects multiple times. The server
  // must accept the new connection gracefully.
  constexpr size_t kWriteSize = 8192;
  std::vector<std::byte> received;

  TestIngress server_ingress(0);
  auto server = SocketRpcTransport<kReadBufferSize>(
      SocketRpcTransport<kReadBufferSize>::kAsServer,
      kServerPort,
      server_ingress);
  auto server_thread = thread::Thread(thread::stl::Options(), server);

  server.WaitUntilReady();
  auto server_port = server.port();
  SocketSender server_sender(server);

  {
    TestIngress client_ingress(kWriteSize);
    auto client = SocketRpcTransport<kReadBufferSize>(
        SocketRpcTransport<kReadBufferSize>::kAsClient,
        "localhost",
        server_port,
        client_ingress);
    auto client_thread = thread::Thread(thread::stl::Options(), client);

    client.WaitUntilConnected();
    server.WaitUntilConnected();

    server_sender.Send(kWriteSize);
    client_ingress.Wait();
    auto client_received = client_ingress.received();
    std::copy(client_received.begin(),
              client_received.end(),
              std::back_inserter(received));
    EXPECT_EQ(received.size(), kWriteSize);

    // Stop the client but not the server: we're re-using the same server
    // with a new client below.
    client.Stop();
    client_thread.join();
  }

  // Reconnect to the server and keep sending frames.
  {
    TestIngress client_ingress(kWriteSize);
    auto client = SocketRpcTransport<kReadBufferSize>(
        SocketRpcTransport<kReadBufferSize>::kAsClient,
        "localhost",
        server_port,
        client_ingress);
    auto client_thread = thread::Thread(thread::stl::Options(), client);

    client.WaitUntilConnected();
    server.WaitUntilConnected();

    server_sender.Send(kWriteSize);
    client_ingress.Wait();
    auto client_received = client_ingress.received();
    std::copy(client_received.begin(),
              client_received.end(),
              std::back_inserter(received));

    client.Stop();
    client_thread.join();

    // This time stop the server as well.
    SocketSender client_sender(client);
    server.Stop();
    server_thread.join();
  }

  EXPECT_EQ(received.size(), 2 * kWriteSize);
  EXPECT_EQ(server_sender.sent().size(), 2 * kWriteSize);
  EXPECT_TRUE(std::equal(
      received.begin(), received.end(), server_sender.sent().begin()));
}

TEST(SocketRpcTransportTest, ClientReconnects) {
  // Set up a server and a client, then recycle the server. The client must
  // must reconnect gracefully.
  constexpr size_t kWriteSize = 8192;
  uint16_t server_port = 0;

  TestIngress server_ingress(0);
  TestIngress client_ingress(2 * kWriteSize);

  auto server = std::make_unique<SocketRpcTransport<kReadBufferSize>>(
      SocketRpcTransport<kReadBufferSize>::kAsServer,
      kServerPort,
      server_ingress);
  auto server_thread = thread::Thread(thread::stl::Options(), *server);

  server->WaitUntilReady();
  server_port = server->port();

  auto client = SocketRpcTransport<kReadBufferSize>(
      SocketRpcTransport<kReadBufferSize>::kAsClient,
      "localhost",
      server_port,
      client_ingress);
  auto client_thread = thread::Thread(thread::stl::Options(), client);

  client.WaitUntilConnected();
  server->WaitUntilConnected();

  SocketSender client_sender(client);
  SocketSender server1_sender(*server);
  std::vector<std::byte> sent_by_server;

  server1_sender.Send(kWriteSize);
  server->Stop();
  auto server1_sent = server1_sender.sent();
  std::copy(server1_sent.begin(),
            server1_sent.end(),
            std::back_inserter(sent_by_server));

  server_thread.join();
  server = nullptr;

  server = std::make_unique<SocketRpcTransport<kReadBufferSize>>(
      SocketRpcTransport<kReadBufferSize>::kAsServer,
      server_port,
      server_ingress);
  SocketSender server2_sender(*server);
  server_thread = thread::Thread(thread::stl::Options(), *server);

  client.WaitUntilConnected();
  server->WaitUntilConnected();

  server2_sender.Send(kWriteSize);
  client_ingress.Wait();

  server->Stop();
  auto server2_sent = server2_sender.sent();
  std::copy(server2_sent.begin(),
            server2_sent.end(),
            std::back_inserter(sent_by_server));

  server_thread.join();

  client.Stop();
  client_thread.join();
  server = nullptr;

  auto received_by_client = client_ingress.received();
  EXPECT_EQ(received_by_client.size(), 2 * kWriteSize);
  EXPECT_TRUE(std::equal(received_by_client.begin(),
                         received_by_client.end(),
                         sent_by_server.begin()));
}

}  // namespace
}  // namespace pw::rpc
