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
#pragma once

#include <tuple>

#include "pw_allocator/allocator.h"
#include "pw_channel/channel.h"
#include "pw_rpc/server.h"

namespace pw {
namespace system {

class AsyncCore;  // Forward declaration for function declaration.

}  // namespace system

/// Returns a reference to the global pw_system instance. pw::System() provides
/// several features for applications: a memory allocator, an async dispatcher,
/// and a RPC server.
system::AsyncCore& System();

/// Starts running `pw_system:async` with the provided IO channel. This function
/// never returns.
[[noreturn]] void SystemStart(channel::ByteReaderWriter& io_channel);

namespace system {

/// The global `pw::System()` instance. This object is safe to access, whether
/// `pw::SystemStart` has been called or not.
class AsyncCore {
 public:
  /// Returns the system `pw::Allocator` instance.
  Allocator& allocator();

  /// Returns the system `pw::async2::Dispatcher` instance.
  async2::Dispatcher& dispatcher();

  /// Returns the system `pw::rpc::Server` instance.
  rpc::Server& rpc_server() { return rpc_server_; }

 private:
  friend AsyncCore& pw::System();
  friend void pw::SystemStart(channel::ByteReaderWriter&);

  explicit _PW_RPC_CONSTEXPR AsyncCore()
      : rpc_channels_{}, rpc_server_(rpc_channels_) {}

  void Init(channel::ByteReaderWriter& io_channel);

  static async2::Poll<> InitTask(async2::Context&);

  rpc::Channel rpc_channels_[1];
  rpc::Server rpc_server_;
};

}  // namespace system

inline system::AsyncCore& System() {
  _PW_RPC_CONSTINIT static system::AsyncCore system_core;
  return system_core;
}

}  // namespace pw
