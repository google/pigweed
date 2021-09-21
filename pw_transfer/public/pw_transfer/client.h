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

#include <array>

#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_transfer/internal/client_context.h"
#include "pw_transfer/transfer.raw_rpc.pb.h"
#include "pw_work_queue/work_queue.h"

namespace pw::transfer {

// pw_rpc does not yet support raw_rpc clients. These classes are faked for the
// purpose of implementing the core transfer client design. Once the required
// pieces are added to pw_rpc, the fakes will be replaced.
namespace fake {

class ClientReaderWriter {
 public:
  bool active() const { return false; }
  uint32_t channel_id() const { return 42; }

  ByteSpan PayloadBuffer() { return {}; }
  void ReleaseBuffer() {}
  Status Write(ConstByteSpan) { return OkStatus(); }
};

class TransferClient {
 public:
  constexpr TransferClient(rpc::Client&, int) {}

  ClientReaderWriter Read(Function<void(ConstByteSpan)>) {
    return ClientReaderWriter();
  }

  ClientReaderWriter Write(Function<void(ConstByteSpan)>) {
    return ClientReaderWriter();
  }
};

}  // namespace fake

class Client {
 public:
  using CompletionFunc = Function<void(Status)>;

  // Initializes a transfer client on a specified RPC client and channel.
  // Transfers are processed on a work queue so as not to block any RPC threads.
  // The work queue does not have to be unique to the transfer client; it can be
  // shared with other modules (including additional transfer clients).
  //
  // As data is processed within the work queue's context, the original RPC
  // messages received by the transfer service are not available. Therefore,
  // the transfer client requires an additional buffer where transfer data can
  // stored during the context switch. This buffer must be at least large enough
  // to fit one chunk (see below).
  //
  // max_chunk_size_bytes is the largest amount of data that can be sent within
  // a single transfer chunk (read or write), excluding any transport layer
  // overhead. Not all of this size is used to send data -- there is additional
  // overhead in the pw_rpc and pw_transfer protocols (typically ~22B/chunk).
  Client(rpc::Client& rpc_client,
         uint32_t channel_id,
         work_queue::WorkQueue& work_queue,
         ByteSpan transfer_data_buffer,
         size_t max_chunk_size_bytes)
      : client_(rpc_client, channel_id),
        work_queue_(work_queue),
        transfer_data_buffer_(transfer_data_buffer),
        transfer_data_size_(0),
        max_chunk_size_bytes_(max_chunk_size_bytes) {}

  // Begins a new read transfer for the given transfer ID. The data read from
  // the server is written to the provided writer. Returns OK if the transfer is
  // successfully started. When the transfer finishes (successfully or not), the
  // completion callback is invoked with the final status.
  Status Read(uint32_t transfer_id,
              stream::Writer& output,
              CompletionFunc on_completion);

  // Begins a new write transfer for the given transfer ID.
  Status Write(uint32_t transfer_id,
               stream::SeekableReader& input,
               CompletionFunc on_completion);

 private:
  using ClientContext = internal::ClientContext;

  Result<ClientContext*> NewTransfer(uint32_t transfer_id,
                                     stream::Stream& stream,
                                     Function<void(Status)> on_completion,
                                     bool write);
  ClientContext* GetActiveTransfer(uint32_t transfer_id);

  Status StartReadTransfer(ClientContext& ctx);
  Status StartWriteTransfer(ClientContext& ctx);

  // Functions called when a chunk is received, from the context of the RPC
  // client thread.
  void OnReadChunk(ConstByteSpan data);
  void OnWriteChunk(ConstByteSpan data);

  // Handler functions called from within the work queue. At this point, the
  // original chunk data from the RPC callback is no longer valid. Relevant
  // fields have been extracted into the Context and Client objects.
  //
  // To process a chunk, the client needs two things: a `this` pointer, and the
  // ClientContext to which the chunk belongs. These must be forwarded to the
  // work queue function that runs the handler. Unfortunately, in its default
  // configuration, a pw::Function only has space for one pointer, making this
  // context capture impossible with a lambda. A static invoker is used to work
  // around this limitation: the work function captures the transfer context,
  // which internally stores a client pointer used to invoke the Handle* member
  // function.
  static void InvokeHandleWriteChunk(ClientContext& ctx) {
    ctx.client().HandleWriteChunk(ctx);
  }
  static void InvokeHandleReadChunk(ClientContext& ctx) {
    ctx.client().HandleReadChunk(ctx);
  }

  void HandleWriteChunk(ClientContext& ctx);
  void HandleReadChunk(ClientContext& ctx);

  void SendStatusChunk(fake::ClientReaderWriter& stream,
                       uint32_t transfer_id,
                       Status status);
  Status SendTransferParameters(ClientContext& ctx);

  fake::TransferClient client_;
  work_queue::WorkQueue& work_queue_;

  fake::ClientReaderWriter read_stream_;
  fake::ClientReaderWriter write_stream_;

  // TODO(frolv): Make this size configurable.
  std::array<ClientContext, 1> transfer_contexts_
      PW_GUARDED_BY(transfer_context_mutex_);
  sync::Mutex transfer_context_mutex_;

  ByteSpan transfer_data_buffer_;
  size_t transfer_data_size_;

  size_t max_chunk_size_bytes_;
};

}  // namespace pw::transfer
