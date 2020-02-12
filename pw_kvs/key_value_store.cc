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
#include "pw_kvs_private/format.h"
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
    CRT("ERROR: working_buffer_ (%zu bytes) is smaller than sector "
        "size (%zu bytes)",
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
    Entry header;
    TRY(ReadEntryHeader(key_descriptor.address, &header));
    sectors_[sector_id].valid_bytes += header.size();
  }
  initialized_ = true;
  return Status::OK;
}

Status KeyValueStore::LoadEntry(Address entry_address,
                                Address* next_entry_address) {
  Entry header;
  TRY(ReadEntryHeader(entry_address, &header));
  // TODO: Should likely add a "LogHeader" method or similar.
  DBG("Header: ");
  DBG("   Address      = 0x%zx", size_t(entry_address));
  DBG("   Magic        = 0x%zx", size_t(header.magic()));
  DBG("   Checksum     = 0x%zx", size_t(header.checksum()));
  DBG("   Key length   = 0x%zx", size_t(header.key_length()));
  DBG("   Value length = 0x%zx", size_t(header.value_length()));
  DBG("   Entry size   = 0x%zx", size_t(header.size()));
  DBG("   Alignment    = 0x%zx", size_t(header.alignment_bytes()));

  if (HeaderLooksLikeUnwrittenData(header)) {
    return Status::NOT_FOUND;
  }

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
  TRY(ReadEntryKey(entry_address, header.key_length(), key_buffer.data()));
  const string_view key(key_buffer.data(), header.key_length());

  TRY(header.VerifyChecksumInFlash(
      &partition_, entry_address, entry_header_format_.checksum));

  KeyDescriptor key_descriptor(
      key,
      header.key_version(),
      entry_address,
      header.deleted() ? KeyDescriptor::kDeleted : KeyDescriptor::kValid);

  DBG("Key hash: %zx (%zu)",
      size_t(key_descriptor.key_hash),
      size_t(key_descriptor.key_hash));

  TRY(AppendNewOrOverwriteStaleExistingDescriptor(key_descriptor));

  // TODO: Extract this to something like "NextValidEntryAddress".
  *next_entry_address = key_descriptor.address + header.size();

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
    // TODO: Is this the right return code?
    return Status::RESOURCE_EXHAUSTED;
  }
  key_descriptors_.emplace_back();
  *new_descriptor = &key_descriptors_.back();
  return Status::OK;
}

// TODO: Finish.
bool KeyValueStore::HeaderLooksLikeUnwrittenData(const Entry& header) const {
  // TODO: This is not correct; it should call through to flash memory.
  return header.magic() == 0xffffffff;
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
  TRY(CheckOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY(FindKeyDescriptor(key, &key_descriptor));

  if (key_descriptor->deleted()) {
    return Status::NOT_FOUND;
  }

  Entry header;
  TRY(ReadEntryHeader(key_descriptor->address, &header));

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

  TRY(CheckOperation(key));

  if (value.size() > (1 << 24)) {
    // TODO: Reject sizes that are larger than the maximum?
  }

  KeyDescriptor* key_descriptor;
  if (FindKeyDescriptor(key, &key_descriptor).ok()) {
    DBG("Writing over existing entry for key 0x%08" PRIx32 " in sector %zu",
        key_descriptor->key_hash,
        SectorIndex(SectorFromAddress(key_descriptor->address)));
    return WriteEntryForExistingKey(
        key_descriptor, KeyDescriptor::kValid, key, value);
  }

  return WriteEntryForNewKey(key, value);
}

Status KeyValueStore::Delete(string_view key) {
  TRY(CheckOperation(key));

  KeyDescriptor* key_descriptor;
  TRY(FindKeyDescriptor(key, &key_descriptor));

  if (key_descriptor->deleted()) {
    return Status::NOT_FOUND;
  }

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

  Entry header;
  if (item_.kvs_.ReadEntryHeader(descriptor().address, &header).ok()) {
    item_.kvs_.ReadEntryKey(
        descriptor().address, header.key_length(), item_.key_buffer_.data());
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
  TRY(CheckOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY(FindKeyDescriptor(key, &key_descriptor));

  if (key_descriptor->deleted()) {
    return Status::NOT_FOUND;
  }

  Entry header;
  TRY(ReadEntryHeader(key_descriptor->address, &header));

  return StatusWithSize(header.value_length());
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

Status KeyValueStore::FindKeyDescriptor(string_view key,
                                        const KeyDescriptor** result) const {
  char key_buffer[kMaxKeyLength];
  const uint32_t hash = HashKey(key);

  for (auto& descriptor : key_descriptors_) {
    if (descriptor.key_hash == hash) {
      TRY(ReadEntryKey(descriptor.address, key.size(), key_buffer));

      if (key == string_view(key_buffer, key.size())) {
        DBG("Found match for key hash 0x%08" PRIx32, hash);
        *result = &descriptor;
        return Status::OK;
      }
    }
  }
  return Status::NOT_FOUND;
}

Status KeyValueStore::ReadEntryHeader(Address address, Entry* header) const {
  return partition_.Read(address, sizeof(*header), header).status();
}

Status KeyValueStore::ReadEntryKey(Address address,
                                   size_t key_length,
                                   char* key) const {
  // TODO: This check probably shouldn't be here; this is like
  // checking that the Cortex M's RAM isn't corrupt. This should be
  // done at boot time.
  // ^^ This argument sometimes comes from Entry::key_value_len,
  // which is read directly from flash. If it's corrupted, we shouldn't try
  // to read a bunch of extra data.
  if (key_length == 0u || key_length > kMaxKeyLength) {
    return Status::DATA_LOSS;
  }
  // The key is immediately after the entry header.
  return partition_.Read(address + sizeof(EntryHeader), key_length, key)
      .status();
}

StatusWithSize KeyValueStore::ReadEntryValue(
    const KeyDescriptor& key_descriptor,
    const Entry& header,
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
                                               KeyDescriptor::State new_state,
                                               string_view key,
                                               span<const byte> value) {
  // Find the original entry and sector to update the sector's valid_bytes.
  Entry original_entry;
  TRY(ReadEntryHeader(key_descriptor->address, &original_entry));
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
  TRY(AppendEntry(sector, &key_descriptor, key, value));

  // Only add the entry when we are certain the write succeeded.
  key_descriptors_.push_back(key_descriptor);
  return Status::OK;
}

Status KeyValueStore::RelocateEntry(KeyDescriptor& key_descriptor) {
  struct TempEntry {
    std::array<char, kMaxKeyLength + 1> key;
    std::array<char, sizeof(working_buffer_) - sizeof(key)> value;
  };
  TempEntry* entry = reinterpret_cast<TempEntry*>(working_buffer_.data());

  DBG("Relocating entry");  // TODO: add entry info to the log statement.

  // Read the entry to be relocated. Store the header in a local variable and
  // store the key and value in the TempEntry stored in the static allocated
  // working_buffer_.
  Entry header;
  TRY(ReadEntryHeader(key_descriptor.address, &header));
  TRY(ReadEntryKey(
      key_descriptor.address, header.key_length(), entry->key.data()));
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

  // Find a new sector for the entry and write it to the new location.
  SectorDescriptor* new_sector;
  TRY(FindSectorWithSpace(&new_sector, header.size(), old_sector, true));
  TRY(AppendEntry(new_sector, &key_descriptor, key, as_bytes(value)));

  // Do the valid bytes accounting for the sector the entry was relocated out
  // of.
  old_sector->RemoveValidBytes(header.size());

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
  if (result.ok()) {
    return result;
  }
  if (options_.partial_gc_on_write) {
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

  DBG("Found sector %zu to Garbage Collect, %zu recoverable bytes",
      SectorIndex(sector_candidate),
      RecoverableBytes(*sector_candidate));
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
  // write header, key, and value
  Entry header;

  if (new_state == KeyDescriptor::kDeleted) {
    header = Entry::Tombstone(entry_header_format_.magic,
                              entry_header_format_.checksum,
                              key,
                              partition_.alignment_bytes(),
                              key_descriptor->key_version + 1);
  } else {
    header = Entry::Valid(entry_header_format_.magic,
                          entry_header_format_.checksum,
                          key,
                          value,
                          partition_.alignment_bytes(),
                          key_descriptor->key_version + 1);
  }

  DBG("Appending %zu B entry with key version: %x",
      header.size(),
      unsigned(header.key_version()));

  Address address = NextWritableAddress(sector);
  DBG("Appending to address: %#zx", size_t(address));

  // Write multiple concatenated buffers and pad the results.
  FlashPartition::Output flash(partition_, address);
  TRY_ASSIGN(const size_t written,
             AlignedWrite<32>(
                 flash,
                 header.alignment_bytes(),
                 {as_bytes(span(&header, 1)), as_bytes(span(key)), value}));

  if (options_.verify_on_write) {
    TRY(header.VerifyChecksumInFlash(
        &partition_, address, entry_header_format_.checksum));
  }

  key_descriptor->address = address;
  key_descriptor->key_version = header.key_version();
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

void KeyValueStore::LogSectors(void) {
  for (auto& sector : sectors_) {
    DBG("  - Sector %zu: valid %hu, recoverable %zu, free %hu",
        SectorIndex(&sector),
        sector.valid_bytes,
        RecoverableBytes(sector),
        sector.tail_free_bytes);
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
