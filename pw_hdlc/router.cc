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

#include "pw_hdlc/router.h"

#include <inttypes.h>

#include <algorithm>

#include "pw_hdlc/encoder.h"
#include "pw_log/log.h"
#include "pw_multibuf/multibuf.h"
#include "pw_multibuf/stream.h"
#include "pw_result/result.h"
#include "pw_stream/null_stream.h"

namespace pw::hdlc {

using ::pw::async2::Context;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::channel::ByteReaderWriter;
using ::pw::channel::DatagramReaderWriter;
using ::pw::multibuf::Chunk;
using ::pw::multibuf::MultiBuf;
using ::pw::stream::CountingNullStream;

namespace {

/// HDLC encodes the contents of ``payload`` to ``writer``.
Status WriteMultiBufUIFrame(uint64_t address,
                            const MultiBuf& payload,
                            stream::Writer& writer) {
  Encoder encoder(writer);
  if (Status status = encoder.StartUnnumberedFrame(address); !status.ok()) {
    return status;
  }
  for (const Chunk& chunk : payload.Chunks()) {
    if (Status status = encoder.WriteData(chunk); !status.ok()) {
      return status;
    }
  }
  return encoder.FinishFrame();
}

/// Calculates the size of ``payload`` once HDLC-encoded.
Result<size_t> CalculateSizeOnceEncoded(uint64_t address,
                                        const MultiBuf& payload) {
  CountingNullStream null_stream;
  Status encode_status = WriteMultiBufUIFrame(address, payload, null_stream);
  if (!encode_status.ok()) {
    return encode_status;
  }
  return null_stream.bytes_written();
}

/// Attempts to decode a frame from ``data``, advancing ``data`` forwards by
/// any bytes that are consumed.
std::optional<Frame> DecodeFrame(Decoder& decoder, MultiBuf& data) {
  size_t processed = 0;
  for (std::byte byte : data) {
    Result<Frame> frame_result = decoder.Process(byte);
    ++processed;
    if (frame_result.status().IsUnavailable()) {
      // No frame is yet available.
    } else if (frame_result.ok()) {
      data.DiscardPrefix(processed);
      return std::move(*frame_result);
    } else if (frame_result.status().IsDataLoss()) {
      PW_LOG_ERROR("Discarding invalid incoming HDLC frame.");
    } else if (frame_result.status().IsResourceExhausted()) {
      PW_LOG_ERROR("Discarding incoming HDLC frame: too large for buffer.");
    }
  }
  data.DiscardPrefix(processed);
  return std::nullopt;
}

}  // namespace

Status Router::AddChannel(DatagramReaderWriter& channel,
                          uint64_t receive_address,
                          uint64_t send_address) {
  for (const ChannelData& data : channel_datas_) {
    if ((data.channel == &channel) ||
        (data.receive_address == receive_address) ||
        (data.send_address == send_address)) {
      return Status::AlreadyExists();
    }
  }
  channel_datas_.emplace_back(channel, receive_address, send_address);
  return OkStatus();
}

Status Router::RemoveChannel(DatagramReaderWriter& channel,
                             uint64_t receive_address,
                             uint64_t send_address) {
  auto channel_entry = std::find_if(
      channel_datas_.begin(),
      channel_datas_.end(),
      [&channel, receive_address, send_address](const ChannelData& data) {
        return (data.channel == &channel) &&
               (data.receive_address == receive_address) &&
               (data.send_address == send_address);
      });
  if (channel_entry == channel_datas_.end()) {
    return Status::NotFound();
  }
  if (channel_datas_.size() == 1) {
    channel_datas_.clear();
  } else {
    // Put the ChannelData in the back of
    // the list and pop it out to avoid shifting
    // all elements.
    std::swap(*channel_entry, channel_datas_.back());
    channel_datas_.pop_back();
  }
  return OkStatus();
}

Router::ChannelData* Router::FindChannelForReceiveAddress(
    uint64_t receive_address) {
  for (auto& channel : channel_datas_) {
    if (channel.receive_address == receive_address) {
      return &channel;
    }
  }
  return nullptr;
}

Poll<> Router::PollDeliverIncomingFrame(Context& cx, const Frame& frame) {
  ConstByteSpan data = frame.data();
  uint64_t address = frame.address();
  ChannelData* channel = FindChannelForReceiveAddress(address);
  if (channel == nullptr) {
    PW_LOG_ERROR("Received incoming HDLC packet with address %" PRIu64
                 ", but no channel with that incoming address is registered.",
                 address);
    incoming_allocation_future_ = std::nullopt;
    return Ready();
  }
  if (channel->channel->PendReadyToWrite(cx).IsPending()) {
    return Pending();
  }
  if (!incoming_allocation_future_.has_value()) {
    incoming_allocation_future_ =
        channel->channel->GetWriteAllocator().AllocateAsync(data.size());
  }
  Poll<std::optional<MultiBuf>> buffer = incoming_allocation_future_->Pend(cx);
  if (buffer.IsPending()) {
    return Pending();
  }
  incoming_allocation_future_ = std::nullopt;
  if (!buffer->has_value()) {
    PW_LOG_ERROR(
        "Unable to allocate a buffer of size %zu destined for incoming "
        "HDLC address %" PRIu64 ". Packet will be discarded.",
        data.size(),
        frame.address());
    return Ready();
  }
  std::copy(frame.data().begin(), frame.data().end(), (**buffer).begin());
  Status write_status = channel->channel->Write(std::move(**buffer)).status();
  if (!write_status.ok()) {
    PW_LOG_ERROR(
        "Failed to write a buffer of size %zu destined for incoming HDLC "
        "address %" PRIu64 ". Status: %d",
        data.size(),
        channel->receive_address,
        write_status.code());
  }
  return Ready();
}

void Router::DecodeAndWriteIncoming(Context& cx) {
  while (true) {
    if (decoded_frame_.has_value()) {
      if (PollDeliverIncomingFrame(cx, *decoded_frame_).IsPending()) {
        return;
      }
      // Zero out the frame delivery state.
      decoded_frame_ = std::nullopt;
    }

    while (incoming_data_.empty()) {
      Poll<Result<MultiBuf>> incoming = io_channel_.PendRead(cx);
      if (incoming.IsPending()) {
        return;
      }
      if (!incoming->ok()) {
        if (incoming->status().IsFailedPrecondition()) {
          PW_LOG_WARN("HDLC io_channel has closed.");
        } else {
          PW_LOG_ERROR("Unable to read from HDLC io_channel. Status: %d",
                       incoming->status().code());
        }
        return;
      }
      incoming_data_ = std::move(**incoming);
    }

    decoded_frame_ = DecodeFrame(decoder_, incoming_data_);
  }
}

void Router::TryFillBufferToEncodeAndSend(Context& cx) {
  if (buffer_to_encode_and_send_.has_value()) {
    return;
  }
  for (size_t i = 0; i < channel_datas_.size(); ++i) {
    ChannelData& cd =
        channel_datas_[(next_first_read_index_ + i) % channel_datas_.size()];
    Poll<Result<MultiBuf>> buf = cd.channel->PendRead(cx);
    if (buf.IsPending()) {
      continue;
    }
    if (!buf->ok()) {
      if (buf->status().IsUnimplemented()) {
        PW_LOG_ERROR("Channel registered for outgoing HDLC address %" PRIu64
                     " is not readable.",
                     cd.send_address);
      }
      // We ignore FAILED_PRECONDITION (closed) because it will be handled
      // elsewhere. OUT_OF_RANGE just means we have finished writing. No
      // action is needed because the channel may still be receiving data.
      continue;
    }
    buffer_to_encode_and_send_ = std::move(**buf);
    address_to_encode_and_send_to_ = cd.send_address;
    // We received data, so ensure that we start by reading from a different
    // index next time.
    next_first_read_index_ =
        (next_first_read_index_ + 1) % channel_datas_.size();
    return;
  }
}

void Router::WriteOutgoingMessages(Context& cx) {
  while (io_channel_.is_write_open() &&
         io_channel_.PendReadyToWrite(cx).IsReady()) {
    TryFillBufferToEncodeAndSend(cx);
    if (!buffer_to_encode_and_send_.has_value()) {
      // No channels have new data to send.
      return;
    }
    if (!outgoing_allocation_future_.has_value()) {
      Result<size_t> encoded_size = CalculateSizeOnceEncoded(
          address_to_encode_and_send_to_, *buffer_to_encode_and_send_);
      if (!encoded_size.ok()) {
        PW_LOG_ERROR(
            "Unable to compute size of encoded packet for outgoing buffer of "
            "size %zu destined for outgoing HDLC address %" PRIu64
            ". Packet will be discarded.",
            buffer_to_encode_and_send_->size(),
            address_to_encode_and_send_to_);
        buffer_to_encode_and_send_ = std::nullopt;
        continue;
      }
      outgoing_allocation_future_ =
          io_channel_.GetWriteAllocator().AllocateAsync(*encoded_size);
    }
    Poll<std::optional<MultiBuf>> maybe_write_buffer =
        outgoing_allocation_future_->Pend(cx);
    if (maybe_write_buffer.IsPending()) {
      // Channel cannot write any further messages until we can allocate.
      return;
    }
    // We've gotten the allocation: discard the future.
    size_t buffer_size = outgoing_allocation_future_->min_size();
    outgoing_allocation_future_ = std::nullopt;
    if (!maybe_write_buffer->has_value()) {
      // We can't allocate a write buffer large enough for our encoded frame.
      // Sadly, we have to throw the frame away.
      PW_LOG_ERROR(
          "Unable to allocate a buffer of size %zu destined for outgoing "
          "HDLC address %" PRIu64 ". Packet will be discarded.",
          buffer_size,
          address_to_encode_and_send_to_);
      buffer_to_encode_and_send_ = std::nullopt;
      continue;
    }
    MultiBuf write_buffer = std::move(**maybe_write_buffer);
    Status encode_status =
        WriteMultiBufUIFrame(address_to_encode_and_send_to_,
                             *buffer_to_encode_and_send_,
                             pw::multibuf::Stream(write_buffer));
    buffer_to_encode_and_send_ = std::nullopt;
    if (!encode_status.ok()) {
      PW_LOG_ERROR(
          "Failed to encode a buffer destined for outgoing HDLC address "
          "%" PRIu64 ". Status: %d",
          address_to_encode_and_send_to_,
          encode_status.code());
      continue;
    }
    Status write_status = io_channel_.Write(std::move(write_buffer)).status();
    if (!write_status.ok()) {
      PW_LOG_ERROR(
          "Failed to write a buffer of size %zu destined for outgoing HDLC "
          "address %" PRIu64 ". Status: %d",
          buffer_size,
          address_to_encode_and_send_to_,
          write_status.code());
    }
  }
}

Poll<> Router::Pend(Context& cx) {
  // We check for ability to read, but not write, because we may not always
  // attempt a write, which would cause us to miss that the channel has closed
  // for writes.
  //
  // Additionally, it is uncommon for a channel to remain readable but not
  // writeable: the reverse is more common (still readable while no longer
  // writeable).
  if (!io_channel_.is_read_open()) {
    return PendClose(cx);
  }
  DecodeAndWriteIncoming(cx);
  WriteOutgoingMessages(cx);
  RemoveClosedChannels();
  if (!io_channel_.is_read_open()) {
    return PendClose(cx);
  }
  return Pending();
}

Poll<> Router::PendClose(Context& cx) {
  for (ChannelData& cd : channel_datas_) {
    // We ignore the status value from close.
    // If one or more channels are unable to close, they will remain after
    // `RemoveClosedChannels` and `channel_datas_.size()` will be nonzero.
    cd.channel->PendClose(cx).IgnorePoll();
  }
  RemoveClosedChannels();
  if (io_channel_.PendClose(cx).IsPending()) {
    return Pending();
  }
  if (channel_datas_.empty()) {
    return Ready();
  } else {
    return Pending();
  }
}

void Router::RemoveClosedChannels() {
  auto first_to_remove = std::remove_if(
      channel_datas_.begin(), channel_datas_.end(), [](const ChannelData& cd) {
        return !cd.channel->is_read_or_write_open();
      });
  channel_datas_.erase(first_to_remove, channel_datas_.end());
}

}  // namespace pw::hdlc
