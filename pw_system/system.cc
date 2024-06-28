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

#define PW_LOG_MODULE_NAME "pw_system"

#include "pw_system/system.h"

#include <utility>

#include "pw_allocator/best_fit_block_allocator.h"
#include "pw_assert/check.h"
#include "pw_async2/allocate_task.h"
#include "pw_async2/pend_func_task.h"
#include "pw_log/log.h"
#include "pw_rpc/echo_service_pwpb.h"
#include "pw_system/config.h"
#include "pw_system/internal/async_packet_io.h"
#include "pw_system/thread_snapshot_service.h"
#include "pw_system/transfer_service.h"
#include "pw_system_private/log.h"
#include "pw_system_private/threads.h"
#include "pw_thread/detached_thread.h"

namespace pw {

void SystemStart(channel::ByteReaderWriter& io_channel) {
  System().Init(io_channel);

  system::StartScheduler();
}

namespace system {
namespace {

// TODO: b/349654108 - Standardize component declaration and initialization.
alignas(internal::PacketIO) std::byte packet_io_[sizeof(internal::PacketIO)];

internal::PacketIO& InitializePacketIoGlobal(
    channel::ByteReaderWriter& io_channel) {
  static std::byte buffer[256];
  internal::PacketIO* packet_io = new (packet_io_) internal::PacketIO(
      io_channel, buffer, System().allocator(), System().rpc_server());
  return *packet_io;
}

// Functions for quickly creating a task from a function. This functions could
// be moved to `pw::system::AsyncCore`.
template <typename Func>
[[nodiscard]] bool PostTaskFunction(Func&& func) {
  async2::Task* task = async2::AllocateTask<async2::PendFuncTask<Func>>(
      System().allocator(), std::forward<Func>(func));
  if (task == nullptr) {
    return false;
  }
  System().dispatcher().Post(*task);
  return true;
}

template <typename Func>
void PostTaskFunctionOrCrash(Func&& func) {
  PW_CHECK(PostTaskFunction(std::forward<Func>(func)));
}

}  // namespace

async2::Dispatcher& AsyncCore::dispatcher() {
  static async2::Dispatcher dispatcher;
  return dispatcher;
}

Allocator& AsyncCore::allocator() {
  alignas(uintptr_t) static std::byte buffer[8192];
  static allocator::BestFitBlockAllocator allocator(buffer);
  return allocator;
}

void AsyncCore::Init(channel::ByteReaderWriter& io_channel) {
  PW_LOG_INFO("Initializing pw_system");

  PostTaskFunctionOrCrash(InitTask);

  // Initialize the packet_io subsystem
  internal::PacketIO& packet_io = InitializePacketIoGlobal(io_channel);
  packet_io.Start(System().dispatcher(), RpcThreadOptions());

  thread::DetachedThread(DispatcherThreadOptions(),
                         [] { System().dispatcher().RunToCompletion(); });
}

async2::Poll<> AsyncCore::InitTask(async2::Context&) {
  PW_LOG_INFO("Initializing pw_system");

  const Status status = GetLogThread().OpenUnrequestedLogStream(
      kLoggingRpcChannelId, System().rpc_server(), GetLogService());
  if (!status.ok()) {
    PW_LOG_ERROR("Error opening unrequested log streams %d",
                 static_cast<int>(status.code()));
  }

  System().rpc_server().RegisterService(GetLogService());
  thread::DetachedThread(system::LogThreadOptions(), GetLogThread());

  static rpc::EchoService echo_service;
  System().rpc_server().RegisterService(echo_service);

  if (PW_SYSTEM_ENABLE_THREAD_SNAPSHOT_SERVICE != 0) {
    RegisterThreadSnapshotService(System().rpc_server());
  }

  if (PW_SYSTEM_ENABLE_TRANSFER_SERVICE != 0) {
    RegisterTransferService(System().rpc_server());
    thread::DetachedThread(system::TransferThreadOptions(),
                           GetTransferThread());
    InitTransferService();
  }

  PW_LOG_INFO("pw_system initialization complete");
  return async2::Ready();
}

}  // namespace system
}  // namespace pw
