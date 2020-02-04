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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "pw_kvs/checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

// TODO: Resolve uses of partition_.sector_count() vs kMaxUsableSectors.

namespace pw::kvs {
namespace internal {

template <typename T, typename = decltype(span(std::declval<T>()))>
constexpr bool ConvertsToSpan(int) {
  return true;
}

// If the expression span(T) fails, then the type can't be converted to a span.
template <typename T>
constexpr bool ConvertsToSpan(...) {
  return false;
}

}  // namespace internal

// Traits class to detect if the type is a span. This is used to ensure that the
// correct overload of the Put function is selected.
template <typename T>
using ConvertsToSpan =
    std::bool_constant<internal::ConvertsToSpan<std::remove_reference_t<T>>(0)>;

// Internal-only persistent storage header format.
struct EntryHeader;

struct EntryHeaderFormat {
  uint32_t magic;  // unique identifier
  ChecksumAlgorithm* checksum;
};

// TODO: Select the appropriate defaults, add descriptions.
struct Options {
  bool partial_gc_on_write = true;
  bool verify_on_read = true;
  bool verify_on_write = true;
};

class KeyValueStore {
 public:
  // TODO: Make these configurable
  static constexpr size_t kMaxKeyLength = 64;
  static constexpr size_t kMaxEntries = 64;
  static constexpr size_t kUsableSectors = 64;

  // +1 for null-terminator.
  using KeyBuffer = std::array<char, kMaxKeyLength + 1>;

  // In the future, will be able to provide additional EntryHeaderFormats for
  // backwards compatibility.
  constexpr KeyValueStore(FlashPartition* partition,
                          const EntryHeaderFormat& format,
                          const Options& options = {})
      : partition_(*partition),
        entry_header_format_(format),
        options_(options),
        key_descriptor_list_{},
        key_descriptor_list_size_(0),
        sector_map_{},
        last_written_sector_(0) {}

  Status Init();
  bool initialized() const { return false; }  // TODO: Implement this

  StatusWithSize Get(std::string_view key, span<std::byte> value) const;

  template <typename T>
  Status Get(const std::string_view& key, T* value) {
    static_assert(std::is_trivially_copyable<T>(), "KVS values must copyable");
    static_assert(!std::is_pointer<T>(), "KVS values cannot be pointers");

    // Ensure that the size of the stored value matches the size of the type.
    // Otherwise, report error. This check avoids potential memory corruption.
    StatusWithSize result = ValueSize(key);
    if (!result.ok()) {
      return result.status();
    }
    if (result.size() != sizeof(T)) {
      return Status::INVALID_ARGUMENT;
    }
    return Get(key, as_writable_bytes(span(value, 1))).status();
  }

  Status Put(std::string_view key, span<const std::byte> value);

  template <typename T,
            typename = std::enable_if_t<std::is_trivially_copyable_v<T> &&
                                        !std::is_pointer_v<T> &&
                                        !ConvertsToSpan<T>::value>>
  Status Put(const std::string_view& key, const T& value) {
    return Put(key, as_bytes(span(&value, 1)));
  }

  Status Delete(std::string_view key);

  StatusWithSize ValueSize(std::string_view key) const;

  void LogDebugInfo();

  // Classes and functions to support STL-style iteration.
  class Iterator;

  class Entry {
   public:
    // Guaranteed to be null-terminated
    std::string_view key() const { return key_buffer_.data(); }

    Status Get(span<std::byte> value_buffer) const {
      return kvs_.Get(key(), value_buffer).status();
    }

    template <typename T>
    Status Get(T* value) const {
      return kvs_.Get(key(), value);
    }

    StatusWithSize ValueSize() const { return kvs_.ValueSize(key()); }

   private:
    friend class Iterator;

    constexpr Entry(const KeyValueStore& kvs) : kvs_(kvs), key_buffer_{} {}

    const KeyValueStore& kvs_;
    KeyBuffer key_buffer_;
  };

  class Iterator {
   public:
    Iterator& operator++() {
      index_ += 1;
      return *this;
    }

    Iterator& operator++(int) { return operator++(); }

    // Reads the entry's key from flash.
    const Entry& operator*();

    const Entry* operator->() {
      operator*();  // Read the key into the Entry object.
      return &entry_;
    }

    constexpr bool operator==(const Iterator& rhs) const {
      return index_ == rhs.index_;
    }

    constexpr bool operator!=(const Iterator& rhs) const {
      return index_ != rhs.index_;
    }

   private:
    friend class KeyValueStore;

    constexpr Iterator(const KeyValueStore& kvs, size_t index)
        : entry_(kvs), index_(index) {}

    Entry entry_;
    size_t index_;
  };

  // Standard aliases for iterator types.
  using iterator = Iterator;
  using const_iterator = Iterator;

  Iterator begin() const { return Iterator(*this, 0); }
  Iterator end() const { return Iterator(*this, size()); }

  // Returns the number of valid entries in the KeyValueStore.
  size_t size() const { return key_descriptor_list_size_; }

  static constexpr size_t max_size() { return kMaxKeyLength; }

  size_t empty() const { return size() == 0u; }

 private:
  using Address = FlashPartition::Address;

  struct KeyDescriptor {
    uint32_t key_hash;
    uint32_t key_version;
    Address address;  // In partition address.
  };

  struct SectorDescriptor {
    uint16_t tail_free_bytes;
    uint16_t valid_bytes;  // sum of sizes of valid entries

    bool HasSpace(size_t required_space) const {
      return (tail_free_bytes >= required_space);
    }
  };

  Status InvalidOperation(std::string_view key) const;

  static constexpr bool InvalidKey(std::string_view key) {
    return key.empty() || (key.size() > kMaxKeyLength);
  }

  Status FindKeyDescriptor(std::string_view key,
                           const KeyDescriptor** result) const;

  // Non-const version of FindKeyDescriptor.
  Status FindKeyDescriptor(std::string_view key, KeyDescriptor** result) {
    return static_cast<const KeyValueStore&>(*this).FindKeyDescriptor(
        key, const_cast<const KeyDescriptor**>(result));
  }

  Status ReadEntryHeader(const KeyDescriptor& descriptor,
                         EntryHeader* header) const;
  Status ReadEntryKey(const KeyDescriptor& descriptor,
                      size_t key_length,
                      char* key) const;

  StatusWithSize ReadEntryValue(const KeyDescriptor& key_descriptor,
                                const EntryHeader& header,
                                span<std::byte> value) const;

  Status ValidateEntryChecksum(const EntryHeader& header,
                               std::string_view key,
                               span<const std::byte> value) const;

  Status LoadEntry(Address entry_address, Address* next_entry_address);
  Status AppendNewOrOverwriteStaleExistingDescriptor(
      const KeyDescriptor& key_descriptor);
  Status AppendEmptyDescriptor(KeyDescriptor** new_descriptor);

  Status ValidateEntryChecksumInFlash(const EntryHeader& header,
                                      std::string_view key,
                                      const KeyDescriptor& entry) const;

  Status WriteEntryForExistingKey(KeyDescriptor* key_descriptor,
                                  std::string_view key,
                                  span<const std::byte> value);

  Status WriteEntryForNewKey(std::string_view key, span<const std::byte> value);

  Status RelocateEntry(KeyDescriptor& key_descriptor);

  SectorDescriptor* FindSectorWithSpace(size_t size);

  Status FindOrRecoverSectorWithSpace(SectorDescriptor** sector, size_t size);

  Status GarbageCollectOneSector(SectorDescriptor** sector);

  SectorDescriptor* FindSectorToGarbageCollect();

  bool HeaderLooksLikeUnwrittenData(const EntryHeader& header) const;

  KeyDescriptor* FindDescriptor(uint32_t hash);

  Status AppendEntry(SectorDescriptor* sector,
                     KeyDescriptor* key_descriptor,
                     std::string_view key,
                     span<const std::byte> value);

  Status VerifyEntry(SectorDescriptor* sector, KeyDescriptor* key_descriptor);

  span<const std::byte> CalculateEntryChecksum(
      const EntryHeader& header,
      std::string_view key,
      span<const std::byte> value) const;

  bool AddressInSector(const SectorDescriptor& sector, Address address) const {
    const Address sector_base = SectorBaseAddress(&sector);
    const Address sector_end = sector_base + partition_.sector_size_bytes();

    return ((address >= sector_base) && (address < sector_end));
  }

  bool SectorEmpty(const SectorDescriptor& sector) const {
    return (sector.tail_free_bytes == partition_.sector_size_bytes());
  }

  size_t RecoverableBytes(const SectorDescriptor& sector) {
    return partition_.sector_size_bytes() - sector.valid_bytes -
           sector.tail_free_bytes;
  }

  Address SectorBaseAddress(const SectorDescriptor* sector) const {
    return (sector - sector_map_.data()) * partition_.sector_size_bytes();
  }

  Address NextWritableAddress(SectorDescriptor* sector) const {
    return SectorBaseAddress(sector) + partition_.sector_size_bytes() -
           sector->tail_free_bytes;
  }

  bool KeyListFull() const { return key_descriptor_list_size_ == kMaxEntries; }

  span<KeyDescriptor> key_descriptors() {
    return span(key_descriptor_list_.data(), key_descriptor_list_size_);
  }

  span<const KeyDescriptor> key_descriptors() const {
    return span(key_descriptor_list_.data(), key_descriptor_list_size_);
  }

  FlashPartition& partition_;
  EntryHeaderFormat entry_header_format_;
  Options options_;

  // Map is unordered; finding a key requires scanning and
  // verifying a match by reading the actual entry.
  std::array<KeyDescriptor, kMaxEntries> key_descriptor_list_;
  size_t key_descriptor_list_size_;  // Number of valid entries in
                                     // key_descriptor_list_

  // This is dense, so sector_id == indexof(SectorDescriptor) in sector_map
  std::array<SectorDescriptor, kUsableSectors> sector_map_;
  size_t last_written_sector_;

  bool enabled_ = false;
};

}  // namespace pw::kvs
