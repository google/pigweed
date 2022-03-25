// Copyright 2022 The Pigweed Authors
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
#include <optional>

#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_rpc/writer.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_transfer/internal/chunk.h"
#include "pw_transfer/internal/event.h"
#include "pw_transfer/rate_estimate.h"

namespace pw::transfer::internal {

class TransferThread;

class TransferParameters {
 public:
  constexpr TransferParameters(uint32_t pending_bytes,
                               uint32_t max_chunk_size_bytes,
                               uint32_t extend_window_divisor)
      : pending_bytes_(pending_bytes),
        max_chunk_size_bytes_(max_chunk_size_bytes),
        extend_window_divisor_(extend_window_divisor) {
    PW_ASSERT(pending_bytes > 0);
    PW_ASSERT(max_chunk_size_bytes > 0);
    PW_ASSERT(extend_window_divisor > 1);
  }

  uint32_t pending_bytes() const { return pending_bytes_; }
  void set_pending_bytes(uint32_t pending_bytes) {
    pending_bytes_ = pending_bytes;
  }

  uint32_t max_chunk_size_bytes() const { return max_chunk_size_bytes_; }
  void set_max_chunk_size_bytes(uint32_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
  }

  uint32_t extend_window_divisor() const { return extend_window_divisor_; }
  void set_extend_window_divisor(uint32_t extend_window_divisor) {
    PW_DASSERT(extend_window_divisor > 1);
    extend_window_divisor_ = extend_window_divisor;
  }

 private:
  uint32_t pending_bytes_;
  uint32_t max_chunk_size_bytes_;
  uint32_t extend_window_divisor_;
};

// Information about a single transfer.
class Context {
 public:
  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;

  constexpr uint32_t transfer_id() const { return transfer_id_; }

  // True if the context has been used for a transfer (it has an ID).
  bool initialized() const {
    return transfer_state_ != TransferState::kInactive;
  }

  // True if the transfer is active.
  bool active() const { return transfer_state_ >= TransferState::kWaiting; }

  std::optional<chrono::SystemClock::time_point> timeout() const {
    return active() && next_timeout_ != kNoTimeout
               ? std::optional(next_timeout_)
               : std::nullopt;
  }

  // Returns true if the transfer's most recently set timeout has passed.
  bool timed_out() const {
    std::optional<chrono::SystemClock::time_point> next_timeout = timeout();
    return next_timeout.has_value() &&
           chrono::SystemClock::now() >= next_timeout.value();
  }

  // Processes an event for this transfer.
  void HandleEvent(const Event& event);

 protected:
  ~Context() = default;

  constexpr Context()
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
        max_parameters_(nullptr),
        thread_(nullptr),
        last_chunk_offset_(0),
        chunk_timeout_(chrono::SystemClock::duration::zero()),
        interchunk_delay_(chrono::SystemClock::for_at_least(
            std::chrono::microseconds(kDefaultChunkDelayMicroseconds))),
        next_timeout_(kNoTimeout) {}

  constexpr TransferType type() const {
    return static_cast<TransferType>(flags_ & kFlagsType);
  }

 private:
  enum class TransferState : uint8_t {
    // This ServerContext has never been used for a transfer. It is available
    // for use for a transfer.
    kInactive,
    // A transfer completed and the final status chunk was sent. The Context
    // is
    // available for use for a new transfer. A receive transfer uses this
    // state
    // to allow a transmitter to retry its last chunk if the final status
    // chunk
    // was dropped.
    kCompleted,
    // Waiting for the other end to send a chunk.
    kWaiting,
    // Transmitting a window of data to a receiver.
    kTransmitting,
    // Recovering after one or more chunks was dropped in an active transfer.
    kRecovery,
  };

  enum class TransmitAction {
    // Start of a new transfer.
    kBegin,
    // Extend the current window length.
    kExtend,
    // Retransmit from a specified offset.
    kRetransmit,
  };

  void set_transfer_state(TransferState state) { transfer_state_ = state; }

  // The transfer ID as unsigned instead of uint32_t so it can be used with %u.
  unsigned id_for_log() const {
    static_assert(sizeof(unsigned) >= sizeof(transfer_id_));
    return static_cast<unsigned>(transfer_id_);
  }

  stream::Reader& reader() {
    PW_DASSERT(active() && type() == TransferType::kTransmit);
    return static_cast<stream::Reader&>(*stream_);
  }

  stream::Writer& writer() {
    PW_DASSERT(active() && type() == TransferType::kReceive);
    return static_cast<stream::Writer&>(*stream_);
  }

  // Calculates the maximum size of actual data that can be sent within a
  // single client write transfer chunk, accounting for the overhead of the
  // transfer protocol and RPC system.
  //
  // Note: This function relies on RPC protocol internals. This is generally a
  // *bad* idea, but is necessary here due to limitations of the RPC system
  // and its asymmetric ingress and egress paths.
  //
  // TODO(frolv): This should be investigated further and perhaps addressed
  // within the RPC system, at the least through a helper function.
  uint32_t MaxWriteChunkSize(uint32_t max_chunk_size_bytes,
                             uint32_t channel_id) const;

  // Initializes a new transfer using new_transfer. The provided stream
  // argument is used in place of the NewTransferEvent's stream. Only
  // initializes state; no packets are sent.
  //
  // Precondition: context is not active.
  void Initialize(const NewTransferEvent& new_transfer);

  // Starts a new transfer from an initialized context by sending the initial
  // transfer chunk. This is only used by transfer clients, as the transfer
  // service cannot initiate transfers.
  //
  // Calls Finish(), which calls the on_completion callback, if initiating a
  // transfer fails.
  void InitiateTransferAsClient();

  // Starts a new transfer on the server after receiving a request from a
  // client.
  void StartTransferAsServer(const NewTransferEvent& new_transfer);

  // Does final cleanup specific to the server or client. Returns whether the
  // cleanup succeeded. An error in cleanup indicates that the transfer
  // failed.
  virtual Status FinalCleanup(Status status) = 0;

  // Processes a chunk in either a transfer or receive transfer.
  void HandleChunkEvent(const ChunkEvent& event);

  // Processes a chunk in a transmit transfer.
  void HandleTransmitChunk(const Chunk& chunk);

  // Processes a transfer parameters update in a transmit transfer.
  void HandleTransferParametersUpdate(const Chunk& chunk);

  // Sends the next chunk in a transmit transfer, if any.
  void TransmitNextChunk(bool retransmit_requested);

  // Processes a chunk in a receive transfer.
  void HandleReceiveChunk(const Chunk& chunk);

  // Processes a data chunk in a received while in the kWaiting state.
  void HandleReceivedData(const Chunk& chunk);

  // Sends the first chunk in a transmit transfer.
  void SendInitialTransmitChunk();

  // In a receive transfer, sends a parameters chunk telling the transmitter
  // how much data they can send.
  void SendTransferParameters(TransmitAction action);

  // Updates the current receive transfer parameters from the provided object,
  // then sends them.
  void UpdateAndSendTransferParameters(TransmitAction action);

  // Sends a final status chunk of a completed transfer without updating the
  // the transfer. Sends status_, which MUST have been set by a previous
  // Finish() call.
  void SendFinalStatusChunk();

  // Marks the transfer as completed and calls FinalCleanup(). Sets status_ to
  // the final status for this transfer. The transfer MUST be active when this
  // is called.
  void Finish(Status status);

  // Encodes the specified chunk to the encode buffer and sends it with the
  // rpc_writer_. Calls Finish() with an error if the operation fails.
  void EncodeAndSendChunk(const Chunk& chunk);

  void SetTimeout(chrono::SystemClock::duration timeout);
  void ClearTimeout() { next_timeout_ = kNoTimeout; }

  // Called when the transfer's timeout expires.
  void HandleTimeout();

  // Resends the last packet or aborts the transfer if the maximum retries has
  // been exceeded.
  void Retry();

  static constexpr uint8_t kFlagsType = 1 << 0;
  static constexpr uint8_t kFlagsDataSent = 1 << 1;

  static constexpr uint32_t kDefaultChunkDelayMicroseconds = 2000;

  // How long to wait for the other side to ACK a final transfer chunk before
  // resetting the context so that it can be reused. During this time, the
  // status chunk will be re-sent for every non-ACK chunk received,
  // continually notifying the other end that the transfer is over.
  static constexpr chrono::SystemClock::duration kFinalChunkAckTimeout =
      std::chrono::milliseconds(5000);

  static constexpr chrono::SystemClock::time_point kNoTimeout =
      chrono::SystemClock::time_point(chrono::SystemClock::duration(0));

  uint32_t transfer_id_;
  uint8_t flags_;
  TransferState transfer_state_;
  uint8_t retries_;
  uint8_t max_retries_;

  // The stream from which to read or to which to write data.
  stream::Stream* stream_;
  rpc::Writer* rpc_writer_;

  uint32_t offset_;
  uint32_t window_size_;
  uint32_t window_end_offset_;
  // TODO(pwbug/584): Remove pending_bytes in favor of window_end_offset.
  uint32_t pending_bytes_;
  uint32_t max_chunk_size_bytes_;

  const TransferParameters* max_parameters_;
  TransferThread* thread_;

  union {
    Status status_;               // Used when state is kCompleted.
    uint32_t last_chunk_offset_;  // Used in states kWaiting and kRecovery.
  };

  // How long to wait for a chunk from the other end.
  chrono::SystemClock::duration chunk_timeout_;

  // How long to delay between transmitting subsequent data chunks within a
  // window.
  chrono::SystemClock::duration interchunk_delay_;

  // Timestamp at which the transfer will next time out, or kNoTimeout.
  chrono::SystemClock::time_point next_timeout_;

  RateEstimate transfer_rate_;
};

}  // namespace pw::transfer::internal
