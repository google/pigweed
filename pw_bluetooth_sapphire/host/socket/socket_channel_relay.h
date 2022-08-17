// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_CHANNEL_RELAY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_CHANNEL_RELAY_H_

#include <lib/async/cpp/wait.h>
#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <zircon/status.h>

#include <deque>
#include <utility>

#include "lib/zx/socket.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::socket {

// SocketChannelRelay relays data between a zx::socket and a Channel. This class
// should not be used directly. Instead, see SocketFactory.
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
  SocketChannelRelay(zx::socket socket, fxl::WeakPtr<ChannelT> channel,
                     DeactivationCallback deactivation_cb,
                     size_t socket_write_queue_max_frames = kDefaultSocketWriteQueueLimitFrames);
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
  void OnChannelDataReceived(ByteBufferPtr sdu);
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

  // Get a trace_flow_id given a counter. Used to make sure trace flows are unique while they are
  // active.
  // Composed of channel UniqueId and the given counter, with the unique_id taking the top 32 bits.
  trace_flow_id_t GetTraceId(uint32_t id);

  RelayState state_;  // Initial state is kActivating.

  zx::socket socket_;
  const fxl::WeakPtr<ChannelT> channel_;
  async_dispatcher_t* const dispatcher_;
  DeactivationCallback deactivation_cb_;
  const size_t socket_write_queue_max_frames_;

  async::Wait sock_read_waiter_;
  async::Wait sock_write_waiter_;
  async::Wait sock_close_waiter_;

  // Count of packets received from the peer on the channel.
  uint32_t channel_rx_packet_count_;
  // Count of packets sent to the peer on the channel.
  uint32_t channel_tx_packet_count_;
  // Count of packets sent to the socket (should match |channel_rx_packet_count_| normally)
  uint32_t socket_packet_sent_count_;
  // Count of packets received from the socket (should match |channel_Tx_packet_count_| normally)
  uint32_t socket_packet_recv_count_;

  // We use a std::deque here to minimize the number dynamic memory
  // allocations (cf. std::list, which would require allocation on each
  // SDU). This comes, however, at the cost of higher memory usage when the
  // number of SDUs is small. (libc++ uses a minimum of 4KB per deque.)
  //
  // TODO(fxbug.dev/709): We should set an upper bound on the size of this queue.
  std::deque<ByteBufferPtr> socket_write_queue_;

  fxl::WeakPtrFactory<SocketChannelRelay> weak_ptr_factory_;  // Keep last.

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SocketChannelRelay);
};

template <typename ChannelT>
SocketChannelRelay<ChannelT>::SocketChannelRelay(zx::socket socket, fxl::WeakPtr<ChannelT> channel,
                                                 DeactivationCallback deactivation_cb,
                                                 size_t socket_write_queue_max_frames)
    : state_(RelayState::kActivating),
      socket_(std::move(socket)),
      channel_(std::move(channel)),
      dispatcher_(async_get_default_dispatcher()),
      deactivation_cb_(std::move(deactivation_cb)),
      socket_write_queue_max_frames_(socket_write_queue_max_frames),
      channel_rx_packet_count_(0u),
      channel_tx_packet_count_(0u),
      socket_packet_sent_count_(0u),
      socket_packet_recv_count_(0u),
      weak_ptr_factory_(this) {
  BT_ASSERT(dispatcher_);
  BT_ASSERT(socket_);
  BT_ASSERT(channel_);

  // Note: binding |this| is safe, as BindWait() wraps the bound method inside
  // of a lambda which verifies that |this| hasn't been destroyed.
  BindWait(ZX_SOCKET_READABLE, "socket read waiter", &sock_read_waiter_,
           fit::bind_member<&SocketChannelRelay::OnSocketReadable>(this));
  BindWait(ZX_SOCKET_WRITE_THRESHOLD, "socket write waiter", &sock_write_waiter_,
           fit::bind_member<&SocketChannelRelay::OnSocketWritable>(this));
  BindWait(ZX_SOCKET_PEER_CLOSED, "socket close waiter", &sock_close_waiter_,
           fit::bind_member<&SocketChannelRelay::OnSocketClosed>(this));
}

template <typename ChannelT>
SocketChannelRelay<ChannelT>::~SocketChannelRelay() {
  if (state_ != RelayState::kDeactivated) {
    bt_log(TRACE, "l2cap", "Deactivating relay for channel %u in dtor", channel_->id());
    Deactivate();
  }
}

template <typename ChannelT>
bool SocketChannelRelay<ChannelT>::Activate() {
  BT_ASSERT(state_ == RelayState::kActivating);

  // Note: we assume that BeginWait() does not synchronously dispatch any
  // events. The wait handler will assert otherwise.
  if (!BeginWait("socket close waiter", &sock_close_waiter_)) {
    // Perhaps |dispatcher| is already stopped.
    return false;
  }

  if (!BeginWait("socket read waiter", &sock_read_waiter_)) {
    // Perhaps |dispatcher| is already stopped.
    return false;
  }

  const auto self = weak_ptr_factory_.GetWeakPtr();
  const auto channel_id = channel_->id();
  const bool activate_success = channel_->Activate(
      [self, channel_id](ByteBufferPtr rx_data) {
        // Note: this lambda _may_ be invoked immediately for buffered packets.
        if (self) {
          self->OnChannelDataReceived(std::move(rx_data));
        } else {
          bt_log(TRACE, "l2cap", "Ignoring data received on destroyed relay (channel_id=%#.4x)",
                 channel_id);
        }
      },
      [self, channel_id] {
        if (self) {
          self->OnChannelClosed();
        } else {
          bt_log(TRACE, "l2cap", "Ignoring channel closure on destroyed relay (channel_id=%#.4x)",
                 channel_id);
        }
      });
  if (!activate_success) {
    return false;
  }

  state_ = RelayState::kActivated;
  return true;
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::Deactivate() {
  BT_ASSERT(state_ != RelayState::kDeactivated);

  state_ = RelayState::kDeactivating;
  if (!socket_write_queue_.empty()) {
    bt_log(DEBUG, "l2cap", "Dropping %zu packets from channel %u due to channel closure",
           socket_write_queue_.size(), channel_->id());
    socket_write_queue_.clear();
  }
  channel_->Deactivate();

  // We assume that UnbindAndCancelWait() will not trigger a re-entrant call
  // into Deactivate(). And the RelayIsDestroyedWhenDispatcherIsShutDown test
  // verifies that to be the case. (If we had re-entrant calls, a
  // BT_ASSERT() in the lambda bound by BindWait() would cause an abort.)
  UnbindAndCancelWait(&sock_read_waiter_);
  UnbindAndCancelWait(&sock_write_waiter_);
  UnbindAndCancelWait(&sock_close_waiter_);
  socket_.reset();

  // Any further callbacks are bugs. Update state_, to help us detect
  // those bugs.
  state_ = RelayState::kDeactivated;
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::DeactivateAndRequestDestruction() {
  Deactivate();
  if (deactivation_cb_) {
    // NOTE: deactivation_cb_ is expected to destroy |this|. Since |this|
    // owns deactivation_cb_, we move() deactivation_cb_ outside of |this|
    // before invoking the callback.
    auto moved_deactivation_cb = std::move(deactivation_cb_);
    moved_deactivation_cb();
  }
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::OnSocketReadable(zx_status_t status) {
  BT_ASSERT(state_ == RelayState::kActivated);
  if (!CopyFromSocketToChannel() || !BeginWait("socket read waiter", &sock_read_waiter_)) {
    DeactivateAndRequestDestruction();
  }
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::OnSocketWritable(zx_status_t status) {
  BT_ASSERT(state_ == RelayState::kActivated);
  BT_ASSERT(!socket_write_queue_.empty());
  ServiceSocketWriteQueue();
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::OnSocketClosed(zx_status_t status) {
  BT_ASSERT(state_ == RelayState::kActivated);
  DeactivateAndRequestDestruction();
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::OnChannelDataReceived(ByteBufferPtr rx_data) {
  // Note: kActivating is deliberately permitted, as ChannelImpl::Activate()
  // will synchronously deliver any queued frames.
  BT_ASSERT(state_ != RelayState::kDeactivated);
  TRACE_DURATION("bluetooth", "SocketChannelRelay::OnChannelDataReceived", "channel id",
                 channel_->id());

  if (state_ == RelayState::kDeactivating) {
    bt_log(DEBUG, "l2cap", "Ignoring %s on socket for channel %u while deactivating", __func__,
           channel_->id());
    return;
  }

  BT_ASSERT(rx_data);
  if (rx_data->size() == 0) {
    bt_log(DEBUG, "l2cap", "Ignoring empty %s on socket on channel %u", __func__, channel_->id());
    return;
  }

  BT_ASSERT(socket_write_queue_.size() <= socket_write_queue_max_frames_);
  // On a full queue, we drop the oldest element, on the theory that newer data
  // is more useful. This should be true, e.g., for real-time applications such
  // as voice calls. In the future, we may want to make the drop-head vs.
  // drop-tail choice configurable.
  if (socket_write_queue_.size() == socket_write_queue_max_frames_) {
    // TODO(fxbug.dev/1325): Add a metric for number of dropped frames.
    socket_write_queue_.pop_front();
  }

  channel_rx_packet_count_++;
  TRACE_FLOW_BEGIN("bluetooth", "SocketChannelRelay::OnChannelDataReceived queued",
                   GetTraceId(channel_rx_packet_count_));
  socket_write_queue_.push_back(std::move(rx_data));
  ServiceSocketWriteQueue();
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::OnChannelClosed() {
  BT_ASSERT(state_ != RelayState::kActivating);
  BT_ASSERT(state_ != RelayState::kDeactivated);

  if (state_ == RelayState::kDeactivating) {
    bt_log(DEBUG, "l2cap", "Ignorning %s on socket for channel %u while deactivating", __func__,
           channel_->id());
    return;
  }

  BT_ASSERT(state_ == RelayState::kActivated);
  if (!socket_write_queue_.empty()) {
    ServiceSocketWriteQueue();
  }
  DeactivateAndRequestDestruction();
}

template <typename ChannelT>
bool SocketChannelRelay<ChannelT>::CopyFromSocketToChannel() {
  // Subtle: we make the read buffer larger than the TX MTU, so that we can
  // detect truncated datagrams.
  const size_t read_buf_size = channel_->max_tx_sdu_size() + 1;

  // TODO(fxbug.dev/735): Consider yielding occasionally. As-is, we run the risk of
  // starving other SocketChannelRelays on the same |dispatcher| (and anyone
  // else on |dispatcher|), if a misbehaving process spams its zx::socket. And
  // even if starvation isn't an issue, latency/jitter might be.
  zx_status_t read_res;
  uint8_t read_buf[read_buf_size];
  do {
    size_t n_bytes_read = 0;
    read_res = socket_.read(0, read_buf, read_buf_size, &n_bytes_read);
    BT_ASSERT_MSG(
        read_res == ZX_OK || read_res == ZX_ERR_SHOULD_WAIT || read_res == ZX_ERR_PEER_CLOSED, "%s",
        zx_status_get_string(read_res));
    BT_ASSERT_MSG(n_bytes_read <= read_buf_size, "(n_bytes_read=%zu, read_buf_size=%zu)",
                  n_bytes_read, read_buf_size);
    if (read_res == ZX_ERR_SHOULD_WAIT) {
      return true;
    }

    if (read_res == ZX_ERR_PEER_CLOSED) {
      bt_log(TRACE, "l2cap", "Failed to read from socket for channel %u: %s", channel_->id(),
             zx_status_get_string(read_res));
      return false;
    }

    BT_ASSERT(n_bytes_read > 0);
    socket_packet_recv_count_++;
    if (n_bytes_read > channel_->max_tx_sdu_size()) {
      bt_log(TRACE, "l2cap", "Dropping %zu bytes for channel %u as max TX SDU is %u ", n_bytes_read,
             channel_->id(), channel_->max_tx_sdu_size());
      return false;
    }

    // TODO(fxbug.dev/734): For low latency and low jitter, IWBN to avoid allocating
    // dynamic memory on every read.
    bool write_success =
        channel_->Send(std::make_unique<DynamicByteBuffer>(BufferView(read_buf, n_bytes_read)));
    if (!write_success) {
      bt_log(TRACE, "l2cap", "Failed to write %zu bytes to channel %u", n_bytes_read,
             channel_->id());
    }
    channel_tx_packet_count_++;
  } while (read_res == ZX_OK);

  return true;
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::ServiceSocketWriteQueue() {
  // TODO(fxbug.dev/708): Similarly to CopyFromSocketToChannel(), we may want to
  // consider yielding occasionally. The data-rate from the Channel into the
  // socket write queue should be bounded by PHY layer data rates, which are
  // much lower than the CPU's data processing throughput, so starvation
  // shouldn't be an issue. However, latency might be.
  zx_status_t write_res;
  TRACE_DURATION("bluetooth", "SocketChannelRelay::ServiceSocketWriteQueue", "channel_id",
                 channel_->id());
  do {
    BT_ASSERT(!socket_write_queue_.empty());
    BT_ASSERT(socket_write_queue_.front());
    TRACE_DURATION("bluetooth", "SocketChannelRelay::ServiceSocketWriteQueue write", "channel_id",
                   channel_->id());

    const ByteBuffer& rx_data = *socket_write_queue_.front();
    BT_ASSERT_MSG(rx_data.size(), "Zero-length message on write queue");

    socket_packet_sent_count_++;
    TRACE_FLOW_END("bluetooth", "SocketChannelRelay::OnChannelDataReceived queued",
                   GetTraceId(socket_packet_sent_count_));

    // TODO: we probably need to make this unique across all profile sockets.
    TRACE_FLOW_BEGIN("bluetooth", "ProfilePacket", socket_packet_sent_count_);

    size_t n_bytes_written = 0;
    write_res = socket_.write(0, rx_data.data(), rx_data.size(), &n_bytes_written);
    BT_ASSERT_MSG(
        write_res == ZX_OK || write_res == ZX_ERR_SHOULD_WAIT || write_res == ZX_ERR_PEER_CLOSED,
        "%s", zx_status_get_string(write_res));
    if (write_res != ZX_OK) {
      BT_ASSERT(n_bytes_written == 0);
      bt_log(TRACE, "l2cap", "Failed to write %zu bytes to socket for channel %u: %s",
             rx_data.size(), channel_->id(), zx_status_get_string(write_res));
      break;
    }
    BT_ASSERT_MSG(n_bytes_written == rx_data.size(), "(n_bytes_written=%zu, rx_data.size()=%zu)",
                  n_bytes_written, rx_data.size());
    socket_write_queue_.pop_front();
  } while (write_res == ZX_OK && !socket_write_queue_.empty());

  if (!socket_write_queue_.empty() && write_res == ZX_ERR_SHOULD_WAIT) {
    // Since we hava data to write, we want to be woken when the socket has free
    // space in its buffer. And, to avoid spinning, we want to be woken only
    // when the free space is large enough for our first pending buffer.
    //
    // Note: it is safe to leave TX_THRESHOLD set, even when our queue is empty,
    // because we will only be woken if we also have an active Wait for
    // ZX_SOCKET_WRITE_THRESHOLD, and Waits are one-shot.
    const size_t rx_data_len = socket_write_queue_.front()->size();
    const auto prop_set_res =
        socket_.set_property(ZX_PROP_SOCKET_TX_THRESHOLD, &rx_data_len, sizeof(rx_data_len));
    switch (prop_set_res) {
      case ZX_OK:
        if (!BeginWait("socket write waiter", &sock_write_waiter_)) {
          DeactivateAndRequestDestruction();
        }
        break;
      case ZX_ERR_PEER_CLOSED:
        // Peer closed the socket after the while loop above. Nothing to do
        // here, as closure event will be handled by OnSocketClosed().
        break;
      default:
        BT_PANIC("Unexpected zx_object_set_property() result: %s",
                 zx_status_get_string(prop_set_res));
        break;
    }
  }
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::BindWait(zx_signals_t trigger, const char* wait_name,
                                            async::Wait* wait,
                                            fit::function<void(zx_status_t)> handler) {
  wait->set_object(socket_.get());
  wait->set_trigger(trigger);
  wait->set_handler([self = weak_ptr_factory_.GetWeakPtr(), channel_id = channel_->id(), wait_name,
                     expected_wait = wait, handler = std::move(handler)](
                        async_dispatcher_t* actual_dispatcher, async::WaitBase* actual_wait,
                        zx_status_t status, const zx_packet_signal_t* signal) {
    BT_ASSERT_MSG(self, "(%s, channel_id=%u)", wait_name, channel_id);
    BT_ASSERT_MSG(actual_dispatcher == self->dispatcher_, "(%s, channel_id=%u)", wait_name,
                  channel_id);
    BT_ASSERT_MSG(actual_wait == expected_wait, "(%s, channel_id=%u)", wait_name, channel_id);
    BT_ASSERT_MSG(status == ZX_OK || status == ZX_ERR_CANCELED, "(%s, channel_id=%u)", wait_name,
                  channel_id);

    if (status == ZX_ERR_CANCELED) {  // Dispatcher is shutting down.
      bt_log(DEBUG, "l2cap", "%s canceled on socket for channel %u", wait_name, channel_id);
      self->DeactivateAndRequestDestruction();
      return;
    }

    BT_ASSERT_MSG(signal, "(%s, channel_id=%u)", wait_name, channel_id);
    BT_ASSERT_MSG(signal->trigger == expected_wait->trigger(), "(%s, channel_id=%u)", wait_name,
                  channel_id);
    BT_ASSERT_MSG(self->state_ != RelayState::kActivating, "(%s, channel_id=%u)", wait_name,
                  channel_id);
    BT_ASSERT_MSG(self->state_ != RelayState::kDeactivated, "(%s, channel_id=%u)", wait_name,
                  channel_id);

    if (self->state_ == RelayState::kDeactivating) {
      bt_log(DEBUG, "l2cap", "Ignorning %s on socket for channel %u while deactivating", wait_name,
             channel_id);
      return;
    }
    handler(status);
  });
}

template <typename ChannelT>
bool SocketChannelRelay<ChannelT>::BeginWait(const char* wait_name, async::Wait* wait) {
  BT_ASSERT(state_ != RelayState::kDeactivating);
  BT_ASSERT(state_ != RelayState::kDeactivated);

  if (wait->is_pending()) {
    return true;
  }

  zx_status_t wait_res = wait->Begin(dispatcher_);
  BT_ASSERT(wait_res == ZX_OK || wait_res == ZX_ERR_BAD_STATE);

  if (wait_res != ZX_OK) {
    bt_log(ERROR, "l2cap", "Failed to enable waiting on %s: %s", wait_name,
           zx_status_get_string(wait_res));
    return false;
  }

  return true;
}

template <typename ChannelT>
void SocketChannelRelay<ChannelT>::UnbindAndCancelWait(async::Wait* wait) {
  BT_ASSERT(state_ != RelayState::kActivating);
  BT_ASSERT(state_ != RelayState::kDeactivated);
  zx_status_t cancel_res;
  wait->set_handler(nullptr);
  cancel_res = wait->Cancel();
  BT_ASSERT_MSG(cancel_res == ZX_OK || cancel_res == ZX_ERR_NOT_FOUND, "Cancel failed: %s",
                zx_status_get_string(cancel_res));
}

template <typename ChannelT>
trace_flow_id_t SocketChannelRelay<ChannelT>::GetTraceId(uint32_t id) {
  static_assert(sizeof(trace_flow_id_t) >= (sizeof(typename ChannelT::UniqueId) + sizeof(uint32_t)),
                "UniqueId needs to be small enough to make unique trace IDs");
  return (static_cast<trace_flow_id_t>(channel_->unique_id()) << (sizeof(uint32_t) * CHAR_BIT)) |
         id;
}

}  // namespace bt::socket

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SOCKET_SOCKET_CHANNEL_RELAY_H_
