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

// KVS is a key-value storage format for flash based memory.
//
// Currently it stores key-value sets in chunks aligned with the flash memory.
// Each chunk contains a header (KvsHeader), which is immediately followed by
// the key, and then the data blob. To support different alignments both the
// key length and value length are rounded up to be aligned.
//
// Example memory layout of sector with two KVS chunks:
//    [ SECTOR_HEADER - Meta | alignment_padding ]
//    [ SECTOR_HEADER - Cleaning State | alignment_padding ]
//    [First Chunk Header | alignment_padding ]
//    [First Chunk Key | alignment_padding ]
//    [First Chunk Value | alignment_padding ]
//    [Second Chunk Header | alignment_padding ]
//    [Second Chunk Key | alignment_padding ]
//    [Second Chunk Value | alignment_padding ]
//
// For efficiency if a key's value is rewritten the new value is just appended
// to the same sector, a clean of the sector is only needed if there is no more
// room. Cleaning the sector involves moving the most recent value of each key
// to another sector and erasing the sector. Erasing data is treated the same
// as rewriting data, but an erased chunk has the erased flag set, and no data.
//
// KVS maintains a data structure in RAM for quick indexing of each key's most
// recent value, but no caching of data is ever performed. If a write/erase
// function returns successfully, it is guaranteed that the data has been
// pushed to flash. The KVS should also be resistant to sudden unplanned power
// outages and be capable of recovering even mid clean (this is accomplished
// using a flag which marks the sector before the clean is started).

#include "pw_kvs/key_value_store.h"

#include <cstring>

#include "pw_kvs/os/mutex.h"
#include "pw_kvs/util/ccitt_crc16.h"
#include "pw_kvs/util/constexpr.h"
#include "pw_kvs/util/flash.h"

namespace pw {

// Declare static constexpr variables so it can be used for pass-by-reference
// functions.
constexpr uint16_t KeyValueStore::kSectorReadyValue;

Status KeyValueStore::Enable() {
  os::MutexLock lock(&lock_);
  if (enabled_) {
    return Status::OK;
  }

  // Reset parameters.
  memset(sector_space_remaining_, 0, sizeof(sector_space_remaining_));
  map_size_ = 0;

  // For now alignment is set to use partitions alignment.
  alignment_bytes_ = partition_.GetAlignmentBytes();
  DCHECK(alignment_bytes_ <= kMaxAlignmentBytes);

  LOG_WARN_IF(partition_.GetSectorCount() > kSectorCountMax,
              "Partition is larger then KVS max sector count, not all space "
              "will be used.");
  // Load map and setup sectors if needed (first word isn't kSectorReadyValue).
  next_sector_clean_order_ = 0;
  for (SectorIndex i = 0; i < SectorCount(); i++) {
    KvsSectorHeaderMeta sector_header_meta;
    // Because underlying flash can be encrypted + erased, trying to readback
    // may give random values. It's important to make sure that data is not
    // erased before trying to see if there is a token match.
    bool is_sector_meta_erased;
    RETURN_IF_ERROR(partition_.IsChunkErased(
        SectorIndexToAddress(i),
        RoundUpForAlignment(sizeof(sector_header_meta)),
        &is_sector_meta_erased));
    RETURN_IF_ERROR(
        UnalignedRead(&partition_,
                      reinterpret_cast<uint8_t*>(&sector_header_meta),
                      SectorIndexToAddress(i),
                      sizeof(sector_header_meta)));

    constexpr int kVersion3 = 3;  // Version 3 only cleans 1 sector at a time.
    constexpr int kVersion2 = 2;  // Version 2 is always 1 byte aligned.
    if (is_sector_meta_erased ||
        sector_header_meta.synchronize_token != kSectorReadyValue) {
      // Sector needs to be setup
      RETURN_IF_ERROR(ResetSector(i));
      continue;
    } else if (sector_header_meta.version != kVersion &&
               sector_header_meta.version != kVersion3 &&  // Allow version 3
               sector_header_meta.version != kVersion2) {  // Allow version 2
      LOG(ERROR) << "Unsupported KVS version in sector: " << i;
      return Status::FAILED_PRECONDITION;
    }
    uint32_t sector_header_cleaning_offset =
        RoundUpForAlignment(sizeof(KvsSectorHeaderMeta));

    bool clean_not_pending;
    RETURN_IF_ERROR(partition_.IsChunkErased(
        SectorIndexToAddress(i) + sector_header_cleaning_offset,
        RoundUpForAlignment(sizeof(KvsSectorHeaderCleaning)),
        &clean_not_pending));

    if (!clean_not_pending) {
      // Sector is marked for cleaning, read the sector_clean_order
      RETURN_IF_ERROR(
          UnalignedRead(&partition_,
                        reinterpret_cast<uint8_t*>(&sector_clean_order_[i]),
                        SectorIndexToAddress(i) + sector_header_cleaning_offset,
                        sizeof(KvsSectorHeaderCleaning::sector_clean_order)));
      next_sector_clean_order_ =
          std::max(sector_clean_order_[i] + 1, next_sector_clean_order_);
    } else {
      sector_clean_order_[i] = kSectorCleanNotPending;
    }

    // Handle alignment
    if (sector_header_meta.version == kVersion2) {
      sector_header_meta.alignment_bytes = 1;
    }
    if (sector_header_meta.alignment_bytes != alignment_bytes_) {
      // NOTE: For now all sectors must have same alignment.
      LOG(ERROR) << "Sector " << i << " has unexpected alignment "
                 << alignment_bytes_
                 << " != " << sector_header_meta.alignment_bytes;
      return Status::FAILED_PRECONDITION;
    }

    // Scan through sector and add key/value pairs to map.
    FlashPartition::Address offset =
        RoundUpForAlignment(sizeof(KvsSectorHeaderMeta)) +
        RoundUpForAlignment(sizeof(KvsSectorHeaderCleaning));
    sector_space_remaining_[i] = partition_.GetSectorSizeBytes() - offset;
    while (offset < partition_.GetSectorSizeBytes() -
                        RoundUpForAlignment(sizeof(KvsHeader))) {
      FlashPartition::Address address = SectorIndexToAddress(i) + offset;

      // Read header
      KvsHeader header;
      bool is_kvs_header_erased;
      // Because underlying flash can be encrypted + erased, trying to readback
      // may give random values. It's important to make sure that data is not
      // erased before trying to see if there is a token match.
      RETURN_IF_ERROR(partition_.IsChunkErased(
          address, RoundUpForAlignment(sizeof(header)), &is_kvs_header_erased));
      RETURN_IF_ERROR(UnalignedRead(&partition_,
                                    reinterpret_cast<uint8_t*>(&header),
                                    address,
                                    sizeof(header)));
      if (is_kvs_header_erased || header.synchronize_token != kChunkSyncValue) {
        if (!is_kvs_header_erased) {
          LOG_ERROR("Next sync_token is not clear!");
          // TODO: handle this?
        }
        break;  // End of elements in sector
      }

      CHECK(header.key_len <= kChunkKeyLengthMax);
      static_assert(sizeof(temp_key_buffer_) >= (kChunkKeyLengthMax + 1u),
                    "Key buffer must be at least big enough for a key and a "
                    "nul-terminator.");

      // Read key and add to map
      RETURN_IF_ERROR(
          UnalignedRead(&partition_,
                        reinterpret_cast<uint8_t*>(&temp_key_buffer_),
                        address + RoundUpForAlignment(sizeof(header)),
                        header.key_len));
      temp_key_buffer_[header.key_len] = '\0';
      bool is_erased = header.flags & kFlagsIsErasedMask;

      KeyIndex index = FindKeyInMap(temp_key_buffer_);
      if (index == kListCapacityMax) {
        RETURN_IF_ERROR(AppendToMap(
            temp_key_buffer_, address, header.chunk_len, is_erased));
      } else if (sector_clean_order_[i] >=
                 sector_clean_order_[AddressToSectorIndex(
                     key_map_[index].address)]) {
        // The value being added is also in another sector (which is marked for
        // cleaning), but the current sector's order is larger and thefore this
        // is a newer version then what is already in the map.
        UpdateMap(index, address, header.chunk_len, is_erased);
      }

      // Increment search
      offset += ChunkSize(header.key_len, header.chunk_len);
    }
    sector_space_remaining_[i] =
        clean_not_pending ? partition_.GetSectorSizeBytes() - offset : 0;
  }

  LOG_IF_ERROR(EnforceFreeSector())
      << "Failed to force clean at boot, no free sectors available!";
  enabled_ = true;
  return Status::OK;
}

Status KeyValueStore::Get(const char* key,
                          void* raw_value,
                          uint16_t size,
                          uint16_t offset) {
  uint8_t* const value = reinterpret_cast<uint8_t*>(raw_value);

  if (key == nullptr || value == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  size_t key_len = util::StringLength(key, kChunkKeyLengthMax + 1u);
  if (key_len == 0 || key_len > kChunkKeyLengthMax) {
    return Status::INVALID_ARGUMENT;
  }

  // TODO: Support unaligned offset reads.
  if (offset % alignment_bytes_ != 0) {
    LOG_ERROR("Currently unaligned offsets are not supported");
    return Status::INVALID_ARGUMENT;
  }
  os::MutexLock lock(&lock_);
  if (!enabled_) {
    return Status::FAILED_PRECONDITION;
  }

  KeyIndex key_index = FindKeyInMap(key);
  if (key_index == kListCapacityMax || key_map_[key_index].is_erased) {
    return Status::NOT_FOUND;
  }
  KvsHeader header;
  // TODO: Could cache the CRC and avoid reading the header.
  RETURN_IF_ERROR(UnalignedRead(&partition_,
                                reinterpret_cast<uint8_t*>(&header),
                                key_map_[key_index].address,
                                sizeof(header)));
  if (kChunkSyncValue != header.synchronize_token) {
    return Status::DATA_LOSS;
  }
  if (size + offset > header.chunk_len) {
    LOG_ERROR("Out of bounds read: offset(%u) + size(%u) > data_size(%u)",
              offset,
              size,
              header.chunk_len);
    return Status::INVALID_ARGUMENT;
  }
  RETURN_IF_ERROR(UnalignedRead(
      &partition_,
      value,
      key_map_[key_index].address + RoundUpForAlignment(sizeof(KvsHeader)) +
          RoundUpForAlignment(header.key_len) + offset,
      size));

  // Verify CRC only when full packet was read.
  if (offset == 0 && size == header.chunk_len) {
    uint16_t crc = CalculateCrc(key, key_len, value, size);
    if (crc != header.crc) {
      LOG_ERROR("KVS CRC does not match for key=%s [expected %u, found %u]",
                key,
                header.crc,
                crc);
      return Status::DATA_LOSS;
    }
  }
  return Status::OK;
}

uint16_t KeyValueStore::CalculateCrc(const char* key,
                                     uint16_t key_size,
                                     const uint8_t* value,
                                     uint16_t value_size) const {
  CcittCrc16 crc;
  crc.AppendBytes(ConstBuffer(reinterpret_cast<const uint8_t*>(key), key_size));
  return crc.AppendBytes(ConstBuffer(value, value_size));
}

Status KeyValueStore::CalculateCrcFromValueAddress(
    const char* key,
    uint16_t key_size,
    FlashPartition::Address value_address,
    uint16_t value_size,
    uint16_t* crc_ret) {
  CcittCrc16 crc;
  crc.AppendBytes(ConstBuffer(reinterpret_cast<const uint8_t*>(key), key_size));
  for (size_t i = 0; i < value_size; i += TempBufferAlignedSize()) {
    auto read_size = std::min(value_size - i, TempBufferAlignedSize());
    RETURN_IF_ERROR(
        UnalignedRead(&partition_, temp_buffer_, value_address + i, read_size));
    crc.AppendBytes(ConstBuffer(temp_buffer_, read_size));
  }
  *crc_ret = crc.CurrentValue();
  return Status::OK;
}

Status KeyValueStore::Put(const char* key,
                          const void* raw_value,
                          uint16_t size) {
  const uint8_t* const value = reinterpret_cast<const uint8_t*>(raw_value);
  if (key == nullptr || value == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  size_t key_len = util::StringLength(key, (kChunkKeyLengthMax + 1u));
  if (key_len == 0 || key_len > kChunkKeyLengthMax ||
      size > kChunkValueLengthMax) {
    return Status::INVALID_ARGUMENT;
  }

  os::MutexLock lock(&lock_);
  if (!enabled_) {
    return Status::FAILED_PRECONDITION;
  }

  KeyIndex index = FindKeyInMap(key);
  if (index != kListCapacityMax) {  // Key already in map, rewrite value
    return RewriteValue(index, value, size);
  }

  FlashPartition::Address address = FindSpace(ChunkSize(key_len, size));
  if (address == kSectorInvalid) {
    return Status::RESOURCE_EXHAUSTED;
  }

  // Check if this would use the last empty sector on KVS with multiple sectors
  if (SectorCount() > 1 && IsInLastFreeSector(address)) {
    // Forcing a full garbage collect to free more sectors.
    RETURN_IF_ERROR(FullGarbageCollect());
    address = FindSpace(ChunkSize(key_len, size));
    if (address == kSectorInvalid || IsInLastFreeSector(address)) {
      // Couldn't find space, KVS is full.
      return Status::RESOURCE_EXHAUSTED;
    }
  }

  RETURN_IF_ERROR(WriteKeyValue(address, key, value, size));
  RETURN_IF_ERROR(AppendToMap(key, address, size));

  return Status::OK;
}

// Garbage collection cleans sectors to try to free up space.
// If clean_pending_sectors is true, it will only clean sectors which are
// currently pending a clean.
// If clean_pending_sectors is false, it will only clean sectors which are not
// currently pending a clean, instead it will mark them for cleaning and attempt
// a clean.
// If exit_when_have_free_sector is true, it will exit once a single free sector
// exists.
Status KeyValueStore::GarbageCollectImpl(bool clean_pending_sectors,
                                         bool exit_when_have_free_sector) {
  // Try to clean any pending sectors
  for (SectorIndex sector = 0; sector < SectorCount(); sector++) {
    if (clean_pending_sectors ==
        (sector_clean_order_[sector] != kSectorCleanNotPending)) {
      if (!clean_pending_sectors) {
        RETURN_IF_ERROR(MarkSectorForClean(sector));
      }
      Status status = CleanSector(sector);
      if (!status.ok() && status != Status::RESOURCE_EXHAUSTED) {
        return status;
      }
      if (exit_when_have_free_sector && HaveEmptySectorImpl()) {
        return Status::OK;  // Now have a free sector
      }
    }
  }
  return Status::OK;
}

Status KeyValueStore::EnforceFreeSector() {
  if (SectorCount() == 1 || HasEmptySector()) {
    return Status::OK;
  }
  LOG_INFO("KVS garbage collecting to get a free sector");
  RETURN_IF_ERROR(GarbageCollectImpl(true /*clean_pending_sectors*/,
                                     true /*exit_when_have_free_sector*/));
  if (HasEmptySector()) {
    return Status::OK;
  }
  LOG_INFO("KVS: trying to clean non-pending sectors for more space");
  RETURN_IF_ERROR(GarbageCollectImpl(false /*clean_pending_sectors*/,
                                     true /*exit_when_have_free_sector*/));
  return HaveEmptySectorImpl() ? Status::OK : Status::RESOURCE_EXHAUSTED;
}

Status KeyValueStore::FullGarbageCollect() {
  LOG_INFO("KVS: Full garbage collecting to try to free space");
  RETURN_IF_ERROR(GarbageCollectImpl(true /*clean_pending_sectors*/,
                                     false /*exit_when_have_free_sector*/));
  return GarbageCollectImpl(false /*clean_pending_sectors*/,
                            false /*exit_when_have_free_sector*/);
}

Status KeyValueStore::RewriteValue(KeyIndex key_index,
                                   const uint8_t* value,
                                   uint16_t size,
                                   bool is_erased) {
  // Compare values, return if values are the same.
  if (ValueMatches(key_index, value, size, is_erased)) {
    return Status::OK;
  }

  size_t key_length =
      util::StringLength(key_map_[key_index].key, (kChunkKeyLengthMax + 1u));
  RETURN_STATUS_IF(key_length > kChunkKeyLengthMax, Status::INTERNAL);

  uint32_t space_required = ChunkSize(key_length, size);
  SectorIndex sector = AddressToSectorIndex(key_map_[key_index].address);
  uint32_t sector_space_remaining = SectorSpaceRemaining(sector);

  FlashPartition::Address address = kSectorInvalid;
  if (sector_space_remaining >= space_required) {
    // Space available within sector, append to end
    address = SectorIndexToAddress(sector) + partition_.GetSectorSizeBytes() -
              sector_space_remaining;
  } else {
    // No space in current sector, mark sector for clean and use another sector.
    RETURN_IF_ERROR(MarkSectorForClean(sector));
    address = FindSpace(ChunkSize(key_length, size));
  }
  if (address == kSectorInvalid) {
    return Status::RESOURCE_EXHAUSTED;
  }
  RETURN_IF_ERROR(
      WriteKeyValue(address, key_map_[key_index].key, value, size, is_erased));
  UpdateMap(key_index, address, size, is_erased);

  return EnforceFreeSector();
}

bool KeyValueStore::ValueMatches(KeyIndex index,
                                 const uint8_t* value,
                                 uint16_t size,
                                 bool is_erased) {
  // Compare sizes of CRC.
  if (size != key_map_[index].chunk_len) {
    return false;
  }
  KvsHeader header;
  UnalignedRead(&partition_,
                reinterpret_cast<uint8_t*>(&header),
                key_map_[index].address,
                sizeof(header));
  uint8_t key_len =
      util::StringLength(key_map_[index].key, (kChunkKeyLengthMax + 1u));
  if (key_len > kChunkKeyLengthMax) {
    return false;
  }

  if ((header.flags & kFlagsIsErasedMask) != is_erased) {
    return false;
  } else if ((header.flags & kFlagsIsErasedMask) && is_erased) {
    return true;
  }

  // Compare checksums.
  if (header.crc != CalculateCrc(key_map_[index].key, key_len, value, size)) {
    return false;
  }
  FlashPartition::Address address = key_map_[index].address +
                                    RoundUpForAlignment(sizeof(KvsHeader)) +
                                    RoundUpForAlignment(key_len);
  // Compare full values.
  for (size_t i = 0; i < key_map_[index].chunk_len;
       i += TempBufferAlignedSize()) {
    auto read_size =
        std::min(key_map_[index].chunk_len - i, TempBufferAlignedSize());
    auto status =
        UnalignedRead(&partition_, temp_buffer_, address + i, read_size);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to read chunk: " << status;
      return false;
    }
    if (memcmp(value + i, temp_buffer_, read_size) != 0) {
      return false;
    }
  }
  return true;
}

Status KeyValueStore::Erase(const char* key) {
  if (key == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  size_t key_len = util::StringLength(key, (kChunkKeyLengthMax + 1u));
  if (key_len == 0 || key_len > kChunkKeyLengthMax) {
    return Status::INVALID_ARGUMENT;
  }
  os::MutexLock lock(&lock_);
  if (!enabled_) {
    return Status::FAILED_PRECONDITION;
  }

  KeyIndex key_index = FindKeyInMap(key);
  if (key_index == kListCapacityMax || key_map_[key_index].is_erased) {
    return Status::NOT_FOUND;
  }
  return RewriteValue(key_index, nullptr, 0, true);
}

Status KeyValueStore::ResetSector(SectorIndex sector_index) {
  KvsSectorHeaderMeta sector_header = {.synchronize_token = kSectorReadyValue,
                                       .version = kVersion,
                                       .alignment_bytes = alignment_bytes_,
                                       .padding = 0xFFFF};
  bool sector_erased = false;
  partition_.IsChunkErased(SectorIndexToAddress(sector_index),
                           partition_.GetSectorSizeBytes(),
                           &sector_erased);
  auto status = partition_.Erase(SectorIndexToAddress(sector_index), 1);

  // If erasure failed, check first to see if it's merely unimplemented
  // (as in sub-sector KVSs).
  if (!status.ok() && !(status == Status::UNIMPLEMENTED && sector_erased)) {
    return status;
  }

  RETURN_IF_ERROR(PaddedWrite(&partition_,
                              SectorIndexToAddress(sector_index),
                              reinterpret_cast<const uint8_t*>(&sector_header),
                              sizeof(sector_header)));

  // Update space remaining
  sector_clean_order_[sector_index] = kSectorCleanNotPending;
  sector_space_remaining_[sector_index] = SectorSpaceAvailableWhenEmpty();
  return Status::OK;
}

Status KeyValueStore::WriteKeyValue(FlashPartition::Address address,
                                    const char* key,
                                    const uint8_t* value,
                                    uint16_t size,
                                    bool is_erased) {
  uint16_t key_length = util::StringLength(key, (kChunkKeyLengthMax + 1u));
  RETURN_STATUS_IF(key_length > kChunkKeyLengthMax, Status::INTERNAL);

  constexpr uint16_t kFlagDefaultValue = 0;
  KvsHeader header = {
      .synchronize_token = kChunkSyncValue,
      .crc = CalculateCrc(key, key_length, value, size),
      .flags = is_erased ? kFlagsIsErasedMask : kFlagDefaultValue,
      .key_len = key_length,
      .chunk_len = size};

  SectorIndex sector = AddressToSectorIndex(address);
  RETURN_IF_ERROR(PaddedWrite(&partition_,
                              address,
                              reinterpret_cast<uint8_t*>(&header),
                              sizeof(header)));
  address += RoundUpForAlignment(sizeof(header));
  RETURN_IF_ERROR(PaddedWrite(
      &partition_, address, reinterpret_cast<const uint8_t*>(key), key_length));
  address += RoundUpForAlignment(key_length);
  if (size > 0) {
    RETURN_IF_ERROR(PaddedWrite(&partition_, address, value, size));
  }
  sector_space_remaining_[sector] -= ChunkSize(key_length, size);
  return Status::OK;
}

Status KeyValueStore::MoveChunk(FlashPartition::Address dest_address,
                                FlashPartition::Address src_address,
                                uint16_t size) {
  DCHECK_EQ(src_address % partition_.GetAlignmentBytes(), 0);
  DCHECK_EQ(dest_address % partition_.GetAlignmentBytes(), 0);
  DCHECK_EQ(size % partition_.GetAlignmentBytes(), 0);

  // Copy data over in chunks to reduce the size of the temporary buffer
  for (size_t i = 0; i < size; i += TempBufferAlignedSize()) {
    size_t move_size = std::min(size - i, TempBufferAlignedSize());
    DCHECK_EQ(move_size % alignment_bytes_, 0);
    RETURN_IF_ERROR(partition_.Read(temp_buffer_, src_address + i, move_size));
    RETURN_IF_ERROR(
        partition_.Write(dest_address + i, temp_buffer_, move_size));
  }
  return Status::OK;
}

Status KeyValueStore::MarkSectorForClean(SectorIndex sector) {
  if (sector_clean_order_[sector] != kSectorCleanNotPending) {
    return Status::OK;  // Sector is already marked for clean
  }

  // Flag the sector as clean being active. This ensures we can handle losing
  // power while a clean is active.
  const KvsSectorHeaderCleaning kValue = {next_sector_clean_order_};
  RETURN_IF_ERROR(
      PaddedWrite(&partition_,
                  SectorIndexToAddress(sector) +
                      RoundUpForAlignment(sizeof(KvsSectorHeaderMeta)),
                  reinterpret_cast<const uint8_t*>(&kValue),
                  sizeof(kValue)));
  sector_space_remaining_[sector] = 0;
  sector_clean_order_[sector] = next_sector_clean_order_;
  next_sector_clean_order_++;
  return Status::OK;
}

Status KeyValueStore::CleanSector(SectorIndex sector) {
  // Iterate through the map, for each valid element which is in this sector:
  //    - Find space in another sector which can fit this chunk.
  //    - Add to the other sector and update the map.
  for (KeyValueStore::KeyIndex i = 0; i < map_size_; i++) {
    // If this key is an erased chunk don't need to move it.
    while (i < map_size_ &&
           sector == AddressToSectorIndex(key_map_[i].address) &&
           key_map_[i].is_erased) {  // Remove this key from the map.
      RemoveFromMap(i);
      // NOTE: i is now a new key, and map_size_ has been decremented.
    }

    if (i < map_size_ && sector == AddressToSectorIndex(key_map_[i].address)) {
      uint8_t key_len =
          util::StringLength(key_map_[i].key, (kChunkKeyLengthMax + 1u));
      FlashPartition::Address address = key_map_[i].address;
      auto size = ChunkSize(key_len, key_map_[i].chunk_len);
      FlashPartition::Address move_address = FindSpace(size);
      if (move_address == kSectorInvalid) {
        return Status::RESOURCE_EXHAUSTED;
      }
      RETURN_IF_ERROR(MoveChunk(move_address, address, size));
      sector_space_remaining_[AddressToSectorIndex(move_address)] -= size;
      key_map_[i].address = move_address;  // Update map
    }
  }
  ResetSector(sector);
  return Status::OK;
}

Status KeyValueStore::CleanOneSector(bool* all_sectors_have_been_cleaned) {
  if (all_sectors_have_been_cleaned == nullptr) {
    return Status::INVALID_ARGUMENT;
  }
  os::MutexLock lock(&lock_);
  bool have_cleaned_sector = false;
  for (SectorIndex sector = 0; sector < SectorCount(); sector++) {
    if (sector_clean_order_[sector] != kSectorCleanNotPending) {
      if (have_cleaned_sector) {  // only clean 1 sector
        *all_sectors_have_been_cleaned = false;
        return Status::OK;
      }
      RETURN_IF_ERROR(CleanSector(sector));
      have_cleaned_sector = true;
    }
  }
  *all_sectors_have_been_cleaned = true;
  return Status::OK;
}

Status KeyValueStore::CleanAllInternal() {
  for (SectorIndex sector = 0; sector < SectorCount(); sector++) {
    if (sector_clean_order_[sector] != kSectorCleanNotPending) {
      RETURN_IF_ERROR(CleanSector(sector));
    }
  }
  return Status::OK;
}

FlashPartition::Address KeyValueStore::FindSpace(
    uint16_t requested_size) const {
  if (requested_size > SectorSpaceAvailableWhenEmpty()) {
    return kSectorInvalid;  // This would never fit
  }
  // Iterate through sectors, find first available sector with enough space.
  SectorIndex first_empty_sector = kSectorInvalid;
  for (SectorIndex i = 0; i < SectorCount(); i++) {
    uint32_t space_remaining = SectorSpaceRemaining(i);
    if (space_remaining == SectorSpaceAvailableWhenEmpty() &&
        first_empty_sector == kSectorInvalid) {
      // Skip the first empty sector to encourage keeping one sector free.
      first_empty_sector = i;
      continue;
    }
    if (space_remaining >= requested_size) {
      return SectorIndexToAddress(i) + partition_.GetSectorSizeBytes() -
             space_remaining;
    }
  }
  // Use first empty sector if that is all that is available.
  if (first_empty_sector != kSectorInvalid) {
    return SectorIndexToAddress(first_empty_sector) +
           partition_.GetSectorSizeBytes() - SectorSpaceAvailableWhenEmpty();
  }
  return kSectorInvalid;
}

uint32_t KeyValueStore::SectorSpaceRemaining(SectorIndex sector_index) const {
  return sector_space_remaining_[sector_index];
}

Status KeyValueStore::GetValueSize(const char* key, uint16_t* value_size) {
  if (key == nullptr || value_size == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  size_t key_len = util::StringLength(key, (kChunkKeyLengthMax + 1u));
  if (key_len == 0 || key_len > kChunkKeyLengthMax) {
    return Status::INVALID_ARGUMENT;
  }
  os::MutexLock lock(&lock_);
  if (!enabled_) {
    return Status::FAILED_PRECONDITION;
  }

  uint8_t idx = FindKeyInMap(key);
  if (idx == kListCapacityMax || key_map_[idx].is_erased) {
    return Status::NOT_FOUND;
  }
  *value_size = key_map_[idx].chunk_len;
  return Status::OK;
}

Status KeyValueStore::AppendToMap(const char* key,
                                  FlashPartition::Address address,
                                  uint16_t chunk_len,
                                  bool is_erased) {
  if (map_size_ >= kListCapacityMax) {
    LOG_ERROR("Can't add: reached max supported keys %d", kListCapacityMax);
    return Status::INTERNAL;
  }

  // Copy incoming key into map entry, ensuring size checks and nul-termination.
  StringBuilder key_builder(key_map_[map_size_].key,
                            sizeof(key_map_[map_size_].key));
  key_builder.append(key);

  if (!key_builder.status().ok()) {
    LOG_ERROR("Can't add: got invalid key: %s!", key_builder.status().str());
    return Status::INTERNAL;
  }

  key_map_[map_size_].address = address;
  key_map_[map_size_].chunk_len = chunk_len;
  key_map_[map_size_].is_erased = is_erased;
  map_size_++;

  return Status::OK;
}

KeyValueStore::KeyIndex KeyValueStore::FindKeyInMap(const char* key) const {
  for (KeyIndex i = 0; i < map_size_; i++) {
    if (strncmp(key, key_map_[i].key, sizeof(key_map_[i].key)) == 0) {
      return i;
    }
  }
  return kListCapacityMax;
}

void KeyValueStore::UpdateMap(KeyIndex index,
                              FlashPartition::Address address,
                              uint16_t chunk_len,
                              bool is_erased) {
  key_map_[index].address = address;
  key_map_[index].chunk_len = chunk_len;
  key_map_[index].is_erased = is_erased;
}

void KeyValueStore::RemoveFromMap(KeyIndex key_index) {
  key_map_[key_index] = key_map_[--map_size_];
}

uint8_t KeyValueStore::KeyCount() const {
  uint8_t count = 0;
  for (unsigned i = 0; i < map_size_; i++) {
    count += key_map_[i].is_erased ? 0 : 1;
  }
  return count;
}

const char* KeyValueStore::GetKey(uint8_t idx) const {
  uint8_t count = 0;
  for (unsigned i = 0; i < map_size_; i++) {
    if (!key_map_[i].is_erased) {
      if (idx == count) {
        return key_map_[i].key;
      }
      count++;
    }
  }
  return nullptr;
}

uint16_t KeyValueStore::GetValueSize(uint8_t idx) const {
  uint8_t count = 0;
  for (unsigned i = 0; i < map_size_; i++) {
    if (!key_map_[i].is_erased) {
      if (idx == count++) {
        return key_map_[i].chunk_len;
      }
    }
  }
  return 0;
}

}  // namespace pw
