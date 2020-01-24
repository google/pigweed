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
#pragma once

#include <algorithm>
#include <type_traits>

#include "pw_kvs/devices/flash_memory.h"
#include "pw_kvs/logging.h"
#include "pw_kvs/os/mutex.h"
#include "pw_kvs/status.h"

namespace pw {

// This object is very large and should not be placed on the stack.
class KeyValueStore {
 public:
  KeyValueStore(FlashPartition* partition) : partition_(*partition) {}

  // Enable the KVS, scans the sectors of the partition for any current KVS
  // data. Erases and initializes any sectors which are not initialized.
  // Checks the CRC of all data in the KVS; on failure the corrupted data is
  // lost and Enable will return Status::DATA_LOSS, but the KVS will still load
  // other data and will be enabled.
  Status Enable();
  bool IsEnabled() const { return enabled_; }
  void Disable() {
    if (enabled_ == false) {
      return;
    }
    os::MutexLock lock(&lock_);
    enabled_ = false;
  }

  // Erase a key value element.
  // key is a null terminated c string.
  // Returns OK if erased
  //         NOT_FOUND if key was not found.
  Status Erase(const char* key);

  // Copy the first size bytes of key's value into value buffer.
  // key is a null terminated c string. Size is the amount of bytes to read,
  // Optional offset argument supports reading size bytes starting at the
  // specified offset.
  // Returns OK if successful
  //         NOT_FOUND if key was not found
  //         DATA_LOSS if the value failed crc check
  Status Get(const char* key, void* value, uint16_t size, uint16_t offset = 0);

  // Interpret the key's value as the given type and return it.
  // Returns OK if successful
  //         NOT_FOUND if key was not found
  //         DATA_LOSS if the value failed crc check
  //         INVALID_ARGUMENT if size of value != sizeof(T)
  template <typename T>
  Status Get(const char* key, T* value) {
    static_assert(std::is_trivially_copyable<T>(), "KVS values must copyable");
    static_assert(!std::is_pointer<T>(), "KVS values cannot be pointers");
    uint16_t value_size = 0;
    RETURN_IF_ERROR(GetValueSize(key, &value_size));
    RETURN_STATUS_IF(value_size != sizeof(T), Status::INVALID_ARGUMENT);
    return Get(key, value, sizeof(T));
  }

  // Writes the key value store to the partition. If the key already exists it
  // will be deleted before writing the new value.
  // key is a null terminated c string.
  // Returns OK if successful
  //         RESOURCE_EXHAUSTED if there is not enough available space
  Status Put(const char* key, const void* value, uint16_t size);

  // Writes the value to the partition. If the key already exists it will be
  // deleted before writing the new value.
  // key is a null terminated c string.
  // Returns OK if successful
  //         RESOURCE_EXHAUSTED if there is not enough available space
  template <typename T>
  Status Put(const char* key, const T& value) {
    static_assert(std::is_trivially_copyable<T>(), "KVS values must copyable");
    static_assert(!std::is_pointer<T>(), "KVS values cannot be pointers");
    return Put(key, &value, sizeof(T));
  }

  // Gets the size of the value stored for provided key.
  // Returns OK if successful
  //         INVALID_ARGUMENT if args are invalid.
  //         FAILED_PRECONDITION if KVS is not enabled.
  //         NOT_FOUND if key was not found.
  Status GetValueSize(const char* key, uint16_t* value_size);

  // Tests if the proposed key value entry can be stored in the KVS.
  bool CanFitEntry(uint16_t key_len, uint16_t value_len) {
    return kSectorInvalid != FindSpace(ChunkSize(key_len, value_len));
  }

  // CleanAll cleans each sector which is currently marked for cleaning.
  // Note: if any data is invalid/corrupt it could be lost.
  Status CleanAll() {
    os::MutexLock lock(&lock_);
    return CleanAllInternal();
  }
  size_t PendingCleanCount() {
    os::MutexLock lock(&lock_);
    size_t ret = 0;
    for (size_t i = 0; i < SectorCount(); i++) {
      ret += sector_space_remaining_[i] == 0 ? 1 : 0;
    }
    return ret;
  }

  // Clean a single sector and return, if all sectors are clean it will set
  // all_sectors_have_been_cleaned to true and return.
  Status CleanOneSector(bool* all_sectors_have_been_cleaned);

  // For debugging, logging, and testing. (Don't use in regular code)
  // Note: a key_index is not valid after an element is erased or updated.
  uint8_t KeyCount() const;
  const char* GetKey(uint8_t idx) const;
  uint16_t GetValueSize(uint8_t idx) const;
  size_t GetMaxKeys() const { return kListCapacityMax; }
  bool HasEmptySector() const { return HaveEmptySectorImpl(); }

  static constexpr size_t kHeaderSize = 8;  // Sector and KVS Header size
  static constexpr uint16_t MaxValueLength() { return kChunkValueLengthMax; }
  KeyValueStore(const KeyValueStore&) = delete;
  KeyValueStore& operator=(const KeyValueStore&) = delete;

 private:
  using KeyIndex = uint8_t;
  using SectorIndex = uint32_t;

  static constexpr uint16_t kVersion = 4;
  static constexpr KeyIndex kListCapacityMax = cfg::kKvsMaxKeyCount;
  static constexpr SectorIndex kSectorCountMax = cfg::kKvsMaxSectorCount;

  // Number of bits for fields in KVSHeader:
  static constexpr int kChunkHeaderKeyFieldNumBits = 4;
  static constexpr int kChunkHeaderChunkFieldNumBits = 12;

  static constexpr uint16_t kSectorReadyValue = 0xABCD;
  static constexpr uint16_t kChunkSyncValue = 0x55AA;

  static constexpr uint16_t kChunkLengthMax =
      ((1 << kChunkHeaderChunkFieldNumBits) - 1);
  static constexpr uint16_t kChunkKeyLengthMax =
      ((1 << kChunkHeaderKeyFieldNumBits) - 1);
  static constexpr uint16_t kChunkValueLengthMax =
      (kChunkLengthMax - kChunkKeyLengthMax);

  static constexpr SectorIndex kSectorInvalid = 0xFFFFFFFFul;
  static constexpr FlashPartition::Address kAddressInvalid = 0xFFFFFFFFul;
  static constexpr uint64_t kSectorCleanNotPending = 0xFFFFFFFFFFFFFFFFull;

  // TODO: Use BitPacker if/when have more flags.
  static constexpr uint16_t kFlagsIsErasedMask = 0x0001;
  static constexpr uint16_t kMaxAlignmentBytes = 128;

  // This packs into 16 bytes.
  struct KvsSectorHeaderMeta {
    uint16_t synchronize_token;
    uint16_t version;
    uint16_t alignment_bytes;  // alignment used for each chunk in this sector.
    uint16_t padding;          // padding to support uint64_t alignment.
  } __attribute__((__packed__));
  static_assert(sizeof(KvsSectorHeaderMeta) == kHeaderSize,
                "Invalid KvsSectorHeaderMeta size");

  // sector_clean_pending is broken off from KvsSectorHeaderMeta to support
  // larger than sizeof(KvsSectorHeaderMeta) flash write alignments.
  struct KvsSectorHeaderCleaning {
    // When this sector is not marked for cleaning this will be in the erased
    // state. When marked for clean this value indicates the order the sectors
    // were marked for cleaning, and therfore which data is the newest.
    // To remain backwards compatible with v2 and v3 KVS this is 8 bytes, if the
    // alignment is greater than 8 we will check the entire alignment is in the
    // default erased state.
    uint64_t sector_clean_order;
  };
  static_assert(sizeof(KvsSectorHeaderCleaning) == kHeaderSize,
                "Invalid KvsSectorHeaderCleaning size");

  // This packs into 8 bytes, to support uint64_t alignment.
  struct KvsHeader {
    uint16_t synchronize_token;
    uint16_t crc;  // Crc of the key + data (Ignoring any padding bytes)
    // flags is a Bitmask: bits 15-1 reserved = 0,
    // bit 0: is_erased [0 when not erased, 1 when erased]
    uint16_t flags;
    // On little endian the length fields map to memory as follows:
    // byte array: [ 0x(c0to3 k0to3) 0x(c8to11 c4to7) ]
    // Key length does not include trailing zero. That is not stored.
    uint16_t key_len : kChunkHeaderKeyFieldNumBits;
    // Chunk length is the total length of the chunk before alignment.
    // That way we can work out the length of the value as:
    // (chunk length - key length - size of chunk header).
    uint16_t chunk_len : kChunkHeaderChunkFieldNumBits;
  } __attribute__((__packed__));
  static_assert(sizeof(KvsHeader) == kHeaderSize, "Invalid KvsHeader size");

  struct KeyMap {
    char key[kChunkKeyLengthMax + 1];  // + 1 for null terminator
    FlashPartition::Address address;
    uint16_t chunk_len;
    bool is_erased;
  };

  // NOTE: All public APIs handle the locking, the internal methods assume the
  // lock has already been acquired.

  SectorIndex AddressToSectorIndex(FlashPartition::Address address) const {
    return address / partition_.GetSectorSizeBytes();
  }

  FlashPartition::Address SectorIndexToAddress(SectorIndex index) const {
    return index * partition_.GetSectorSizeBytes();
  }

  // Returns kAddressInvalid if no space is found, otherwise the address.
  FlashPartition::Address FindSpace(uint16_t requested_size) const;

  // Attempts to rewrite a key's value by appending the new value to the same
  // sector. If the sector is full the value is written to another sector, and
  // the sector is marked for cleaning.
  // Returns RESOURCE_EXHAUSTED if no space is available, OK otherwise.
  Status RewriteValue(KeyIndex key_index,
                      const uint8_t* value,
                      uint16_t size,
                      bool is_erased = false);

  bool ValueMatches(KeyIndex key_index,
                    const uint8_t* value,
                    uint16_t size,
                    bool is_erased);

  // ResetSector erases the sector and writes the sector header.
  Status ResetSector(SectorIndex sector_index);
  Status WriteKeyValue(FlashPartition::Address address,
                       const char* key,
                       const uint8_t* value,
                       uint16_t size,
                       bool is_erased = false);
  uint32_t SectorSpaceRemaining(SectorIndex sector_index) const;

  // Returns idx if key is found, otherwise kListCapacityMax.
  KeyIndex FindKeyInMap(const char* key) const;
  bool IsKeyInMap(const char* key) const {
    return FindKeyInMap(key) != kListCapacityMax;
  }

  void RemoveFromMap(KeyIndex key_index);
  Status AppendToMap(const char* key,
                     FlashPartition::Address address,
                     uint16_t chunk_len,
                     bool is_erased = false);
  void UpdateMap(KeyIndex key_index,
                 FlashPartition::Address address,
                 uint16_t chunk_len,
                 bool is_erased = false);
  uint16_t CalculateCrc(const char* key,
                        uint16_t key_size,
                        const uint8_t* value,
                        uint16_t value_size) const;

  // Calculates the CRC by reading the value from flash in chunks.
  Status CalculateCrcFromValueAddress(const char* key,
                                      uint16_t key_size,
                                      FlashPartition::Address value_address,
                                      uint16_t value_size,
                                      uint16_t* crc_ret);

  uint16_t RoundUpForAlignment(uint16_t size) const {
    if (size % alignment_bytes_ != 0) {
      return size + alignment_bytes_ - size % alignment_bytes_;
    }
    return size;
  }

  // The buffer is chosen to be no larger then the config value, and
  // be a multiple of the flash alignment.
  size_t TempBufferAlignedSize() const {
    CHECK_GE(sizeof(temp_buffer_), partition_.GetAlignmentBytes());
    return sizeof(temp_buffer_) -
           (sizeof(temp_buffer_) % partition_.GetAlignmentBytes());
  }

  // Moves a key/value chunk from one address to another, all fields expected to
  // be aligned.
  Status MoveChunk(FlashPartition::Address dest_address,
                   FlashPartition::Address src_address,
                   uint16_t size);

  // Size of a chunk including header, key, value, and alignment padding.
  uint16_t ChunkSize(uint16_t key_len, uint16_t chunk_len) const {
    return RoundUpForAlignment(sizeof(KvsHeader)) +
           RoundUpForAlignment(key_len) + RoundUpForAlignment(chunk_len);
  }

  // Sectors should be cleaned when full, every valid (Most recent, not erased)
  // chunk of data is moved to another sector and the sector is erased.
  // TODO: Clean sectors with lots of invalid data, without a rewrite
  // or erase triggering it.
  Status MarkSectorForClean(SectorIndex sector);
  Status CleanSector(SectorIndex sector);
  Status CleanAllInternal();
  Status GarbageCollectImpl(bool clean_pending_sectors,
                            bool exit_when_have_free_sector);
  Status FullGarbageCollect();
  Status EnforceFreeSector();

  SectorIndex SectorCount() const {
    return std::min(partition_.GetSectorCount(), kSectorCountMax);
  }

  size_t SectorSpaceAvailableWhenEmpty() const {
    return partition_.GetSectorSizeBytes() -
           RoundUpForAlignment(sizeof(KvsSectorHeaderMeta)) -
           RoundUpForAlignment(sizeof(KvsSectorHeaderCleaning));
  }

  bool HaveEmptySectorImpl(SectorIndex skip_sector = kSectorInvalid) const {
    for (SectorIndex i = 0; i < SectorCount(); i++) {
      if (i != skip_sector &&
          sector_space_remaining_[i] == SectorSpaceAvailableWhenEmpty()) {
        return true;
      }
    }
    return false;
  }

  bool IsInLastFreeSector(FlashPartition::Address address) {
    return sector_space_remaining_[AddressToSectorIndex(address)] ==
               SectorSpaceAvailableWhenEmpty() &&
           !HaveEmptySectorImpl(AddressToSectorIndex(address));
  }

  FlashPartition& partition_;
  os::Mutex lock_;
  bool enabled_ = false;
  uint8_t alignment_bytes_ = 0;
  uint64_t next_sector_clean_order_ = 0;

  // Free space available in each sector, set to 0 when clean is pending/active
  uint32_t sector_space_remaining_[kSectorCountMax] = {0};
  uint64_t sector_clean_order_[kSectorCountMax] = {kSectorCleanNotPending};
  KeyMap key_map_[kListCapacityMax] = {{{0}}};
  KeyIndex map_size_ = 0;

  // +1 for nul-terminator since keys are stored as Length + Value and no nul
  // termination but we are using them as nul-terminated strings through
  // loading-up and comparing the keys.
  char temp_key_buffer_[kChunkKeyLengthMax + 1u] = {0};
  uint8_t temp_buffer_[cfg::kKvsBufferSize] = {0};
};

}  // namespace pw
