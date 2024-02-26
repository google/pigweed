// Copyright 2023 The Pigweed Authors
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pw_preprocessor/util.h"
#include "pw_varint/varint.h"

/// @file pw_containers/inline_var_len_entry_queue.h
///
/// A `InlineVarLenEntryQueue` is a queue of inline variable-length binary
/// entries. It is implemented as a ring (circular) buffer and supports
/// operations to append entries and overwrite if necessary. Entries may be zero
/// bytes up to the maximum size supported by the queue.
///
/// The `InlineVarLenEntryQueue` has a few interesting properties.
///
/// - Data and metadata are stored inline in a contiguous block of
///   `uint32_t`-aligned memory.
/// - The data structure is trivially copyable.
/// - All state changes are accomplished with a single update to a `uint32_t`.
///   The memory is always in a valid state and may be parsed offline.
///
/// This data structure is a much simpler version of
/// @cpp_class{pw::ring_buffer::PrefixedEntryRingBuffer}. Prefer this
/// sized-entry ring buffer to `PrefixedEntryRingBuffer` when:
/// - A simple ring buffer of variable-length entries is needed. Advanced
///   features like multiple readers and a user-defined preamble are not
///   required.
/// - A consistent, parsable, in-memory representation is required (e.g. to
///   decode the buffer from a block of memory).
/// - C support is required.
///
/// `InlineVarLenEntryQueue` is implemented in C and provides complete C and C++
/// APIs. The `InlineVarLenEntryQueue` C++ class is structured similarly to
/// `pw::InlineQueue` and `pw::Vector`.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// @defgroup inline_var_len_entry_queue_c_api InlineVarLenEntryQueue C API
/// @{

/// Handle that refers to a `InlineVarLenEntryQueue`. In memory, the queue is a
/// `uint32_t` array.
typedef uint32_t* pw_InlineVarLenEntryQueue_Handle;
typedef const uint32_t* pw_InlineVarLenEntryQueue_ConstHandle;

/// Declares and initializes a `InlineVarLenEntryQueue` that can hold up to
/// `max_size_bytes` bytes. `max_size_bytes` is the largest supported size for a
/// single entry; attempting to store larger entries is invalid and will fail an
/// assertion.
///
/// @param variable variable name for the queue
/// @param max_size_bytes the capacity of the queue
#define PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(variable, max_size_bytes) \
  uint32_t variable[PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 +  \
                    _PW_VAR_QUEUE_DATA_SIZE_UINT32(max_size_bytes)] = {  \
      _PW_VAR_QUEUE_DATA_SIZE_BYTES(max_size_bytes), /*head=*/0u, /*tail=*/0u}

/// The size of the `InlineVarLenEntryQueue` header, in `uint32_t` elements.
/// This header stores the buffer length and head and tail offsets.
///
/// The underlying `uint32_t` array of a `InlineVarLenEntryQueue` must be larger
/// than this size.
#define PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 (3)

/// Initializes a `InlineVarLenEntryQueue` in place in a `uint32_t` array. The
/// array MUST be larger than
/// @c_macro{PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32} (3) elements.
static inline void pw_InlineVarLenEntryQueue_Init(uint32_t array[],
                                                  size_t array_size_uint32);

/// Empties the queue.
static inline void pw_InlineVarLenEntryQueue_Clear(
    pw_InlineVarLenEntryQueue_Handle queue);

/// Appends an entry to the end of the queue.
///
/// @pre The entry MUST NOT be larger than `max_size_bytes()`.
void pw_InlineVarLenEntryQueue_Push(pw_InlineVarLenEntryQueue_Handle queue,
                                    const void* data,
                                    uint32_t data_size_bytes);

/// Appends an entry to the end of the queue, removing entries with `Pop`
/// as necessary to make room.
///
/// @pre The entry MUST NOT be larger than `max_size_bytes()`.
void pw_InlineVarLenEntryQueue_PushOverwrite(
    pw_InlineVarLenEntryQueue_Handle queue,
    const void* data,
    uint32_t data_size_bytes);

/// Removes the first entry from queue.
///
/// @pre The queue MUST have at least one entry.
void pw_InlineVarLenEntryQueue_Pop(pw_InlineVarLenEntryQueue_Handle queue);

/// Iterator object for a `InlineVarLenEntryQueue`. Iterators are checked for
/// equality with @cpp_func{pw_InlineVarLenEntryQueue_Iterator_Equal}.
///
/// Iterators are invalidated by any operations that change the container or
/// its underlying data (push/pop/init).
typedef struct {
  // Private: do not access these fields directly!
  pw_InlineVarLenEntryQueue_ConstHandle _pw_queue;
  uint32_t _pw_offset;
} pw_InlineVarLenEntryQueue_Iterator;

/// An entry in the queue. Entries may be stored in up to two segments, so this
/// struct includes pointers to both portions of the entry.
typedef struct {
  const uint8_t* data_1;
  uint32_t size_1;
  const uint8_t* data_2;
  uint32_t size_2;
} pw_InlineVarLenEntryQueue_Entry;

/// Returns an iterator to the start of the `InlineVarLenEntryQueue`.
static inline pw_InlineVarLenEntryQueue_Iterator
pw_InlineVarLenEntryQueue_Begin(pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns an iterator that points past the end of the queue.
static inline pw_InlineVarLenEntryQueue_Iterator pw_InlineVarLenEntryQueue_End(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Advances an iterator to point to the next entry in the queue. It is
/// invalid to call `Advance` on an iterator equal to the `End` iterator.
void pw_InlineVarLenEntryQueue_Iterator_Advance(
    pw_InlineVarLenEntryQueue_Iterator* iterator);

/// Compares two iterators for equality.
static inline bool pw_InlineVarLenEntryQueue_Iterator_Equal(
    const pw_InlineVarLenEntryQueue_Iterator* lhs,
    const pw_InlineVarLenEntryQueue_Iterator* rhs);

/// Dereferences an iterator, loading the entry it points to.
pw_InlineVarLenEntryQueue_Entry pw_InlineVarLenEntryQueue_GetEntry(
    const pw_InlineVarLenEntryQueue_Iterator* iterator);

/// Copies the contents of the entry to the provided buffer. The entry may be
/// split into two regions; this serializes it into one buffer.
///
/// @param entry The entry whose contents to copy
/// @param dest The buffer into which to copy the serialized entry
/// @param count Copy up to this many bytes; must not be larger than the `dest`
///     buffer, but may be larger than the entry
uint32_t pw_InlineVarLenEntryQueue_Entry_Copy(
    const pw_InlineVarLenEntryQueue_Entry* entry, void* dest, uint32_t count);

/// Returns the byte at the specified index in the entry. Asserts if index is
/// out-of-bounds.
static inline uint8_t pw_InlineVarLenEntryQueue_Entry_At(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index);

/// Returns the number of variable-length entries in the queue. This is O(n) in
/// the number of entries in the queue.
uint32_t pw_InlineVarLenEntryQueue_Size(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns the combined size in bytes of all entries in the queue, excluding
/// metadata. This is O(n) in the number of entries in the queue.
uint32_t pw_InlineVarLenEntryQueue_SizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns the the maximum number of bytes that can be stored in the queue.
/// This is largest possible value of `size_bytes()`, and the size of the
/// largest single entry that can be stored in this queue. Attempting to store a
/// larger entry is invalid and results in a crash.
static inline uint32_t pw_InlineVarLenEntryQueue_MaxSizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns the size of the raw underlying `InlineVarLenEntryQueue` storage.
/// This size may be used to copy a `InlineVarLenEntryQueue` into another
/// 32-bit aligned memory location.
static inline uint32_t pw_InlineVarLenEntryQueue_RawStorageSizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns true if the `InlineVarLenEntryQueue` is empty, false if it has at
/// least one entry.
static inline bool pw_InlineVarLenEntryQueue_Empty(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// @}

// Implementation details.

#define _PW_VAR_QUEUE_DATA_SIZE_UINT32(max_size_bytes) \
  ((_PW_VAR_QUEUE_DATA_SIZE_BYTES(max_size_bytes) + 3 /* round up */) / 4)

#define _PW_VAR_QUEUE_DATA_SIZE_BYTES(max_size_bytes)              \
  (PW_VARINT_ENCODED_SIZE_BYTES(max_size_bytes) + max_size_bytes + \
   1 /*end byte*/)

#define _PW_VAR_QUEUE_ARRAY_SIZE_BYTES queue[0]
#define _PW_VAR_QUEUE_HEAD queue[1]
#define _PW_VAR_QUEUE_TAIL queue[2]  // points after the last byte
#define _PW_VAR_QUEUE_DATA ((const uint8_t*)&queue[3])

#define _PW_VAR_QUEUE_GET_ARRAY_SIZE_BYTES(array_size_uint32)     \
  (uint32_t)(array_size_uint32 -                                  \
             PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32) * \
      sizeof(uint32_t)

static inline void pw_InlineVarLenEntryQueue_Init(uint32_t array[],
                                                  size_t array_size_uint32) {
  array[0] = _PW_VAR_QUEUE_GET_ARRAY_SIZE_BYTES(array_size_uint32);
  array[1] = 0;  // head
  array[2] = 0;  // tail
}

static inline void pw_InlineVarLenEntryQueue_Clear(
    pw_InlineVarLenEntryQueue_Handle queue) {
  _PW_VAR_QUEUE_HEAD = 0;  // head
  _PW_VAR_QUEUE_TAIL = 0;  // tail
}

static inline pw_InlineVarLenEntryQueue_Iterator
pw_InlineVarLenEntryQueue_Begin(pw_InlineVarLenEntryQueue_ConstHandle queue) {
  pw_InlineVarLenEntryQueue_Iterator begin = {queue, _PW_VAR_QUEUE_HEAD};
  return begin;
}

static inline pw_InlineVarLenEntryQueue_Iterator pw_InlineVarLenEntryQueue_End(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  pw_InlineVarLenEntryQueue_Iterator end = {queue, _PW_VAR_QUEUE_TAIL};
  return end;
}

static inline bool pw_InlineVarLenEntryQueue_Iterator_Equal(
    const pw_InlineVarLenEntryQueue_Iterator* lhs,
    const pw_InlineVarLenEntryQueue_Iterator* rhs) {
  return lhs->_pw_offset == rhs->_pw_offset && lhs->_pw_queue == rhs->_pw_queue;
}

// Private function that returns a pointer to the specified index in the Entry.
static inline const uint8_t* _pw_InlineVarLenEntryQueue_Entry_GetPointer(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index) {
  if (index < entry->size_1) {
    return &entry->data_1[index];
  }
  return &entry->data_2[index - entry->size_1];
}

const uint8_t* _pw_InlineVarLenEntryQueue_Entry_GetPointerChecked(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index);

static inline uint8_t pw_InlineVarLenEntryQueue_Entry_At(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index) {
  return *_pw_InlineVarLenEntryQueue_Entry_GetPointerChecked(entry, index);
}

static inline uint32_t pw_InlineVarLenEntryQueue_RawStorageSizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  return PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 * sizeof(uint32_t) +
         _PW_VAR_QUEUE_ARRAY_SIZE_BYTES;
}

static inline uint32_t pw_InlineVarLenEntryQueue_MaxSizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  return _PW_VAR_QUEUE_ARRAY_SIZE_BYTES - 1 -
         (uint32_t)pw_varint_EncodedSizeBytes(_PW_VAR_QUEUE_ARRAY_SIZE_BYTES -
                                              1);
}

static inline bool pw_InlineVarLenEntryQueue_Empty(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  return _PW_VAR_QUEUE_HEAD == _PW_VAR_QUEUE_TAIL;
}

// These macros are not part of the public API, so undefine them.
#undef _PW_VAR_QUEUE_ARRAY_SIZE_BYTES
#undef _PW_VAR_QUEUE_HEAD
#undef _PW_VAR_QUEUE_TAIL
#undef _PW_VAR_QUEUE_DATA

#ifdef __cplusplus
}  // extern "C"

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "pw_containers/internal/raw_storage.h"
#include "pw_span/span.h"

namespace pw {

// A`BasicInlineVarLenEntryQueue` with a known maximum size of a single entry.
// The member functions are immplemented in the generic-capacity base.
// TODO: b/303056683 - Add helper for calculating kMaxSizeBytes for N entries of
//     a particular size.
template <typename T,
          size_t kMaxSizeBytes = containers::internal::kGenericSized>
class BasicInlineVarLenEntryQueue
    : public BasicInlineVarLenEntryQueue<T,
                                         containers::internal::kGenericSized> {
 private:
  using Base =
      BasicInlineVarLenEntryQueue<T, containers::internal::kGenericSized>;

 public:
  constexpr BasicInlineVarLenEntryQueue() : Base(kMaxSizeBytes) {}

  // `BasicInlineVarLenEntryQueue` is trivially copyable.
  BasicInlineVarLenEntryQueue(const BasicInlineVarLenEntryQueue&) = default;
  BasicInlineVarLenEntryQueue& operator=(const BasicInlineVarLenEntryQueue&) =
      default;

 private:
  static_assert(kMaxSizeBytes <=
                std::numeric_limits<typename Base::size_type>::max());

  using Base::Init;  // Disallow Init since the size template param is not used.

  uint32_t data_[_PW_VAR_QUEUE_DATA_SIZE_UINT32(kMaxSizeBytes)];
};

/// @defgroup inline_var_len_entry_queue_cpp_api
/// @{

/// Variable-length entry queue class template for any byte type (e.g.
/// ``std::byte`` or ``uint8_t``).
///
/// ``BasicInlineVarLenEntryQueue`` instances are declared with their capacity
/// / max single entry size (``BasicInlineVarLenEntryQueue<char, 64>``), but
/// may be referred to without the size
/// (``BasicInlineVarLenEntryQueue<char>&``).
template <typename T>
class BasicInlineVarLenEntryQueue<T, containers::internal::kGenericSized> {
 public:
  class Entry;

  using value_type = Entry;
  using size_type = std::uint32_t;
  using pointer = const value_type*;
  using const_pointer = pointer;
  using reference = const value_type&;
  using const_reference = reference;

  // Refers to an entry in-place in the queue. Entries may not be contiguous.
  class iterator;

  // Currently, iterators provide read-only access.
  // TODO: b/303046109 - Provide a non-const iterator.
  using const_iterator = iterator;

  /// @copydoc pw_InlineVarLenEntryQueue_Init
  template <size_t kArraySize>
  static BasicInlineVarLenEntryQueue& Init(uint32_t (&array)[kArraySize]) {
    static_assert(
        kArraySize > PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32,
        "InlineVarLenEntryQueue must be backed by an array with more than "
        "PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 (3) elements");
    return Init(array, kArraySize);
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Init
  static BasicInlineVarLenEntryQueue& Init(uint32_t array[],
                                           size_t array_size_uint32) {
    pw_InlineVarLenEntryQueue_Init(array, array_size_uint32);
    return *std::launder(reinterpret_cast<BasicInlineVarLenEntryQueue*>(array));
  }

  /// Returns the first entry in the queue.
  Entry front() const { return *begin(); }

  /// @copydoc pw_InlineVarLenEntryQueue_Begin
  const_iterator begin() const {
    return const_iterator(pw_InlineVarLenEntryQueue_Begin(array_));
  }
  const_iterator cbegin() const { return begin(); }

  /// @copydoc pw_InlineVarLenEntryQueue_End
  const_iterator end() const {
    return const_iterator(pw_InlineVarLenEntryQueue_End(array_));
  }
  const_iterator cend() const { return end(); }

  /// @copydoc pw_InlineVarLenEntryQueue_Empty
  [[nodiscard]] bool empty() const {
    return pw_InlineVarLenEntryQueue_Empty(array_);
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Size
  size_type size() const { return pw_InlineVarLenEntryQueue_Size(array_); }

  /// @copydoc pw_InlineVarLenEntryQueue_SizeBytes
  size_type size_bytes() const {
    return pw_InlineVarLenEntryQueue_SizeBytes(array_);
  }

  /// @copydoc pw_InlineVarLenEntryQueue_MaxSizeBytes
  size_type max_size_bytes() const {
    return pw_InlineVarLenEntryQueue_MaxSizeBytes(array_);
  }

  /// Underlying storage of the variable-length entry queue. May be used to
  /// memcpy the queue.
  span<const T> raw_storage() const {
    return span<const T>(reinterpret_cast<const T*>(array_),
                         pw_InlineVarLenEntryQueue_RawStorageSizeBytes(array_));
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Clear
  void clear() { pw_InlineVarLenEntryQueue_Clear(array_); }

  /// @copydoc pw_InlineVarLenEntryQueue_Push
  void push(span<const T> value) {
    pw_InlineVarLenEntryQueue_Push(
        array_, value.data(), static_cast<size_type>(value.size()));
  }

  /// @copydoc pw_InlineVarLenEntryQueue_PushOverwrite
  void push_overwrite(span<const T> value) {
    pw_InlineVarLenEntryQueue_PushOverwrite(
        array_, value.data(), static_cast<size_type>(value.size()));
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Pop
  void pop() { pw_InlineVarLenEntryQueue_Pop(array_); }

 protected:
  constexpr BasicInlineVarLenEntryQueue(uint32_t max_size_bytes)
      : array_{_PW_VAR_QUEUE_DATA_SIZE_BYTES(max_size_bytes), 0, 0} {}

  BasicInlineVarLenEntryQueue(const BasicInlineVarLenEntryQueue&) = default;
  BasicInlineVarLenEntryQueue& operator=(const BasicInlineVarLenEntryQueue&) =
      default;

 private:
  static_assert(std::is_integral_v<T> || std::is_same_v<T, std::byte>);
  static_assert(sizeof(T) == sizeof(std::byte));

  uint32_t array_[PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32];
};

/// Refers to an entry in-place in the queue. Entries may be discontiguous.
template <typename T>
class BasicInlineVarLenEntryQueue<T>::Entry {
 public:
  using value_type = T;
  using size_type = std::uint32_t;
  using pointer = const T*;
  using const_pointer = pointer;
  using reference = const T&;
  using const_reference = reference;

  /// Iterator for the bytes in an Entry. Entries may be discontiguous, so a
  /// pointer cannot serve as an iterator.
  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;
    using iterator_category = std::forward_iterator_tag;

    constexpr iterator() : entry_(nullptr), index_(0) {}

    constexpr iterator(const iterator&) = default;
    constexpr iterator& operator=(const iterator&) = default;

    constexpr iterator& operator++() {
      index_ += 1;
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator previous_value(*this);
      operator++();
      return previous_value;
    }

    reference operator*() const { return *GetIndex(*entry_, index_); }
    pointer operator->() const { return GetIndex(*entry_, index_); }

    bool operator==(const iterator& rhs) const {
      return entry_->data_1 == rhs.entry_->data_1 && index_ == rhs.index_;
    }
    bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

   private:
    friend class Entry;

    constexpr iterator(const pw_InlineVarLenEntryQueue_Entry& entry,
                       size_t index)
        : entry_(&entry), index_(index) {}

    const pw_InlineVarLenEntryQueue_Entry* entry_;
    size_t index_;
  };

  // TODO: b/303046109 - Provide mutable access to Entry contents.
  using const_iterator = iterator;

  constexpr Entry(const Entry&) = default;
  constexpr Entry& operator=(const Entry&) = default;

  const_reference at(size_t index) const {
    return *reinterpret_cast<const T*>(
        _pw_InlineVarLenEntryQueue_Entry_GetPointerChecked(&entry_, index));
  }

  const_reference operator[](size_t index) const {
    return *GetIndex(entry_, index);
  }

  const_reference front() const { return *entry_.data_1; }
  const_reference back() const { *GetIndex(entry_, size() - 1); }

  /// Entries may be stored in up to two segments, so this returns spans
  /// refering to both portions of the entry. If the entry is contiguous, the
  /// second span is empty.
  std::pair<span<const value_type>, span<const value_type>> contiguous_data()
      const {
    return std::make_pair(
        span(reinterpret_cast<const_pointer>(entry_.data_1), entry_.size_1),
        span(reinterpret_cast<const_pointer>(entry_.data_2), entry_.size_2));
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Entry_Copy
  ///
  /// Copying with `copy()` is likely more efficient than an iterator-based copy
  /// with `std::copy()`, since `copy()` uses one or two `memcpy` calls instead
  /// of copying byte-by-byte.
  size_type copy(T* dest, size_type count) const {
    return pw_InlineVarLenEntryQueue_Entry_Copy(&entry_, dest, count);
  }

  const_iterator begin() const { return const_iterator(entry_, 0); }
  const_iterator cbegin() const { return begin(); }

  const_iterator end() const { return const_iterator(entry_, size()); }
  const_iterator cend() const { return cend(); }

  [[nodiscard]] bool empty() const { return size() == 0; }

  size_type size() const { return entry_.size_1 + entry_.size_2; }

 private:
  friend class BasicInlineVarLenEntryQueue;

  static const T* GetIndex(const pw_InlineVarLenEntryQueue_Entry& entry,
                           size_t index) {
    return reinterpret_cast<const T*>(
        _pw_InlineVarLenEntryQueue_Entry_GetPointer(&entry, index));
  }

  explicit constexpr Entry(const pw_InlineVarLenEntryQueue_Entry& entry)
      : entry_(entry) {}

  constexpr Entry() : entry_{} {}

  pw_InlineVarLenEntryQueue_Entry entry_;
};

/// Iterator object for a `InlineVarLenEntryQueue`.
///
/// Iterators are invalidated by any operations that change the container or
/// its underlying data (push/pop/init).
template <typename T>
class BasicInlineVarLenEntryQueue<T>::iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = Entry;
  using pointer = const Entry*;
  using reference = const Entry&;
  using iterator_category = std::forward_iterator_tag;

  constexpr iterator() : iterator_{}, entry_{} {}

  constexpr iterator(const iterator&) = default;
  constexpr iterator& operator=(const iterator&) = default;

  iterator& operator++() {
    pw_InlineVarLenEntryQueue_Iterator_Advance(&iterator_);
    entry_.entry_.data_1 = nullptr;  // mark the entry as unloaded
    return *this;
  }
  iterator operator++(int) {
    iterator previous_value(*this);
    operator++();
    return previous_value;
  }

  reference operator*() const {
    LoadEntry();
    return entry_;
  }
  pointer operator->() const {
    LoadEntry();
    return &entry_;
  }

  bool operator==(const iterator& rhs) const {
    return pw_InlineVarLenEntryQueue_Iterator_Equal(&iterator_, &rhs.iterator_);
  }
  bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

 private:
  friend class BasicInlineVarLenEntryQueue;

  explicit constexpr iterator(const pw_InlineVarLenEntryQueue_Iterator& it)
      : iterator_(it) {}

  void LoadEntry() const {
    if (entry_.entry_.data_1 == nullptr) {
      entry_.entry_ = pw_InlineVarLenEntryQueue_GetEntry(&iterator_);
    }
  }

  pw_InlineVarLenEntryQueue_Iterator iterator_;
  mutable Entry entry_;
};

/// Variable-length entry queue that uses ``std::byte`` for the byte type.
template <size_t kMaxSizeBytes = containers::internal::kGenericSized>
using InlineVarLenEntryQueue =
    BasicInlineVarLenEntryQueue<std::byte, kMaxSizeBytes>;

/// @}

}  // namespace pw

#endif  // __cplusplus
