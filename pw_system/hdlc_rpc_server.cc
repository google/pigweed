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

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_hdlc/decoder.h"
#include "pw_hdlc/default_addresses.h"
#include "pw_hdlc/encoded_size.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_log/log.h"
#include "pw_rpc/channel.h"
#include "pw_sync/mutex.h"
#include "pw_system/config.h"
#include "pw_system/io.h"
#include "pw_system/rpc_server.h"
#include "pw_trace/trace.h"

#if PW_SYSTEM_DEFAULT_CHANNEL_ID != PW_SYSTEM_LOGGING_CHANNEL_ID && \
    PW_SYSTEM_DEFAULT_RPC_HDLC_ADDRESS == PW_SYSTEM_LOGGING_RPC_HDLC_ADDRESS
#error \
    "Default and logging addresses must be different to support multiple channels."
#endif

namespace pw::system {
namespace {

constexpr size_t kMaxTransmissionUnit = PW_SYSTEM_MAX_TRANSMISSION_UNIT;

static_assert(kMaxTransmissionUnit ==
              hdlc::MaxEncodedFrameSize(rpc::cfg::kEncodingBufferSizeBytes));

#if PW_SYSTEM_DEFAULT_CHANNEL_ID == PW_SYSTEM_LOGGING_CHANNEL_ID
hdlc::FixedMtuChannelOutput<kMaxTransmissionUnit> hdlc_channel_output(
    GetWriter(), PW_SYSTEM_DEFAULT_RPC_HDLC_ADDRESS, "HDLC channel");
rpc::Channel channels[] = {
    rpc::Channel::Create<kDefaultRpcChannelId>(&hdlc_channel_output)};
#else
class SynchronizedChannelOutput : public rpc::ChannelOutput {
 public:
  SynchronizedChannelOutput(stream::Writer& writer,
                            uint64_t address,
                            const char* channel_name)
      : rpc::ChannelOutput(channel_name),
        inner_(writer, address, channel_name) {}

  Status Send(span<const std::byte> buffer) override {
    std::lock_guard guard(mtx_);
    auto s = inner_.Send(buffer);
    return s;
  }

  size_t MaximumTransmissionUnit() override {
    std::lock_guard guard(mtx_);
    auto s = inner_.MaximumTransmissionUnit();
    return s;
  }

 private:
  sync::Mutex mtx_;
  hdlc::FixedMtuChannelOutput<kMaxTransmissionUnit> inner_ PW_GUARDED_BY(mtx_);
};

SynchronizedChannelOutput hdlc_channel_output[] = {
    SynchronizedChannelOutput(GetWriter(),
                              PW_SYSTEM_DEFAULT_RPC_HDLC_ADDRESS,
                              "HDLC default channel"),
    SynchronizedChannelOutput(GetWriter(),
                              PW_SYSTEM_LOGGING_RPC_HDLC_ADDRESS,
                              "HDLC logging channel"),
};
rpc::Channel channels[] = {
    rpc::Channel::Create<kDefaultRpcChannelId>(&hdlc_channel_output[0]),
    rpc::Channel::Create<kLoggingRpcChannelId>(&hdlc_channel_output[1]),
};
#endif
rpc::Server server(channels);

constexpr size_t kDecoderBufferSize =
    hdlc::Decoder::RequiredBufferSizeForFrameSize(kMaxTransmissionUnit);
// Declare a buffer for decoding incoming HDLC frames.
std::array<std::byte, kDecoderBufferSize> input_buffer;
hdlc::Decoder decoder(input_buffer);

std::array<std::byte, 1> data;

}  // namespace

rpc::Server& GetRpcServer() { return server; }

class RpcDispatchThread final : public thread::ThreadCore {
 public:
  RpcDispatchThread() = default;
  RpcDispatchThread(const RpcDispatchThread&) = delete;
  RpcDispatchThread(RpcDispatchThread&&) = delete;
  RpcDispatchThread& operator=(const RpcDispatchThread&) = delete;
  RpcDispatchThread& operator=(RpcDispatchThread&&) = delete;

  void Run() override {
    PW_LOG_INFO("Running RPC server");
    while (true) {
      auto ret_val = GetReader().Read(data);
      if (ret_val.ok()) {
        for (std::byte byte : ret_val.value()) {
          if (auto result = decoder.Process(byte); result.ok()) {
            hdlc::Frame& frame = result.value();
            PW_TRACE_SCOPE("RPC process frame");
            if (frame.address() == PW_SYSTEM_DEFAULT_RPC_HDLC_ADDRESS ||
                frame.address() == PW_SYSTEM_LOGGING_RPC_HDLC_ADDRESS) {
              server.ProcessPacket(frame.data());
            }
          }
        }
      }
    }
  }
};

thread::ThreadCore& GetRpcDispatchThread() {
  static RpcDispatchThread rpc_dispatch_thread;
  return rpc_dispatch_thread;
}

}  // namespace pw::system
