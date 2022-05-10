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

#include <optional>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_transfer/internal/protocol.h"

namespace pw::transfer::internal {

class Chunk {
 public:
  enum class Type {
    kTransferData = 0,
    kTransferStart = 1,
    kParametersRetransmit = 2,
    kParametersContinue = 3,
    kTransferCompletion = 4,
    kTransferCompletionAck = 5,  // Currently unused.
  };

  // Constructs a new chunk with the given transfer protocol version. All fields
  // are initialized to their zero values.
  constexpr Chunk(ProtocolVersion version, Type type)
      : Chunk(version, std::optional<Type>(type)) {}

  // Parses a chunk from a serialized protobuf message.
  static Result<Chunk> Parse(ConstByteSpan message);

  // Partially decodes a transfer chunk to find its session ID field.
  static Result<uint32_t> ExtractSessionId(ConstByteSpan message);

  // Creates a terminating status chunk within a transfer.
  static Chunk Final(ProtocolVersion version,
                     uint32_t session_id,
                     Status status) {
    return Chunk(version, Type::kTransferCompletion)
        .set_session_id(session_id)
        .set_status(status);
  }

  // Encodes the chunk to the specified buffer, returning a span of the
  // serialized data on success.
  Result<ConstByteSpan> Encode(ByteSpan buffer) const;

  // Returns the size of the serialized chunk based on the fields currently set
  // within the chunk object.
  size_t EncodedSize() const;

  constexpr Chunk& set_session_id(uint32_t session_id) {
    session_id_ = session_id;
    return *this;
  }

  constexpr Chunk& set_resource_id(uint32_t resource_id) {
    resource_id_ = resource_id;
    return *this;
  }

  constexpr Chunk& set_window_end_offset(uint32_t window_end_offset) {
    window_end_offset_ = window_end_offset;
    return *this;
  }

  constexpr Chunk& set_max_chunk_size_bytes(uint32_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
    return *this;
  }

  constexpr Chunk& set_min_delay_microseconds(uint32_t min_delay_microseconds) {
    min_delay_microseconds_ = min_delay_microseconds;
    return *this;
  }

  constexpr Chunk& set_offset(uint32_t offset) {
    offset_ = offset;
    return *this;
  }

  constexpr Chunk& set_payload(ConstByteSpan payload) {
    payload_ = payload;
    return *this;
  }

  constexpr Chunk& set_remaining_bytes(uint64_t remaining_bytes) {
    remaining_bytes_ = remaining_bytes;
    return *this;
  }

  // TODO(frolv): For some reason, the compiler complains if this setter is
  // marked constexpr. Leaving it off for now, but this should be investigated
  // and fixed.
  Chunk& set_status(Status status) {
    status_ = status;
    return *this;
  }

  constexpr uint32_t session_id() const { return session_id_; }

  constexpr std::optional<uint32_t> resource_id() const {
    if (is_legacy()) {
      // In the legacy protocol, resource_id and session_id are the same (i.e.
      // transfer_id).
      return session_id_;
    }

    return resource_id_;
  }

  constexpr uint32_t window_end_offset() const { return window_end_offset_; }
  constexpr uint32_t offset() const { return offset_; }
  constexpr std::optional<Status> status() const { return status_; }

  constexpr bool has_payload() const { return !payload_.empty(); }
  constexpr ConstByteSpan payload() const { return payload_; }

  constexpr std::optional<uint32_t> max_chunk_size_bytes() const {
    return max_chunk_size_bytes_;
  }
  constexpr std::optional<uint32_t> min_delay_microseconds() const {
    return min_delay_microseconds_;
  }
  constexpr std::optional<uint64_t> remaining_bytes() const {
    return remaining_bytes_;
  }

  constexpr bool is_legacy() const {
    return protocol_version_ == ProtocolVersion::kLegacy;
  }

  constexpr Type type() const {
    // Legacy protocol chunks may not have a type, but newer versions always
    // will. Try to deduce the type of a legacy chunk without one set.
    if (!is_legacy() || type_.has_value()) {
      return type_.value();
    }

    // The type-less legacy transfer protocol doesn't support handshakes or
    // continuation parameters. Therefore, there are only three possible chunk
    // types: start, data, and retransmit.
    if (IsInitialChunk()) {
      return Type::kTransferStart;
    }

    if (has_payload()) {
      return Type::kTransferData;
    }

    return Type::kParametersRetransmit;
  }

  // Returns true if this parameters chunk is requesting that the transmitter
  // transmit from its set offset instead of simply ACKing.
  constexpr bool RequestsTransmissionFromOffset() const {
    if (is_legacy() && !type_.has_value()) {
      return true;
    }

    return type_.value() == Type::kParametersRetransmit ||
           type_.value() == Type::kTransferStart;
  }

  constexpr bool IsInitialChunk() const {
    if (protocol_version_ >= ProtocolVersion::kVersionTwo) {
      return type_ == Type::kTransferStart;
    }

    // In legacy versions of the transfer protocol, the chunk type is not always
    // set. Infer that a chunk is initial if it has an offset of 0 and no data
    // or status.
    return type_ == Type::kTransferStart ||
           (offset_ == 0 && !has_payload() && !status_.has_value());
  }

  // The final chunk from the transmitter sets remaining_bytes to 0 in both Read
  // and Write transfers.
  constexpr bool IsFinalTransmitChunk() const { return remaining_bytes_ == 0u; }

 private:
  constexpr Chunk(ProtocolVersion version, std::optional<Type> type)
      : session_id_(0),
        resource_id_(std::nullopt),
        window_end_offset_(0),
        max_chunk_size_bytes_(std::nullopt),
        min_delay_microseconds_(std::nullopt),
        offset_(0),
        payload_({}),
        remaining_bytes_(std::nullopt),
        status_(std::nullopt),
        type_(type),
        protocol_version_(version) {}

  constexpr Chunk() : Chunk(ProtocolVersion::kUnknown, std::nullopt) {}

  // Returns true if this chunk should write legacy protocol fields to the
  // serialized message.
  //
  // The first chunk of a transfer (type TRANSFER_START) is a special case: as
  // we do not yet know what version of the protocol the other end is speaking,
  // every legacy field must be encoded alongside newer ones to ensure that the
  // chunk is processable. Following a response, the common protocol version
  // will be determined and fields omitted as necessary.
  constexpr bool ShouldEncodeLegacyFields() const {
    return is_legacy() || type_ == Chunk::Type::kTransferStart;
  }

  uint32_t session_id_;
  std::optional<uint32_t> resource_id_;
  uint32_t window_end_offset_;
  std::optional<uint32_t> max_chunk_size_bytes_;
  std::optional<uint32_t> min_delay_microseconds_;
  uint32_t offset_;
  ConstByteSpan payload_;
  std::optional<uint64_t> remaining_bytes_;
  std::optional<Status> status_;
  std::optional<Type> type_;
  ProtocolVersion protocol_version_;
};

}  // namespace pw::transfer::internal
