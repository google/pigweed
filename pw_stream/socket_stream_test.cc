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

#include "pw_stream/socket_stream.h"

#include <thread>

#include "gtest/gtest.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::stream {
namespace {

// Helper function to create a ServerSocket and connect to it via loopback.
void RunConnectTest(const char* hostname) {
  ServerSocket server;
  EXPECT_EQ(server.Listen(), OkStatus());

  Result<SocketStream> server_stream = Status::Unavailable();
  auto accept_thread = std::thread{[&]() { server_stream = server.Accept(); }};

  SocketStream client;
  EXPECT_EQ(client.Connect(hostname, server.port()), OkStatus());

  accept_thread.join();
  EXPECT_EQ(server_stream.status(), OkStatus());

  server_stream.value().Close();
  server.Close();
  client.Close();
}

TEST(SocketStreamTest, ConnectIpv4) { RunConnectTest("127.0.0.1"); }

TEST(SocketStreamTest, ConnectIpv6) { RunConnectTest("::1"); }

TEST(SocketStreamTest, ConnectSpecificPort) {
  // We want to test the "listen on a specific port" functionality,
  // but hard-coding a port number in a test is inherently problematic, as
  // port numbers are a global resource.
  //
  // We use the automatic port assignment initially to get a port assignment,
  // close that server, and then use that port explicitly in a new server.
  //
  // There's still the possibility that the port will get swiped, but it
  // shouldn't happen by chance.
  ServerSocket initial_server;
  EXPECT_EQ(initial_server.Listen(), OkStatus());
  uint16_t port = initial_server.port();
  initial_server.Close();

  ServerSocket server;
  EXPECT_EQ(server.Listen(port), OkStatus());
  EXPECT_EQ(server.port(), port);

  Result<SocketStream> server_stream = Status::Unavailable();
  auto accept_thread = std::thread{[&]() { server_stream = server.Accept(); }};

  SocketStream client;
  EXPECT_EQ(client.Connect("localhost", server.port()), OkStatus());

  accept_thread.join();
  EXPECT_EQ(server_stream.status(), OkStatus());

  server_stream.value().Close();
  server.Close();
  client.Close();
}

// Helper function to test exchanging data on a pair of sockets.
void ExchangeData(SocketStream& stream1, SocketStream& stream2) {
  auto kPayload1 = as_bytes(span("some data"));
  auto kPayload2 = as_bytes(span("other bytes"));
  std::array<char, 100> read_buffer{};

  // Write data from stream1 and read it from stream2.
  auto write_status = Status::Unavailable();
  auto write_thread =
      std::thread{[&]() { write_status = stream1.Write(kPayload1); }};
  Result<ByteSpan> read_result =
      stream2.Read(as_writable_bytes(span(read_buffer)));
  EXPECT_EQ(read_result.status(), OkStatus());
  EXPECT_EQ(read_result.value().size(), kPayload1.size());
  EXPECT_TRUE(
      std::equal(kPayload1.begin(), kPayload1.end(), read_result->begin()));

  write_thread.join();
  EXPECT_EQ(write_status, OkStatus());

  // Read data in the client and write it from the server.
  auto read_thread = std::thread{[&]() {
    read_result = stream1.Read(as_writable_bytes(span(read_buffer)));
  }};
  EXPECT_EQ(stream2.Write(kPayload2), OkStatus());

  read_thread.join();
  EXPECT_EQ(read_result.status(), OkStatus());
  EXPECT_EQ(read_result.value().size(), kPayload2.size());
  EXPECT_TRUE(
      std::equal(kPayload2.begin(), kPayload2.end(), read_result->begin()));

  // Close stream1 and attempt to read from stream2.
  stream1.Close();
  read_result = stream2.Read(as_writable_bytes(span(read_buffer)));
  EXPECT_EQ(read_result.status(), Status::OutOfRange());

  stream2.Close();
}

TEST(SocketStreamTest, ReadWrite) {
  ServerSocket server;
  EXPECT_EQ(server.Listen(), OkStatus());

  Result<SocketStream> server_stream = Status::Unavailable();
  auto accept_thread = std::thread{[&]() { server_stream = server.Accept(); }};

  SocketStream client;
  EXPECT_EQ(client.Connect("localhost", server.port()), OkStatus());

  accept_thread.join();
  EXPECT_EQ(server_stream.status(), OkStatus());

  ExchangeData(client, server_stream.value());
  server.Close();
}

TEST(SocketStreamTest, MultipleClients) {
  ServerSocket server;
  EXPECT_EQ(server.Listen(), OkStatus());

  Result<SocketStream> server_stream1 = Status::Unavailable();
  Result<SocketStream> server_stream2 = Status::Unavailable();
  Result<SocketStream> server_stream3 = Status::Unavailable();
  auto accept_thread = std::thread{[&]() {
    server_stream1 = server.Accept();
    server_stream2 = server.Accept();
    server_stream3 = server.Accept();
  }};

  SocketStream client1;
  SocketStream client2;
  SocketStream client3;
  EXPECT_EQ(client1.Connect("localhost", server.port()), OkStatus());
  EXPECT_EQ(client2.Connect("localhost", server.port()), OkStatus());
  EXPECT_EQ(client3.Connect("localhost", server.port()), OkStatus());

  accept_thread.join();
  EXPECT_EQ(server_stream1.status(), OkStatus());
  EXPECT_EQ(server_stream2.status(), OkStatus());
  EXPECT_EQ(server_stream3.status(), OkStatus());

  ExchangeData(client1, server_stream1.value());
  ExchangeData(client2, server_stream2.value());
  ExchangeData(client3, server_stream3.value());
  server.Close();
}

TEST(SocketStreamTest, ReuseAutomaticServerPort) {
  uint16_t server_port = 0;
  SocketStream client_stream;
  ServerSocket server;

  EXPECT_EQ(server.Listen(0), OkStatus());
  server_port = server.port();
  EXPECT_NE(server_port, 0);

  Result<SocketStream> server_stream = Status::Unavailable();
  auto accept_thread = std::thread{[&]() { server_stream = server.Accept(); }};

  EXPECT_EQ(client_stream.Connect(nullptr, server_port), OkStatus());
  accept_thread.join();
  ASSERT_EQ(server_stream.status(), OkStatus());

  server_stream->Close();
  server.Close();

  ServerSocket server2;
  EXPECT_EQ(server2.Listen(server_port), OkStatus());
}

}  // namespace
}  // namespace pw::stream
