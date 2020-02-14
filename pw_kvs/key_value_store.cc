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
#include <cinttypes>
#include <cstring>
#include <type_traits>

#define PW_LOG_USE_ULTRA_SHORT_NAMES 1
#include "pw_kvs_private/entry.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {

using std::byte;
using std::string_view;

KeyValueStore::KeyValueStore(FlashPartition* partition,
                             const EntryHeaderFormat& format,
                             const Options& options)
    : partition_(*partition),
      entry_header_format_(format),
      options_(options),
      sectors_(partition_.sector_count()),
      last_new_sector_(sectors_.data()),
      working_buffer_{} {}

Status KeyValueStore::Init() {
  if (kMaxUsableSectors < sectors_.size()) {
    CRT("KeyValueStore::kMaxUsableSectors must be at least as large as the "
        "number of sectors in the flash partition");
    return Status::FAILED_PRECONDITION;
  }

  if (kMaxUsableSectors > sectors_.size()) {
    DBG("KeyValueStore::kMaxUsableSectors is %zu sectors larger than needed",
        kMaxUsableSectors - sectors_.size());
  }

  // Reset the number of occupied key descriptors; we will fill them later.
  key_descriptors_.clear();

  // TODO: init last_new_sector_ to a random sector. Since the on-flash stored
  // information does not allow recovering the previous last_new_sector_ after
  // clean start, random is a good second choice.

  const size_t sector_size_bytes = partition_.sector_size_bytes();

  if (working_buffer_.size() < sector_size_bytes) {
    CRT("working_buffer_ (%zu bytes) is smaller than sector size (%zu bytes)",
        working_buffer_.size(),
        sector_size_bytes);
    return Status::INVALID_ARGUMENT;
  }

  DBG("First pass: Read all entries from all sectors");
  for (size_t sector_id = 0; sector_id < sectors_.size(); ++sector_id) {
    // Track writable bytes in this sector. Updated after reading each entry.
    sectors_[sector_id].tail_free_bytes = sector_size_bytes;

    const Address sector_address = sector_id * sector_size_bytes;
    Address entry_address = sector_address;

    for (int num_entries_in_sector = 0;; num_entries_in_sector++) {
      DBG("Load entry: sector=%zu, entry#=%d, address=%zu",
          sector_id,
          num_entries_in_sector,
          size_t(entry_address));

      if (!AddressInSector(sectors_[sector_id], entry_address)) {
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
      sectors_[sector_id].tail_free_bytes =
          sector_size_bytes - (entry_address - sector_address);
    }
  }

  DBG("Second pass: Count valid bytes in each sector");
  // Initialize the sector sizes.
  for (SectorDescriptor& sector : sectors_) {
    sector.valid_bytes = 0;
  }
  // For every valid key, increment the valid bytes for that sector.
  for (KeyDescriptor& key_descriptor : key_descriptors_) {
    uint32_t sector_id = key_descriptor.address / sector_size_bytes;
    Entry entry;
    TRY(Entry::Read(partition_, key_descriptor.address, &entry));
    sectors_[sector_id].valid_bytes += entry.size();
  }
  initialized_ = true;
  return Status::OK;
}

Status KeyValueStore::LoadEntry(Address entry_address,
                                Address* next_entry_address) {
  Entry entry;
  TRY(Entry::Read(partition_, entry_address, &entry));

  // TODO: Handle multiple magics for formats that have changed.
  if (entry.magic() != entry_header_format_.magic) {
    // TODO: It may be cleaner to have some logging helpers for these cases.
    ERR("Found corrupt magic: %zx; expecting %zx; at address %zx",
        size_t(entry.magic()),
        size_t(entry_header_format_.magic),
        size_t(entry_address));
    return Status::DATA_LOSS;
  }

  // Read the key from flash & validate the entry (which reads the value).
  KeyBuffer key_buffer;
  TRY_ASSIGN(size_t key_length, entry.ReadKey(key_buffer));
  const string_view key(key_buffer.data(), key_length);

  TRY(entry.VerifyChecksumInFlash(entry_header_format_.checksum));

  KeyDescriptor key_descriptor(
      key,
      entry.key_version(),
      entry_address,
      entry.deleted() ? KeyDescriptor::kDeleted : KeyDescriptor::kValid);

  DBG("Key hash: %zx (%zu)",
      size_t(key_descriptor.key_hash),
      size_t(key_descriptor.key_hash));

  TRY(AppendNewOrOverwriteStaleExistingDescriptor(key_descriptor));

  *next_entry_address = entry.next_address();
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
  if (key_descriptors_.full()) {
    return Status::RESOURCE_EXHAUSTED;
  }
  key_descriptors_.emplace_back();
  *new_descriptor = &key_descriptors_.back();
  return Status::OK;
}

KeyValueStore::KeyDescriptor* KeyValueStore::FindDescriptor(uint32_t hash) {
  for (KeyDescriptor& key_descriptor : key_descriptors_) {
    if (key_descriptor.key_hash == hash) {
      return &key_descriptor;
    }
  }
  return nullptr;
}

StatusWithSize KeyValueStore::Get(string_view key,
                                  span<byte> value_buffer) const {
  TRY_WITH_SIZE(CheckOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY_WITH_SIZE(FindExistingKeyDescriptor(key, &key_descriptor));

  Entry entry;
  TRY_WITH_SIZE(Entry::Read(partition_, key_descriptor->address, &entry));

  StatusWithSize result = entry.ReadValue(value_buffer);
  if (result.ok() && options_.verify_on_read) {
    Status verify_result =
        entry.VerifyChecksum(entry_header_format_.checksum,
                             key,
                             value_buffer.subspan(0, result.size()));
    if (!verify_result.ok()) {
      std::memset(
          value_buffer.subspan(0, result.size()).data(), 0, result.size());
      return StatusWithSize(verify_result);
    }

    return StatusWithSize(verify_result, result.size());
  }
  return result;
}

Status KeyValueStore::Put(string_view key, span<const byte> value) {
  DBG("Writing key/value; key length=%zu, value length=%zu",
      key.size(),
      value.size());

  TRY(CheckOperation(key));

  if (value.size() > (1 << 24)) {
    // TODO: Reject sizes that are larger than the maximum?
  }

  KeyDescriptor* key_descriptor;
  Status status = FindKeyDescriptor(key, &key_descriptor);

  if (status.ok()) {
    DBG("Writing over existing entry for key 0x%08" PRIx32 " in sector %zu",
        key_descriptor->key_hash,
        SectorIndex(SectorFromAddress(key_descriptor->address)));
    return WriteEntryForExistingKey(
        key_descriptor, KeyDescriptor::kValid, key, value);
  }

  if (status == Status::NOT_FOUND) {
    return WriteEntryForNewKey(key, value);
  }

  return status;
}

Status KeyValueStore::Delete(string_view key) {
  TRY(CheckOperation(key));

  KeyDescriptor* key_descriptor;
  TRY(FindExistingKeyDescriptor(key, &key_descriptor));

  DBG("Writing tombstone for existing key 0x%08" PRIx32 " in sector %zu",
      key_descriptor->key_hash,
      SectorIndex(SectorFromAddress(key_descriptor->address)));
  return WriteEntryForExistingKey(
      key_descriptor, KeyDescriptor::kDeleted, key, {});
}

KeyValueStore::iterator& KeyValueStore::iterator::operator++() {
  // Skip to the next entry that is valid (not deleted).
  while (++index_ < item_.kvs_.key_descriptors_.size() &&
         descriptor().deleted()) {
  }
  return *this;
}

const KeyValueStore::Item& KeyValueStore::iterator::operator*() {
  std::memset(item_.key_buffer_.data(), 0, item_.key_buffer_.size());

  Entry entry;
  if (Entry::Read(item_.kvs_.partition_, descriptor().address, &entry).ok()) {
    entry.ReadKey(item_.key_buffer_);
  }

  return item_;
}

KeyValueStore::iterator KeyValueStore::begin() const {
  size_t i = 0;
  // Skip over any deleted entries at the start of the descriptor list.
  while (i < key_descriptors_.size() && key_descriptors_[i].deleted()) {
    i += 1;
  }
  return iterator(*this, i);
}

// TODO(hepler): The valid entry count could be tracked in the KVS to avoid the
// need for this for-loop.
size_t KeyValueStore::size() const {
  size_t valid_entries = 0;

  for (const KeyDescriptor& key_descriptor : key_descriptors_) {
    if (!key_descriptor.deleted()) {
      valid_entries += 1;
    }
  }

  return valid_entries;
}

StatusWithSize KeyValueStore::ValueSize(std::string_view key) const {
  TRY_WITH_SIZE(CheckOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY_WITH_SIZE(FindExistingKeyDescriptor(key, &key_descriptor));

  Entry entry;
  TRY_WITH_SIZE(Entry::Read(partition_, key_descriptor->address, &entry));

  return StatusWithSize(entry.value_size());
}

uint32_t KeyValueStore::HashKey(string_view string) {
  uint32_t hash = 0;
  uint32_t coefficient = 65599u;

  for (char ch : string) {
    hash += coefficient * unsigned(ch);
    coefficient *= 65599u;
  }

  return hash;
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
    DBG("Requested %zu B read, but value is %zu B", size_bytes, result.size());
    return Status::INVALID_ARGUMENT;
  }
  return Get(key, span(value, size_bytes)).status();
}

Status KeyValueStore::CheckOperation(string_view key) const {
  if (InvalidKey(key)) {
    return Status::INVALID_ARGUMENT;
  }
  if (!initialized_) {
    return Status::FAILED_PRECONDITION;
  }
  return Status::OK;
}

// Searches for a KeyDescriptor that matches this key and sets *result to point
// to it if one is found.
//
//             OK: there is a matching descriptor and *result is set
//      NOT_FOUND: there is no descriptor that matches this key, but this key
//                 has a unique hash (and could potentially be added to the KVS)
// ALREADY_EXISTS: there is no descriptor that matches this key, but the
//                 key's hash collides with the hash for an existing descriptor
//
Status KeyValueStore::FindKeyDescriptor(string_view key,
                                        const KeyDescriptor** result) const {
  Entry::KeyBuffer key_buffer;
  const uint32_t hash = HashKey(key);

  for (auto& descriptor : key_descriptors_) {
    if (descriptor.key_hash == hash) {
      TRY(Entry::ReadKey(
          partition_, descriptor.address, key.size(), key_buffer));

      if (key == string_view(key_buffer.data(), key.size())) {
        DBG("Found match for key hash 0x%08" PRIx32, hash);
        *result = &descriptor;
        return Status::OK;
      } else {
        WRN("Found key hash collision for 0x%08" PRIx32, hash);
        return Status::ALREADY_EXISTS;
      }
    }
  }
  return Status::NOT_FOUND;
}

// Searches for a KeyDescriptor that matches this key and sets *result to point
// to it if one is found.
//
//          OK: there is a matching descriptor and *result is set
//   NOT_FOUND: there is no descriptor that matches this key
//
Status KeyValueStore::FindExistingKeyDescriptor(
    string_view key, const KeyDescriptor** result) const {
  Status status = FindKeyDescriptor(key, result);

  // If the key's hash collides with an existing key or if the key is deleted,
  // treat it as if it is not in the KVS.
  if (status == Status::ALREADY_EXISTS ||
      (status.ok() && (*result)->deleted())) {
    return Status::NOT_FOUND;
  }
  return status;
}

Status KeyValueStore::WriteEntryForExistingKey(KeyDescriptor* key_descriptor,
                                               KeyDescriptor::State new_state,
                                               string_view key,
                                               span<const byte> value) {
  // Find the original entry and sector to update the sector's valid_bytes.
  Entry original_entry;
  TRY(Entry::Read(partition_, key_descriptor->address, &original_entry));
  SectorDescriptor* old_sector = SectorFromAddress(key_descriptor->address);

  SectorDescriptor* sector;
  TRY(FindOrRecoverSectorWithSpace(
      &sector, Entry::size(partition_.alignment_bytes(), key, value)));
  DBG("Writing existing entry; found sector: %zu", SectorIndex(sector));

  if (old_sector != SectorFromAddress(key_descriptor->address)) {
    DBG("Sector for old entry (size %zu) was garbage collected. Old entry "
        "relocated to sector %zu",
        original_entry.size(),
        SectorIndex(SectorFromAddress(key_descriptor->address)));

    old_sector = SectorFromAddress(key_descriptor->address);
  }

  TRY(AppendEntry(sector, key_descriptor, key, value, new_state));

  old_sector->RemoveValidBytes(original_entry.size());
  return Status::OK;
}

Status KeyValueStore::WriteEntryForNewKey(string_view key,
                                          span<const byte> value) {
  if (key_descriptors_.full()) {
    WRN("KVS full: trying to store a new entry, but can't. Have %zu entries",
        key_descriptors_.size());
    return Status::RESOURCE_EXHAUSTED;
  }

  // Create the KeyDescriptor that will be added to the list. The version and
  // address will be set by AppendEntry.
  KeyDescriptor key_descriptor(key, 0, 0);

  SectorDescriptor* sector;
  TRY(FindOrRecoverSectorWithSpace(
      &sector, Entry::size(partition_.alignment_bytes(), key, value)));
  DBG("Writing new entry; found sector: %zu", SectorIndex(sector));
  TRY(AppendEntry(sector, &key_descriptor, key, value, KeyDescriptor::kValid));

  // Only add the entry when we are certain the write succeeded.
  key_descriptors_.push_back(key_descriptor);
  return Status::OK;
}

Status KeyValueStore::RelocateEntry(KeyDescriptor& key_descriptor) {
  struct TempEntry {
    Entry::KeyBuffer key;
    std::array<char, sizeof(working_buffer_) - sizeof(key)> value;
  };
  TempEntry* temp_entry = reinterpret_cast<TempEntry*>(working_buffer_.data());

  DBG("Relocating entry");  // TODO: add entry info to the log statement.

  // Read the entry to be relocated. Store the entry in a local variable and
  // store the key and value in the TempEntry stored in the static allocated
  // working_buffer_.
  Entry entry;
  TRY(Entry::Read(partition_, key_descriptor.address, &entry));
  TRY_ASSIGN(size_t key_length, entry.ReadKey(temp_entry->key));
  string_view key = string_view(temp_entry->key.data(), key_length);
  auto result = entry.ReadValue(as_writable_bytes(span(temp_entry->value)));
  if (!result.status().ok()) {
    return Status::INTERNAL;
  }

  auto value = span(temp_entry->value.data(), result.size());
  TRY(entry.VerifyChecksum(
      entry_header_format_.checksum, key, as_bytes(value)));

  SectorDescriptor* old_sector = SectorFromAddress(key_descriptor.address);

  // Find a new sector for the entry and write it to the new location.
  SectorDescriptor* new_sector;
  TRY(FindSectorWithSpace(&new_sector, entry.size(), old_sector, true));
  TRY(AppendEntry(
      new_sector, &key_descriptor, key, as_bytes(value), key_descriptor.state));

  // Do the valid bytes accounting for the sector the entry was relocated out
  // of.
  old_sector->RemoveValidBytes(entry.size());

  return Status::OK;
}

// Find either an existing sector with enough space that is not the sector to
// skip, or an empty sector. Maintains the invariant that there is always at
// least 1 empty sector unless set to bypass the rule.
Status KeyValueStore::FindSectorWithSpace(
    SectorDescriptor** found_sector,
    size_t size,
    const SectorDescriptor* sector_to_skip,
    bool bypass_empty_sector_rule) {
  // The last_new_sector_ is the sector that was last selected as the "new empty
  // sector" to write to. This last new sector is used as the starting point for
  // the next "find a new empty sector to write to" operation. By using the last
  // new sector as the start point we will cycle which empty sector is selected
  // next, spreading the wear across all the empty sectors and get a wear
  // leveling benefit, rather than putting more wear on the lower number
  // sectors.
  //
  // Locally use the sector index for ease of iterating through the sectors. For
  // the persistent storage use SectorDescriptor* rather than sector index
  // because SectorDescriptor* is the standard way to identify a sector.
  size_t last_new_sector_index_ = SectorIndex(last_new_sector_);
  size_t start = (last_new_sector_index_ + 1) % sectors_.size();
  SectorDescriptor* first_empty_sector = nullptr;
  bool at_least_two_empty_sectors = bypass_empty_sector_rule;

  DBG("Find sector with %zu bytes available", size);
  if (sector_to_skip != nullptr) {
    DBG("  Skip sector %zu", SectorIndex(sector_to_skip));
  }
  if (bypass_empty_sector_rule) {
    DBG("  Bypassing empty sector rule");
  }

  // Look for a partial sector to use with enough space. Immediately use the
  // first one of those that is found. While scanning for a partial sector, keep
  // track of the first empty sector and if a second sector was seen.
  for (size_t j = 0; j < sectors_.size(); j++) {
    size_t i = (j + start) % sectors_.size();
    SectorDescriptor& sector = sectors_[i];

    if (sector_to_skip == &sector) {
      DBG("  Skipping the skip sector %zu", i);
      continue;
    }

    DBG("  Examining sector %zu with %hu bytes available",
        i,
        sector.tail_free_bytes);
    if (!SectorEmpty(sector) && sector.HasSpace(size)) {
      DBG("  Partially occupied sector %zu with enough space; done!", i);
      *found_sector = &sector;
      return Status::OK;
    }

    if (SectorEmpty(sector)) {
      if (first_empty_sector == nullptr) {
        first_empty_sector = &sector;
      } else {
        at_least_two_empty_sectors = true;
      }
    }
  }

  // If the scan for a partial sector does not find a suitable sector, use the
  // first empty sector that was found. Normally it is required to keep 1 empty
  // sector after the sector found here, but that rule can be bypassed in
  // special circumstances (such as during garbage collection).
  if (at_least_two_empty_sectors) {
    DBG("  Found a usable empty sector; returning the first found (%zu)",
        SectorIndex(first_empty_sector));
    last_new_sector_ = first_empty_sector;
    *found_sector = first_empty_sector;
    return Status::OK;
  }

  // No sector was found.
  DBG("  Unable to find a usable sector");
  *found_sector = nullptr;
  return Status::RESOURCE_EXHAUSTED;
}

Status KeyValueStore::FindOrRecoverSectorWithSpace(SectorDescriptor** sector,
                                                   size_t size) {
  Status result = FindSectorWithSpace(sector, size);
  if (result == Status::RESOURCE_EXHAUSTED && options_.partial_gc_on_write) {
    // Garbage collect and then try again to find the best sector.
    TRY(GarbageCollectOneSector());
    return FindSectorWithSpace(sector, size);
  }
  return result;
}

KeyValueStore::SectorDescriptor* KeyValueStore::FindSectorToGarbageCollect() {
  SectorDescriptor* sector_candidate = nullptr;
  size_t candidate_bytes = 0;

  // Step 1: Try to find a sectors with stale keys and no valid keys (no
  // relocation needed). If any such sectors are found, use the sector with the
  // most reclaimable bytes.
  for (auto& sector : sectors_) {
    if ((sector.valid_bytes == 0) &&
        (RecoverableBytes(sector) > candidate_bytes)) {
      sector_candidate = &sector;
      candidate_bytes = RecoverableBytes(sector);
    }
  }

  // Step 2: If step 1 yields no sectors, just find the sector with the most
  // reclaimable bytes.
  if (sector_candidate == nullptr) {
    for (auto& sector : sectors_) {
      if (RecoverableBytes(sector) > candidate_bytes) {
        sector_candidate = &sector;
        candidate_bytes = RecoverableBytes(sector);
      }
    }
  }

  if (sector_candidate != nullptr) {
    DBG("Found sector %zu to Garbage Collect, %zu recoverable bytes",
        SectorIndex(sector_candidate),
        RecoverableBytes(*sector_candidate));
  } else {
    DBG("Unable to find sector to garbage collect!");
  }
  return sector_candidate;
}

Status KeyValueStore::GarbageCollectOneSector() {
  DBG("Garbage Collect a single sector");

  // Step 1: Find the sector to garbage collect
  SectorDescriptor* sector_to_gc = FindSectorToGarbageCollect();
  LogSectors();

  if (sector_to_gc == nullptr) {
    return Status::RESOURCE_EXHAUSTED;
  }

  // Step 2: Move any valid entries in the GC sector to other sectors
  if (sector_to_gc->valid_bytes != 0) {
    for (auto& descriptor : key_descriptors_) {
      if (AddressInSector(*sector_to_gc, descriptor.address)) {
        DBG("  Relocate entry");
        TRY(RelocateEntry(descriptor));
      }
    }
  }

  if (sector_to_gc->valid_bytes != 0) {
    ERR("  Failed to relocate valid entries from sector being garbage "
        "collected, %hu valid bytes remain",
        sector_to_gc->valid_bytes);
    return Status::INTERNAL;
  }

  // Step 3: Reinitialize the sector
  sector_to_gc->tail_free_bytes = 0;
  TRY(partition_.Erase(SectorBaseAddress(sector_to_gc), 1));
  sector_to_gc->tail_free_bytes = partition_.sector_size_bytes();

  DBG("  Garbage Collect complete");
  LogSectors();
  return Status::OK;
}

Status KeyValueStore::AppendEntry(SectorDescriptor* sector,
                                  KeyDescriptor* key_descriptor,
                                  const string_view key,
                                  span<const byte> value,
                                  KeyDescriptor::State new_state) {
  const Address address = NextWritableAddress(sector);
  DBG("Appending to address: %#zx", size_t(address));

  Entry entry;

  if (new_state == KeyDescriptor::kDeleted) {
    entry = Entry::Tombstone(partition_,
                             address,
                             entry_header_format_.magic,
                             entry_header_format_.checksum,
                             key,
                             partition_.alignment_bytes(),
                             key_descriptor->key_version + 1);
  } else {
    entry = Entry::Valid(partition_,
                         address,
                         entry_header_format_.magic,
                         entry_header_format_.checksum,
                         key,
                         value,
                         partition_.alignment_bytes(),
                         key_descriptor->key_version + 1);
  }

  DBG("Appending %zu B entry with key version: %x",
      entry.size(),
      unsigned(entry.key_version()));

  TRY_ASSIGN(const size_t written, entry.Write(key, value));

  if (options_.verify_on_write) {
    TRY(entry.VerifyChecksumInFlash(entry_header_format_.checksum));
  }

  key_descriptor->address = address;
  key_descriptor->key_version = entry.key_version();
  key_descriptor->state = new_state;

  sector->valid_bytes += written;
  sector->RemoveFreeBytes(written);
  return Status::OK;
}

void KeyValueStore::LogDebugInfo() {
  const size_t sector_size_bytes = partition_.sector_size_bytes();
  DBG("====================== KEY VALUE STORE DUMP =========================");
  DBG(" ");
  DBG("Flash partition:");
  DBG("  Sector count     = %zu", partition_.sector_count());
  DBG("  Sector max count = %zu", kMaxUsableSectors);
  DBG("  Sectors in use   = %zu", sectors_.size());
  DBG("  Sector size      = %zu", sector_size_bytes);
  DBG("  Total size       = %zu", partition_.size_bytes());
  DBG("  Alignment        = %zu", partition_.alignment_bytes());
  DBG(" ");
  DBG("Key descriptors:");
  DBG("  Entry count     = %zu", key_descriptors_.size());
  DBG("  Max entry count = %zu", kMaxEntries);
  DBG(" ");
  DBG("      #     hash        version    address   address (hex)");
  for (size_t i = 0; i < key_descriptors_.size(); ++i) {
    const KeyDescriptor& kd = key_descriptors_[i];
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
  for (size_t sector_id = 0; sector_id < sectors_.size(); ++sector_id) {
    const SectorDescriptor& sd = sectors_[sector_id];
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
  for (size_t sector_id = 0; sector_id < sectors_.size(); ++sector_id) {
    // Read sector data. Yes, this will blow the stack on embedded.
    std::array<byte, 500> raw_sector_data;  // TODO!!!
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

void KeyValueStore::LogSectors() const {
  DBG("Sector descriptors: count %zu", sectors_.size());
  for (auto& sector : sectors_) {
    DBG("  - Sector %zu: valid %hu, recoverable %zu, free %hu",
        SectorIndex(&sector),
        sector.valid_bytes,
        RecoverableBytes(sector),
        sector.tail_free_bytes);
  }
}

void KeyValueStore::LogKeyDescriptor() const {
  DBG("Key descriptors: count %zu", key_descriptors_.size());
  for (auto& key : key_descriptors_) {
    DBG("  - Key: %s, hash %#zx, version %zu, address %#zx",
        key.deleted() ? "Deleted" : "Valid",
        static_cast<size_t>(key.key_hash),
        static_cast<size_t>(key.key_version),
        static_cast<size_t>(key.address));
  }
}

void KeyValueStore::SectorDescriptor::RemoveValidBytes(size_t size) {
  // TODO: add safety check for valid_bytes > size.
  if (size > valid_bytes) {
    CRT("!!!!!!!!!!!!!!!");
    CRT("Remove too many valid bytes!!! remove %zu, only have %hu",
        size,
        valid_bytes);
    valid_bytes = size;
  }
  valid_bytes -= size;
}

}  // namespace pw::kvs
