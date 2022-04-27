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

#include "pw_transfer/internal/chunk.h"

#include "pw_assert/check.h"
#include "pw_protobuf/decoder.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"

namespace pw::transfer::internal {

namespace ProtoChunk = transfer::Chunk;

Result<uint32_t> Chunk::ExtractSessionId(ConstByteSpan message) {
  protobuf::Decoder decoder(message);

  while (decoder.Next().ok()) {
    ProtoChunk::Fields field =
        static_cast<ProtoChunk::Fields>(decoder.FieldNumber());

    switch (field) {
      case ProtoChunk::Fields::SESSION_ID: {
        uint32_t session_id;
        PW_TRY(decoder.ReadUint32(&session_id));
        return session_id;
      }

      default:
        continue;
    }
  }

  return Status::DataLoss();
}

Result<Chunk> Chunk::Parse(ConstByteSpan message) {
  protobuf::Decoder decoder(message);
  Status status;
  uint32_t value;

  Chunk chunk;

  // Assume the legacy protocol by default. Field presence in the serialized
  // message may change this.
  chunk.protocol_version_ = ProtocolVersion::kLegacy;

  // Some older versions of the protocol set the deprecated pending_bytes field
  // in their chunks. The newer transfer handling code does not process this
  // field, instead working only in terms of window_end_offset. If pending_bytes
  // is encountered in the serialized message, save its value, then calculate
  // window_end_offset from it once parsing is complete.
  uint32_t pending_bytes = 0;

  while ((status = decoder.Next()).ok()) {
    ProtoChunk::Fields field =
        static_cast<ProtoChunk::Fields>(decoder.FieldNumber());

    switch (field) {
      case ProtoChunk::Fields::SESSION_ID:
        PW_TRY(decoder.ReadUint32(&chunk.session_id_));
        break;

      case ProtoChunk::Fields::PENDING_BYTES:
        PW_TRY(decoder.ReadUint32(&pending_bytes));
        break;

      case ProtoChunk::Fields::MAX_CHUNK_SIZE_BYTES:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.set_max_chunk_size_bytes(value);
        break;

      case ProtoChunk::Fields::MIN_DELAY_MICROSECONDS:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.set_min_delay_microseconds(value);
        break;

      case ProtoChunk::Fields::OFFSET:
        PW_TRY(decoder.ReadUint32(&chunk.offset_));
        break;

      case ProtoChunk::Fields::DATA:
        PW_TRY(decoder.ReadBytes(&chunk.payload_));
        break;

      case ProtoChunk::Fields::REMAINING_BYTES: {
        uint64_t remaining_bytes;
        PW_TRY(decoder.ReadUint64(&remaining_bytes));
        chunk.set_remaining_bytes(remaining_bytes);
        break;
      }

      case ProtoChunk::Fields::STATUS:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.set_status(static_cast<Status::Code>(value));
        break;

      case ProtoChunk::Fields::WINDOW_END_OFFSET:
        PW_TRY(decoder.ReadUint32(&chunk.window_end_offset_));
        break;

      case ProtoChunk::Fields::TYPE: {
        uint32_t type;
        PW_TRY(decoder.ReadUint32(&type));
        chunk.type_ = static_cast<Chunk::Type>(type);
        break;
      }

      case ProtoChunk::Fields::RESOURCE_ID:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.set_resource_id(value);

        // The existence of a resource_id field indicates that a newer protocol
        // is running.
        chunk.protocol_version_ = ProtocolVersion::kVersionTwo;
        break;

        // Silently ignore any unrecognized fields.
    }
  }

  if (pending_bytes != 0) {
    // Compute window_end_offset if it isn't explicitly provided (in older
    // protocol versions).
    chunk.set_window_end_offset(chunk.offset() + pending_bytes);
  }

  if (status.ok() || status.IsOutOfRange()) {
    return chunk;
  }

  return status;
}

Result<ConstByteSpan> Chunk::Encode(ByteSpan buffer) const {
  PW_CHECK(protocol_version_ != ProtocolVersion::kUnknown,
           "Cannot encode a transfer chunk with an unknown protocol version");

  ProtoChunk::MemoryEncoder encoder(buffer);

  encoder.WriteSessionId(session_id_).IgnoreError();

  if (type_.has_value()) {
    encoder.WriteType(static_cast<ProtoChunk::Type>(type_.value()))
        .IgnoreError();
  }

  if (window_end_offset_ != 0) {
    encoder.WriteWindowEndOffset(window_end_offset_).IgnoreError();
  }

  if (protocol_version_ == ProtocolVersion::kLegacy) {
    // In the legacy protocol, the pending_bytes field must be set alongside
    // window_end_offset, as some transfer implementations require it.
    encoder.WritePendingBytes(window_end_offset_ - offset_).IgnoreError();
  }

  if (max_chunk_size_bytes_.has_value()) {
    encoder.WriteMaxChunkSizeBytes(max_chunk_size_bytes_.value()).IgnoreError();
  }
  if (min_delay_microseconds_.has_value()) {
    encoder.WriteMinDelayMicroseconds(min_delay_microseconds_.value())
        .IgnoreError();
  }

  if (offset_ != 0) {
    encoder.WriteOffset(offset_).IgnoreError();
  }

  if (has_payload()) {
    encoder.WriteData(payload_).IgnoreError();
  }

  if (remaining_bytes_.has_value()) {
    encoder.WriteRemainingBytes(remaining_bytes_.value()).IgnoreError();
  }

  if (status_.has_value()) {
    encoder.WriteStatus(status_.value().code()).IgnoreError();
  }

  if (resource_id_.has_value()) {
    encoder.WriteResourceId(resource_id_.value()).IgnoreError();
  }

  PW_TRY(encoder.status());
  return ConstByteSpan(encoder);
}

Status DecodeChunk(ConstByteSpan, Chunk&) { return Status::Unimplemented(); }

}  // namespace pw::transfer::internal
