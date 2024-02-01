// Copyright 2024 The Pigweed Authors
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

#include <cstring>
#include <map>
#include <string_view>
#include <type_traits>

#include "pw_assert/check.h"
#include "pw_bytes/byte_builder.h"
#include "pw_bytes/span.h"
#include "pw_grpc/connection.h"
#include "pw_grpc/examples/echo/echo.rpc.pwpb.h"
#include "pw_grpc/grpc_channel_output.h"
#include "pw_grpc/pw_rpc_handler.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc_transport/service_registry.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/socket_stream.h"
#include "pw_stream/stream.h"
#include "pw_string/string.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"

using pw::grpc::StreamId;

namespace {

class EchoService
    : public ::grpc::examples::echo::pw_rpc::pwpb::Echo::Service<EchoService> {
 public:
  void UnaryEcho(
      const ::grpc::examples::echo::pwpb::EchoRequest::Message& request,
      pw::rpc::PwpbUnaryResponder<
          ::grpc::examples::echo::pwpb::EchoResponse::Message>& responder) {
    PW_LOG_INFO("UnaryEcho %s", request.message.c_str());
    quiet_ = request.message.compare("quiet") == 0;
    last_unary_responder_ = std::move(responder);
    if (quiet_) {
      return;
    }
    last_unary_responder_.Finish({.message = request.message}, pw::OkStatus())
        .IgnoreError();
  }

  void ServerStreamingEcho(
      const ::grpc::examples::echo::pwpb::EchoRequest::Message& request,
      ServerWriter<::grpc::examples::echo::pwpb::EchoResponse::Message>&
          writer) {
    PW_LOG_INFO("ServerStreamingEcho %s", request.message.c_str());
    quiet_ = request.message.compare("quiet") == 0;
    last_writer_ = std::move(writer);
    if (quiet_) {
      PW_LOG_INFO("not writing server streaming echo");
      return;
    }
    for (size_t i = 0; i < 3; ++i) {
      last_writer_.Write({.message = request.message}).IgnoreError();
    }
    last_writer_.Finish(pw::OkStatus()).IgnoreError();
  }

  void ClientStreamingEcho(
      ServerReader<::grpc::examples::echo::pwpb::EchoRequest::Message,
                   ::grpc::examples::echo::pwpb::EchoResponse::Message>&
          reader) {
    PW_LOG_INFO("ClientStreamingEcho");
    last_reader_ = std::move(reader);
    last_reader_.set_on_next(
        [this](
            const ::grpc::examples::echo::pwpb::EchoRequest::Message& request) {
          quiet_ = request.message.compare("quiet") == 0;
          PW_LOG_INFO("ClientStreaming message %s", request.message.c_str());
        });

    last_reader_.set_on_completion_requested([this]() {
      if (quiet_) {
        return;
      }
      last_reader_.Finish({.message = "done"}).IgnoreError();
    });
  }

  void BidirectionalStreamingEcho(
      ServerReaderWriter<::grpc::examples::echo::pwpb::EchoRequest::Message,
                         ::grpc::examples::echo::pwpb::EchoResponse::Message>&
          reader_writer) {
    PW_LOG_INFO("BidirectionalStreamingEcho");
    last_reader_writer_ = std::move(reader_writer);
    last_reader_writer_.set_on_completion_requested([this]() {
      if (quiet_) {
        return;
      }
      last_reader_writer_.Finish(pw::OkStatus()).IgnoreError();
    });
    last_reader_writer_.set_on_next(
        [this](
            const ::grpc::examples::echo::pwpb::EchoRequest::Message& request) {
          PW_LOG_INFO("BidiStreaming message %s", request.message.c_str());
          quiet_ = request.message.compare("quiet") == 0;
          if (quiet_) {
            return;
          }
          last_reader_writer_.Write({.message = request.message}).IgnoreError();
        });
  }

 private:
  pw::rpc::PwpbUnaryResponder<
      ::grpc::examples::echo::pwpb::EchoResponse::Message>
      last_unary_responder_{};
  ServerWriter<::grpc::examples::echo::pwpb::EchoResponse::Message>
      last_writer_{};
  ServerReader<::grpc::examples::echo::pwpb::EchoRequest::Message,
               ::grpc::examples::echo::pwpb::EchoResponse::Message>
      last_reader_{};
  ServerReaderWriter<::grpc::examples::echo::pwpb::EchoRequest::Message,
                     ::grpc::examples::echo::pwpb::EchoResponse::Message>
      last_reader_writer_{};
  bool quiet_ = false;
};

constexpr uint32_t kTestChannelId = 1;

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  int port = 3400;
  int num_connections = 1;

  if (args.size() > 1) {
    if (args[1] == "--help") {
      PW_LOG_INFO("Usage: [port=3400] [num_connections=1]");
      PW_LOG_INFO(
          "  num_connections positional arg sets how many socket connections "
          "should be processed before exit");
      exit(0);
    }
    port = stoi(args[1]);
  }

  if (args.size() > 2) {
    num_connections = stoi(args[2]);
  }

  std::setbuf(stdout, nullptr);  // unbuffered stdout

  pw::stream::ServerSocket server;
  std::array<std::byte, 8192> packet_encode_buffer;
  pw::grpc::GrpcChannelOutput rpc_egress;
  std::array<pw::rpc::Channel, 1> tx_channels(
      {pw::rpc::Channel::Create<kTestChannelId>(&rpc_egress)});
  pw::rpc::ServiceRegistry service_registry(tx_channels);

  EchoService echo_service;
  service_registry.RegisterService(echo_service);

  pw::grpc::PwRpcHandler handler(kTestChannelId,
                                 service_registry,
                                 service_registry.client_server().server(),
                                 packet_encode_buffer);
  rpc_egress.set_callbacks(handler);

  PW_LOG_INFO("Main.Listen on port=%d", port);
  if (auto status = server.Listen(port); !status.ok()) {
    PW_LOG_ERROR("Main.Listen failed code=%d", status.code());
    return 1;
  }

  for (int i = 0; i < num_connections; ++i) {
    PW_LOG_INFO("Main.Accept");
    auto socket = server.Accept();
    if (!socket.ok()) {
      PW_LOG_ERROR("Main.Accept failed code=%d", socket.status().code());
      return 1;
    }

    PW_LOG_INFO("Main.Run");

    pw::thread::test::TestThreadContext connection_thread_context;
    pw::thread::test::TestThreadContext send_thread_context;
    pw::grpc::ConnectionThread conn(
        *socket, send_thread_context.options(), handler, [&socket]() {
          socket->Close();
        });
    rpc_egress.set_connection(conn);

    pw::thread::Thread conn_thread(connection_thread_context.options(), conn);
    conn_thread.join();
  }

  PW_LOG_INFO("Main.Run completed");
  return 0;
}
