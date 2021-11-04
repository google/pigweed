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
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_transfer/internal/chunk.h"
#include "pw_transfer/internal/chunk_data_buffer.h"

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

 private:
  uint32_t pending_bytes_;
  uint32_t max_chunk_size_bytes_;
};

// TODO(pwbug/547): This is a temporary virtual base to unify write operations
// across RPC server and client contexts, with derived classes being implemented
// by the transfer server/client. This capability should be added directly to
// RPC by exposing a subset of the Call class.
class RawWriter {
 public:
  virtual ~RawWriter() = default;

  virtual uint32_t channel_id() const = 0;
  virtual ByteSpan PayloadBuffer() = 0;
  virtual void ReleaseBuffer() = 0;
  virtual Status Write(ConstByteSpan data) = 0;
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
  constexpr bool initialized() const { return state_ != kInactive; }

  // True if the transfer is active.
  constexpr bool active() const { return state_ >= kData; }

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
                     const Chunk& chunk) {
    if (type_ == kTransmit) {
      return ReadTransmitChunk(max_parameters, chunk);
    }
    return ReadReceiveChunk(buffer, max_parameters, chunk);
  }

  // Handles the chunk from the previous invocation of ReadChunkData(). This
  // operation is intended to be deferred, running from a different context than
  // the RPC callback in which the chunk was received.
  void ProcessChunk(ChunkDataBuffer& buffer,
                    const TransferParameters& max_parameters) {
    if (type_ == kTransmit) {
      ProcessTransmitChunk();
      return;
    }
    ProcessReceiveChunk(buffer, max_parameters);
  }

 protected:
  using CompletionFunction = Status (*)(Context&, Status);

  constexpr Context(CompletionFunction on_completion)
      : transfer_id_(0),
        state_(kInactive),
        type_(kTransmit),
        stream_(nullptr),
        rpc_writer_(nullptr),
        offset_(0),
        pending_bytes_(0),
        max_chunk_size_bytes_(std::numeric_limits<uint32_t>::max()),
        status_(Status::Unknown()),
        last_received_offset_(0),
        on_completion_(on_completion) {}

  enum State : uint8_t {
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
  };

  constexpr State state() const { return state_; }
  constexpr void set_state(State state) { state_ = state; }

  // Begins a new transmit transfer from this context.
  // Precontion: context is not active.
  void InitializeForTransmit(uint32_t transfer_id,
                             RawWriter& rpc_writer,
                             stream::Reader& reader) {
    Initialize(kTransmit, transfer_id, rpc_writer, reader);
  }

  // Begins a new receive transfer from this context.
  // Precontion: context is not active.
  void InitializeForReceive(uint32_t transfer_id,
                            RawWriter& rpc_writer,
                            stream::Writer& writer) {
    Initialize(kReceive, transfer_id, rpc_writer, writer);
  }

  constexpr Type type() const { return type_; }

  constexpr uint32_t offset() const { return offset_; }
  constexpr void set_offset(size_t offset) { offset_ = offset; }

  constexpr uint32_t pending_bytes() const { return pending_bytes_; }
  constexpr void set_pending_bytes(size_t pending_bytes) {
    pending_bytes_ = pending_bytes;
  }

  constexpr uint32_t max_chunk_size_bytes() const {
    return max_chunk_size_bytes_;
  }
  constexpr void set_max_chunk_size_bytes(size_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
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
  void Initialize(Type type,
                  uint32_t transfer_id,
                  RawWriter& rpc_writer,
                  stream::Stream& stream);

  stream::Reader& reader() const {
    PW_DASSERT(active() && type_ == kTransmit);
    return static_cast<stream::Reader&>(*stream_);
  }

  stream::Writer& writer() const {
    PW_DASSERT(active() && type_ == kReceive);
    return static_cast<stream::Writer&>(*stream_);
  }

  // Updates the context's current parameters based on the fields in a chunk.
  void UpdateParameters(const TransferParameters& max_parameters,
                        const Chunk& chunk);

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
  Status SendTransferParameters(const TransferParameters& max_parameters);

  void SendStatusChunk(Status status);

  void FinishAndSendStatus(Status status);

  uint32_t transfer_id_;
  State state_;
  Type type_;

  stream::Stream* stream_;
  RawWriter* rpc_writer_;

  size_t offset_;
  size_t pending_bytes_;
  size_t max_chunk_size_bytes_;

  Status status_;
  size_t last_received_offset_;

  CompletionFunction on_completion_;
};

}  // namespace pw::transfer::internal
