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

#include <array>
#include <cstdint>

#include "pw_allocator/allocator.h"
#include "pw_bytes/byte_builder.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_grpc/send_queue.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_string/string.h"
#include "pw_sync/inline_borrowable.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"

namespace pw::grpc {
namespace internal {

struct FrameHeader;
enum class Http2Error : uint32_t;

// Parameters of this implementation.
// RFC 9113 §5.1.2
inline constexpr uint32_t kMaxConcurrentStreams = 16;

// RFC 9113 §4.2 and §6.5.2
inline constexpr uint32_t kMaxFramePayloadSize = 16384;

// Limits on grpc message sizes. The length prefix includes the compressed byte
// and 32-bit length from Length-Prefixed-Message.
// See: https://github.com/grpc/grpc/blob/v1.60.x/doc/PROTOCOL-HTTP2.md.
inline constexpr uint32_t kMaxGrpcMessageSizeWithLengthPrefix =
    kMaxFramePayloadSize;
inline constexpr uint32_t kMaxGrpcMessageSize =
    kMaxGrpcMessageSizeWithLengthPrefix - 5;

}  // namespace internal

// RFC 9113 §5.1.1: Streams are identified by unsigned 31-bit integers.
using StreamId = uint32_t;

inline constexpr uint32_t kMaxMethodNameSize = 127;

// Implements a gRPC over HTTP2 server.
//
// Basic usage:
// * Provide a Connection::RequestCallbacks implementation that handles RPC
//   events.
// * Provide a readable/writeable stream object that will be used like a
//   socket over which the HTTP2 frames are read/written. When the underlying
//   stream should be closed, the provided connection_close_callback will be
//   called.
// * Drive the connection by calling ProcessConnectionPreface then ProcessFrame
//   in a loop while status is Ok on one thread.
// * RPC responses can be sent from any thread by calling
//   SendResponseMessage/SendResponseComplete. The SendQueue object will
//   handle concurrent access.
//
// One thread should be dedicated to driving reads (ProcessFrame calls), while
// another thread (implemented by SendQueue) handles all writes. Refer to
// the ConnectionThread class for an implementation of this.
//
// By default, each gRPC message must be entirely contained within a single
// HTTP2 DATA frame, as supporting fragmented messages requires buffering
// up to the maximum message size per stream. To support fragmented messages,
// provide a message_assembly_allocator, which will be used to allocate
// temporary storage for fragmented gRPC messages when required. If no
// allocator is provided, or allocation fails, the stream will be closed.
class Connection {
 public:
  // Callbacks invoked on requests from the client. Called on same thread as
  // ProcessFrame is being called on.
  class RequestCallbacks {
   public:
    virtual ~RequestCallbacks() = default;

    // Called on startup of connection.
    virtual void OnNewConnection() = 0;

    // Called on a new RPC. full_method_name is "<ServiceName>/<MethodName>".
    // This is guaranteed to be called before any other method with the same id.
    virtual Status OnNew(StreamId id,
                         InlineString<kMaxMethodNameSize> full_method_name) = 0;

    // Called on a new request message for an RPC. The `message` must not be
    // accessed after this method returns.
    //
    // Return an error status to cause the stream to be closed with RST_STREAM
    // frame.
    virtual Status OnMessage(StreamId id, ByteSpan message) = 0;

    // Called after the client has sent all request messages for an RPC.
    virtual void OnHalfClose(StreamId id) = 0;

    // Called when an RPC has been canceled.
    virtual void OnCancel(StreamId id) = 0;
  };

  Connection(stream::ReaderWriter& stream,
             SendQueue& send_queue,
             RequestCallbacks& callbacks,
             allocator::Allocator* message_assembly_allocator = nullptr);

  // Reads from stream and processes required connection preface frames. Should
  // be called before ProcessFrame(). Return OK if connection preface was found.
  Status ProcessConnectionPreface() {
    return reader_.ProcessConnectionPreface();
  }

  // Reads from stream and processes next frame on connection. Returns OK
  // as long as connection is open. Should be called from a single thread.
  Status ProcessFrame() { return reader_.ProcessFrame(); }

  // Sends a response message for an RPC. The `message` will not be accessed
  // after this method returns. Thread safe.
  //
  // Errors are:
  //
  // * NOT_FOUND if stream_id does not reference an active stream, including
  //   RPCs that have already completed and IDs that do not refer to any prior
  //   RPC.
  // * RESOURCE_EXHAUSTED if the flow control window is not large enough to send
  //   this RPC immediately. In this case, no response will be send.
  // * UNAVAILABLE if the connection is closed.
  Status SendResponseMessage(StreamId stream_id, pw::ConstByteSpan message) {
    return writer_.SendResponseMessage(stream_id, message);
  }

  // Completes an RPC with the given status code. Thread safe. Pigweed status
  // codes happen to align exactly with grpc status codes. Compare:
  // https://grpc.github.io/grpc/core/md_doc_statuscodes.html
  // https://pigweed.dev/pw_status/#quick-reference
  //
  // Errors are:
  //
  // * NOT_FOUND if stream_id does not reference an active stream, including
  //   RPCs that have already completed, or if stream_id does not refer to any
  //   prior RPC.
  // * UNAVAILABLE if the connection is closed.
  Status SendResponseComplete(StreamId stream_id, pw::Status response_code) {
    return writer_.SendResponseComplete(stream_id, response_code);
  }

 private:
  // RFC 9113 §6.9.2. Flow control windows are unsigned 31-bit numbers, but
  // because of the following requirement from §6.9.2, we track flow control
  // windows with signed integers. "A change to SETTINGS_INITIAL_WINDOW_SIZE can
  // cause the available space in a flow-control window to become negative. A
  // sender MUST track the negative flow-control window ..."
  static inline constexpr int32_t kDefaultInitialWindowSize = 65535;

  // From RFC 9113 §5.1, we use only the following states:
  // * idle, which have `id > last_stream_id_`
  // * open, which are in `streams_` with `half_closed = false`
  // * half-closed (remote), which are in `streams_` with `half_closed = true`
  //
  // Regarding other states:
  // * reserved is ignored because we do not sent PUSH_PROMISE
  // * half-closed (local) is merged into close, because once a grpc server has
  //   sent a response, the RPC is complete
  struct Stream {
    StreamId id;
    bool half_closed;
    bool started_response;
    int32_t send_window;

    // Fragmented gRPC message assembly, nullptr if not assembling a message.
    std::byte* assembly_buffer;
    union {
      struct {
        // Buffer for the length-prefix, if fragmented.
        std::array<std::byte, 5> prefix_buffer;
        // Bytes of the prefix received so far.
        uint8_t prefix_received;
      };
      struct {
        // Total length of the message.
        uint32_t message_length;
        // Length of the message received so far (during assembly).
        uint32_t message_received;
      };
    };

    void Reset() {
      id = 0;
      half_closed = false;
      started_response = false;
      send_window = 0;

      assembly_buffer = nullptr;
      message_length = 0;
      message_received = 0;
      prefix_received = 0;
    }
  };

  // Internal state is divided into what is needed for reading/writing/shared to
  // both.

  struct SharedState {
    pw::Result<std::reference_wrapper<Stream>> LookupStream(StreamId id);

    // Stream state
    std::array<Stream, internal::kMaxConcurrentStreams> streams{};
    int32_t connection_send_window = kDefaultInitialWindowSize;

    // Allocator for fragmented grpc message reassembly
    allocator::Allocator* message_assembly_allocator_;
  };

  class Writer {
   public:
    Writer(Connection& connection) : connection_(connection) {}

    Status SendResponseMessage(StreamId stream_id, pw::ConstByteSpan message);
    Status SendResponseComplete(StreamId stream_id, pw::Status response_code);

   private:
    Connection& connection_;
  };

  class Reader {
   public:
    Reader(Connection& connection, RequestCallbacks& callbacks)
        : connection_(connection), callbacks_(callbacks) {}

    Status ProcessConnectionPreface();
    Status ProcessFrame();

   private:
    pw::Status CreateStream(StreamId id);
    void CloseStream(Stream& stream);

    Status ProcessDataFrame(const internal::FrameHeader&);
    Status ProcessHeadersFrame(const internal::FrameHeader&);
    Status ProcessRstStreamFrame(const internal::FrameHeader&);
    Status ProcessSettingsFrame(const internal::FrameHeader&, bool send_ack);
    Status ProcessPingFrame(const internal::FrameHeader&);
    Status ProcessWindowUpdateFrame(const internal::FrameHeader&);
    Status ProcessIgnoredFrame(const internal::FrameHeader&);
    Result<ByteSpan> ReadFramePayload(const internal::FrameHeader&);

    // Send GOAWAY frame and signal connection should be closed.
    void SendGoAway(internal::Http2Error code);
    Status SendRstStreamAndClose(Stream& stream, internal::Http2Error code);

    Connection& connection_;
    RequestCallbacks& callbacks_;
    int32_t initial_send_window_ = kDefaultInitialWindowSize;
    bool received_connection_preface_ = false;

    std::array<std::byte, internal::kMaxFramePayloadSize> payload_scratch_{};
    StreamId last_stream_id_ = 0;
  };

  sync::BorrowedPointer<SharedState> LockState() {
    return shared_state_.acquire();
  }

  void UnlockState(sync::BorrowedPointer<SharedState>&& state) {
    sync::BorrowedPointer<SharedState> moved_state = std::move(state);
    static_cast<void>(moved_state);
  }

  // Shared state that is thread-safe.
  stream::ReaderWriter& socket_;
  SendQueue& send_queue_;

  sync::InlineBorrowable<SharedState> shared_state_;
  Reader reader_;
  Writer writer_;
};

class ConnectionThread : public Connection, public thread::ThreadCore {
 public:
  // The ConnectionCloseCallback will be called when this thread is shutting
  // down and all data has finished sending. It will be called from this
  // ConnectionThread.
  using ConnectionCloseCallback = Function<void()>;

  ConnectionThread(stream::NonSeekableReaderWriter& stream,
                   const thread::Options& send_thread_options,
                   RequestCallbacks& callbacks,
                   ConnectionCloseCallback&& connection_close_callback,
                   allocator::Allocator* message_assembly_allocator = nullptr)
      : Connection(stream, send_queue_, callbacks, message_assembly_allocator),
        send_queue_(stream),
        send_queue_thread_options_(send_thread_options),
        connection_close_callback_(std::move(connection_close_callback)) {}

  // Process the connection. Does not return until the connection is closed.
  void Run() override {
    thread::Thread send_thread(send_queue_thread_options_, send_queue_);
    Status status = ProcessConnectionPreface();
    while (status.ok()) {
      status = ProcessFrame();
    }
    send_queue_.RequestStop();
    send_thread.join();
    if (connection_close_callback_) {
      connection_close_callback_();
    }
  };

 private:
  SendQueue send_queue_;
  const thread::Options& send_queue_thread_options_;
  ConnectionCloseCallback connection_close_callback_;
};

}  // namespace pw::grpc
