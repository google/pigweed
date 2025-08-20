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

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>

#include "pw_toolchain/constexpr_tag.h"

#else

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#endif  // __cplusplus

#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/util.h"
#include "pw_varint/varint.h"

// TODO: https://pwbug.dev/426012010 - Find a way to single-source this
// content on the Sphinx site without Breathe.
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

/// @module{pw_containers}

/// @addtogroup pw_containers_queues
/// @{

/// @defgroup inline_var_len_entry_queue_c_api C API
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
/// @pre There must be sufficient space in the queue for this entry.
void pw_InlineVarLenEntryQueue_Push(pw_InlineVarLenEntryQueue_Handle queue,
                                    const void* data,
                                    uint32_t data_size_bytes);

/// Appends an entry to the end of the queue, but only if there is sufficient
/// space for it.
///
/// @returns true if the data was added to the queue; false if it did not fit
/// @pre The entry MUST NOT be larger than `max_size_bytes()`.
bool pw_InlineVarLenEntryQueue_TryPush(pw_InlineVarLenEntryQueue_Handle queue,
                                       const void* data,
                                       const uint32_t data_size_bytes);

/// Appends an entry to the end of the queue, removing entries with `Pop`
/// as necessary to make room. Calling this function drops old entries to make
/// room for new; call `try_push()` to drop new entries instead.
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
} pw_InlineVarLenEntryQueue_ConstIterator;

typedef struct {
  // Private: do not access these fields directly!
  pw_InlineVarLenEntryQueue_ConstIterator _iterator;
} pw_InlineVarLenEntryQueue_Iterator;

/// An entry in the queue. Entries may be stored in up to two segments, so this
/// struct includes pointers to both portions of the entry.
typedef struct {
  uint8_t* data_1;
  uint32_t size_1;
  uint8_t* data_2;
  uint32_t size_2;
} pw_InlineVarLenEntryQueue_Entry;

/// Const version of `pw_InlineVarLenEntryQueue_Entry`.
typedef struct {
  const uint8_t* data_1;
  uint32_t size_1;
  const uint8_t* data_2;
  uint32_t size_2;
} pw_InlineVarLenEntryQueue_ConstEntry;

/// Returns an iterator to the start of the `InlineVarLenEntryQueue`.
static inline pw_InlineVarLenEntryQueue_Iterator
pw_InlineVarLenEntryQueue_Begin(pw_InlineVarLenEntryQueue_Handle queue);

/// Returns an iterator that points past the end of the queue.
static inline pw_InlineVarLenEntryQueue_Iterator pw_InlineVarLenEntryQueue_End(
    pw_InlineVarLenEntryQueue_Handle queue);

/// Returns an iterator that points past the end of the queue.
static inline pw_InlineVarLenEntryQueue_ConstIterator
pw_InlineVarLenEntryQueue_ConstEnd(pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Advances an iterator to point to the next entry in the queue. It is
/// invalid to call `Advance` on an iterator equal to the `End` iterator.
static inline void pw_InlineVarLenEntryQueue_Iterator_Advance(
    pw_InlineVarLenEntryQueue_Iterator* iterator);

/// Advances a const iterator.
void pw_InlineVarLenEntryQueue_ConstIterator_Advance(
    pw_InlineVarLenEntryQueue_ConstIterator* iterator);

/// Compares two iterators for equality.
static inline bool pw_InlineVarLenEntryQueue_Iterator_Equal(
    const pw_InlineVarLenEntryQueue_Iterator* lhs,
    const pw_InlineVarLenEntryQueue_Iterator* rhs);

/// Compares two const iterators for equality.
static inline bool pw_InlineVarLenEntryQueue_ConstIterator_Equal(
    const pw_InlineVarLenEntryQueue_ConstIterator* lhs,
    const pw_InlineVarLenEntryQueue_ConstIterator* rhs);

/// Dereferences an iterator, loading the entry it points to.
static inline pw_InlineVarLenEntryQueue_Entry
pw_InlineVarLenEntryQueue_GetEntry(
    const pw_InlineVarLenEntryQueue_Iterator* iterator);

/// Dereferences a const iterator, loading the entry it points to.
pw_InlineVarLenEntryQueue_ConstEntry pw_InlineVarLenEntryQueue_GetConstEntry(
    const pw_InlineVarLenEntryQueue_ConstIterator* iterator);

/// Copies the contents of the entry to the provided buffer. The entry may be
/// split into two regions; this serializes it into one buffer.
///
/// @param entry The entry whose contents to copy
/// @param dest The buffer into which to copy the serialized entry
/// @param count Copy up to this many bytes; must not be larger than the `dest`
///     buffer, but may be larger than the entry
uint32_t pw_InlineVarLenEntryQueue_ConstEntry_Copy(
    const pw_InlineVarLenEntryQueue_ConstEntry* entry,
    void* dest,
    uint32_t count);

// clang-tidy gives errors for `static inline` functions in headers, so disable
// these diagnostics.
PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC_CLANG(ignored, "-Wunused-function");
PW_MODIFY_DIAGNOSTIC_CLANG(ignored, "-Wunneeded-internal-declaration");

/// Copies the contents of a mutable entry.
static inline uint32_t pw_InlineVarLenEntryQueue_Entry_Copy(
    const pw_InlineVarLenEntryQueue_Entry* entry, void* dest, uint32_t count) {
  pw_InlineVarLenEntryQueue_ConstEntry const_entry;
  memcpy(&const_entry, entry, sizeof(const_entry));
  return pw_InlineVarLenEntryQueue_ConstEntry_Copy(&const_entry, dest, count);
}

/// Returns the byte at the specified index in the entry. Asserts if index is
/// out-of-bounds.
static inline uint8_t pw_InlineVarLenEntryQueue_Entry_At(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index);

/// Returns the byte at the specified index in the entry. Asserts if index is
/// out-of-bounds.
static inline uint8_t pw_InlineVarLenEntryQueue_ConstEntry_At(
    const pw_InlineVarLenEntryQueue_ConstEntry* entry, size_t index);

/// Returns the number of variable-length entries in the queue. This is O(n) in
/// the number of entries in the queue.
uint32_t pw_InlineVarLenEntryQueue_Size(
    pw_InlineVarLenEntryQueue_ConstHandle queue);

/// Returns the maximum number of entries in the queue. This is only attainable
/// if all entries are empty.
static inline uint32_t pw_InlineVarLenEntryQueue_MaxSize(
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

static inline void pw_InlineVarLenEntryQueue_Init(uint32_t array[],
                                                  size_t array_size_uint32) {
  array[0] = (uint32_t)(array_size_uint32 -
                        PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32) *
             sizeof(uint32_t);
  array[1] = 0;  // head
  array[2] = 0;  // tail
}

static inline void pw_InlineVarLenEntryQueue_Clear(
    pw_InlineVarLenEntryQueue_Handle queue) {
  _PW_VAR_QUEUE_HEAD = 0;  // head
  _PW_VAR_QUEUE_TAIL = 0;  // tail
}

static inline pw_InlineVarLenEntryQueue_Iterator
pw_InlineVarLenEntryQueue_Begin(pw_InlineVarLenEntryQueue_Handle queue) {
  pw_InlineVarLenEntryQueue_Iterator begin = {{queue, _PW_VAR_QUEUE_HEAD}};
  return begin;
}

static inline pw_InlineVarLenEntryQueue_ConstIterator
pw_InlineVarLenEntryQueue_ConstBegin(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  pw_InlineVarLenEntryQueue_ConstIterator begin = {queue, _PW_VAR_QUEUE_HEAD};
  return begin;
}

static inline pw_InlineVarLenEntryQueue_Iterator pw_InlineVarLenEntryQueue_End(
    pw_InlineVarLenEntryQueue_Handle queue) {
  pw_InlineVarLenEntryQueue_Iterator end = {{queue, _PW_VAR_QUEUE_TAIL}};
  return end;
}

static inline pw_InlineVarLenEntryQueue_ConstIterator
pw_InlineVarLenEntryQueue_ConstEnd(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  pw_InlineVarLenEntryQueue_ConstIterator end = {queue, _PW_VAR_QUEUE_TAIL};
  return end;
}

static inline void pw_InlineVarLenEntryQueue_Iterator_Advance(
    pw_InlineVarLenEntryQueue_Iterator* iterator) {
  pw_InlineVarLenEntryQueue_ConstIterator_Advance(&iterator->_iterator);
}

static inline bool pw_InlineVarLenEntryQueue_Iterator_Equal(
    const pw_InlineVarLenEntryQueue_Iterator* lhs,
    const pw_InlineVarLenEntryQueue_Iterator* rhs) {
  return pw_InlineVarLenEntryQueue_ConstIterator_Equal(&lhs->_iterator,
                                                       &rhs->_iterator);
}

static inline bool pw_InlineVarLenEntryQueue_ConstIterator_Equal(
    const pw_InlineVarLenEntryQueue_ConstIterator* lhs,
    const pw_InlineVarLenEntryQueue_ConstIterator* rhs) {
  return lhs->_pw_offset == rhs->_pw_offset && lhs->_pw_queue == rhs->_pw_queue;
}

static inline pw_InlineVarLenEntryQueue_Entry
pw_InlineVarLenEntryQueue_GetEntry(
    const pw_InlineVarLenEntryQueue_Iterator* iterator) {
  pw_InlineVarLenEntryQueue_ConstEntry const_entry =
      pw_InlineVarLenEntryQueue_GetConstEntry(&iterator->_iterator);
  pw_InlineVarLenEntryQueue_Entry entry;
  memcpy(&entry, &const_entry, sizeof(entry));
  return entry;
}

// Private function that returns a pointer to the specified index in the entry.
static inline const uint8_t* _pw_InlineVarLenEntryQueue_ConstEntry_GetPointer(
    const pw_InlineVarLenEntryQueue_ConstEntry* entry, size_t index) {
  if (index < entry->size_1) {
    return &entry->data_1[index];
  }
  return &entry->data_2[index - entry->size_1];
}

const uint8_t* _pw_InlineVarLenEntryQueue_ConstEntry_GetPointerChecked(
    const pw_InlineVarLenEntryQueue_ConstEntry* entry, size_t index);

static inline uint8_t pw_InlineVarLenEntryQueue_Entry_At(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index) {
  pw_InlineVarLenEntryQueue_ConstEntry const_entry;
  memcpy(&const_entry, entry, sizeof(const_entry));
  return pw_InlineVarLenEntryQueue_ConstEntry_At(&const_entry, index);
}

static inline uint8_t pw_InlineVarLenEntryQueue_ConstEntry_At(
    const pw_InlineVarLenEntryQueue_ConstEntry* entry, size_t index) {
  return *_pw_InlineVarLenEntryQueue_ConstEntry_GetPointerChecked(entry, index);
}

static inline uint32_t pw_InlineVarLenEntryQueue_RawStorageSizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  return PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 * sizeof(uint32_t) +
         _PW_VAR_QUEUE_ARRAY_SIZE_BYTES;
}

static inline uint32_t pw_InlineVarLenEntryQueue_MaxSize(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  return _PW_VAR_QUEUE_ARRAY_SIZE_BYTES - 1;
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
PW_MODIFY_DIAGNOSTICS_POP();

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
namespace containers::internal {

template <typename T>
class VarLenEntryQueueEntry;

}  // namespace containers::internal

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
  BasicInlineVarLenEntryQueue() : Base(kMaxSizeBytes) {}

  // Explicit zero element constexpr constructor. Using this constructor will
  // place the entire object in .data, which will increase ROM size. Use with
  // caution if working with large capacity sizes.
  constexpr BasicInlineVarLenEntryQueue(ConstexprTag)
      : Base(kMaxSizeBytes), data_{} {}

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

/// @module{pw_containers}

/// @addtogroup pw_containers_queues
/// @{

/// @defgroup inline_var_len_entry_queue_cpp_api C++ API
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
  using value_type = containers::internal::VarLenEntryQueueEntry<T>;
  using const_value_type = containers::internal::VarLenEntryQueueEntry<const T>;
  using size_type = std::uint32_t;
  using pointer = const value_type*;
  using const_pointer = pointer;
  using reference = const value_type&;
  using const_reference = reference;

  /// Iterator object for an `InlineVarLenEntryQueue`.
  ///
  /// Iterators are invalidated by any operations that change the container or
  /// its underlying data (push/pop/init).
  class iterator;

  /// Const iterator object for an `InlineVarLenEntryQueue`.
  class const_iterator;

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
  /// @pre The queue must NOT empty (`empty()` is false).
  value_type front() { return *begin(); }

  const_value_type front() const { return *cbegin(); }

  /// @copydoc pw_InlineVarLenEntryQueue_Begin
  iterator begin() {
    return iterator(pw_InlineVarLenEntryQueue_ConstBegin(array_));
  }
  const_iterator begin() const {
    return const_iterator(pw_InlineVarLenEntryQueue_ConstBegin(array_));
  }
  const_iterator cbegin() const { return begin(); }

  /// @copydoc pw_InlineVarLenEntryQueue_End
  iterator end() {
    return iterator(pw_InlineVarLenEntryQueue_ConstEnd(array_));
  }
  const_iterator end() const {
    return const_iterator(pw_InlineVarLenEntryQueue_ConstEnd(array_));
  }
  const_iterator cend() const { return end(); }

  /// @copydoc pw_InlineVarLenEntryQueue_Empty
  [[nodiscard]] bool empty() const {
    return pw_InlineVarLenEntryQueue_Empty(array_);
  }

  /// @copydoc pw_InlineVarLenEntryQueue_Size
  size_type size() const { return pw_InlineVarLenEntryQueue_Size(array_); }

  /// @copydoc pw_InlineVarLenEntryQueue_MaxSize
  size_type max_size() const {
    return pw_InlineVarLenEntryQueue_MaxSize(array_);
  }

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

  /// @copydoc pw_InlineVarLenEntryQueue_TryPush
  [[nodiscard]] bool try_push(span<const T> value) {
    return pw_InlineVarLenEntryQueue_TryPush(
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

  // Polymorphic-sized queues cannot be destroyed directly due to the lack of a
  // virtual destructor.
  ~BasicInlineVarLenEntryQueue() = default;

  BasicInlineVarLenEntryQueue(const BasicInlineVarLenEntryQueue&) = default;
  BasicInlineVarLenEntryQueue& operator=(const BasicInlineVarLenEntryQueue&) =
      default;

 private:
  static_assert(std::is_integral_v<T> || std::is_same_v<T, std::byte>);
  static_assert(sizeof(T) == sizeof(std::byte));

  uint32_t array_[PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32];
};

namespace containers::internal {

template <typename T>
class VarLenEntryQueueEntryIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::forward_iterator_tag;

  constexpr VarLenEntryQueueEntryIterator() : entry_(nullptr), index_(0) {}

  constexpr VarLenEntryQueueEntryIterator(
      const VarLenEntryQueueEntryIterator&) = default;
  constexpr VarLenEntryQueueEntryIterator& operator=(
      const VarLenEntryQueueEntryIterator&) = default;

  constexpr VarLenEntryQueueEntryIterator& operator++() {
    index_ += 1;
    return *this;
  }
  constexpr VarLenEntryQueueEntryIterator operator++(int) {
    VarLenEntryQueueEntryIterator previous_value(*this);
    operator++();
    return previous_value;
  }

  constexpr VarLenEntryQueueEntryIterator& operator+=(difference_type n) {
    index_ += static_cast<size_t>(n);
    return *this;
  }

  reference operator*() const { return const_cast<reference>(*operator->()); }

  pointer operator->() const {
    return const_cast<pointer>(reinterpret_cast<const T*>(
        _pw_InlineVarLenEntryQueue_ConstEntry_GetPointer(entry_, index_)));
  }

  friend VarLenEntryQueueEntryIterator operator+(
      const VarLenEntryQueueEntryIterator& it, difference_type n) {
    return VarLenEntryQueueEntryIterator(*it.entry_,
                                         it.index_ + static_cast<size_t>(n));
  }

  friend VarLenEntryQueueEntryIterator operator+(
      difference_type n, const VarLenEntryQueueEntryIterator& it) {
    return VarLenEntryQueueEntryIterator(*it.entry_,
                                         it.index_ + static_cast<size_t>(n));
  }

  friend bool operator==(const VarLenEntryQueueEntryIterator& lhs,
                         const VarLenEntryQueueEntryIterator& rhs) {
    return lhs.entry_->data_1 == rhs.entry_->data_1 && lhs.index_ == rhs.index_;
  }
  friend bool operator!=(const VarLenEntryQueueEntryIterator& lhs,
                         const VarLenEntryQueueEntryIterator& rhs) {
    return !(lhs == rhs);
  }

 private:
  template <typename U>
  friend class VarLenEntryQueueEntry;

  constexpr VarLenEntryQueueEntryIterator(
      const pw_InlineVarLenEntryQueue_ConstEntry& entry, size_t index)
      : entry_(&entry), index_(index) {}

  const pw_InlineVarLenEntryQueue_ConstEntry* entry_;
  size_t index_;
};

/// Refers to an entry in-place in the queue. Entries may be discontiguous.
template <typename T>
class VarLenEntryQueueEntry {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::uint32_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  /// Iterator for the bytes in an Entry. Entries may be discontiguous, so a
  /// pointer cannot serve as an iterator.
  using iterator = VarLenEntryQueueEntryIterator<T>;
  using const_iterator = VarLenEntryQueueEntryIterator<const T>;

  constexpr VarLenEntryQueueEntry(const VarLenEntryQueueEntry&) = default;
  constexpr VarLenEntryQueueEntry& operator=(const VarLenEntryQueueEntry&) =
      default;

  constexpr operator VarLenEntryQueueEntry<const T>() const {
    return VarLenEntryQueueEntry<const T>(entry_);
  }

  reference at(size_t index) const {
    return const_cast<reference>(*reinterpret_cast<const_pointer>(
        _pw_InlineVarLenEntryQueue_ConstEntry_GetPointerChecked(&entry_,
                                                                index)));
  }

  reference operator[](size_t index) const {
    return const_cast<reference>(*reinterpret_cast<const_pointer>(
        _pw_InlineVarLenEntryQueue_ConstEntry_GetPointer(&entry_, index)));
  }

  reference front() const {
    return const_cast<reference>(
        reinterpret_cast<const_reference>(*entry_.data_1));
  }
  reference back() const { return operator[](size() - 1); }

  /// Entries may be stored in up to two segments, so this returns spans
  /// refering to both portions of the entry. If the entry is contiguous, the
  /// second span is empty.
  std::pair<span<element_type>, span<element_type>> contiguous_data() const {
    return std::make_pair(
        span(
            const_cast<pointer>(reinterpret_cast<const_pointer>(entry_.data_1)),
            entry_.size_1),
        span(
            const_cast<pointer>(reinterpret_cast<const_pointer>(entry_.data_2)),
            entry_.size_2));
  }

  /// @copydoc pw_InlineVarLenEntryQueue_ConstEntry_Copy
  ///
  /// Copying with `copy()` is likely more efficient than an iterator-based copy
  /// with `std::copy()`, since `copy()` uses one or two `memcpy` calls instead
  /// of copying byte-by-byte.
  size_type copy(value_type* dest, size_type count) const {
    return pw_InlineVarLenEntryQueue_ConstEntry_Copy(&entry_, dest, count);
  }

  iterator begin() const { return iterator(entry_, 0); }
  const_iterator cbegin() const { return const_iterator(entry_, 0); }

  iterator end() const { return iterator(entry_, size()); }
  const_iterator cend() const { return const_iterator(entry_, size()); }

  [[nodiscard]] bool empty() const { return size() == 0; }

  size_type size() const { return entry_.size_1 + entry_.size_2; }

 private:
  friend class VarLenEntryQueueEntry<std::remove_const_t<T>>;
  friend class BasicInlineVarLenEntryQueue;
  friend class BasicInlineVarLenEntryQueue<std::remove_const_t<T>>::iterator;
  friend class BasicInlineVarLenEntryQueue<
      std::remove_const_t<T>>::const_iterator;

  explicit constexpr VarLenEntryQueueEntry(
      const pw_InlineVarLenEntryQueue_ConstEntry& entry)
      : entry_(entry) {}

  constexpr VarLenEntryQueueEntry() : entry_{} {}

  pw_InlineVarLenEntryQueue_ConstEntry entry_;
};

}  // namespace containers::internal

template <typename T>
class BasicInlineVarLenEntryQueue<T>::const_iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = BasicInlineVarLenEntryQueue<T>::const_value_type;
  using pointer = const value_type*;
  using reference = const value_type&;
  using iterator_category = std::forward_iterator_tag;

  constexpr const_iterator() : iterator_{}, entry_{} {}

  constexpr const_iterator(const const_iterator&) = default;
  constexpr const_iterator& operator=(const const_iterator&) = default;

  const_iterator& operator++() {
    pw_InlineVarLenEntryQueue_ConstIterator_Advance(&iterator_);
    entry_.entry_.data_1 = nullptr;  // mark the entry as unloaded
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator previous_value(*this);
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

  bool operator==(const const_iterator& rhs) const {
    return pw_InlineVarLenEntryQueue_ConstIterator_Equal(&iterator_,
                                                         &rhs.iterator_);
  }
  bool operator!=(const const_iterator& rhs) const { return !(*this == rhs); }

 private:
  friend class BasicInlineVarLenEntryQueue;

  explicit constexpr const_iterator(
      const pw_InlineVarLenEntryQueue_ConstIterator& it)
      : iterator_(it) {}

  void LoadEntry() const {
    if (entry_.entry_.data_1 == nullptr) {
      entry_.entry_ = pw_InlineVarLenEntryQueue_GetConstEntry(&iterator_);
    }
  }

  pw_InlineVarLenEntryQueue_ConstIterator iterator_;
  mutable value_type entry_;
};

template <typename T>
class BasicInlineVarLenEntryQueue<T>::iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = BasicInlineVarLenEntryQueue<T>::value_type;
  using pointer = const value_type*;
  using reference = const value_type&;
  using iterator_category = std::forward_iterator_tag;

  constexpr iterator() : iterator_{}, entry_{} {}

  constexpr iterator(const iterator&) = default;
  constexpr iterator& operator=(const iterator&) = default;

  iterator& operator++() {
    pw_InlineVarLenEntryQueue_ConstIterator_Advance(&iterator_);
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
    return pw_InlineVarLenEntryQueue_ConstIterator_Equal(&iterator_,
                                                         &rhs.iterator_);
  }
  bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

 private:
  friend class BasicInlineVarLenEntryQueue;

  explicit constexpr iterator(const pw_InlineVarLenEntryQueue_ConstIterator& it)
      : iterator_(it) {}

  void LoadEntry() const {
    if (entry_.entry_.data_1 == nullptr) {
      entry_.entry_ = pw_InlineVarLenEntryQueue_GetConstEntry(&iterator_);
    }
  }

  pw_InlineVarLenEntryQueue_ConstIterator iterator_;
  mutable value_type entry_;
};

/// Variable-length entry queue that uses ``std::byte`` for the byte type.
template <size_t kMaxSizeBytes = containers::internal::kGenericSized>
using InlineVarLenEntryQueue =
    BasicInlineVarLenEntryQueue<std::byte, kMaxSizeBytes>;

/// @}

/// @}

}  // namespace pw

#endif  // __cplusplus
