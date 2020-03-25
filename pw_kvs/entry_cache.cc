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

#include "pw_kvs/internal/entry_cache.h"

#include <cinttypes>

#include "pw_kvs/flash_memory.h"
#include "pw_kvs/internal/entry.h"
#include "pw_kvs/internal/hash.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs::internal {
namespace {

using std::string_view;

constexpr FlashPartition::Address kNoAddress = FlashPartition::Address(-1);

}  // namespace

void EntryMetadata::RemoveAddress(Address address_to_remove) {
  // Find the index of the address to remove.
  for (Address& address : addresses_) {
    if (address == address_to_remove) {
      // Move the address at the back of the list to the slot of the address
      // being removed. Do this unconditionally, even if the address to remove
      // is the last slot since the logic still works.
      address = addresses_.back();

      // Remove the back entry of the address list.
      addresses_.back() = kNoAddress;
      addresses_ = span(addresses_.begin(), addresses_.size() - 1);
      break;
    }
  }
}

void EntryMetadata::Reset(const KeyDescriptor& descriptor, Address address) {
  *descriptor_ = descriptor;

  addresses_[0] = address;
  for (size_t i = 1; i < addresses_.size(); ++i) {
    addresses_[i] = kNoAddress;
  }
  addresses_ = addresses_.first(1);
}

Status EntryCache::Find(FlashPartition& partition,
                        string_view key,
                        EntryMetadata* metadata) const {
  const uint32_t hash = internal::Hash(key);
  Entry::KeyBuffer key_buffer;

  for (size_t i = 0; i < descriptors_.size(); ++i) {
    if (descriptors_[i].key_hash == hash) {
      TRY(Entry::ReadKey(
          partition, *first_address(i), key.size(), key_buffer.data()));

      if (key == string_view(key_buffer.data(), key.size())) {
        PW_LOG_DEBUG("Found match for key hash 0x%08" PRIx32, hash);
        *metadata = EntryMetadata(descriptors_[i], addresses(i));
        return Status::OK;
      } else {
        PW_LOG_WARN("Found key hash collision for 0x%08" PRIx32, hash);
        return Status::ALREADY_EXISTS;
      }
    }
  }
  return Status::NOT_FOUND;
}

Status EntryCache::FindExisting(FlashPartition& partition,
                                string_view key,
                                EntryMetadata* metadata) const {
  Status status = Find(partition, key, metadata);

  // If the key's hash collides with an existing key or if the key is deleted,
  // treat it as if it is not in the KVS.
  if (status == Status::ALREADY_EXISTS ||
      (status.ok() && metadata->state() == EntryState::kDeleted)) {
    return Status::NOT_FOUND;
  }
  return status;
}

EntryMetadata EntryCache::AddNew(const KeyDescriptor& descriptor,
                                 Address entry_address) {
  // TODO(hepler): DCHECK(!full());
  Address* first_address = ResetAddresses(descriptors_.size(), entry_address);
  descriptors_.push_back(descriptor);
  return EntryMetadata(descriptors_.back(), span(first_address, 1));
}

// TODO: This method is the trigger of the O(valid_entries * all_entries) time
// complexity for reading. At some cost to memory, this could be optimized by
// using a hash table instead of scanning, but in practice this should be fine
// for a small number of keys
Status EntryCache::AddNewOrUpdateExisting(const KeyDescriptor& descriptor,
                                          Address address,
                                          size_t sector_size_bytes) {
  // With the new key descriptor, either add it to the descriptor table or
  // overwrite an existing entry with an older version of the key.
  const int index = FindIndex(descriptor.key_hash);

  // Write a new entry if there is room.
  if (index == -1) {
    if (full()) {
      return Status::RESOURCE_EXHAUSTED;
    }
    AddNew(descriptor, address);
    return Status::OK;
  }

  // Existing entry is old; replace the existing entry with the new one.
  if (descriptor.transaction_id > descriptors_[index].transaction_id) {
    descriptors_[index] = descriptor;
    ResetAddresses(index, address);
    return Status::OK;
  }

  // If the entries have a duplicate transaction ID, add the new (redundant)
  // entry to the existing descriptor.
  if (descriptors_[index].transaction_id == descriptor.transaction_id) {
    if (descriptors_[index].key_hash != descriptor.key_hash) {
      PW_LOG_ERROR("Duplicate entry for key 0x%08" PRIx32
                   " with transaction ID %" PRIu32 " has non-matching hash",
                   descriptor.key_hash,
                   descriptor.transaction_id);
      return Status::DATA_LOSS;
    }

    // Verify that this entry is not in the same sector as an existing copy of
    // this same key.
    for (Address existing_address : addresses(index)) {
      if (existing_address / sector_size_bytes == address / sector_size_bytes) {
        PW_LOG_DEBUG("Multiple Redundant entries in same sector %u",
                     unsigned(address / sector_size_bytes));
        return Status::DATA_LOSS;
      }
    }

    AddAddressIfRoom(index, address);
  } else {
    PW_LOG_DEBUG("Found stale entry when appending; ignoring");
  }
  return Status::OK;
}

size_t EntryCache::present_entries() const {
  size_t present_entries = 0;

  for (const KeyDescriptor& descriptor : descriptors_) {
    if (descriptor.state != EntryState::kDeleted) {
      present_entries += 1;
    }
  }

  return present_entries;
}

int EntryCache::FindIndex(uint32_t key_hash) const {
  for (size_t i = 0; i < descriptors_.size(); ++i) {
    if (descriptors_[i].key_hash == key_hash) {
      return i;
    }
  }
  return -1;
}

void EntryCache::AddAddressIfRoom(size_t descriptor_index, Address address) {
  Address* const existing = first_address(descriptor_index);

  for (size_t i = 0; i < redundancy(); ++i) {
    if (existing[i] == kNoAddress) {
      existing[i] = address;
      return;
    }
  }
}

span<EntryCache::Address> EntryCache::addresses(size_t descriptor_index) const {
  Address* const addresses = first_address(descriptor_index);

  size_t size = 0;
  while (size < redundancy() && addresses[size] != kNoAddress) {
    size += 1;
  }

  return span(addresses, size);
}

EntryCache::Address* EntryCache::ResetAddresses(size_t descriptor_index,
                                                Address address) {
  Address* first = first_address(descriptor_index);
  *first = address;

  // Clear the additional addresses, if any.
  for (size_t i = 1; i < redundancy_; ++i) {
    first[i] = kNoAddress;
  }

  return first;
}

}  // namespace pw::kvs::internal
