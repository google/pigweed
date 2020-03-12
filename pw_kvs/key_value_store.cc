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

#include "pw_kvs/format.h"

#define PW_LOG_USE_ULTRA_SHORT_NAMES 1
#include "pw_kvs/internal/entry.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {
namespace {

using std::byte;
using std::string_view;

constexpr bool InvalidKey(std::string_view key) {
  return key.empty() || (key.size() > internal::Entry::kMaxKeyLength);
}

}  // namespace

KeyValueStore::KeyValueStore(FlashPartition* partition,
                             Vector<KeyDescriptor>& key_descriptor_list,
                             Vector<SectorDescriptor>& sector_descriptor_list,
                             size_t redundancy,
                             span<const EntryFormat> formats,
                             const Options& options)
    : partition_(*partition),
      formats_(formats),
      key_descriptors_(key_descriptor_list),
      sectors_(sector_descriptor_list),
      redundancy_(redundancy),
      options_(options) {
  initialized_ = false;
  last_new_sector_ = nullptr;
  last_transaction_id_ = 0;
}

Status KeyValueStore::Init() {
  initialized_ = false;
  last_new_sector_ = nullptr;
  last_transaction_id_ = 0;
  key_descriptors_.clear();

  INF("Initializing key value store");
  if (partition_.sector_count() > sectors_.max_size()) {
    ERR("KVS init failed: kMaxUsableSectors (=%zu) must be at least as "
        "large as the number of sectors in the flash partition (=%zu)",
        sectors_.max_size(),
        partition_.sector_count());
    return Status::FAILED_PRECONDITION;
  }

  const size_t sector_size_bytes = partition_.sector_size_bytes();

  if (working_buffer_.size() < sector_size_bytes) {
    ERR("KVS init failed: working_buffer_ (%zu B) is smaller than sector size "
        "(%zu B)",
        working_buffer_.size(),
        sector_size_bytes);
    return Status::INVALID_ARGUMENT;
  }

  DBG("First pass: Read all entries from all sectors");
  Address sector_address = 0;

  sectors_.assign(partition_.sector_count(),
                  SectorDescriptor(sector_size_bytes));

  size_t total_corrupt_bytes = 0;
  int corrupt_entries = 0;
  bool empty_sector_found = false;

  for (SectorDescriptor& sector : sectors_) {
    Address entry_address = sector_address;

    size_t sector_corrupt_bytes = 0;

    for (int num_entries_in_sector = 0; true; num_entries_in_sector++) {
      DBG("Load entry: sector=%" PRIx32 ", entry#=%d, address=%" PRIx32,
          sector_address,
          num_entries_in_sector,
          entry_address);

      if (!AddressInSector(sector, entry_address)) {
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
        // The entry could not be read, indicating data corruption within the
        // sector. Try to scan the remainder of the sector for other entries.
        WRN("KVS init: data loss detected in sector %u at address %zu",
            SectorIndex(&sector),
            size_t(entry_address));

        corrupt_entries++;

        status = ScanForEntry(sector,
                              entry_address + Entry::kMinAlignmentBytes,
                              &next_entry_address);
        if (status == Status::NOT_FOUND) {
          // No further entries in this sector. Mark the remaining bytes in the
          // sector as corrupt (since we can't reliably know the size of the
          // corrupt entry).
          sector_corrupt_bytes +=
              sector_size_bytes - (entry_address - sector_address);
          break;
        }

        if (!status.ok()) {
          ERR("Unexpected error in KVS initialization: %s", status.str());
          return Status::UNKNOWN;
        }

        sector_corrupt_bytes += next_entry_address - entry_address;
      } else if (!status.ok()) {
        ERR("Unexpected error in KVS initialization: %s", status.str());
        return Status::UNKNOWN;
      }

      // Entry loaded successfully; so get ready to load the next one.
      entry_address = next_entry_address;

      // Update of the number of writable bytes in this sector.
      sector.set_writable_bytes(sector_size_bytes -
                                (entry_address - sector_address));
    }

    if (sector_corrupt_bytes > 0) {
      // If the sector contains corrupt data, prevent any further entries from
      // being written to it by indicating that it has no space. This should
      // also make it a decent GC candidate. Valid keys in the sector are still
      // readable as normal.
      sector.set_writable_bytes(0);

      WRN("Sector %u contains %zuB of corrupt data",
          SectorIndex(&sector),
          sector_corrupt_bytes);
    }

    if (sector.Empty(sector_size_bytes)) {
      empty_sector_found = true;
    }
    sector_address += sector_size_bytes;
    total_corrupt_bytes += sector_corrupt_bytes;
  }

  DBG("Second pass: Count valid bytes in each sector");
  const KeyDescriptor* newest_key = nullptr;

  // For every valid key, increment the valid bytes for that sector.
  for (KeyDescriptor& key_descriptor : key_descriptors_) {
    for (auto& address : key_descriptor.addresses()) {
      Entry entry;
      TRY(Entry::Read(partition_, address, formats_, &entry));
      SectorFromAddress(address)->AddValidBytes(entry.size());
    }
    if (key_descriptor.IsNewerThan(last_transaction_id_)) {
      last_transaction_id_ = key_descriptor.transaction_id();
      newest_key = &key_descriptor;
    }
  }

  if (newest_key == nullptr) {
    last_new_sector_ = sectors_.begin();
  } else {
    last_new_sector_ = SectorFromAddress(newest_key->addresses().back());
  }

  if (!empty_sector_found) {
    // TODO: Record/report the error condition and recovery result.
    Status gc_result = GarbageCollectPartial();

    if (!gc_result.ok()) {
      ERR("KVS init failed: Unable to maintain required free sector");
      return Status::INTERNAL;
    }
  }

  initialized_ = true;

  INF("KeyValueStore init complete: active keys %zu, deleted keys %zu, sectors "
      "%zu, logical sector size %zu bytes",
      size(),
      (key_descriptors_.size() - size()),
      sectors_.size(),
      partition_.sector_size_bytes());

  if (total_corrupt_bytes > 0) {
    WRN("Found %zu corrupt bytes and %d corrupt entries during init process; "
        "some keys may be missing",
        total_corrupt_bytes,
        corrupt_entries);
    return Status::DATA_LOSS;
  }

  return Status::OK;
}

KeyValueStore::StorageStats KeyValueStore::GetStorageStats() const {
  StorageStats stats{0, 0, 0};
  const size_t sector_size = partition_.sector_size_bytes();
  bool found_empty_sector = false;

  for (const SectorDescriptor& sector : sectors_) {
    stats.in_use_bytes += sector.valid_bytes();
    stats.reclaimable_bytes += sector.RecoverableBytes(sector_size);

    if (!found_empty_sector && sector.Empty(sector_size)) {
      // The KVS tries to always keep an empty sector for GC, so don't count
      // the first empty sector seen as writable space. However, a free sector
      // cannot always be assumed to exist; if a GC operation fails, all sectors
      // may be partially written, in which case the space reported might be
      // inaccurate.
      found_empty_sector = true;
      continue;
    }

    stats.writable_bytes += sector.writable_bytes();
  }

  return stats;
}

Status KeyValueStore::LoadEntry(Address entry_address,
                                Address* next_entry_address) {
  Entry entry;
  TRY(Entry::Read(partition_, entry_address, formats_, &entry));

  // Read the key from flash & validate the entry (which reads the value).
  Entry::KeyBuffer key_buffer;
  TRY_ASSIGN(size_t key_length, entry.ReadKey(key_buffer));
  const string_view key(key_buffer.data(), key_length);

  TRY(entry.VerifyChecksumInFlash());

  // A valid entry was found, so update the next entry address before doing any
  // of the checks that happen in AppendNewOrOverwriteStaleExistingDescriptor().
  *next_entry_address = entry.next_address();
  TRY(AppendNewOrOverwriteStaleExistingDescriptor(entry.descriptor(key)));

  return Status::OK;
}

// Scans flash memory within a sector to find a KVS entry magic.
Status KeyValueStore::ScanForEntry(const SectorDescriptor& sector,
                                   Address start_address,
                                   Address* next_entry_address) {
  DBG("Scanning sector %u for entries starting from address %zx",
      SectorIndex(&sector),
      size_t(start_address));

  // Entries must start at addresses which are aligned on a multiple of
  // Entry::kMinAlignmentBytes. However, that multiple can vary between entries.
  // When scanning, we don't have an entry to tell us what the current alignment
  // is, so the minimum alignment is used to be exhaustive.
  for (Address address = AlignUp(start_address, Entry::kMinAlignmentBytes);
       AddressInSector(sector, address);
       address += Entry::kMinAlignmentBytes) {
    uint32_t magic;
    TRY(partition_.Read(address, as_writable_bytes(span(&magic, 1))));
    if (formats_.KnownMagic(magic)) {
      DBG("Found entry magic at address %zx", size_t(address));
      *next_entry_address = address;
      return Status::OK;
    }
  }

  return Status::NOT_FOUND;
}

// TODO: This method is the trigger of the O(valid_entries * all_entries) time
// complexity for reading. At some cost to memory, this could be optimized by
// using a hash table instead of scanning, but in practice this should be fine
// for a small number of keys
Status KeyValueStore::AppendNewOrOverwriteStaleExistingDescriptor(
    const KeyDescriptor& key_descriptor) {
  // With the new key descriptor, either add it to the descriptor table or
  // overwrite an existing entry with an older version of the key.
  KeyDescriptor* existing_descriptor = FindDescriptor(key_descriptor.hash());

  // Write a new entry.
  if (existing_descriptor == nullptr) {
    if (key_descriptors_.full()) {
      return Status::RESOURCE_EXHAUSTED;
    }
    key_descriptors_.push_back(key_descriptor);
  } else if (key_descriptor.IsNewerThan(
                 existing_descriptor->transaction_id())) {
    // Existing entry is old; replace the existing entry with the new one.
    *existing_descriptor = key_descriptor;
  } else if (existing_descriptor->transaction_id() ==
             key_descriptor.transaction_id()) {
    // If the entries have a duplicate transaction ID, add the new (redundant)
    // entry to the existing descriptor.
    if (existing_descriptor->hash() != key_descriptor.hash()) {
      ERR("Duplicate entry for key 0x%08" PRIx32 " with transaction ID %" PRIu32
          " has non-matching hash",
          key_descriptor.hash(),
          key_descriptor.transaction_id());
      return Status::DATA_LOSS;
    }

    // Verify that this entry is not in the same sector as an existing copy of
    // this same key.
    for (auto address : existing_descriptor->addresses()) {
      if (SectorFromAddress(address) ==
          SectorFromAddress(key_descriptor.address())) {
        DBG("Multiple Redundant entries in same sector %u",
            SectorIndex(SectorFromAddress(address)));
        return Status::DATA_LOSS;
      }
    }
    existing_descriptor->addresses().push_back(key_descriptor.address());
  } else {
    DBG("Found stale entry when appending; ignoring");
  }
  return Status::OK;
}

KeyValueStore::KeyDescriptor* KeyValueStore::FindDescriptor(uint32_t hash) {
  for (KeyDescriptor& key_descriptor : key_descriptors_) {
    if (key_descriptor.hash() == hash) {
      return &key_descriptor;
    }
  }
  return nullptr;
}

StatusWithSize KeyValueStore::Get(string_view key,
                                  span<byte> value_buffer,
                                  size_t offset_bytes) const {
  TRY_WITH_SIZE(CheckOperation(key));

  const KeyDescriptor* key_descriptor;
  TRY_WITH_SIZE(FindExistingKeyDescriptor(key, &key_descriptor));

  return Get(key, *key_descriptor, value_buffer, offset_bytes);
}

Status KeyValueStore::PutBytes(string_view key, span<const byte> value) {
  DBG("Writing key/value; key length=%zu, value length=%zu",
      key.size(),
      value.size());

  TRY(CheckOperation(key));

  if (Entry::size(partition_, key, value) > partition_.sector_size_bytes()) {
    DBG("%zu B value with %zu B key cannot fit in one sector",
        value.size(),
        key.size());
    return Status::INVALID_ARGUMENT;
  }

  KeyDescriptor* key_descriptor;
  Status status = FindKeyDescriptor(key, &key_descriptor);

  if (status.ok()) {
    // TODO: figure out logging how to support multiple addresses.
    DBG("Overwriting entry for key 0x%08" PRIx32 " in %u sectors including %u",
        key_descriptor->hash(),
        unsigned(key_descriptor->addresses().size()),
        SectorIndex(SectorFromAddress(key_descriptor->address())));
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

  // TODO: figure out logging how to support multiple addresses.
  DBG("Writing tombstone for key 0x%08" PRIx32 " in %u sectors including %u",
      key_descriptor->hash(),
      unsigned(key_descriptor->addresses().size()),
      SectorIndex(SectorFromAddress(key_descriptor->address())));
  return WriteEntryForExistingKey(
      key_descriptor, KeyDescriptor::kDeleted, key, {});
}

void KeyValueStore::Item::ReadKey() {
  key_buffer_.fill('\0');

  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  if (Entry::Read(
          kvs_.partition_, descriptor_->address(), kvs_.formats_, &entry)
          .ok()) {
    entry.ReadKey(key_buffer_);
  }
}

KeyValueStore::iterator& KeyValueStore::iterator::operator++() {
  // Skip to the next entry that is valid (not deleted).
  while (++item_.descriptor_ != item_.kvs_.key_descriptors_.end() &&
         item_.descriptor_->deleted()) {
  }
  return *this;
}

KeyValueStore::iterator KeyValueStore::begin() const {
  const KeyDescriptor* descriptor = key_descriptors_.begin();
  // Skip over any deleted entries at the start of the descriptor list.
  while (descriptor != key_descriptors_.end() && descriptor->deleted()) {
    ++descriptor;
  }
  return iterator(*this, descriptor);
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

  return ValueSize(*key_descriptor);
}

StatusWithSize KeyValueStore::Get(string_view key,
                                  const KeyDescriptor& descriptor,
                                  span<std::byte> value_buffer,
                                  size_t offset_bytes) const {
  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY_WITH_SIZE(
      Entry::Read(partition_, descriptor.address(), formats_, &entry));

  StatusWithSize result = entry.ReadValue(value_buffer, offset_bytes);
  if (result.ok() && options_.verify_on_read && offset_bytes == 0u) {
    Status verify_result =
        entry.VerifyChecksum(key, value_buffer.first(result.size()));
    if (!verify_result.ok()) {
      std::memset(value_buffer.data(), 0, result.size());
      return StatusWithSize(verify_result, 0);
    }

    return StatusWithSize(verify_result, result.size());
  }
  return result;
}

Status KeyValueStore::FixedSizeGet(std::string_view key,
                                   void* value,
                                   size_t size_bytes) const {
  TRY(CheckOperation(key));

  const KeyDescriptor* descriptor;
  TRY(FindExistingKeyDescriptor(key, &descriptor));

  return FixedSizeGet(key, *descriptor, value, size_bytes);
}

Status KeyValueStore::FixedSizeGet(std::string_view key,
                                   const KeyDescriptor& descriptor,
                                   void* value,
                                   size_t size_bytes) const {
  // Ensure that the size of the stored value matches the size of the type.
  // Otherwise, report error. This check avoids potential memory corruption.
  TRY_ASSIGN(const size_t actual_size, ValueSize(descriptor));

  if (actual_size != size_bytes) {
    DBG("Requested %zu B read, but value is %zu B", size_bytes, actual_size);
    return Status::INVALID_ARGUMENT;
  }

  StatusWithSize result =
      Get(key, descriptor, span(static_cast<byte*>(value), size_bytes), 0);

  return result.status();
}

StatusWithSize KeyValueStore::ValueSize(const KeyDescriptor& descriptor) const {
  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY_WITH_SIZE(
      Entry::Read(partition_, descriptor.address(), formats_, &entry));

  return StatusWithSize(entry.value_size());
}

Status KeyValueStore::CheckOperation(string_view key) const {
  if (InvalidKey(key)) {
    return Status::INVALID_ARGUMENT;
  }
  if (!initialized()) {
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
  const uint32_t hash = internal::Hash(key);
  Entry::KeyBuffer key_buffer;

  for (auto& descriptor : key_descriptors_) {
    if (descriptor.hash() == hash) {
      // TODO: add support for using one of the redundant entries if reading the
      // first copy fails.
      TRY(Entry::ReadKey(
          partition_, descriptor.address(), key.size(), key_buffer.data()));

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
  Entry original_entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY(Entry::Read(
      partition_, key_descriptor->address(), formats_, &original_entry));

  // Create a new temporary key descriptor to use while writing the new
  // key-value out to flash. Once the writing is done, update the main
  // descriptor for this key with the new information.
  KeyDescriptor new_key_descriptor(key);
  TRY(WriteEntry(&new_key_descriptor, key, value, new_state));

  // Update the main descriptor for the new key version.
  KeyDescriptor old_key_descriptor = *key_descriptor;
  *key_descriptor = new_key_descriptor;

  // Remove all the valid bytes for the old key version, which are now stale.
  for (auto& address : old_key_descriptor.addresses()) {
    SectorFromAddress(address)->RemoveValidBytes(original_entry.size());
  }

  return Status::OK;
}

Status KeyValueStore::WriteEntryForNewKey(string_view key,
                                          span<const byte> value) {
  if (key_descriptors_.full()) {
    WRN("KVS full: trying to store a new entry, but can't. Have %zu entries",
        key_descriptors_.size());
    return Status::RESOURCE_EXHAUSTED;
  }

  // Create the KeyDescriptor that will be added to the list and write it to
  // flash.
  KeyDescriptor key_descriptor(key);
  TRY(WriteEntry(&key_descriptor, key, value, KeyDescriptor::kValid));

  // Only add the entry when we are certain the write succeeded.
  key_descriptors_.push_back(key_descriptor);
  return Status::OK;
}

Status KeyValueStore::WriteEntry(KeyDescriptor* key_descriptor,
                                 string_view key,
                                 span<const byte> value,
                                 KeyDescriptor::State new_state) {
  size_t entry_size = Entry::size(partition_, key, value);

  Entry entry = CreateEntry(0, key, value, new_state);
  *key_descriptor = entry.descriptor(key);
  key_descriptor->addresses().clear();

  // For number of redundany entries to be written, do the following:
  // - Find a sector to write an individual entry to. This optionally will
  //   include garbage collecting one or more sectors if needed.
  // - Write the entry to the sector.
  // - Repeat for redundancy number of total entries.
  for (size_t i = 0; i < redundancy_; i++) {
    SectorDescriptor* sector;
    TRY(GetSectorForWrite(&sector, entry_size, key_descriptor));

    DBG("Writing entry %zu; found sector: %u", i, SectorIndex(sector));
    const Address write_address = NextWritableAddress(sector);
    TRY(AppendEntry(write_address, entry, key, value));

    // Entry copy was written successfully; update the key descriptor to reflect
    // the new entry.
    key_descriptor->addresses().push_back(write_address);
  }

  // Once all the entries are written, add valid bytes to each of the sectors
  // that entries were written to.
  for (auto new_address : key_descriptor->addresses()) {
    SectorFromAddress(new_address)->AddValidBytes(entry.size());
  }

  return Status::OK;
}

// Find a sector to use for writing a new entry to. Do automatic garbage
// collection if needed and allowed.
//
//                 OK: Sector found with needed space.
// RESOURCE_EXHAUSTED: No sector available with the needed space.
Status KeyValueStore::GetSectorForWrite(SectorDescriptor** sector,
                                        size_t entry_size,
                                        KeyDescriptor* key_descriptor) {
  Status result = FindSectorWithSpace(
      sector, entry_size, kAppendEntry, key_descriptor->addresses());

  size_t gc_sector_count = 0;
  bool do_auto_gc = options_.gc_on_write != GargbageCollectOnWrite::kDisabled;

  // Do garbage collection as needed, so long as policy allows.
  while (result == Status::RESOURCE_EXHAUSTED && do_auto_gc) {
    if (options_.gc_on_write == GargbageCollectOnWrite::kOneSector) {
      // If GC config option is kOneSector clear the flag to not do any more
      // GC after this try.
      do_auto_gc = false;
    }
    // Garbage collect and then try again to find the best sector.
    Status gc_status = GarbageCollectPartial(key_descriptor);
    if (!gc_status.ok()) {
      if (gc_status == Status::NOT_FOUND) {
        // Not enough space, and no reclaimable bytes, this KVS is full!
        return Status::RESOURCE_EXHAUSTED;
      }
      return gc_status;
    }

    result = FindSectorWithSpace(
        sector, entry_size, kAppendEntry, key_descriptor->addresses());

    gc_sector_count++;
    // Allow total sectors + 2 number of GC cycles so that once reclaimable
    // bytes in all the sectors have been reclaimed can try and free up space by
    // moving entries for keys other than the one being worked on in to sectors
    // that have copies of the key trying to be written.
    if (gc_sector_count > (partition_.sector_count() + 2)) {
      ERR("Did more GC sectors than total sectors!!!!");
      return Status::RESOURCE_EXHAUSTED;
    }
  }

  if (!result.ok()) {
    WRN("Unable to find sector to write %zu B", entry_size);
  }
  return result;
}

Status KeyValueStore::AppendEntry(Address write_address,
                                  Entry& entry,
                                  string_view key,
                                  span<const byte> value) {
  entry.set_address(write_address);

  StatusWithSize result = entry.Write(key, value);
  // Remove any bytes that were written, even if the write was not successful.
  // This is important to retain the writable space invariant on the sectors.
  SectorFromAddress(write_address)->RemoveWritableBytes(result.size());

  if (!result.ok()) {
    ERR("Failed to write %zu bytes at %#zx. %zu actually written",
        entry.size(),
        size_t(write_address),
        result.size());
    return result.status();
  }

  if (options_.verify_on_write) {
    TRY(entry.VerifyChecksumInFlash());
  }

  return Status::OK;
}

Status KeyValueStore::RelocateEntry(KeyDescriptor* key_descriptor,
                                    KeyValueStore::Address old_address) {
  Entry entry;
  TRY(Entry::Read(partition_, old_address, formats_, &entry));

  // Find a new sector for the entry and write it to the new location. For
  // relocation the find should not not be a sector already containing the key
  // but can be the always empty sector, since this is part of the GC process
  // that will result in a new empty sector. Also find a sector that does not
  // have reclaimable space (mostly for the full GC, where that would result in
  // an immediate extra relocation).
  SectorDescriptor* new_sector;

  TRY(FindSectorWithSpace(
      &new_sector, entry.size(), kGarbageCollect, key_descriptor->addresses()));
  const Address new_address = NextWritableAddress(new_sector);
  TRY(MoveEntry(new_address, entry));

  // TODO: Perhaps add check that the entry matches the key descriptor (key
  // hash, ID, checksum).

  // Entry was written successfully; update the key descriptor and the sector
  // descriptors to reflect the new entry.
  TRY(key_descriptor->UpdateAddress(old_address, new_address));

  if ((key_descriptor >= key_descriptors_.begin()) &&
      (key_descriptor < key_descriptors_.end())) {
    // If the key_descriptor is in the main key_descriptors_ list, it has been
    // accounted for in valid bytes, so only do valid byte updates for those
    // descriptors.
    new_sector->AddValidBytes(entry.size());
    SectorFromAddress(old_address)->RemoveValidBytes(entry.size());
  }

  return Status::OK;
}

Status KeyValueStore::MoveEntry(Address new_address, Entry& entry) {
  // Step 1: Read the old entry.
  struct TempEntry {
    Entry::KeyBuffer key;
    std::array<byte, sizeof(working_buffer_) - sizeof(key)> value;
  };
  auto [key_buffer, value_buffer] =
      *std::launder(reinterpret_cast<TempEntry*>(working_buffer_.data()));

  // Read the entry to be relocated. Store the entry in a local variable and
  // store the key and value in the TempEntry stored in the static allocated
  // working_buffer_.
  TRY_ASSIGN(size_t key_length, entry.ReadKey(key_buffer));
  string_view key = string_view(key_buffer.data(), key_length);

  StatusWithSize result = entry.ReadValue(value_buffer);
  if (!result.ok()) {
    return Status::INTERNAL;
  }

  const span value = span(value_buffer.data(), result.size());
  TRY(entry.VerifyChecksum(key, value));

  DBG("Moving %zu B entry with transaction ID %zu to to sector %u address %#zx",
      entry.size(),
      size_t(entry.transaction_id()),
      SectorIndex(SectorFromAddress(new_address)),
      size_t(new_address));

  // Step 2: Write the entry to the new location.
  entry.set_address(new_address);
  result = entry.Write(key, value);

  // Remove any bytes that were written, even if the write was not successful.
  // This is important to retain the writable space invariant on the sectors.
  SectorFromAddress(new_address)->RemoveWritableBytes(result.size());

  if (!result.ok()) {
    ERR("Failed to write %zu bytes at %" PRIx32 ". %zu actually written",
        entry.size(),
        new_address,
        result.size());
    return result.status();
  }

  // Step 3: Verify write to the new location.
  if (options_.verify_on_write) {
    TRY(entry.VerifyChecksumInFlash());
  }

  return Status::OK;
}

// Find if search_set contains value.
// TODO: At some point move this to pw_containers, along with adding tests for
// it.
template <typename Container, typename T>
bool Contains(const Container& search_set, const T& value) {
  return std::find(std::begin(search_set), std::end(search_set), value) !=
         std::end(search_set);
}

// Find either an existing sector with enough space that is not the sector to
// skip, or an empty sector. Maintains the invariant that there is always at
// least 1 empty sector except during GC. On GC, skip sectors that have
// reclaimable bytes.
Status KeyValueStore::FindSectorWithSpace(
    SectorDescriptor** found_sector,
    size_t size,
    FindSectorMode find_mode,
    span<const Address> addresses_to_skip) {
  SectorDescriptor* first_empty_sector = nullptr;
  bool at_least_two_empty_sectors = (find_mode == kGarbageCollect);

  // Used for the GC reclaimable bytes check
  SectorDescriptor* non_empty_least_reclaimable_sector = nullptr;
  const size_t sector_size_bytes = partition_.sector_size_bytes();

  // Build a vector of sectors to avoid.
  Vector<SectorDescriptor*, internal::kEntryRedundancy> sectors_to_skip;
  for (auto& address : addresses_to_skip) {
    sectors_to_skip.push_back(SectorFromAddress(address));
  }

  DBG("Find sector with %zu bytes available, starting with sector %u, %s",
      size,
      SectorIndex(last_new_sector_),
      (find_mode == kAppendEntry) ? "Append" : "GC");
  for (auto& skip_sector : sectors_to_skip) {
    DBG("  Skip sector %u", SectorIndex(skip_sector));
  }

  // The last_new_sector_ is the sector that was last selected as the "new empty
  // sector" to write to. This last new sector is used as the starting point for
  // the next "find a new empty sector to write to" operation. By using the last
  // new sector as the start point we will cycle which empty sector is selected
  // next, spreading the wear across all the empty sectors and get a wear
  // leveling benefit, rather than putting more wear on the lower number
  // sectors.
  SectorDescriptor* sector = last_new_sector_;

  // Look for a sector to use with enough space. The search uses a 3 priority
  // tier process.
  //
  // Tier 1 is sector that already has valid data. During GC only select a
  // sector that has no reclaimable bytes. Immediately use the first matching
  // sector that is found.
  //
  // Tier 2 is find sectors that are empty/erased. While scanning for a partial
  // sector, keep track of the first empty sector and if a second empty sector
  // was seen. If during GC then count the second empty sector as always seen.
  //
  // Tier 3 is during garbage collection, find sectors with enough space that
  // are not empty but have recoverable bytes. Pick the sector with the least
  // recoverable bytes to minimize the likelyhood of this sector needing to be
  // garbage collected soon.
  for (size_t j = 0; j < sectors_.size(); j++) {
    sector += 1;
    if (sector == sectors_.end()) {
      sector = sectors_.begin();
    }

    // Skip sectors in the skip list.
    if (Contains(sectors_to_skip, sector)) {
      continue;
    }

    if (!sector->Empty(sector_size_bytes) && sector->HasSpace(size)) {
      if ((find_mode == kAppendEntry) ||
          (sector->RecoverableBytes(sector_size_bytes) == 0)) {
        *found_sector = sector;
        return Status::OK;
      } else {
        if ((non_empty_least_reclaimable_sector == nullptr) ||
            (non_empty_least_reclaimable_sector->RecoverableBytes(
                 sector_size_bytes) <
             sector->RecoverableBytes(sector_size_bytes))) {
          non_empty_least_reclaimable_sector = sector;
        }
      }
    }

    if (sector->Empty(sector_size_bytes)) {
      if (first_empty_sector == nullptr) {
        first_empty_sector = sector;
      } else {
        at_least_two_empty_sectors = true;
      }
    }
  }

  // Tier 2 check: If the scan for a partial sector does not find a suitable
  // sector, use the first empty sector that was found. Normally it is required
  // to keep 1 empty sector after the sector found here, but that rule does not
  // apply during GC.
  if (first_empty_sector != nullptr && at_least_two_empty_sectors) {
    DBG("  Found a usable empty sector; returning the first found (%u)",
        SectorIndex(first_empty_sector));
    last_new_sector_ = first_empty_sector;
    *found_sector = first_empty_sector;
    return Status::OK;
  }

  // Tier 3 check: If we got this far, use the sector with least recoverable
  // bytes
  if (non_empty_least_reclaimable_sector != nullptr) {
    *found_sector = non_empty_least_reclaimable_sector;
    DBG("  Found a usable sector %u, with %zu B recoverable, in GC",
        SectorIndex(*found_sector),
        (*found_sector)->RecoverableBytes(sector_size_bytes));
    return Status::OK;
  }

  // No sector was found.
  DBG("  Unable to find a usable sector");
  *found_sector = nullptr;
  return Status::RESOURCE_EXHAUSTED;
}

// TODO: Break up this function in to smaller sub-chunks including create an
// abstraction for the sector list. Look in to being able to unit test this as
// its own thing
KeyValueStore::SectorDescriptor* KeyValueStore::FindSectorToGarbageCollect(
    span<const Address> addresses_to_avoid) {
  const size_t sector_size_bytes = partition_.sector_size_bytes();
  SectorDescriptor* sector_candidate = nullptr;
  size_t candidate_bytes = 0;

  // Build a vector of sectors to avoid.
  Vector<const SectorDescriptor*, internal::kEntryRedundancy> sectors_to_skip;
  for (auto& address : addresses_to_avoid) {
    sectors_to_skip.push_back(SectorFromAddress(address));
    DBG("    Skip sector %u", SectorIndex(SectorFromAddress(address)));
  }

  // Step 1: Try to find a sectors with stale keys and no valid keys (no
  // relocation needed). If any such sectors are found, use the sector with the
  // most reclaimable bytes.
  for (auto& sector : sectors_) {
    if ((sector.valid_bytes() == 0) &&
        (sector.RecoverableBytes(sector_size_bytes) > candidate_bytes) &&
        !Contains(sectors_to_skip, &sector)) {
      sector_candidate = &sector;
      candidate_bytes = sector.RecoverableBytes(sector_size_bytes);
    }
  }

  // Step 2a: If step 1 yields no sectors, just find the sector with the most
  // reclaimable bytes but no addresses to avoid.
  if (sector_candidate == nullptr) {
    for (auto& sector : sectors_) {
      if ((sector.RecoverableBytes(sector_size_bytes) > candidate_bytes) &&
          !Contains(sectors_to_skip, &sector)) {
        sector_candidate = &sector;
        candidate_bytes = sector.RecoverableBytes(sector_size_bytes);
      }
    }
  }

  // Step 2b: If step 1 yields no sectors, just find the sector with the most
  // reclaimable bytes.
  if (sector_candidate == nullptr) {
    for (auto& sector : sectors_) {
      if (sector.RecoverableBytes(sector_size_bytes) > candidate_bytes) {
        sector_candidate = &sector;
        candidate_bytes = sector.RecoverableBytes(sector_size_bytes);
      }
    }
  }

  // Step 3: If no sectors with reclaimable bytes, select the sector with the
  // most free bytes. This at least will allow entries of existing keys to get
  // spread to other sectors, including sectors that already have copies of the
  // current key being written.
  if (sector_candidate == nullptr) {
    for (auto& sector : sectors_) {
      if ((sector.valid_bytes() > candidate_bytes) &&
          !Contains(sectors_to_skip, &sector)) {
        sector_candidate = &sector;
        candidate_bytes = sector.valid_bytes();
        DBG("    Doing GC on sector with no reclaimable bytes!");
      }
    }
  }

  if (sector_candidate != nullptr) {
    DBG("Found sector %u to Garbage Collect, %zu recoverable bytes",
        SectorIndex(sector_candidate),
        sector_candidate->RecoverableBytes(sector_size_bytes));
  } else {
    DBG("Unable to find sector to garbage collect!");
  }
  return sector_candidate;
}

Status KeyValueStore::GarbageCollectFull() {
  DBG("Garbage Collect all sectors");
  SectorDescriptor* sector = last_new_sector_;

  // TODO: look in to making an iterator method for cycling through sectors
  // starting from last_new_sector_.
  for (size_t j = 0; j < sectors_.size(); j++) {
    sector += 1;
    if (sector == sectors_.end()) {
      sector = sectors_.begin();
    }

    if (sector->RecoverableBytes(partition_.sector_size_bytes()) > 0) {
      TRY(GarbageCollectSector(sector));
    }
  }

  DBG("Garbage Collect all complete");
  return Status::OK;
}

Status KeyValueStore::GarbageCollectPartial(KeyDescriptor* key_in_progress) {
  DBG("Garbage Collect a single sector%s",
      (key_in_progress == nullptr) ? "" : ", with key in progress");

  // Step 1: Find the sector to garbage collect
  auto addresses = span<const Address>();
  if (key_in_progress != nullptr) {
    DBG("  Use addresses to avoid");
    addresses = key_in_progress->addresses();
  }
  SectorDescriptor* sector_to_gc = FindSectorToGarbageCollect(addresses);

  if (sector_to_gc == nullptr) {
    // Nothing to GC.
    return Status::NOT_FOUND;
  }

  TRY(GarbageCollectSector(sector_to_gc, key_in_progress));
  return Status::OK;
}

Status KeyValueStore::RelocateKeyAddressesInSector(
    internal::SectorDescriptor* sector_to_gc,
    internal::KeyDescriptor* descriptor) {
  for (auto address : descriptor->addresses()) {
    if (AddressInSector(*sector_to_gc, address)) {
      DBG("  Relocate entry for Key 0x%08zx, address %zu",
          size_t(descriptor->hash()),
          size_t(address));
      TRY(RelocateEntry(descriptor, address));
    }
  }
  return Status::OK;
};

Status KeyValueStore::GarbageCollectSector(SectorDescriptor* sector_to_gc,
                                           KeyDescriptor* key_in_progress) {
  // Pre-step: Check if the key in progress has any addresses in the sector to
  // GC.
  bool key_in_progress_in_sector = false;
  if (key_in_progress != nullptr) {
    for (Address address : key_in_progress->addresses()) {
      if (AddressInSector(*sector_to_gc, address)) {
        key_in_progress_in_sector = true;
        break;
      }
    }
  }

  // Step 1: Move any valid entries in the GC sector to other sectors
  if ((sector_to_gc->valid_bytes() != 0 || key_in_progress_in_sector)) {
    if (key_in_progress != nullptr) {
      TRY(RelocateKeyAddressesInSector(sector_to_gc, key_in_progress));
    }
    for (auto& descriptor : key_descriptors_) {
      TRY(RelocateKeyAddressesInSector(sector_to_gc, &descriptor));
    }
  }

  if (sector_to_gc->valid_bytes() != 0) {
    ERR("  Failed to relocate valid entries from sector being garbage "
        "collected, %zu valid bytes remain",
        sector_to_gc->valid_bytes());
    return Status::INTERNAL;
  }

  // Step 2: Reinitialize the sector
  sector_to_gc->set_writable_bytes(0);
  TRY(partition_.Erase(SectorBaseAddress(sector_to_gc), 1));
  sector_to_gc->set_writable_bytes(partition_.sector_size_bytes());

  DBG("  Garbage Collect sector %u complete", SectorIndex(sector_to_gc));
  return Status::OK;
}

KeyValueStore::Entry KeyValueStore::CreateEntry(Address address,
                                                std::string_view key,
                                                span<const byte> value,
                                                KeyDescriptor::State state) {
  // Always bump the transaction ID when creating a new entry.
  //
  // Burning transaction IDs prevents inconsistencies between flash and memory
  // that which could happen if a write succeeds, but for some reason the read
  // and verify step fails. Here's how this would happen:
  //
  //   1. The entry is written but for some reason the flash reports failure OR
  //      The write succeeds, but the read / verify operation fails.
  //   2. The transaction ID is NOT incremented, because of the failure
  //   3. (later) A new entry is written, re-using the transaction ID (oops)
  //
  // By always burning transaction IDs, the above problem can't happen.
  last_transaction_id_ += 1;

  if (state == KeyDescriptor::kDeleted) {
    return Entry::Tombstone(
        partition_, address, formats_.primary(), key, last_transaction_id_);
  }
  return Entry::Valid(partition_,
                      address,
                      formats_.primary(),
                      key,
                      value,
                      last_transaction_id_);
}

void KeyValueStore::LogDebugInfo() {
  const size_t sector_size_bytes = partition_.sector_size_bytes();
  DBG("====================== KEY VALUE STORE DUMP =========================");
  DBG(" ");
  DBG("Flash partition:");
  DBG("  Sector count     = %zu", partition_.sector_count());
  DBG("  Sector max count = %zu", sectors_.max_size());
  DBG("  Sectors in use   = %zu", sectors_.size());
  DBG("  Sector size      = %zu", sector_size_bytes);
  DBG("  Total size       = %zu", partition_.size_bytes());
  DBG("  Alignment        = %zu", partition_.alignment_bytes());
  DBG(" ");
  DBG("Key descriptors:");
  DBG("  Entry count     = %zu", key_descriptors_.size());
  DBG("  Max entry count = %zu", key_descriptors_.max_size());
  DBG(" ");
  DBG("      #     hash        version    address   address (hex)");
  for (size_t i = 0; i < key_descriptors_.size(); ++i) {
    const KeyDescriptor& kd = key_descriptors_[i];
    DBG("   |%3zu: | %8zx  |%8zu  | %8zu | %8zx",
        i,
        size_t(kd.hash()),
        size_t(kd.transaction_id()),
        size_t(kd.address()),
        size_t(kd.address()));
  }
  DBG(" ");

  DBG("Sector descriptors:");
  DBG("      #     tail free  valid    has_space");
  for (size_t sector_id = 0; sector_id < sectors_.size(); ++sector_id) {
    const SectorDescriptor& sd = sectors_[sector_id];
    DBG("   |%3zu: | %8zu  |%8zu  | %s",
        sector_id,
        size_t(sd.writable_bytes()),
        sd.valid_bytes(),
        sd.writable_bytes() ? "YES" : "");
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
    DBG("  - Sector %u: valid %zu, recoverable %zu, free %zu",
        SectorIndex(&sector),
        sector.valid_bytes(),
        sector.RecoverableBytes(partition_.sector_size_bytes()),
        sector.writable_bytes());
  }
}

void KeyValueStore::LogKeyDescriptor() const {
  DBG("Key descriptors: count %zu", key_descriptors_.size());
  for (auto& key : key_descriptors_) {
    DBG("  - Key: %s, hash %#zx, transaction ID %zu, address %#zx",
        key.deleted() ? "Deleted" : "Valid",
        static_cast<size_t>(key.hash()),
        static_cast<size_t>(key.transaction_id()),
        static_cast<size_t>(key.address()));
  }
}

}  // namespace pw::kvs
