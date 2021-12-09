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
#pragma once

#include <cinttypes>
#include <cstddef>
#include <limits>

#include "pw_assert/assert.h"
#include "pw_chrono/system_timer.h"
#include "pw_rpc/writer.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_transfer/internal/chunk.h"
#include "pw_transfer/internal/chunk_data_buffer.h"
#include "pw_transfer/internal/config.h"
#include "pw_work_queue/work_queue.h"

namespace pw::transfer::internal {

class TransferParameters {
 public:
  constexpr TransferParameters(uint32_t pending_bytes,
                               uint32_t max_chunk_size_bytes)
      : pending_bytes_(pending_bytes),
        max_chunk_size_bytes_(max_chunk_size_bytes) {
    PW_ASSERT(pending_bytes > 0);
    PW_ASSERT(max_chunk_size_bytes > 0);
  }

  uint32_t pending_bytes() const { return pending_bytes_; }
  uint32_t max_chunk_size_bytes() const { return max_chunk_size_bytes_; }

  // The fractional position within a window at which a receive transfer should
  // extend its window size to minimize the amount of time the transmitter
  // spends blocked.
  //
  // For example, a divisor of 2 will extend the window when half of the
  // requested data has been received, a divisor of three will extend at a third
  // of the window, and so on.
  //
  // TODO(frolv): Find a good threshold for this; maybe make it configurable?
  static constexpr uint32_t kExtendWindowDivisor = 2;
  static_assert(kExtendWindowDivisor > 1);

 private:
  uint32_t pending_bytes_;
  uint32_t max_chunk_size_bytes_;
};

// Information about a single transfer.
class Context {
 public:
  enum Type : bool { kTransmit, kReceive };

  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;

  constexpr uint32_t transfer_id() const { return transfer_id_; }

  // True if the context has been used for a transfer (it has an ID).
  bool initialized() {
    state_lock_.lock();
    bool initialized = transfer_state_ != TransferState::kInactive;
    state_lock_.unlock();
    return initialized;
  }

  // True if the transfer is active.
  bool active() {
    state_lock_.lock();
    bool active = transfer_state_ >= TransferState::kData;
    state_lock_.unlock();
    return active;
  }

  // Starts a new transfer from an initialized context by sending the initial
  // transfer chunk. This is generally called from within a transfer client, as
  // it is unusual for a server to initiate a transfer.
  Status InitiateTransfer(const TransferParameters& max_parameters);

  // Extracts data from the provided chunk into the transfer context. This is
  // intended to be the immediate part of the transfer, run directly from within
  // the RPC message handler.
  //
  // Returns true if there is any deferred work required for this chunk (i.e.
  // ProcessChunk should be called to complete the operation).
  bool ReadChunkData(ChunkDataBuffer& buffer,
                     const TransferParameters& max_parameters,
                     const Chunk& chunk);

  // Handles the chunk from the previous invocation of ReadChunkData(). This
  // operation is intended to be deferred, running from a different context than
  // the RPC callback in which the chunk was received.
  void ProcessChunk(ChunkDataBuffer& buffer,
                    const TransferParameters& max_parameters);

 protected:
  using CompletionFunction = Status (*)(Context&, Status);

  Context(CompletionFunction on_completion)
      : transfer_id_(0),
        flags_(0),
        transfer_state_(TransferState::kInactive),
        retries_(0),
        max_retries_(0),
        stream_(nullptr),
        rpc_writer_(nullptr),
        offset_(0),
        window_size_(0),
        window_end_offset_(0),
        pending_bytes_(0),
        max_chunk_size_bytes_(std::numeric_limits<uint32_t>::max()),
        last_chunk_offset_(0),
        timer_([this](chrono::SystemClock::time_point) { this->OnTimeout(); }),
        chunk_timeout_(chrono::SystemClock::duration::zero()),
        work_queue_(nullptr),
        on_completion_(on_completion) {}

  enum class TransferState : uint8_t {
    // This ServerContext has never been used for a transfer. It is available
    // for use for a transfer.
    kInactive,
    // A transfer completed and the final status chunk was sent. The Context is
    // available for use for a new transfer. A receive transfer uses this state
    // to allow a transmitter to retry its last chunk if the final status chunk
    // was dropped.
    kCompleted,
    // Sending or receiving data for an active transfer.
    kData,
    // Recovering after one or more chunks was dropped in an active transfer.
    kRecovery,
    // Hit a timeout and waiting for the timeout handler to run.
    kTimedOut,
  };

  constexpr Type type() const { return static_cast<Type>(flags_ & kFlagsType); }

  void set_transfer_state(TransferState state) {
    state_lock_.lock();
    transfer_state_ = state;
    state_lock_.unlock();
  }

  // Begins a new transmit transfer from this context.
  // Precondition: context is not active.
  void InitializeForTransmit(
      uint32_t transfer_id,
      work_queue::WorkQueue& work_queue,
      rpc::Writer& rpc_writer,
      stream::Reader& reader,
      chrono::SystemClock::duration chunk_timeout = cfg::kDefaultChunkTimeout,
      uint8_t max_retries = cfg::kDefaultMaxRetries) {
    Initialize(kTransmit,
               transfer_id,
               work_queue,
               rpc_writer,
               reader,
               chunk_timeout,
               max_retries);
  }

  // Begins a new receive transfer from this context.
  // Precondition: context is not active.
  void InitializeForReceive(
      uint32_t transfer_id,
      work_queue::WorkQueue& work_queue,
      rpc::Writer& rpc_writer,
      stream::Writer& writer,
      chrono::SystemClock::duration chunk_timeout = cfg::kDefaultChunkTimeout,
      uint8_t max_retries = cfg::kDefaultMaxRetries) {
    Initialize(kReceive,
               transfer_id,
               work_queue,
               rpc_writer,
               writer,
               chunk_timeout,
               max_retries);
  }

  // Calculates the maximum size of actual data that can be sent within a single
  // client write transfer chunk, accounting for the overhead of the transfer
  // protocol and RPC system.
  //
  // Note: This function relies on RPC protocol internals. This is generally a
  // *bad* idea, but is necessary here due to limitations of the RPC system and
  // its asymmetric ingress and egress paths.
  //
  // TODO(frolv): This should be investigated further and perhaps addressed
  // within the RPC system, at the least through a helper function.
  uint32_t MaxWriteChunkSize(uint32_t max_chunk_size_bytes,
                             uint32_t channel_id) const;

 private:
  enum TransmitAction : bool { kExtend, kRetransmit };

  void Initialize(Type type,
                  uint32_t transfer_id,
                  work_queue::WorkQueue& work_queue,
                  rpc::Writer& rpc_writer,
                  stream::Stream& stream,
                  chrono::SystemClock::duration chunk_timeout,
                  uint8_t max_retries);

  stream::Reader& reader() {
    PW_DASSERT(active() && type() == kTransmit);
    return static_cast<stream::Reader&>(*stream_);
  }

  stream::Writer& writer() {
    PW_DASSERT(active() && type() == kReceive);
    return static_cast<stream::Writer&>(*stream_);
  }

  // Sends the first chunk in a transmit transfer.
  Status SendInitialTransmitChunk();

  // Functions which extract relevant data from a chunk into the context.
  bool ReadTransmitChunk(const TransferParameters& max_parameters,
                         const Chunk& chunk);
  bool ReadReceiveChunk(ChunkDataBuffer& buffer,
                        const TransferParameters& max_parameters,
                        const Chunk& chunk);

  // Functions which handle the last received chunk.
  void ProcessTransmitChunk();
  void ProcessReceiveChunk(ChunkDataBuffer& buffer,
                           const TransferParameters& max_parameters);

  // In a transmit transfer, sends the next data chunk from the local stream.
  // Returns status indicating what to do next:
  //
  //    OK - continue
  //    OUT_OF_RANGE - done for now
  //    other errors - abort transfer with this error
  //
  Status SendNextDataChunk();

  // In a receive transfer, processes the fields from a data chunk and stages
  // the data for a deferred write. Returns true if there is a deferred
  // operation to complete.
  bool HandleDataChunk(ChunkDataBuffer& buffer,
                       const TransferParameters& max_parameters,
                       const Chunk& chunk);

  // In a receive transfer, sends a parameters chunk telling the transmitter how
  // much data they can send.
  Status SendTransferParameters(TransmitAction action);

  // Updates the current receive transfer parameters from the provided object,
  // then sends them.
  Status UpdateAndSendTransferParameters(
      const TransferParameters& max_parameters, TransmitAction action);

  void SendStatusChunk(Status status);
  void FinishAndSendStatus(Status status);

  void CancelTimer() {
    timer_.Cancel();
    retries_ = 0;
  }

  // Timeout function invoked from the timer context. This may occur in an
  // interrupt, so no real work can be done. Instead, sets state to timed out
  // and adds a job to run the timeout handler.
  void OnTimeout();

  // The acutal timeout handler, invoked from within the work queue.
  void HandleTimeout();

  static constexpr uint8_t kFlagsType = 1 << 0;
  static constexpr uint8_t kFlagsDataSent = 1 << 1;

  // TODO(frolv): Make this value configurable per transfer.
  static constexpr uint32_t kDefaultChunkDelayMicroseconds = 2000;

  uint32_t transfer_id_;
  uint8_t flags_;
  TransferState transfer_state_ PW_GUARDED_BY(state_lock_);
  uint8_t retries_;
  uint8_t max_retries_;

  sync::InterruptSpinLock state_lock_;

  stream::Stream* stream_;
  rpc::Writer* rpc_writer_;

  uint32_t offset_;
  uint32_t window_size_;
  uint32_t window_end_offset_;
  // TODO(pwbug/584): Remove pending_bytes in favor of window_end_offset.
  uint32_t pending_bytes_;
  uint32_t max_chunk_size_bytes_;

  union {
    Status status_;               // Used when state is kCompleted.
    uint32_t last_chunk_offset_;  // Used in states kData and kRecovery.
  };

  // Timer used to handle timeouts waiting for chunks.
  chrono::SystemTimer timer_;
  chrono::SystemClock::duration chunk_timeout_;
  work_queue::WorkQueue* work_queue_;

  CompletionFunction on_completion_;
};

}  // namespace pw::transfer::internal
