// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_proxy/client_channel.h"

#include <mutex>

#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

void ClientChannel::MoveFields(ClientChannel& other) {
  state_ = other.state();
  event_fn_ = std::move(other.event_fn_);
  {
    std::lock_guard lock(send_queue_mutex_);
    std::lock_guard other_lock(other.send_queue_mutex_);
    payload_queue_ = std::move(other.payload_queue_);
    notify_on_dequeue_ = other.notify_on_dequeue_;
  }
  other.Undefine();
}

ClientChannel::ClientChannel(ClientChannel&& other) { MoveFields(other); }

ClientChannel& ClientChannel::operator=(ClientChannel&& other) {
  if (this != &other) {
    MoveFields(other);
  }
  return *this;
}

ClientChannel::~ClientChannel() {
  // Don't log dtor of moved-from channels.
  if (state_ != State::kUndefined) {
    PW_LOG_INFO("btproxy: ClientChannel dtor - state_: %u",
                cpp23::to_underlying(state_));
  }

  // Channel objects may outlive `ProxyHost`, but they are closed on
  // `ProxyHost` dtor, so this check will prevent a crash from trying to access
  // a destructed queue.
  if (state_ != State::kClosed) {
    ClearQueue();
  }
}

void ClientChannel::Stop() {
  PW_LOG_INFO("btproxy: ClientChannel::Stop - previous state_: %u",
              cpp23::to_underlying(state_));

  PW_CHECK(state_ != State::kUndefined && state_ != State::kClosed);

  state_ = State::kStopped;
  ClearQueue();
  HandleStop();
}

void ClientChannel::Close() {
  HandleClose();
  InternalClose();
}

void ClientChannel::InternalClose(ClientChannelEvent event) {
  PW_LOG_INFO("btproxy: ClientChannel::Close - previous state_: %u",
              cpp23::to_underlying(state_));

  PW_CHECK(state_ != State::kUndefined);
  if (state_ == State::kClosed) {
    return;
  }
  state_ = State::kClosed;

  ClearQueue();
  SendEvent(event);
}

void ClientChannel::Undefine() { state_ = State::kUndefined; }

Status ClientChannel::QueuePacket(H4PacketWithH4&& packet) {
  PW_CHECK(!UsesPayloadQueue());

  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }

  Status status;
  {
    std::lock_guard lock(send_queue_mutex_);
    if (send_queue_.full()) {
      status = Status::Unavailable();
      notify_on_dequeue_ = true;
    } else {
      send_queue_.push(std::move(packet));
      status = OkStatus();
    }
  }
  ReportPacketsMayBeReadyToSend();
  return status;
}

namespace {

// TODO: https://pwbug.dev/389724307 - Move to pw utility function once created.
pw::span<const uint8_t> AsConstUint8Span(ConstByteSpan s) {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size_bytes()};
}

}  // namespace

StatusWithMultiBuf ClientChannel::WriteToPayloadQueue(
    multibuf::MultiBuf&& payload) {
  if (!payload.IsContiguous()) {
    return {Status::InvalidArgument(), std::move(payload)};
  }

  if (state() != State::kRunning) {
    return {Status::FailedPrecondition(), std::move(payload)};
  }

  PW_CHECK(UsesPayloadQueue());

  return QueuePayload(std::move(payload));
}

// TODO: https://pwbug.dev/379337272 - Delete when all channels are
// transitioned to using payload queues.
StatusWithMultiBuf ClientChannel::WriteToPduQueue(
    multibuf::MultiBuf&& payload) {
  if (!payload.IsContiguous()) {
    return {Status::InvalidArgument(), std::move(payload)};
  }

  if (state() != State::kRunning) {
    return {Status::FailedPrecondition(), std::move(payload)};
  }

  PW_CHECK(!UsesPayloadQueue());

  std::optional<ByteSpan> span = payload.ContiguousSpan();
  PW_CHECK(span.has_value());
  Status status = Write(AsConstUint8Span(span.value()));

  if (!status.ok()) {
    return {status, std::move(payload)};
  }

  return {OkStatus(), std::nullopt};
}

pw::Status ClientChannel::Write(
    [[maybe_unused]] pw::span<const uint8_t> payload) {
  PW_LOG_ERROR(
      "btproxy: Write(span) called on class that only supports "
      "Write(MultiBuf)");
  return Status::Unimplemented();
}

Status ClientChannel::IsWriteAvailable() {
  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }

  std::lock_guard lock(send_queue_mutex_);

  // TODO: https://pwbug.dev/379337272 - Only check payload_queue_ once all
  // channels have transitioned to payload_queue_.
  const bool queue_full =
      UsesPayloadQueue() ? payload_queue_.full() : send_queue_.full();
  if (queue_full) {
    notify_on_dequeue_ = true;
    return Status::Unavailable();
  }

  notify_on_dequeue_ = false;
  return OkStatus();
}

std::optional<H4PacketWithH4> ClientChannel::DequeuePacket() {
  std::optional<H4PacketWithH4> packet;
  bool should_notify = false;
  {
    std::lock_guard lock(send_queue_mutex_);
    packet = GenerateNextTxPacket();
    if (packet) {
      should_notify = notify_on_dequeue_;
      notify_on_dequeue_ = false;
    }
  }

  if (should_notify) {
    SendEvent(ClientChannelEvent::kWriteAvailable);
  }

  return packet;
}

StatusWithMultiBuf ClientChannel::QueuePayload(multibuf::MultiBuf&& buf) {
  PW_CHECK(UsesPayloadQueue());

  PW_CHECK(state() == State::kRunning);
  PW_CHECK(buf.IsContiguous());

  {
    std::lock_guard lock(send_queue_mutex_);
    if (payload_queue_.full()) {
      notify_on_dequeue_ = true;
      return {Status::Unavailable(), std::move(buf)};
    }
    payload_queue_.push(std::move(buf));
  }

  ReportPacketsMayBeReadyToSend();
  return {OkStatus(), std::nullopt};
}

void ClientChannel::ReportPacketsMayBeReadyToSend() {
  HandlePacketsMayBeReadyToSend();
}

void ClientChannel::PopFrontPayload() {
  PW_CHECK(!payload_queue_.empty());
  payload_queue_.pop();
}

ConstByteSpan ClientChannel::GetFrontPayloadSpan() const {
  PW_CHECK(!payload_queue_.empty());
  const multibuf::MultiBuf& buf = payload_queue_.front();
  std::optional<ConstByteSpan> span = buf.ContiguousSpan();
  PW_CHECK(span);
  return *span;
}

bool ClientChannel::PayloadQueueEmpty() const { return payload_queue_.empty(); }

ClientChannel::ClientChannel(
    Function<void(ClientChannelEvent event)>&& event_fn)
    : state_(State::kRunning), event_fn_(std::move(event_fn)) {
  PW_LOG_INFO("btproxy: ClientChannel ctor");
}

// Send `event` to client if an event callback was provided.
void ClientChannel::SendEvent(ClientChannelEvent event) {
  // We don't log kWriteAvailable since they happen often. Optimally we would
  // just debug log them also, but one of our downstreams logs all levels.
  if (event != ClientChannelEvent::kWriteAvailable) {
    PW_LOG_INFO("btproxy: SendEvent - event: %u, state_: %u",
                cpp23::to_underlying(event),
                cpp23::to_underlying(state_));
  }

  if (event_fn_) {
    event_fn_(event);
  }
}

std::optional<H4PacketWithH4> ClientChannel::GenerateNextTxPacket() {
  if (send_queue_.empty()) {
    return std::nullopt;
  }
  H4PacketWithH4 packet = std::move(send_queue_.front());
  send_queue_.pop();
  return packet;
}

void ClientChannel::SetNotifyOnDequeue() {
  std::lock_guard lock(send_queue_mutex_);
  notify_on_dequeue_ = true;
}

void ClientChannel::ClearQueue() {
  std::lock_guard lock(send_queue_mutex_);
  send_queue_.clear();
}

}  // namespace pw::bluetooth::proxy
