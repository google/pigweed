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

// clang-format off
#include "pw_rpc/internal/log_config.h"  // PW_LOG_* macros must be first.

#include "pw_rpc/internal/endpoint.h"
// clang-format on

#include <chrono>

#include "pw_log/log.h"
#include "pw_rpc/internal/lock.h"
#include "pw_toolchain/no_destructor.h"

#if PW_RPC_YIELD_MODE == PW_RPC_YIELD_MODE_BUSY_LOOP

static_assert(
    PW_RPC_USE_GLOBAL_MUTEX == 0,
    "The RPC global mutex is enabled, but no pw_rpc yield mode is selected! "
    "Because the global mutex is in use, pw_rpc may be used from multiple "
    "threads. This could result in thread starvation. To fix this, set "
    "PW_RPC_YIELD to PW_RPC_YIELD_MODE_SLEEP and add a dependency on "
    "pw_thread:sleep.");

#elif PW_RPC_YIELD_MODE == PW_RPC_YIELD_MODE_SLEEP

#if !__has_include("pw_thread/sleep.h")

static_assert(false,
              "PW_RPC_YIELD_MODE is PW_RPC_YIELD_MODE_SLEEP "
              "(pw::this_thread::sleep_for()), but no backend is set for "
              "pw_thread:sleep. Set a pw_thread:sleep backend or use a "
              "different PW_RPC_YIELD_MODE setting.");

#endif  // !__has_include("pw_thread/sleep.h")

#include "pw_thread/sleep.h"

#elif PW_RPC_YIELD_MODE == PW_RPC_YIELD_MODE_YIELD

#if !__has_include("pw_thread/yield.h")

static_assert(false,
              "PW_RPC_YIELD_MODE is PW_RPC_YIELD_MODE_YIELD "
              "(pw::this_thread::yield()), but no backend is set for "
              "pw_thread:yield. Set a pw_thread:yield backend or use a "
              "different PW_RPC_YIELD_MODE setting.");

#endif  // !__has_include("pw_thread/yield.h")

#include "pw_thread/yield.h"

#else

static_assert(
    false,
    "PW_RPC_YIELD_MODE macro must be set to PW_RPC_YIELD_MODE_BUSY_LOOP, "
    "PW_RPC_YIELD_MODE_SLEEP (pw::this_thread::sleep_for()), or "
    "PW_RPC_YIELD_MODE_YIELD (pw::this_thread::yield())");

#endif  // PW_RPC_YIELD_MODE

namespace pw::rpc::internal {

RpcLock& rpc_lock() {
  static NoDestructor<RpcLock> lock;
  return *lock;
}

void YieldRpcLock() {
  rpc_lock().unlock();
#if PW_RPC_YIELD_MODE == PW_RPC_YIELD_MODE_SLEEP
  static constexpr chrono::SystemClock::duration kSleepDuration =
      PW_RPC_YIELD_SLEEP_DURATION;
  this_thread::sleep_for(kSleepDuration);
#elif PW_RPC_YIELD_MODE == PW_RPC_YIELD_MODE_YIELD
  this_thread::yield();
#endif  // PW_RPC_YIELD_MODE
  rpc_lock().lock();
}

Endpoint::~Endpoint() {
  // Since the calls remove themselves from the Endpoint in
  // CloseAndSendResponse(), close responders until no responders remain.
  while (!calls_.empty()) {
    calls_.front().CloseAndSendResponse(OkStatus()).IgnoreError();
  }
}

Result<Packet> Endpoint::ProcessPacket(span<const std::byte> data,
                                       Packet::Destination destination) {
  Result<Packet> result = Packet::FromBuffer(data);

  if (!result.ok()) {
    PW_LOG_WARN("Failed to decode pw_rpc packet");
    return Status::DataLoss();
  }

  Packet& packet = *result;

  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    PW_LOG_WARN("Received malformed pw_rpc packet");
    return Status::DataLoss();
  }

  if (packet.destination() != destination) {
    return Status::InvalidArgument();
  }

  return result;
}

void Endpoint::RegisterCall(Call& call) {
  Call* const existing_call = FindCallById(
      call.channel_id_locked(), call.service_id(), call.method_id(), call.id());

  calls_.push_front(call);

  if (existing_call != nullptr) {
    // TODO(b/260922913): The HandleError() call needs to be deferred to avoid
    //   releasing the lock before finishing state updates. Could move the call
    //   to the planned calls_to_abort list and clean up later.
    existing_call->HandleError(Status::Cancelled());
    rpc_lock().lock();
  }
}

Call* Endpoint::FindCallById(uint32_t channel_id,
                             uint32_t service_id,
                             uint32_t method_id,
                             uint32_t call_id) {
  for (Call& call : calls_) {
    if (channel_id == call.channel_id_locked() &&
        service_id == call.service_id() && method_id == call.method_id()) {
      if (call_id == call.id() || call_id == kOpenCallId) {
        return &call;
      }
      if (call.id() == kOpenCallId) {
        // Calls with ID of `kOpenCallId` were unrequested, and
        // are updated to have the call ID of the first matching request.
        call.set_id(call_id);
        return &call;
      }
    }
  }
  return nullptr;
}

Status Endpoint::CloseChannel(uint32_t channel_id) {
  LockGuard lock(rpc_lock());

  Channel* channel = channels_.Get(channel_id);
  if (channel == nullptr) {
    return Status::NotFound();
  }
  channel->Close();

  // Close pending calls on the channel that's going away.
  AbortCalls(AbortIdType::kChannel, channel_id);
  return OkStatus();
}

void Endpoint::AbortCalls(AbortIdType type, uint32_t id) {
  auto previous = calls_.before_begin();
  auto current = calls_.begin();

  while (current != calls_.end()) {
    if (id == (type == AbortIdType::kChannel ? current->channel_id_locked()
                                             : current->service_id())) {
      current->Abort();
      current = calls_.erase_after(previous);  // previous stays the same
    } else {
      previous = current;
      ++current;
    }
  }
}

}  // namespace pw::rpc::internal
