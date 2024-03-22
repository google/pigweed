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

#include "pw_stream_shmem_mcuxpresso/stream.h"

#include <atomic>
#include <cstdint>

namespace pw::stream {
namespace {

constexpr uint32_t kMuRegDataSize = 0;
constexpr uint32_t kMuRegDataCopied = 1;

}  // namespace

ShmemMcuxpressoStream::~ShmemMcuxpressoStream() { Disable(); }

void ShmemMcuxpressoStream::Enable() {
  MU_Init(base_);
  MU_EnableInterrupts(base_,
                      kMU_Tx0EmptyInterruptEnable | kMU_Rx0FullInterruptEnable |
                          kMU_Rx1FullInterruptEnable);
}

void ShmemMcuxpressoStream::Disable() {
  MU_DisableInterrupts(base_,
                       kMU_Tx0EmptyInterruptEnable |
                           kMU_Rx0FullInterruptEnable |
                           kMU_Rx1FullInterruptEnable);
  MU_Deinit(base_);
}

StatusWithSize ShmemMcuxpressoStream::DoRead(ByteSpan data) {
  read_semaphore_.acquire();

  const uint32_t msg_len = MU_ReceiveMsgNonBlocking(base_, kMuRegDataSize);
  StatusWithSize result(msg_len);

  if (msg_len > shared_read_buffer_.size()) {
    result = StatusWithSize::Internal();
  } else if (msg_len > data.size()) {
    result = StatusWithSize::InvalidArgument();
  } else {
    std::copy(shared_read_buffer_.begin(),
              shared_read_buffer_.begin() + msg_len,
              data.begin());
    // Ensure all data is read before MU message is written.
    std::atomic_thread_fence(std::memory_order_release);
  }

  // Ack we're done with our copy. Use blocking send as the other side will
  // process the message directly in ISR.
  MU_SendMsg(base_, kMuRegDataCopied, msg_len);

  // Turn back on Rx0 interrupt, which will unblock next read.
  MU_EnableInterrupts(base_, kMU_Rx0FullInterruptEnable);

  return result;
}

Status ShmemMcuxpressoStream::DoWrite(ConstByteSpan data) {
  if (data.size() > shared_write_buffer_.size()) {
    return Status::InvalidArgument();
  }
  write_semaphore_.acquire();

  std::copy(data.begin(), data.end(), shared_write_buffer_.begin());

  // Ensure MU message is written after shared buffer is populated.
  std::atomic_thread_fence(std::memory_order_release);

  MU_SendMsgNonBlocking(base_, kMuRegDataSize, data.size());

  write_done_semaphore_.acquire();

  MU_EnableInterrupts(base_, kMU_Tx0EmptyInterruptEnable);

  return OkStatus();
}

void ShmemMcuxpressoStream::HandleInterrupt() {
  const uint32_t flags = MU_GetInterruptsPending(base_);
  if (flags & kMU_Tx0EmptyFlag) {
    write_semaphore_.release();
    MU_DisableInterrupts(base_, kMU_Tx0EmptyInterruptEnable);
  }
  if (flags & kMU_Rx0FullFlag) {
    read_semaphore_.release();
    MU_DisableInterrupts(base_, kMU_Rx0FullInterruptEnable);
  }
  if (flags & kMU_Rx1FullFlag) {
    write_done_semaphore_.release();
    MU_ReceiveMsgNonBlocking(base_, kMuRegDataCopied);
  }
}

}  // namespace pw::stream
