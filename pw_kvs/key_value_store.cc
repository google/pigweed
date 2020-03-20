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
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {
namespace {

using std::byte;
using std::string_view;

constexpr bool InvalidKey(std::string_view key) {
  return key.empty() || (key.size() > internal::Entry::kMaxKeyLength);
}

// Returns true if the container conatins the value.
// TODO: At some point move this to pw_containers, along with adding tests.
template <typename Container, typename T>
bool Contains(const Container& container, const T& value) {
  return std::find(std::begin(container), std::end(container), value) !=
         std::end(container);
}

}  // namespace

KeyValueStore::KeyValueStore(FlashPartition* partition,
                             span<const EntryFormat> formats,
                             const Options& options,
                             size_t redundancy,
                             Vector<SectorDescriptor>& sector_descriptor_list,
                             const SectorDescriptor** temp_sectors_to_skip,
                             Vector<KeyDescriptor>& key_descriptor_list,
                             Address* addresses)
    : partition_(*partition),
      formats_(formats),
      entry_cache_(key_descriptor_list, addresses, redundancy),
      sectors_(sector_descriptor_list),
      temp_sectors_to_skip_(temp_sectors_to_skip),
      options_(options),
      initialized_(false),
      error_detected_(false),
      last_new_sector_(nullptr),
      last_transaction_id_(0) {}

Status KeyValueStore::Init() {
  initialized_ = false;
  error_detected_ = false;
  last_new_sector_ = nullptr;
  last_transaction_id_ = 0;
  entry_cache_.Reset();

  INF("Initializing key value store");
  if (partition_.sector_count() > sectors_.max_size()) {
    ERR("KVS init failed: kMaxUsableSectors (=%zu) must be at least as "
        "large as the number of sectors in the flash partition (=%zu)",
        sectors_.max_size(),
        partition_.sector_count());
    return Status::FAILED_PRECONDITION;
  }

  const size_t sector_size_bytes = partition_.sector_size_bytes();

  // TODO: investigate doing this as a static assert/compile-time check.
  if (sector_size_bytes > SectorDescriptor::max_sector_size()) {
    ERR("KVS init failed: sector_size_bytes (=%zu) is greater than maximum "
        "allowed sector size (=%zu)",
        sector_size_bytes,
        SectorDescriptor::max_sector_size());
    return Status::FAILED_PRECONDITION;
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

        error_detected_ = true;
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
      sector.mark_corrupt();
      error_detected_ = true;

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
  Address newest_key = 0;

  // For every valid entry, count the valid bytes in that sector. Track which
  // entry has the newest transaction ID for initializing last_new_sector_.
  for (const EntryMetadata& metadata : entry_cache_) {
    if (metadata.addresses().size() < redundancy()) {
      error_detected_ = true;
    }
    for (Address address : metadata.addresses()) {
      Entry entry;
      TRY(Entry::Read(partition_, address, formats_, &entry));
      SectorFromAddress(address)->AddValidBytes(entry.size());
    }
    if (metadata.IsNewerThan(last_transaction_id_)) {
      last_transaction_id_ = metadata.transaction_id();
      newest_key = metadata.addresses().back();
    }
  }

  last_new_sector_ = SectorFromAddress(newest_key);

  if (error_detected_) {
    Status recovery_status = Repair();
    if (recovery_status.ok()) {
      INF("KVS init: Corruption detected and fully repaired");
    } else {
      ERR("KVS init: Corruption detected and unable repair");
    }
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
      (entry_cache_.total_entries() - size()),
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
  // of the checks that happen in AddNewOrUpdateExisting.
  *next_entry_address = entry.next_address();
  return entry_cache_.AddNewOrUpdateExisting(
      entry.descriptor(key), entry.address(), partition_.sector_size_bytes());
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

StatusWithSize KeyValueStore::Get(string_view key,
                                  span<byte> value_buffer,
                                  size_t offset_bytes) const {
  TRY_WITH_SIZE(CheckOperation(key));

  EntryMetadata metadata;
  TRY_WITH_SIZE(entry_cache_.FindExisting(partition_, key, &metadata));

  return Get(key, metadata, value_buffer, offset_bytes);
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

  EntryMetadata metadata;
  Status status = entry_cache_.Find(partition_, key, &metadata);

  if (status.ok()) {
    // TODO: figure out logging how to support multiple addresses.
    DBG("Overwriting entry for key 0x%08" PRIx32 " in %zu sectors including %u",
        metadata.hash(),
        metadata.addresses().size(),
        SectorIndex(SectorFromAddress(metadata.first_address())));
    return WriteEntryForExistingKey(metadata, EntryState::kValid, key, value);
  }

  if (status == Status::NOT_FOUND) {
    return WriteEntryForNewKey(key, value);
  }

  return status;
}

Status KeyValueStore::Delete(string_view key) {
  TRY(CheckOperation(key));

  EntryMetadata metadata;
  TRY(entry_cache_.FindExisting(partition_, key, &metadata));

  // TODO: figure out logging how to support multiple addresses.
  DBG("Writing tombstone for key 0x%08" PRIx32 " in %zu sectors including %u",
      metadata.hash(),
      metadata.addresses().size(),
      SectorIndex(SectorFromAddress(metadata.first_address())));
  return WriteEntryForExistingKey(metadata, EntryState::kDeleted, key, {});
}

void KeyValueStore::Item::ReadKey() {
  key_buffer_.fill('\0');

  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  if (Entry::Read(
          kvs_.partition_, iterator_->first_address(), kvs_.formats_, &entry)
          .ok()) {
    entry.ReadKey(key_buffer_);
  }
}

KeyValueStore::iterator& KeyValueStore::iterator::operator++() {
  // Skip to the next entry that is valid (not deleted).
  while (++item_.iterator_ != item_.kvs_.entry_cache_.end() &&
         item_.iterator_->state() != EntryState::kValid) {
  }
  return *this;
}

KeyValueStore::iterator KeyValueStore::begin() const {
  internal::EntryCache::iterator cache_iterator = entry_cache_.begin();
  // Skip over any deleted entries at the start of the descriptor list.
  while (cache_iterator != entry_cache_.end() &&
         cache_iterator->state() != EntryState::kValid) {
    ++cache_iterator;
  }
  return iterator(*this, cache_iterator);
}

StatusWithSize KeyValueStore::ValueSize(string_view key) const {
  TRY_WITH_SIZE(CheckOperation(key));

  EntryMetadata metadata;
  TRY_WITH_SIZE(entry_cache_.FindExisting(partition_, key, &metadata));

  return ValueSize(metadata);
}

StatusWithSize KeyValueStore::Get(string_view key,
                                  const EntryMetadata& metadata,
                                  span<std::byte> value_buffer,
                                  size_t offset_bytes) const {
  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY_WITH_SIZE(
      Entry::Read(partition_, metadata.first_address(), formats_, &entry));

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

  EntryMetadata metadata;
  TRY(entry_cache_.FindExisting(partition_, key, &metadata));

  return FixedSizeGet(key, metadata, value, size_bytes);
}

Status KeyValueStore::FixedSizeGet(std::string_view key,
                                   const EntryMetadata& metadata,
                                   void* value,
                                   size_t size_bytes) const {
  // Ensure that the size of the stored value matches the size of the type.
  // Otherwise, report error. This check avoids potential memory corruption.
  TRY_ASSIGN(const size_t actual_size, ValueSize(metadata));

  if (actual_size != size_bytes) {
    DBG("Requested %zu B read, but value is %zu B", size_bytes, actual_size);
    return Status::INVALID_ARGUMENT;
  }

  StatusWithSize result =
      Get(key, metadata, span(static_cast<byte*>(value), size_bytes), 0);

  return result.status();
}

StatusWithSize KeyValueStore::ValueSize(const EntryMetadata& metadata) const {
  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY_WITH_SIZE(
      Entry::Read(partition_, metadata.first_address(), formats_, &entry));

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

Status KeyValueStore::WriteEntryForExistingKey(EntryMetadata& metadata,
                                               EntryState new_state,
                                               string_view key,
                                               span<const byte> value) {
  // Read the original entry to get the size for sector accounting purposes.
  Entry entry;
  // TODO: add support for using one of the redundant entries if reading the
  // first copy fails.
  TRY(Entry::Read(partition_, metadata.first_address(), formats_, &entry));

  return WriteEntry(key, value, new_state, &metadata, entry.size());
}

Status KeyValueStore::WriteEntryForNewKey(string_view key,
                                          span<const byte> value) {
  if (entry_cache_.full()) {
    WRN("KVS full: trying to store a new entry, but can't. Have %zu entries",
        entry_cache_.total_entries());
    return Status::RESOURCE_EXHAUSTED;
  }

  return WriteEntry(key, value, EntryState::kValid);
}

Status KeyValueStore::WriteEntry(string_view key,
                                 span<const byte> value,
                                 EntryState new_state,
                                 EntryMetadata* prior_metadata,
                                 size_t prior_size) {
  const size_t entry_size = Entry::size(partition_, key, value);

  // List of addresses for sectors with space for this entry.
  Address* reserved_addresses = entry_cache_.TempReservedAddressesForWrite();

  // Find sectors to write the entry to. This may involve garbage collecting one
  // or more sectors.
  for (size_t i = 0; i < redundancy(); i++) {
    SectorDescriptor* sector;
    TRY(GetSectorForWrite(&sector, entry_size, span(reserved_addresses, i)));

    DBG("Found space for entry in sector %u", SectorIndex(sector));
    reserved_addresses[i] = NextWritableAddress(sector);
  }

  // Write the entry at the first address that was found.
  Entry entry = CreateEntry(reserved_addresses[0], key, value, new_state);
  TRY(AppendEntry(entry, key, value));

  // After writing the first entry successfully, update the key descriptors.
  // Once a single new the entry is written, the old entries are invalidated.
  EntryMetadata new_metadata =
      UpdateKeyDescriptor(entry, key, prior_metadata, prior_size);

  // Write the additional copies of the entry, if redundancy is greater than 1.
  for (size_t i = 1; i < redundancy(); ++i) {
    entry.set_address(reserved_addresses[i]);
    TRY(AppendEntry(entry, key, value));
    new_metadata.AddNewAddress(reserved_addresses[i]);
  }
  return Status::OK;
}

KeyValueStore::EntryMetadata KeyValueStore::UpdateKeyDescriptor(
    const Entry& entry,
    string_view key,
    EntryMetadata* prior_metadata,
    size_t prior_size) {
  // If there is no prior descriptor, create a new one.
  if (prior_metadata == nullptr) {
    return entry_cache_.AddNew(entry.descriptor(key), entry.address());
  }

  // Remove valid bytes for the old entry and its copies, which are now stale.
  for (Address address : prior_metadata->addresses()) {
    SectorFromAddress(address)->RemoveValidBytes(prior_size);
  }

  prior_metadata->Reset(entry.descriptor(key), entry.address());
  return *prior_metadata;
}

// Finds a sector to use for writing a new entry to. Does automatic garbage
// collection if needed and allowed.
//
//                 OK: Sector found with needed space.
// RESOURCE_EXHAUSTED: No sector available with the needed space.
Status KeyValueStore::GetSectorForWrite(SectorDescriptor** sector,
                                        size_t entry_size,
                                        span<const Address> reserved) {
  Status result =
      FindSectorWithSpace(sector, entry_size, kAppendEntry, {}, reserved);

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
    Status gc_status = GarbageCollectPartial(reserved);
    if (!gc_status.ok()) {
      if (gc_status == Status::NOT_FOUND) {
        // Not enough space, and no reclaimable bytes, this KVS is full!
        return Status::RESOURCE_EXHAUSTED;
      }
      return gc_status;
    }

    result =
        FindSectorWithSpace(sector, entry_size, kAppendEntry, {}, reserved);

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

Status KeyValueStore::AppendEntry(const Entry& entry,
                                  string_view key,
                                  span<const byte> value) {
  const StatusWithSize result = entry.Write(key, value);

  // Remove any bytes that were written, even if the write was not successful.
  // This is important to retain the writable space invariant on the sectors.
  SectorDescriptor* const sector = SectorFromAddress(entry.address());
  sector->RemoveWritableBytes(result.size());

  if (!result.ok()) {
    ERR("Failed to write %zu bytes at %#zx. %zu actually written",
        entry.size(),
        size_t(entry.address()),
        result.size());
    return result.status();
  }

  if (options_.verify_on_write) {
    TRY(entry.VerifyChecksumInFlash());
  }

  sector->AddValidBytes(result.size());
  return Status::OK;
}

Status KeyValueStore::RelocateEntry(const EntryMetadata& metadata,
                                    KeyValueStore::Address& address,
                                    span<const Address> reserved_addresses) {
  Entry entry;
  TRY(Entry::Read(partition_, address, formats_, &entry));

  // Find a new sector for the entry and write it to the new location. For
  // relocation the find should not not be a sector already containing the key
  // but can be the always empty sector, since this is part of the GC process
  // that will result in a new empty sector. Also find a sector that does not
  // have reclaimable space (mostly for the full GC, where that would result in
  // an immediate extra relocation).
  SectorDescriptor* new_sector;

  TRY(FindSectorWithSpace(&new_sector,
                          entry.size(),
                          kGarbageCollect,
                          metadata.addresses(),
                          reserved_addresses));

  const Address new_address = NextWritableAddress(new_sector);
  const StatusWithSize result = entry.Copy(new_address);
  new_sector->RemoveWritableBytes(result.size());
  TRY(result);

  // Entry was written successfully; update descriptor's address and the sector
  // descriptors to reflect the new entry.
  SectorFromAddress(address)->RemoveValidBytes(result.size());
  new_sector->AddValidBytes(result.size());
  address = new_address;

  return Status::OK;
}

// Find either an existing sector with enough space that is not the sector to
// skip, or an empty sector. Maintains the invariant that there is always at
// least 1 empty sector except during GC. On GC, skip sectors that have
// reclaimable bytes.
Status KeyValueStore::FindSectorWithSpace(
    SectorDescriptor** found_sector,
    size_t size,
    FindSectorMode find_mode,
    span<const Address> addresses_to_skip,
    span<const Address> reserved_addresses) {
  SectorDescriptor* first_empty_sector = nullptr;
  bool at_least_two_empty_sectors = (find_mode == kGarbageCollect);

  // Used for the GC reclaimable bytes check
  SectorDescriptor* non_empty_least_reclaimable_sector = nullptr;
  const size_t sector_size_bytes = partition_.sector_size_bytes();

  // Build a list of sectors to avoid.
  //
  // This is overly strict. reserved_addresses is populated when there are
  // sectors reserved for a new entry. It is safe to garbage collect into
  // these sectors, as long as there remains room for the pending entry. These
  // reserved sectors could also be garbage collected if they have recoverable
  // space. For simplicitly, avoid both the relocating key's redundant entries
  // (addresses_to_skip) and the sectors reserved for pending writes
  // (reserved_addresses).
  // TODO(hepler): Look into improving garbage collection.
  size_t sectors_to_skip = 0;
  for (Address address : addresses_to_skip) {
    temp_sectors_to_skip_[sectors_to_skip++] = SectorFromAddress(address);
  }
  for (Address address : reserved_addresses) {
    temp_sectors_to_skip_[sectors_to_skip++] = SectorFromAddress(address);
  }

  DBG("Find sector with %zu bytes available, starting with sector %u, %s",
      size,
      SectorIndex(last_new_sector_),
      (find_mode == kAppendEntry) ? "Append" : "GC");
  for (size_t i = 0; i < sectors_to_skip; ++i) {
    DBG("  Skip sector %u", SectorIndex(temp_sectors_to_skip_[i]));
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
    if (Contains(span(temp_sectors_to_skip_, sectors_to_skip), sector)) {
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
    span<const Address> reserved_addresses) {
  const size_t sector_size_bytes = partition_.sector_size_bytes();
  SectorDescriptor* sector_candidate = nullptr;
  size_t candidate_bytes = 0;

  // Build a vector of sectors to avoid.
  for (size_t i = 0; i < reserved_addresses.size(); ++i) {
    temp_sectors_to_skip_[i] = SectorFromAddress(reserved_addresses[i]);
    DBG("    Skip sector %u",
        SectorIndex(SectorFromAddress(reserved_addresses[i])));
  }
  const span sectors_to_skip(temp_sectors_to_skip_, reserved_addresses.size());

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

  // Step 2: If step 1 yields no sectors, just find the sector with the most
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
      TRY(GarbageCollectSector(sector, {}));
    }
  }

  DBG("Garbage Collect all complete");
  return Status::OK;
}

Status KeyValueStore::GarbageCollectPartial(
    span<const Address> reserved_addresses) {
  DBG("Garbage Collect a single sector");
  for (Address address : reserved_addresses) {
    DBG("   Avoid address %u", unsigned(address));
  }

  // Step 1: Find the sector to garbage collect
  SectorDescriptor* sector_to_gc =
      FindSectorToGarbageCollect(reserved_addresses);

  if (sector_to_gc == nullptr) {
    // Nothing to GC.
    return Status::NOT_FOUND;
  }

  // Step 2: Garbage collect the selected sector.
  return GarbageCollectSector(sector_to_gc, reserved_addresses);
}

Status KeyValueStore::RelocateKeyAddressesInSector(
    SectorDescriptor& sector_to_gc,
    const EntryMetadata& metadata,
    span<const Address> reserved_addresses) {
  for (FlashPartition::Address& address : metadata.addresses()) {
    if (AddressInSector(sector_to_gc, address)) {
      DBG("  Relocate entry for Key 0x%08" PRIx32 ", sector %u",
          metadata.hash(),
          SectorIndex(SectorFromAddress(address)));
      TRY(RelocateEntry(metadata, address, reserved_addresses));
    }
  }

  return Status::OK;
};

Status KeyValueStore::GarbageCollectSector(
    SectorDescriptor* sector_to_gc, span<const Address> reserved_addresses) {
  // Step 1: Move any valid entries in the GC sector to other sectors
  if (sector_to_gc->valid_bytes() != 0) {
    for (const EntryMetadata& metadata : entry_cache_) {
      TRY(RelocateKeyAddressesInSector(
          *sector_to_gc, metadata, reserved_addresses));
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
                                                string_view key,
                                                span<const byte> value,
                                                EntryState state) {
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

  if (state == EntryState::kDeleted) {
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

void KeyValueStore::LogDebugInfo() const {
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
  DBG("  Entry count     = %zu", entry_cache_.total_entries());
  DBG("  Max entry count = %zu", entry_cache_.max_entries());
  DBG(" ");
  DBG("      #     hash        version    address   address (hex)");
  size_t i = 0;
  for (const EntryMetadata& metadata : entry_cache_) {
    DBG("   |%3zu: | %8zx  |%8zu  | %8zu | %8zx",
        i++,
        size_t(metadata.hash()),
        size_t(metadata.transaction_id()),
        size_t(metadata.first_address()),
        size_t(metadata.first_address()));
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
  DBG("Key descriptors: count %zu", entry_cache_.total_entries());
  for (auto& metadata : entry_cache_) {
    DBG("  - Key: %s, hash %#zx, transaction ID %zu, first address %#zx",
        metadata.state() == EntryState::kDeleted ? "Deleted" : "Valid",
        static_cast<size_t>(metadata.hash()),
        static_cast<size_t>(metadata.transaction_id()),
        static_cast<size_t>(metadata.first_address()));
  }
}

}  // namespace pw::kvs
