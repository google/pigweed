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
#include <type_traits>

#include "pw_containers/vector.h"
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
class Entry;

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
  static constexpr size_t kMaxEntries = 256;
  static constexpr size_t kMaxUsableSectors = 256;
  static constexpr size_t kWorkingBufferSizeBytes = (4 * 1024);

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

  // Adds a key-value entry to the KVS. If the key was already present, its
  // value is overwritten.
  //
  // In the current implementation, all keys in the KVS must have a unique hash.
  // If Put is called with a key whose hash matches an existing key, nothing
  // is added and ALREADY_EXISTS is returned.
  //
  //                    OK: the entry was successfully added or updated
  //   FAILED_PRECONDITION: the KVS is not initialized
  //    RESOURCE_EXHAUSTED: there is not enough space to add the entry
  //      INVALID_ARGUMENT: key is empty or too long or value is too large
  //        ALREADY_EXISTS: the entry could not be added because a different key
  //                        with the same hash is already in the KVS
  //
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
    // The key as a null-terminated string.
    const char* key() const { return key_buffer_.data(); }

    StatusWithSize Get(span<std::byte> value_buffer) const {
      return kvs_.Get(key(), value_buffer);
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

    // TODO: Remove the duplicate kMaxKeyLength definition. This should be
    // provided by the Entry class.
    static constexpr size_t kMaxKeyLength = 0b111111;

    // Buffer large enough for a null-terminated version of any valid key.
    std::array<char, kMaxKeyLength + 1> key_buffer_;
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
      return item_.kvs_.key_descriptors_[index_];
    }

    Item item_;
    size_t index_;
  };

  using const_iterator = iterator;  // Standard alias for iterable types.

  iterator begin() const;
  iterator end() const { return iterator(*this, key_descriptors_.size()); }

  // Returns the number of valid entries in the KeyValueStore.
  size_t size() const;

  size_t max_size() const { return key_descriptors_.max_size(); }

  size_t empty() const { return size() == 0u; }

 private:
  using Address = FlashPartition::Address;

  struct KeyDescriptor {
    enum State { kValid, kDeleted };

    KeyDescriptor() = default;

    KeyDescriptor(std::string_view key,
                  uint32_t version,
                  Address addr,
                  State initial_state = kValid)
        : key_hash(HashKey(key)),
          key_version(version),
          address(addr),
          state(initial_state) {}

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

    void RemoveFreeBytes(size_t size) {
      // TODO: add safety check for tail_free_bytes > size.
      tail_free_bytes -= size;
    }

    void RemoveValidBytes(size_t size);
  };

  static uint32_t HashKey(std::string_view string);

  Status FixedSizeGet(std::string_view key,
                      std::byte* value,
                      size_t size_bytes) const;

  Status CheckOperation(std::string_view key) const;

  Status FindKeyDescriptor(std::string_view key,
                           const KeyDescriptor** result) const;

  // Non-const version of FindKeyDescriptor.
  Status FindKeyDescriptor(std::string_view key, KeyDescriptor** result) {
    return static_cast<const KeyValueStore&>(*this).FindKeyDescriptor(
        key, const_cast<const KeyDescriptor**>(result));
  }

  Status FindExistingKeyDescriptor(std::string_view key,
                                   const KeyDescriptor** result) const;

  Status FindExistingKeyDescriptor(std::string_view key,
                                   KeyDescriptor** result) {
    return static_cast<const KeyValueStore&>(*this).FindExistingKeyDescriptor(
        key, const_cast<const KeyDescriptor**>(result));
  }

  Status LoadEntry(Address entry_address, Address* next_entry_address);
  Status AppendNewOrOverwriteStaleExistingDescriptor(
      const KeyDescriptor& key_descriptor);
  Status AppendEmptyDescriptor(KeyDescriptor** new_descriptor);

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

  Status GarbageCollectOneSector();

  SectorDescriptor* FindSectorToGarbageCollect();

  KeyDescriptor* FindDescriptor(uint32_t hash);

  Status AppendEntry(SectorDescriptor* sector,
                     KeyDescriptor* key_descriptor,
                     std::string_view key,
                     span<const std::byte> value,
                     KeyDescriptor::State new_state);

  bool AddressInSector(const SectorDescriptor& sector, Address address) const {
    const Address sector_base = SectorBaseAddress(&sector);
    const Address sector_end = sector_base + partition_.sector_size_bytes();

    return ((address >= sector_base) && (address < sector_end));
  }

  bool SectorEmpty(const SectorDescriptor& sector) const {
    return (sector.tail_free_bytes == partition_.sector_size_bytes());
  }

  size_t RecoverableBytes(const SectorDescriptor& sector) const {
    return partition_.sector_size_bytes() - sector.valid_bytes -
           sector.tail_free_bytes;
  }

  size_t SectorIndex(const SectorDescriptor* sector) const {
    // TODO: perhaps add assert that the index is valid.
    return (sector - sectors_.data());
  }

  Address SectorBaseAddress(const SectorDescriptor* sector) const {
    return SectorIndex(sector) * partition_.sector_size_bytes();
  }

  SectorDescriptor* SectorFromAddress(Address address) {
    const size_t index = address / partition_.sector_size_bytes();
    // TODO: Add boundary checking once asserts are supported.
    // DCHECK_LT(index, sector_map_size_);
    return &sectors_[index];
  }

  void LogSectors() const;
  void LogKeyDescriptor() const;

  Address NextWritableAddress(SectorDescriptor* sector) const {
    return SectorBaseAddress(sector) + partition_.sector_size_bytes() -
           sector->tail_free_bytes;
  }

  FlashPartition& partition_;
  EntryHeaderFormat entry_header_format_;
  Options options_;

  // Map is unordered; finding a key requires scanning and
  // verifying a match by reading the actual entry.
  Vector<KeyDescriptor, kMaxEntries> key_descriptors_;

  // This is dense, so sector_id == indexof(SectorDescriptor) in sector_map
  // TODO: This may need to be a span that points to an externally allocated
  // array. This could be handled by a templated KVS derived class.
  Vector<SectorDescriptor, kMaxUsableSectors> sectors_;

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
  // relocate) to use for working space to remove the need to allocate temporary
  // space.
  std::array<char, kWorkingBufferSizeBytes> working_buffer_;
};

}  // namespace pw::kvs
