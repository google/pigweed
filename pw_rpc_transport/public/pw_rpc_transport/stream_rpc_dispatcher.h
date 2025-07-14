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
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "pw_rpc_transport/egress_ingress.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_thread/thread_core.h"

namespace pw::rpc {

class StreamRpcDispatcherTracker {
 public:
  virtual ~StreamRpcDispatcherTracker() = default;
  virtual void ReadError([[maybe_unused]] Status status) {}
  virtual void EgressError([[maybe_unused]] Status status) {}
};

template <size_t kReadSize>
class StreamRpcDispatcher : public pw::thread::ThreadCore {
 public:
  StreamRpcDispatcher(pw::stream::Reader& reader,
                      pw::rpc::RpcIngressHandler& ingress_handler,
                      StreamRpcDispatcherTracker* tracker = nullptr)
      : reader_(reader), ingress_handler_(ingress_handler), tracker_(tracker) {}
  ~StreamRpcDispatcher() override { Stop(); }

  // Once stopped, will no longer process data.
  void Stop() {
    if (stopped_) {
      return;
    }
    stopped_ = true;
  }

 protected:
  // From pw::thread::ThreadCore.
  void Run() final {
    while (!stopped_) {
      auto read = reader_.Read(read_buffer_);
      if (!read.ok()) {
        if (tracker_) {
          tracker_->ReadError(read.status());
        }
        continue;
      }

      if (const auto status = ingress_handler_.ProcessIncomingData(*read);
          !status.ok()) {
        if (tracker_) {
          tracker_->EgressError(status);
        }
        continue;
      }
    }
  }

 private:
  std::array<std::byte, kReadSize> read_buffer_ = {};
  pw::stream::Reader& reader_;
  pw::rpc::RpcIngressHandler& ingress_handler_;
  std::atomic<bool> stopped_ = false;
  StreamRpcDispatcherTracker* tracker_ = nullptr;
};

}  // namespace pw::rpc
