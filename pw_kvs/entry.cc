// Copyright 2020 The Pigweed Authors
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

#include "pw_kvs/internal/entry.h"

#include <cinttypes>
#include <cstring>

#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs::internal {

using std::byte;
using std::string_view;

Status Entry::Read(FlashPartition& partition,
                   Address address,
                   const internal::EntryFormats& formats,
                   Entry* entry) {
  EntryHeader header;
  TRY(partition.Read(address, sizeof(header), &header));

  if (partition.AppearsErased(as_bytes(span(&header.magic, 1)))) {
    return Status::NOT_FOUND;
  }
  if (header.key_length_bytes > kMaxKeyLength) {
    return Status::DATA_LOSS;
  }

  const EntryFormat* format = formats.Find(header.magic);
  if (format == nullptr) {
    PW_LOG_ERROR("Found corrupt magic: %" PRIx32 " at address %zx",
                 header.magic,
                 size_t(address));
    return Status::DATA_LOSS;
  }

  *entry = Entry(&partition, address, *format, header);
  return Status::OK;
}

Status Entry::ReadKey(FlashPartition& partition,
                      Address address,
                      size_t key_length,
                      char* key) {
  if (key_length == 0u || key_length > kMaxKeyLength) {
    return Status::DATA_LOSS;
  }

  return partition.Read(address + sizeof(EntryHeader), key_length, key)
      .status();
}

Entry::Entry(FlashPartition& partition,
             Address address,
             const EntryFormat& format,
             string_view key,
             span<const byte> value,
             uint16_t value_size_bytes,
             uint32_t transaction_id)
    : Entry(&partition,
            address,
            format,
            {.magic = format.magic,
             .checksum = 0,
             .alignment_units =
                 alignment_bytes_to_units(partition.alignment_bytes()),
             .key_length_bytes = static_cast<uint8_t>(key.size()),
             .value_size_bytes = value_size_bytes,
             .transaction_id = transaction_id}) {
  if (checksum_ != nullptr) {
    span<const byte> checksum = CalculateChecksum(key, value);
    std::memcpy(&header_.checksum,
                checksum.data(),
                std::min(checksum.size(), sizeof(header_.checksum)));
  }
}

StatusWithSize Entry::Write(const string_view key,
                            span<const byte> value) const {
  FlashPartition::Output flash(partition(), address_);
  return AlignedWrite<64>(
      flash,
      alignment_bytes(),
      {as_bytes(span(&header_, 1)), as_bytes(span(key)), value});
}

StatusWithSize Entry::ReadValue(span<byte> buffer, size_t offset_bytes) const {
  if (offset_bytes > value_size()) {
    return StatusWithSize::OUT_OF_RANGE;
  }

  const size_t remaining_bytes = value_size() - offset_bytes;
  const size_t read_size = std::min(buffer.size(), remaining_bytes);

  StatusWithSize result = partition().Read(
      address_ + sizeof(EntryHeader) + key_length() + offset_bytes,
      buffer.subspan(0, read_size));
  TRY_WITH_SIZE(result);

  if (read_size != remaining_bytes) {
    return StatusWithSize(Status::RESOURCE_EXHAUSTED, read_size);
  }
  return StatusWithSize(read_size);
}

Status Entry::VerifyChecksum(string_view key, span<const byte> value) const {
  if (checksum_ == nullptr) {
    return checksum() == 0 ? Status::OK : Status::DATA_LOSS;
  }
  CalculateChecksum(key, value);
  return checksum_->Verify(checksum_bytes());
}

Status Entry::VerifyChecksumInFlash() const {
  // Read the entire entry piece-by-piece into a small buffer. If the entry is
  // 32 B or less, only one read is required.
  union {
    EntryHeader header_to_verify;
    byte buffer[sizeof(EntryHeader) * 2];
  };

  size_t bytes_to_read = size();
  size_t read_size = std::min(sizeof(buffer), bytes_to_read);

  Address read_address = address_;

  // Read the first chunk, which includes the header, and compare the checksum.
  TRY(partition().Read(read_address, read_size, buffer));

  if (header_to_verify.checksum != checksum()) {
    PW_LOG_ERROR("Expected checksum %08" PRIx32 ", found %08" PRIx32,
                 checksum(),
                 header_to_verify.checksum);
    return Status::DATA_LOSS;
  }

  if (checksum_ == nullptr) {
    return checksum() == 0 ? Status::OK : Status::DATA_LOSS;
  }

  // The checksum is calculated as if the header's checksum field were 0.
  header_to_verify.checksum = 0;

  checksum_->Reset();

  while (true) {
    // Add the chunk in the buffer to the checksum.
    checksum_->Update(buffer, read_size);

    bytes_to_read -= read_size;
    if (bytes_to_read == 0u) {
      break;
    }

    // Read the next chunk into the buffer.
    read_address += read_size;
    read_size = std::min(sizeof(buffer), bytes_to_read);
    TRY(partition().Read(read_address, read_size, buffer));
  }

  checksum_->Finish();
  return checksum_->Verify(checksum_bytes());
}

void Entry::DebugLog() {
  PW_LOG_DEBUG("Header: ");
  PW_LOG_DEBUG("   Address      = 0x%zx", size_t(address_));
  PW_LOG_DEBUG("   Magic        = 0x%zx", size_t(magic()));
  PW_LOG_DEBUG("   Checksum     = 0x%zx", size_t(checksum()));
  PW_LOG_DEBUG("   Key length   = 0x%zx", size_t(key_length()));
  PW_LOG_DEBUG("   Value length = 0x%zx", size_t(value_size()));
  PW_LOG_DEBUG("   Entry size   = 0x%zx", size_t(size()));
  PW_LOG_DEBUG("   Alignment    = 0x%zx", size_t(alignment_bytes()));
}

span<const byte> Entry::CalculateChecksum(const string_view key,
                                          span<const byte> value) const {
  checksum_->Reset();

  {
    EntryHeader header_for_checksum = header_;
    header_for_checksum.checksum = 0;

    checksum_->Update(&header_for_checksum, sizeof(header_for_checksum));
    checksum_->Update(as_bytes(span(key)));
    checksum_->Update(value);
  }

  // Update the checksum with 0s to pad the entry to its alignment boundary.
  constexpr byte padding[kMinAlignmentBytes - 1] = {};
  size_t padding_to_add = Padding(content_size(), alignment_bytes());

  while (padding_to_add > 0u) {
    const size_t chunk_size = std::min(padding_to_add, sizeof(padding));
    checksum_->Update(padding, chunk_size);
    padding_to_add -= chunk_size;
  }

  return checksum_->Finish();
}

}  // namespace pw::kvs::internal
