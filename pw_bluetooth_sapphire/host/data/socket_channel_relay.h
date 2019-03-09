// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_SOCKET_CHANNEL_RELAY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_SOCKET_CHANNEL_RELAY_H_

#include <deque>

#include <fbl/ref_ptr.h>
#include <lib/async/cpp/wait.h>
#include <lib/fit/function.h>
#include <zircon/status.h>

#include "lib/fxl/macros.h"
#include "lib/fxl/memory/weak_ptr.h"
#include "lib/fxl/synchronization/thread_checker.h"
#include "lib/zx/socket.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace btlib::data::internal {

// SocketChannelRelay relays data between a zx::socket and a Channel. This class
// should not be used directly. Instead, see SocketFactory.
//
// THREAD-SAFETY: This class is thread-hostile. Creation, use, and destruction
// _must_ occur on a single thread. |dispatcher|, which _must_ be
// single-threaded, must run on that same thread.
template <typename ChannelT>
class SocketChannelRelay final {
 public:
  using DeactivationCallback = fit::function<void()>;

  // The kernel allows up to ~256 KB in a socket buffer, which is enough for
  // about 1 second of data at 2 Mbps. Until we have a use case that requires
  // more than 1 second of buffering, we allow only a small amount of buffering
  // within SocketChannelRelay itself.
  static constexpr size_t kDefaultSocketWriteQueueLimitFrames = 2;

  // Creates a SocketChannelRelay which executes on |dispatcher|. Note that
  // |dispatcher| must be single-threaded.
  //
  // The relay works with SocketFactory to manage the relay's lifetime. On any
  // of the "terminal events" (see below), the relay will invoke the
  // DeactivationCallback. On invocation of the DeactivationCallback, the
  // SocketFactory should destroy the relay. The destruction should be done
  // synchronously, as a) destruction must happen on |dispatcher|'s thread, and
  // b) the |dispatcher| may be shutting down.
  //
  // The terminal events are:
  // * the zx::socket is closed
  // * the Channel is closed
  // * the dispatcher begins shutting down
  //
  // Note that requiring |dispatcher| to be single-threaded shouldn't cause
  // increased latency vs. multi-threading, since a) all I/O is non-blocking (so
  // we never leave the thread idle), and b) to provide in-order delivery,
  // moving the data between the zx::socket and the ChannelT needs to be
  // serialized even in the multi-threaded case.
  SocketChannelRelay(zx::socket socket, fbl::RefPtr<ChannelT> channel,
                     DeactivationCallback deactivation_cb,
                     size_t socket_write_queue_max_frames =
                         kDefaultSocketWriteQueueLimitFrames);
  ~SocketChannelRelay();

  // Enables read and close callbacks for the zx::socket and the
  // ChannelT. (Write callbacks aren't necessary until we have data
  // buffered.) Returns true on success.
  //
  // Activate() is guaranteed _not_ to invoke |deactivation_cb|, even in the
  // event of failure. Instead, in the failure case, the caller should dispose
  // of |this| directly.
  __WARN_UNUSED_RESULT bool Activate();

 private:
  enum class RelayState {
    kActivating,
    kActivated,
    kDeactivating,
    kDeactivated,
  };

  // Deactivates and unbinds all callbacks from the zx::socket and the
  // ChannelT. Drops any data still queued for transmission to the
  // zx::socket. Ensures that the zx::socket is closed, and the ChannelT
  // is deactivated. It is an error to call this when |state_ == kDeactivated|.
  // ChannelT.
  //
  // Note that Deactivate() _may_ be called from the dtor. As such, this method
  // avoids doing any "real work" (such as calling ServiceSocketWriteQueue()),
  // and constrains itself to just tearing things down.
  void Deactivate();

  // Deactivates |this|, and invokes deactivation_cb_.
  // It is an error to call this when |state_ == kDeactivated|.
  void DeactivateAndRequestDestruction();

  // Callbacks for zx::socket events.
  void OnSocketReadable(zx_status_t status);
  void OnSocketWritable(zx_status_t status);
  void OnSocketClosed(zx_status_t status);

  // Callbacks for ChannelT events.
  void OnChannelDataReceived(common::ByteBufferPtr sdu);
  void OnChannelClosed();

  // Copies any data currently available on |socket_| to |channel_|. Does not
  // block for data on |socket_|, and does not retry failed writes to
  // |channel_|. Returns true if we should attempt to read from this socket
  // again, and false otherwise.
  __WARN_UNUSED_RESULT bool CopyFromSocketToChannel();

  // Copies any data pending in |socket_write_queue_| to |socket_|.
  void ServiceSocketWriteQueue();

  // Binds an async::Wait to a |handler|, but does not enable the wait.
  // The handler will be wrapped in code that verifies that |this| has not begun
  // destruction.
  void BindWait(zx_signals_t trigger, const char* wait_name, async::Wait* wait,
                fit::function<void(zx_status_t)> handler);

  // Begins waiting on |wait|. Returns true on success.
  // Note that it is safe to BeginWait() even after a socket operation has
  // returned ZX_ERR_PEER_CLOSED. This is because "if the handle is closed, the
  // operation will ... be terminated". (See zx_object_wait_async().)
  bool BeginWait(const char* wait_name, async::Wait* wait);

  // Clears |wait|'s handler, and cancels |wait|.
  void UnbindAndCancelWait(async::Wait* wait);

  RelayState state_;  // Initial state is kActivating.

  zx::socket socket_;
  const fbl::RefPtr<ChannelT> channel_;
  async_dispatcher_t* const dispatcher_;
  DeactivationCallback deactivation_cb_;
  const size_t socket_write_queue_max_frames_;

  async::Wait sock_read_waiter_;
  async::Wait sock_write_waiter_;
  async::Wait sock_close_waiter_;

  // We use a std::deque here to minimize the number dynamic memory
  // allocations (cf. std::list, which would require allocation on each
  // SDU). This comes, however, at the cost of higher memory usage when the
  // number of SDUs is small. (libc++ uses a minimum of 4KB per deque.)
  //
  // TODO(NET-1478): Switch to common::LinkedList.
  // TODO(NET-1476): We should set an upper bound on the size of this queue.
  std::deque<common::ByteBufferPtr> socket_write_queue_;

  const fxl::ThreadChecker thread_checker_;
  fxl::WeakPtrFactory<SocketChannelRelay> weak_ptr_factory_;  // Keep last.

  FXL_DISALLOW_COPY_AND_ASSIGN(SocketChannelRelay);
};

}  // namespace btlib::data::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_SOCKET_CHANNEL_RELAY_H_
