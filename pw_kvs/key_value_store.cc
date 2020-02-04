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

#include "pw_kvs/key_value_store.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

#define PW_LOG_USE_ULTRA_SHORT_NAMES 1
#include "pw_kvs_private/format.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {

using std::byte;
using std::string_view;

namespace {

using Address = FlashPartition::Address;

// constexpr uint32_t SixFiveFiveNineNine(std::string_view string) {
constexpr uint32_t HashKey(std::string_view string) {
  uint32_t hash = 0;
  uint32_t coefficient = 65599u;

  for (char ch : string) {
    hash += coefficient * unsigned(ch);
    coefficient *= 65599u;
  }

  return hash;
}

constexpr size_t EntrySize(string_view key, span<const byte> value) {
  return sizeof(EntryHeader) + key.size() + value.size();
}

constexpr size_t EntrySize(const EntryHeader& header) {
  return sizeof(EntryHeader) + header.key_length() + header.value_length();
}

}  // namespace

Status KeyValueStore::Init() {
  // Reset the number of occupied key descriptors; we will fill them later.
  key_descriptor_list_size_ = 0;

  const size_t sector_size_bytes = partition_.sector_size_bytes();
  const size_t sector_count = partition_.sector_count();

  if (working_buffer_.size() < sector_size_bytes) {
    CRT("ERROR: working_buffer_ (%zu bytes) is smaller than sector "
        "size (%zu bytes)",
        working_buffer_.size(),
        sector_size_bytes);
    return Status::INVALID_ARGUMENT;
  }

  DBG("First pass: Read all entries from all sectors");
  for (size_t sector_id = 0; sector_id < sector_count; ++sector_id) {
    // Track writable bytes in this sector. Updated after reading each entry.
    sector_map_[sector_id].tail_free_bytes = sector_size_bytes;

    const Address sector_address = sector_id * sector_size_bytes;
    Address entry_address = sector_address;

    for (int num_entries_in_sector = 0;; num_entries_in_sector++) {
      DBG("Load entry: sector=%zu, entry#=%d, address=%zu",
          sector_id,
          num_entries_in_sector,
          size_t(entry_address));

      if (!AddressInSector(sector_map_[sector_id], entry_address)) {
        DBG("Fell off end of sector; moving to the next sector");
        break;
      }

      Address next_entry_address;
      Status status = LoadEntry(entry_address, &next_entry_address);
      if (status == Status::NOT_FOUND) {
        DBG("Hit un-written data in sector; moving to the next sector");
        break;
      }
      if (status == Status::DATA_LOSS) {
        // It's not clear KVS can make a unilateral decision about what to do
        // in corruption cases. It's an application decision, for which we
        // should offer some configurability. For now, entirely bail out of
        // loading and give up.
        //
        // Later, scan for remaining valid keys; since it's entirely possible
        // that there is a duplicate of the key elsewhere and everything is
        // fine. Later, we can wipe and maybe recover the sector.
        //
        // TODO: Implement rest-of-sector scanning for valid entries.
        return Status::DATA_LOSS;
      }
      TRY(status);

      // Entry loaded successfully; so get ready to load the next one.
      entry_address = next_entry_address;

      // Update of the number of writable bytes in this sector.
      sector_map_[sector_id].tail_free_bytes =
          sector_size_bytes - (entry_address - sector_address);
    }
  }

  DBG("Second pass: Count valid bytes in each sector");
  // Initialize the sector sizes.
  for (size_t sector_id = 0; sector_id < sector_count; ++sector_id) {
    sector_map_[sector_id].valid_bytes = 0;
  }
  // For every valid key, increment the valid bytes for that sector.
  for (size_t key_id = 0; key_id < key_descriptor_list_size_; ++key_id) {
    uint32_t sector_id =
        key_descriptor_list_[key_id].address / sector_size_bytes;
    KeyDescriptor key_descriptor;
    key_descriptor.address = key_descriptor_list_[key_id].address;
    EntryHeader header;
    TRY(ReadEntryHeader(key_descriptor, &header));
    sector_map_[sector_id].valid_bytes += header.entry_size();
  }
  enabled_ = true;
  return Status::OK;
}

Status KeyValueStore::LoadEntry(Address entry_address,
                                Address* next_entry_address) {
  const size_t alignment_bytes = partition_.alignment_bytes();

  KeyDescriptor key_descriptor;
  key_descriptor.address = entry_address;

  EntryHeader header;
  TRY(ReadEntryHeader(key_descriptor, &header));
  // TODO: Should likely add a "LogHeader" method or similar.
  DBG("Header: ");
  DBG("   Address      = 0x%zx", size_t(entry_address));
  DBG("   Magic        = 0x%zx", size_t(header.magic()));
  DBG("   Checksum     = 0x%zx", size_t(header.checksum()));
  DBG("   Key length   = 0x%zx", size_t(header.key_length()));
  DBG("   Value length = 0x%zx", size_t(header.value_length()));
  DBG("   Entry size   = 0x%zx", size_t(header.entry_size()));
  DBG("   Padded size  = 0x%zx",
      size_t(AlignUp(header.entry_size(), alignment_bytes)));

  if (HeaderLooksLikeUnwrittenData(header)) {
    return Status::NOT_FOUND;
  }
  key_descriptor.key_version = header.key_version();

  // TODO: Handle multiple magics for formats that have changed.
  if (header.magic() != entry_header_format_.magic) {
    // TODO: It may be cleaner to have some logging helpers for these cases.
    CRT("Found corrupt magic: %zx; expecting %zx; at address %zx",
        size_t(header.magic()),
        size_t(entry_header_format_.magic),
        size_t(entry_address));
    return Status::DATA_LOSS;
  }

  // Read the key from flash & validate the entry (which reads the value).
  KeyBuffer key_buffer;
  TRY(ReadEntryKey(key_descriptor, header.key_length(), key_buffer.data()));
  const string_view key(key_buffer.data(), header.key_length());

  TRY(header.VerifyChecksumInFlash(
      &partition_, key_descriptor.address, entry_header_format_.checksum));
  key_descriptor.key_hash = HashKey(key);

  DBG("Key hash: %zx (%zu)",
      size_t(key_descriptor.key_hash),
      size_t(key_descriptor.key_hash));

  TRY(AppendNewOrOverwriteStaleExistingDescriptor(key_descriptor));

  // TODO: Extract this to something like "NextValidEntryAddress".
  *next_entry_address =
      AlignUp(key_descriptor.address + header.entry_size(), alignment_bytes);

  return Status::OK;
}

// TODO: This method is the trigger of the O(valid_entries * all_entries) time
// complexity for reading. At some cost to memory, this could be optimized by
// using a hash table instead of scanning, but in practice this should be fine
// for a small number of keys
Status KeyValueStore::AppendNewOrOverwriteStaleExistingDescriptor(
    const KeyDescriptor& key_descriptor) {
  // With the new key descriptor, either add it to the descriptor table or
  // overwrite an existing entry with an older version of the key.
  KeyDescriptor* existing_descriptor = FindDescriptor(key_descriptor.key_hash);
  if (existing_descriptor) {
    if (existing_descriptor->key_version < key_descriptor.key_version) {
      // Existing entry is old; replace the existing entry with the new one.
      *existing_descriptor = key_descriptor;
    } else {
      // Otherwise, check for data integrity and leave the existing entry.
      if (existing_descriptor->key_version == key_descriptor.key_version) {
        ERR("Data loss: Duplicated old(=%zu) and new(=%zu) version",
            size_t(existing_descriptor->key_version),
            size_t(key_descriptor.key_version));
        return Status::DATA_LOSS;
      }
      DBG("Found stale entry when appending; ignoring");
    }
    return Status::OK;
  }
  // Write new entry.
  KeyDescriptor* newly_allocated_key_descriptor;
  TRY(AppendEmptyDescriptor(&newly_allocated_key_descriptor));
  *newly_allocated_key_descriptor = key_descriptor;
  return Status::OK;
}

// TODO: Need a better name.
Status KeyValueStore::AppendEmptyDescriptor(KeyDescriptor** new_descriptor) {
  if (KeyListFull()) {
    // TODO: Is this the right return code?
    return Status::RESOURCE_EXHAUSTED;
  }
  *new_descriptor = &key_descriptor_list_[key_descriptor_list_size_++];
  return Status::OK;
}

// TODO: Finish.
bool KeyValueStore::HeaderLooksLikeUnwrittenData(
    const EntryHeader& header) const {
  // TODO: This is not correct; it should call through to flash memory.
  return header.magic() == 0xffffffff;
}

KeyValueStore::KeyDescriptor* KeyValueStore::FindDescriptor(uint32_t hash) {
  for (size_t key_id = 0; key_id < key_descriptor_list_size_; key_id++) {
    if (key_descriptor_list_[key_id].key_hash == hash) {
      return &(key_descriptor_list_[key_id]);
    }
  }
  return nullptr;
}

StatusWithSize KeyValueStore::Get(string_view key,
                                  span<byte> value_buffer) const {
  TRY(InvalidOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY(FindKeyDescriptor(key, &key_descriptor));

  EntryHeader header;
  TRY(ReadEntryHeader(*key_descriptor, &header));

  StatusWithSize result = ReadEntryValue(*key_descriptor, header, value_buffer);
  if (result.ok() && options_.verify_on_read) {
    return header.VerifyChecksum(entry_header_format_.checksum,
                                 key,
                                 value_buffer.subspan(0, result.size()));
  }
  return result;
}

Status KeyValueStore::Put(string_view key, span<const byte> value) {
  DBG("Writing key/value; key length=%zu, value length=%zu",
      key.size(),
      value.size());
  TRY(InvalidOperation(key));

  if (value.size() > (1 << 24)) {
    // TODO: Reject sizes that are larger than the maximum?
  }

  KeyDescriptor* key_descriptor;
  if (FindKeyDescriptor(key, &key_descriptor).ok()) {
    DBG("Writing over existing entry");
    return WriteEntryForExistingKey(key_descriptor, key, value);
  }

  DBG("Writing new entry");
  return WriteEntryForNewKey(key, value);
}

Status KeyValueStore::Delete(string_view key) {
  TRY(InvalidOperation(key));
  return Status::UNIMPLEMENTED;
}

const KeyValueStore::Item& KeyValueStore::Iterator::operator*() {
  const KeyDescriptor& descriptor = entry_.kvs_.key_descriptor_list_[index_];

  std::memset(entry_.key_buffer_.data(), 0, entry_.key_buffer_.size());

  EntryHeader header;
  if (entry_.kvs_.ReadEntryHeader(descriptor, &header).ok()) {
    entry_.kvs_.ReadEntryKey(
        descriptor, header.key_length(), entry_.key_buffer_.data());
  }

  return entry_;
}

StatusWithSize KeyValueStore::ValueSize(std::string_view key) const {
  TRY(InvalidOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY(FindKeyDescriptor(key, &key_descriptor));

  EntryHeader header;
  TRY(ReadEntryHeader(*key_descriptor, &header));

  return StatusWithSize(header.value_length());
}

Status KeyValueStore::FixedSizeGet(std::string_view key,
                                   byte* value,
                                   size_t size_bytes) const {
  // Ensure that the size of the stored value matches the size of the type.
  // Otherwise, report error. This check avoids potential memory corruption.
  StatusWithSize result = ValueSize(key);
  if (!result.ok()) {
    return result.status();
  }
  if (result.size() != size_bytes) {
    return Status::INVALID_ARGUMENT;
  }
  return Get(key, span(value, size_bytes)).status();
}

Status KeyValueStore::InvalidOperation(string_view key) const {
  if (InvalidKey(key)) {
    return Status::INVALID_ARGUMENT;
  }
  if (!enabled_) {
    return Status::FAILED_PRECONDITION;
  }
  return Status::OK;
}

Status KeyValueStore::FindKeyDescriptor(string_view key,
                                        const KeyDescriptor** result) const {
  char key_buffer[kMaxKeyLength];
  const uint32_t hash = HashKey(key);

  for (auto& descriptor : key_descriptors()) {
    if (descriptor.key_hash == hash) {
      DBG("Found match! For hash: %zx", size_t(hash));
      TRY(ReadEntryKey(descriptor, key.size(), key_buffer));

      if (key == string_view(key_buffer, key.size())) {
        DBG("Keys matched too");
        *result = &descriptor;
        return Status::OK;
      }
    }
  }
  return Status::NOT_FOUND;
}

Status KeyValueStore::ReadEntryHeader(const KeyDescriptor& descriptor,
                                      EntryHeader* header) const {
  return partition_.Read(descriptor.address, sizeof(*header), header).status();
}

Status KeyValueStore::ReadEntryKey(const KeyDescriptor& descriptor,
                                   size_t key_length,
                                   char* key) const {
  // TODO: This check probably shouldn't be here; this is like
  // checking that the Cortex M's RAM isn't corrupt. This should be
  // done at boot time.
  // ^^ This argument sometimes comes from EntryHeader::key_value_len,
  // which is read directly from flash. If it's corrupted, we shouldn't try
  // to read a bunch of extra data.
  if (key_length == 0u || key_length > kMaxKeyLength) {
    return Status::DATA_LOSS;
  }
  // The key is immediately after the entry header.
  return partition_
      .Read(descriptor.address + sizeof(EntryHeader), key_length, key)
      .status();
}

StatusWithSize KeyValueStore::ReadEntryValue(
    const KeyDescriptor& key_descriptor,
    const EntryHeader& header,
    span<byte> value) const {
  const size_t read_size = std::min(header.value_length(), value.size());
  StatusWithSize result = partition_.Read(
      key_descriptor.address + sizeof(header) + header.key_length(),
      value.subspan(0, read_size));
  TRY(result);
  if (read_size != header.value_length()) {
    return StatusWithSize(Status::RESOURCE_EXHAUSTED, read_size);
  }
  return StatusWithSize(read_size);
}

Status KeyValueStore::WriteEntryForExistingKey(KeyDescriptor* key_descriptor,
                                               string_view key,
                                               span<const byte> value) {
  SectorDescriptor* sector;
  TRY(FindOrRecoverSectorWithSpace(&sector, EntrySize(key, value)));
  DBG("Writing existing entry; found sector: %d",
      static_cast<int>(sector - sector_map_.data()));
  return AppendEntry(sector, key_descriptor, key, value);
}

Status KeyValueStore::WriteEntryForNewKey(string_view key,
                                          span<const byte> value) {
  if (KeyListFull()) {
    WRN("KVS full: trying to store a new entry, but can't. Have %zu entries",
        key_descriptor_list_size_);
    return Status::RESOURCE_EXHAUSTED;
  }

  // Modify the key descriptor at the end of the array, without bumping the map
  // size so the key descriptor is prepared and written without committing
  // first.
  KeyDescriptor& key_descriptor =
      key_descriptor_list_[key_descriptor_list_size_];
  key_descriptor.key_hash = HashKey(key);
  key_descriptor.key_version = 0;  // will be incremented by AppendEntry()

  SectorDescriptor* sector;
  TRY(FindOrRecoverSectorWithSpace(&sector, EntrySize(key, value)));
  DBG("Writing new entry; found sector: %d",
      static_cast<int>(sector - sector_map_.data()));
  TRY(AppendEntry(sector, &key_descriptor, key, value));

  // Only increment bump our size when we are certain the write succeeded.
  key_descriptor_list_size_ += 1;
  return Status::OK;
}

Status KeyValueStore::RelocateEntry(KeyDescriptor& key_descriptor) {
  struct TempEntry {
    std::array<char, kMaxKeyLength + 1> key;
    std::array<char, sizeof(working_buffer_) - sizeof(key)> value;
  };
  TempEntry* entry = reinterpret_cast<TempEntry*>(working_buffer_.data());

  // Read the entry to be relocated. Store the header in a local variable and
  // store the key and value in the TempEntry stored in the static allocated
  // working_buffer_.
  EntryHeader header;
  TRY(ReadEntryHeader(key_descriptor, &header));
  TRY(ReadEntryKey(key_descriptor, header.key_length(), entry->key.data()));
  string_view key = string_view(entry->key.data(), header.key_length());
  StatusWithSize result = ReadEntryValue(
      key_descriptor, header, as_writable_bytes(span(entry->value)));
  if (!result.status().ok()) {
    return Status::INTERNAL;
  }

  auto value = span(entry->value.data(), result.size());

  TRY(header.VerifyChecksum(
      entry_header_format_.checksum, key, as_bytes(value)));

  SectorDescriptor* old_sector = SectorFromAddress(key_descriptor.address);
  if (old_sector == nullptr) {
    return Status::INTERNAL;
  }

  // Find a new sector for the entry and write it to the new location.
  SectorDescriptor* new_sector =
      FindSectorWithSpace(EntrySize(header), old_sector, true);
  if (new_sector == nullptr) {
    return Status::RESOURCE_EXHAUSTED;
  }
  return AppendEntry(new_sector, &key_descriptor, key, as_bytes(value));
}

// Find either an existing sector with enough space that is not the sector to
// skip, or an empty sector. Maintains the invariant that there is always at
// least 1 empty sector unless set to bypass the rule.
KeyValueStore::SectorDescriptor* KeyValueStore::FindSectorWithSpace(
    size_t size,
    SectorDescriptor* sector_to_skip,
    bool bypass_empty_sector_rule) {
  const size_t sector_count = partition_.sector_count();

  // TODO: Ignore last written sector for now and scan from the beginning.
  last_written_sector_ = sector_count - 1;

  size_t start = (last_written_sector_ + 1) % sector_count;
  SectorDescriptor* first_empty_sector = nullptr;
  bool at_least_two_empty_sectors = bypass_empty_sector_rule;

  for (size_t i = start; i != last_written_sector_;
       i = (i + 1) % sector_count) {
    DBG("Examining sector %zu", i);
    SectorDescriptor& sector = sector_map_[i];

    if (sector_to_skip == &sector) {
      DBG("Skipping the skip sector");
      continue;
    }

    if (!SectorEmpty(sector) && sector.HasSpace(size)) {
      DBG("Partially occupied sector with enough space; done!");
      return &sector;
    }

    if (SectorEmpty(sector)) {
      if (first_empty_sector == nullptr) {
        first_empty_sector = &sector;
      } else {
        at_least_two_empty_sectors = true;
      }
    }
  }

  if (at_least_two_empty_sectors) {
    DBG("Found a usable empty sector; returning the first found (%d)",
        static_cast<int>(first_empty_sector - sector_map_.data()));
    return first_empty_sector;
  }
  return nullptr;
}

Status KeyValueStore::FindOrRecoverSectorWithSpace(SectorDescriptor** sector,
                                                   size_t size) {
  *sector = FindSectorWithSpace(size);
  if (*sector != nullptr) {
    return Status::OK;
  }
  if (options_.partial_gc_on_write) {
    return GarbageCollectOneSector(sector);
  }
  return Status::RESOURCE_EXHAUSTED;
}

KeyValueStore::SectorDescriptor* KeyValueStore::FindSectorToGarbageCollect() {
  SectorDescriptor* sector_candidate = nullptr;
  size_t candidate_bytes = 0;

  // Step 1: Try to find a sectors with stale keys and no valid keys (no
  // relocation needed). If any such sectors are found, use the sector with the
  // most reclaimable bytes.
  for (auto& sector : sector_map_) {
    if ((sector.valid_bytes == 0) &&
        (RecoverableBytes(sector) > candidate_bytes)) {
      sector_candidate = &sector;
      candidate_bytes = RecoverableBytes(sector);
    }
  }

  // Step 2: If step 1 yields no sectors, just find the sector with the most
  // reclaimable bytes.
  if (sector_candidate == nullptr) {
    for (auto& sector : sector_map_) {
      if (RecoverableBytes(sector) > candidate_bytes) {
        sector_candidate = &sector;
        candidate_bytes = RecoverableBytes(sector);
      }
    }
  }

  return sector_candidate;
}

Status KeyValueStore::GarbageCollectOneSector(SectorDescriptor** sector) {
  // Step 1: Find the sector to garbage collect
  SectorDescriptor* sector_to_gc = FindSectorToGarbageCollect();

  if (sector_to_gc == nullptr) {
    return Status::RESOURCE_EXHAUSTED;
  }

  // Step 2: Move any valid entries in the GC sector to other sectors
  if (sector_to_gc->valid_bytes != 0) {
    for (auto& descriptor : key_descriptors()) {
      if (AddressInSector(*sector_to_gc, descriptor.address)) {
        TRY(RelocateEntry(descriptor));
      }
    }
  }

  if (sector_to_gc->valid_bytes != 0) {
    return Status::INTERNAL;
  }

  // Step 3: Reinitialize the sector
  sector_to_gc->tail_free_bytes = 0;
  TRY(partition_.Erase(SectorBaseAddress(sector_to_gc), 1));
  sector_to_gc->tail_free_bytes = partition_.sector_size_bytes();

  *sector = sector_to_gc;
  return Status::OK;
}

Status KeyValueStore::AppendEntry(SectorDescriptor* sector,
                                  KeyDescriptor* key_descriptor,
                                  const string_view key,
                                  span<const byte> value) {
  // write header, key, and value
  const EntryHeader header(entry_header_format_.magic,
                           entry_header_format_.checksum,
                           key,
                           value,
                           key_descriptor->key_version + 1);
  DBG("Appending entry with key version: %zx", size_t(header.key_version()));

  // Handles writing multiple concatenated buffers, while breaking up the writes
  // into alignment-sized blocks.
  Address address = NextWritableAddress(sector);
  DBG("Appending to address: %zx", size_t(address));
  TRY_ASSIGN(
      size_t written,
      partition_.Write(
          address, {as_bytes(span(&header, 1)), as_bytes(span(key)), value}));

  if (options_.verify_on_write) {
    TRY(header.VerifyChecksumInFlash(
        &partition_, address, entry_header_format_.checksum));
  }

  // TODO: UPDATE last_written_sector_ appropriately

  key_descriptor->address = address;
  key_descriptor->key_version = header.key_version();
  sector->valid_bytes += written;
  sector->tail_free_bytes -= written;
  return Status::OK;
}

void KeyValueStore::LogDebugInfo() {
  const size_t sector_count = partition_.sector_count();
  const size_t sector_size_bytes = partition_.sector_size_bytes();
  DBG("====================== KEY VALUE STORE DUMP =========================");
  DBG(" ");
  DBG("Flash partition:");
  DBG("  Sector count     = %zu", sector_count);
  DBG("  Sector max count = %zu", kUsableSectors);
  DBG("  Sector size      = %zu", sector_size_bytes);
  DBG("  Total size       = %zu", partition_.size_bytes());
  DBG("  Alignment        = %zu", partition_.alignment_bytes());
  DBG(" ");
  DBG("Key descriptors:");
  DBG("  Entry count     = %zu", key_descriptor_list_size_);
  DBG("  Max entry count = %zu", kMaxEntries);
  DBG(" ");
  DBG("      #     hash        version    address   address (hex)");
  for (size_t i = 0; i < key_descriptor_list_size_; ++i) {
    const KeyDescriptor& kd = key_descriptor_list_[i];
    DBG("   |%3zu: | %8zx  |%8zu  | %8zu | %8zx",
        i,
        size_t(kd.key_hash),
        size_t(kd.key_version),
        size_t(kd.address),
        size_t(kd.address));
  }
  DBG(" ");

  DBG("Sector descriptors:");
  DBG("      #     tail free  valid    has_space");
  for (size_t sector_id = 0; sector_id < sector_count; ++sector_id) {
    const SectorDescriptor& sd = sector_map_[sector_id];
    DBG("   |%3zu: | %8zu  |%8zu  | %s",
        sector_id,
        size_t(sd.tail_free_bytes),
        size_t(sd.valid_bytes),
        sd.tail_free_bytes ? "YES" : "");
  }
  DBG(" ");

  // TODO: This should stop logging after some threshold.
  // size_t dumped_bytes = 0;
  DBG("Sector raw data:");
  for (size_t sector_id = 0; sector_id < sector_count; ++sector_id) {
    // Read sector data. Yes, this will blow the stack on embedded.
    std::array<byte, 500> raw_sector_data;  // TODO
    StatusWithSize sws =
        partition_.Read(sector_id * sector_size_bytes, raw_sector_data);
    DBG("Read: %zu bytes", sws.size());

    DBG("  base    addr  offs   0  1  2  3  4  5  6  7");
    for (size_t i = 0; i < sector_size_bytes; i += 8) {
      DBG("  %3zu %8zx %5zu | %02x %02x %02x %02x %02x %02x %02x %02x",
          sector_id,
          (sector_id * sector_size_bytes) + i,
          i,
          static_cast<unsigned int>(raw_sector_data[i + 0]),
          static_cast<unsigned int>(raw_sector_data[i + 1]),
          static_cast<unsigned int>(raw_sector_data[i + 2]),
          static_cast<unsigned int>(raw_sector_data[i + 3]),
          static_cast<unsigned int>(raw_sector_data[i + 4]),
          static_cast<unsigned int>(raw_sector_data[i + 5]),
          static_cast<unsigned int>(raw_sector_data[i + 6]),
          static_cast<unsigned int>(raw_sector_data[i + 7]));

      // TODO: Fix exit condition.
      if (i > 128) {
        break;
      }
    }
    DBG(" ");
  }

  DBG("////////////////////// KEY VALUE STORE DUMP END /////////////////////");
}

}  // namespace pw::kvs
