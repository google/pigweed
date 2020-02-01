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

#include <cstring>
#include <type_traits>

#include "pw_kvs_private/format.h"
#include "pw_kvs_private/macros.h"

namespace pw::kvs {

using std::byte;
using std::string_view;

namespace {

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

}  // namespace

Status KeyValueStore::Init() {
  // enabled_ = true;

  return Status::UNIMPLEMENTED;
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
    return ValidateEntryChecksum(header, key, value_buffer);
  }
  return result;
}

Status KeyValueStore::Put(string_view key, span<const byte> value) {
  TRY(InvalidOperation(key));

  if (value.size() > (1 << 24)) {
    // TODO: Reject sizes that are larger than the maximum?
  }

  KeyDescriptor* key_descriptor;
  if (FindKeyDescriptor(key, &key_descriptor).ok()) {
    return WriteEntryForExistingKey(key_descriptor, key, value);
  }

  return WriteEntryForNewKey(key, value);
}

Status KeyValueStore::Delete(string_view key) {
  TRY(InvalidOperation(key));

  return Status::UNIMPLEMENTED;
}

const KeyValueStore::Entry& KeyValueStore::Iterator::operator*() {
  const KeyDescriptor& key_descriptor =
      entry_.kvs_.key_descriptor_list_[index_];

  EntryHeader header;
  if (entry_.kvs_.ReadEntryHeader(key_descriptor, &header).ok()) {
    entry_.kvs_.ReadEntryKey(
        key_descriptor, header.key_length(), entry_.key_buffer_.data());
  }

  return entry_;
}

Status KeyValueStore::InvalidOperation(string_view key) const {
  if (InvalidKey(key)) {
    return Status::INVALID_ARGUMENT;
  }
  if (!/*enabled_*/ 0) {
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
      TRY(ReadEntryKey(descriptor, key.size(), key_buffer));

      if (key == string_view(key_buffer, key.size())) {
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
      value.subspan(read_size));
  TRY(result);
  if (read_size != header.value_length()) {
    return StatusWithSize(Status::RESOURCE_EXHAUSTED, read_size);
  }
  return StatusWithSize(read_size);
}

Status KeyValueStore::ValidateEntryChecksum(const EntryHeader& header,
                                            string_view key,
                                            span<const byte> value) const {
  CalculateEntryChecksum(header, key, value);
  return entry_header_format_.checksum->Verify(header.checksum());
}

Status KeyValueStore::WriteEntryForExistingKey(KeyDescriptor* key_descriptor,
                                               string_view key,
                                               span<const byte> value) {
  SectorDescriptor* sector;
  TRY(FindOrRecoverSectorWithSpace(&sector, EntrySize(key, value)));
  return AppendEntry(sector, key_descriptor, key, value);
}

Status KeyValueStore::WriteEntryForNewKey(string_view key,
                                          span<const byte> value) {
  if (KeyListFull()) {
    // TODO: Log, and also expose "in memory keymap full" in stats dump.
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
  TRY(AppendEntry(sector, &key_descriptor, key, value));

  // Only increment key_descriptor_list_size_ when we are certain the write
  // succeeded.
  key_descriptor_list_size_ += 1;
  return Status::OK;
}

Status KeyValueStore::RelocateEntry(KeyDescriptor& key_descriptor) {
  // TODO: implement me
  (void)key_descriptor;
  return Status::UNIMPLEMENTED;
}

// Find either an existing sector with enough space, or an empty sector.
// Maintains the invariant that there is always at least 1 empty sector.
KeyValueStore::SectorDescriptor* KeyValueStore::FindSectorWithSpace(
    size_t size) {
  int start = (last_written_sector_ + 1) % sector_map_.size();
  SectorDescriptor* first_empty_sector = nullptr;
  bool at_least_two_empty_sectors = false;

  for (size_t i = start; i != last_written_sector_;
       i = (i + 1) % sector_map_.size()) {
    SectorDescriptor& sector = sector_map_[i];
    if (!SectorEmpty(sector) && sector.HasSpace(size)) {
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
                           CalculateEntryChecksum(header, key, value),
                           key.size(),
                           value.size(),
                           key_descriptor->key_version + 1);

  // Handles writing multiple concatenated buffers, while breaking up the writes
  // into alignment-sized blocks.
  Address address = NextWritableAddress(sector);
  TRY_ASSIGN(
      size_t written,
      partition_.Write(
          address, {as_bytes(span(&header, 1)), as_bytes(span(key)), value}));

  if (options_.verify_on_write) {
    TRY(VerifyEntry(sector, key_descriptor));
  }

  // TODO: UPDATE last_written_sector_ appropriately

  key_descriptor->address = address;
  key_descriptor->key_version = header.key_version();
  sector->valid_bytes += written;
  sector->tail_free_bytes -= written;
  return Status::OK;
}

Status KeyValueStore::VerifyEntry(SectorDescriptor* sector,
                                  KeyDescriptor* key_descriptor) {
  // TODO: Implement me!
  (void)sector;
  (void)key_descriptor;
  return Status::UNIMPLEMENTED;
}

span<const byte> KeyValueStore::CalculateEntryChecksum(
    const EntryHeader& header,
    const string_view key,
    span<const byte> value) const {
  auto& checksum = *entry_header_format_.checksum;

  checksum.Reset();
  checksum.Update(header.DataForChecksum());
  checksum.Update(as_bytes(span(key)));
  checksum.Update(value.data(), value.size_bytes());
  return checksum.state();
}

}  // namespace pw::kvs
