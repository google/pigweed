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
class EntryHeader;

struct EntryHeaderFormat {
  // Magic is a unique constant identifier for entries.
  //
  // Upon reading from an address in flash, the magic number facilitiates
  // quickly differentiating between:
  //
  // - Reading erased data - typically 0xFF - from flash.
  // - Reading corrupted data
  // - Reading a valid entry
  //
  // When selecting a magic for your particular KVS, pick a random 32 bit
  // integer rather than a human readable 4 bytes. This decreases the
  // probability of a collision with a real string when scanning in the case of
  // corruption. To generate such a number:
  /*
       $ python3 -c 'import random; print(hex(random.randint(0,2**32)))'
       0xaf741757
  */
  uint32_t magic;
  ChecksumAlgorithm* checksum;
};

// TODO: Select the appropriate defaults, add descriptions.
struct Options {
  bool partial_gc_on_write = true;
  bool verify_on_read = true;
  bool verify_on_write = true;
};

class KeyValueStore {
 private:
  struct KeyDescriptor;

 public:
  // TODO: Make these configurable
  static constexpr size_t kMaxKeyLength = 64;
  static constexpr size_t kMaxEntries = 64;
  static constexpr size_t kMaxUsableSectors = 64;
  static constexpr size_t kWorkingBufferSizeBytes = (4 * 1024);

  // +1 for null-terminator.
  using KeyBuffer = std::array<char, kMaxKeyLength + 1>;

  // In the future, will be able to provide additional EntryHeaderFormats for
  // backwards compatibility.
  KeyValueStore(FlashPartition* partition,
                const EntryHeaderFormat& format,
                const Options& options = {});

  Status Init();

  bool initialized() const { return initialized_; }

  StatusWithSize Get(std::string_view key, span<std::byte> value) const;

  // This overload of Get accepts a pointer to a trivially copyable object.
  // const T& is used instead of T* to prevent arrays from satisfying this
  // overload. To call Get with an array, pass as_writable_bytes(span(array)),
  // or pass a pointer to the array instead of the array itself.
  template <typename Pointer,
            typename = std::enable_if_t<std::is_pointer_v<Pointer>>>
  Status Get(const std::string_view& key, const Pointer& pointer) const {
    using T = std::remove_reference_t<std::remove_pointer_t<Pointer>>;

    static_assert(std::is_trivially_copyable<T>(), "Values must be copyable");
    static_assert(!std::is_pointer<T>(), "Values cannot be pointers");

    return FixedSizeGet(key, reinterpret_cast<std::byte*>(pointer), sizeof(T));
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
  class iterator;

  class Item {
   public:
    // Guaranteed to be null-terminated
    std::string_view key() const { return key_buffer_.data(); }

    Status Get(span<std::byte> value_buffer) const {
      return kvs_.Get(key(), value_buffer).status();
    }

    template <typename Pointer,
              typename = std::enable_if_t<std::is_pointer_v<Pointer>>>
    Status Get(const Pointer& pointer) const {
      return kvs_.Get(key(), pointer);
    }

    StatusWithSize ValueSize() const { return kvs_.ValueSize(key()); }

   private:
    friend class iterator;

    constexpr Item(const KeyValueStore& kvs) : kvs_(kvs), key_buffer_{} {}

    const KeyValueStore& kvs_;
    KeyBuffer key_buffer_;
  };

  class iterator {
   public:
    iterator& operator++();

    iterator& operator++(int) { return operator++(); }

    // Reads the entry's key from flash.
    const Item& operator*();

    const Item* operator->() {
      operator*();  // Read the key into the Item object.
      return &item_;
    }

    constexpr bool operator==(const iterator& rhs) const {
      return index_ == rhs.index_;
    }

    constexpr bool operator!=(const iterator& rhs) const {
      return index_ != rhs.index_;
    }

   private:
    friend class KeyValueStore;

    constexpr iterator(const KeyValueStore& kvs, size_t index)
        : item_(kvs), index_(index) {}

    const KeyDescriptor& descriptor() const {
      return item_.kvs_.key_descriptor_list_[index_];
    }

    Item item_;
    size_t index_;
  };

  using const_iterator = iterator;  // Standard alias for iterable types.

  iterator begin() const;
  iterator end() const { return iterator(*this, key_descriptor_list_size_); }

  // Returns the number of valid entries in the KeyValueStore.
  size_t size() const;

  static constexpr size_t max_size() { return kMaxKeyLength; }

  size_t empty() const { return size() == 0u; }

 private:
  using Address = FlashPartition::Address;

  struct KeyDescriptor {
    enum State { kValid, kDeleted };

    KeyDescriptor() = default;

    KeyDescriptor(std::string_view key,
                  uint32_t version,
                  Address address,
                  State state = kValid)
        : key_hash(HashKey(key)),
          key_version(version),
          address(address),
          state(state) {}

    bool deleted() const { return state == kDeleted; }

    uint32_t key_hash;
    uint32_t key_version;
    Address address;  // In partition address.

    // TODO: This information should be packed into the above fields to save RAM
    State state;
  };

  struct SectorDescriptor {
    uint16_t tail_free_bytes;
    uint16_t valid_bytes;  // sum of sizes of valid entries

    bool HasSpace(size_t required_space) const {
      return (tail_free_bytes >= required_space);
    }
  };

  static uint32_t HashKey(std::string_view string);

  Status FixedSizeGet(std::string_view key,
                      std::byte* value,
                      size_t size_bytes) const;

  Status CheckOperation(std::string_view key) const;

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

  Status ReadEntryHeader(Address address, EntryHeader* header) const;
  Status ReadEntryKey(Address address, size_t key_length, char* key) const;

  StatusWithSize ReadEntryValue(const KeyDescriptor& key_descriptor,
                                const EntryHeader& header,
                                span<std::byte> value) const;

  Status LoadEntry(Address entry_address, Address* next_entry_address);
  Status AppendNewOrOverwriteStaleExistingDescriptor(
      const KeyDescriptor& key_descriptor);
  Status AppendEmptyDescriptor(KeyDescriptor** new_descriptor);

  Status ValidateEntryChecksumInFlash(const EntryHeader& header,
                                      std::string_view key,
                                      const KeyDescriptor& entry) const;

  Status WriteEntryForExistingKey(KeyDescriptor* key_descriptor,
                                  KeyDescriptor::State new_state,
                                  std::string_view key,
                                  span<const std::byte> value);

  Status WriteEntryForNewKey(std::string_view key, span<const std::byte> value);

  Status RelocateEntry(KeyDescriptor& key_descriptor);

  Status FindSectorWithSpace(SectorDescriptor** found_sector,
                             size_t size,
                             const SectorDescriptor* sector_to_skip = nullptr,
                             bool bypass_empty_sector_rule = false);

  Status FindOrRecoverSectorWithSpace(SectorDescriptor** sector, size_t size);

  Status GarbageCollectOneSector(SectorDescriptor** sector);

  SectorDescriptor* FindSectorToGarbageCollect();

  bool HeaderLooksLikeUnwrittenData(const EntryHeader& header) const;

  KeyDescriptor* FindDescriptor(uint32_t hash);

  Status AppendEntry(SectorDescriptor* sector,
                     KeyDescriptor* key_descriptor,
                     std::string_view key,
                     span<const std::byte> value,
                     KeyDescriptor::State new_state = KeyDescriptor::kValid);

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

  size_t SectorIndex(const SectorDescriptor* sector) const {
    // TODO: perhaps add assert that the index is valid.
    return (sector - sector_map_.data());
  }

  Address SectorBaseAddress(const SectorDescriptor* sector) const {
    return SectorIndex(sector) * partition_.sector_size_bytes();
  }

  SectorDescriptor& SectorFromAddress(Address address) {
    const size_t index = address / partition_.sector_size_bytes();
    // TODO: Add boundary checking once asserts are supported.
    // DCHECK_LT(index, sector_map_size_);
    return sector_map_[index];
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

  span<SectorDescriptor> sectors() {
    return {sector_map_.data(), sector_map_size_};
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
  // TODO: This may need to be a span that points to an externally allocated
  // array. This could be handled by a templated KVS derived class.
  std::array<SectorDescriptor, kMaxUsableSectors> sector_map_;
  const size_t sector_map_size_;

  // The last sector that was selected as the "new empty sector" to write to.
  // This last new sector is used as the starting point for the next "find a new
  // empty sector to write to" operation. By using the last new sector as the
  // start point we will cycle which empty sector is selected next, spreading
  // the wear across all the empty sectors, rather than putting more wear on the
  // lower number sectors.
  //
  // Use SectorDescriptor* for the persistent storage rather than sector index
  // because SectorDescriptor* is the standard way to identify a sector.
  SectorDescriptor* last_new_sector_;

  bool initialized_ = false;

  // Working buffer is a general purpose buffer for operations (such as init or
  // relcate) to use for working space to remove the need to allocate temporary
  // space.
  std::array<char, kWorkingBufferSizeBytes> working_buffer_;
};

}  // namespace pw::kvs
