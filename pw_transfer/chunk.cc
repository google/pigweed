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

#include "pw_transfer/internal/chunk.h"

#include "pw_protobuf/decoder.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"

namespace pw::transfer::internal {

namespace ProtoChunk = transfer::Chunk;

Result<uint32_t> ExtractTransferId(ConstByteSpan message) {
  protobuf::Decoder decoder(message);

  while (decoder.Next().ok()) {
    ProtoChunk::Fields field =
        static_cast<ProtoChunk::Fields>(decoder.FieldNumber());

    switch (field) {
      case ProtoChunk::Fields::TRANSFER_ID: {
        uint32_t transfer_id;
        PW_TRY(decoder.ReadUint32(&transfer_id));
        return transfer_id;
      }

      default:
        continue;
    }
  }

  return Status::DataLoss();
}

Status DecodeChunk(ConstByteSpan message, Chunk& chunk) {
  protobuf::Decoder decoder(message);
  Status status;
  uint32_t value;

  chunk = {};

  while ((status = decoder.Next()).ok()) {
    ProtoChunk::Fields field =
        static_cast<ProtoChunk::Fields>(decoder.FieldNumber());

    switch (field) {
      case ProtoChunk::Fields::TRANSFER_ID:
        PW_TRY(decoder.ReadUint32(&chunk.transfer_id));
        break;

      case ProtoChunk::Fields::PENDING_BYTES:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.pending_bytes = value;
        break;

      case ProtoChunk::Fields::MAX_CHUNK_SIZE_BYTES:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.max_chunk_size_bytes = value;
        break;

      case ProtoChunk::Fields::MIN_DELAY_MICROSECONDS:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.min_delay_microseconds = value;
        break;

      case ProtoChunk::Fields::OFFSET:
        PW_TRY(decoder.ReadUint32(&chunk.offset));
        break;

      case ProtoChunk::Fields::DATA:
        PW_TRY(decoder.ReadBytes(&chunk.data));
        break;

      case ProtoChunk::Fields::REMAINING_BYTES: {
        uint64_t remaining;
        PW_TRY(decoder.ReadUint64(&remaining));
        chunk.remaining_bytes = remaining;
        break;
      }

      case ProtoChunk::Fields::STATUS:
        PW_TRY(decoder.ReadUint32(&value));
        chunk.status = static_cast<Status::Code>(value);
        break;

      case ProtoChunk::Fields::WINDOW_END_OFFSET:
        PW_TRY(decoder.ReadUint32(&chunk.window_end_offset));
        break;

      case ProtoChunk::Fields::TYPE: {
        uint32_t type;
        PW_TRY(decoder.ReadUint32(&type));
        chunk.type = static_cast<Chunk::Type>(type);
        break;
      }
    }
  }

  return status.IsOutOfRange() ? OkStatus() : status;
}

Result<ConstByteSpan> EncodeChunk(const Chunk& chunk, ByteSpan buffer) {
  ProtoChunk::MemoryEncoder encoder(buffer);

  encoder.WriteTransferId(chunk.transfer_id).IgnoreError();

  if (chunk.window_end_offset != 0) {
    encoder.WriteWindowEndOffset(chunk.window_end_offset).IgnoreError();
  }

  if (chunk.pending_bytes.has_value()) {
    encoder.WritePendingBytes(chunk.pending_bytes.value()).IgnoreError();
  }
  if (chunk.max_chunk_size_bytes.has_value()) {
    encoder.WriteMaxChunkSizeBytes(chunk.max_chunk_size_bytes.value())
        .IgnoreError();
  }
  if (chunk.min_delay_microseconds.has_value()) {
    encoder.WriteMinDelayMicroseconds(chunk.min_delay_microseconds.value())
        .IgnoreError();
  }
  if (chunk.offset != 0) {
    encoder.WriteOffset(chunk.offset).IgnoreError();
  }
  if (!chunk.data.empty()) {
    encoder.WriteData(chunk.data).IgnoreError();
  }
  if (chunk.remaining_bytes.has_value()) {
    encoder.WriteRemainingBytes(chunk.remaining_bytes.value()).IgnoreError();
  }
  if (chunk.status.has_value()) {
    encoder.WriteStatus(chunk.status.value().code()).IgnoreError();
  }

  if (chunk.type.has_value()) {
    encoder.WriteType(static_cast<ProtoChunk::Type>(chunk.type.value()))
        .IgnoreError();
  }

  PW_TRY(encoder.status());
  return ConstByteSpan(encoder);
}

}  // namespace pw::transfer::internal
