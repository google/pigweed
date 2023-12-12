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

/// @file pw_containers/variable_length_entry_queue.h
///
/// A `VariableLengthEntryQueue` is a ended queue of inline variable-length
/// binary entries. It is implemented as a ring (circular) buffer and supports
/// operations to append entries and overwrite if necessary. Entries may be zero
/// bytes up to the maximum size supported by the queue.
///
/// The `VariableLengthEntryQueue` has a few interesting properties.
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
/// - C support is required.
/// - A consistent, parsable, in-memory representation is required (e.g. to
///   decode the buffer from a block of memory).
///
/// A `VariableLengthEntryQueue` may be declared and initialized in C with the
/// @c_macro{PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE} macro.
///
/// @code{c}
///
///   // Declares a queue with a maximum single entry size of 10 bytes.
///   PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(queue, 10);
///
///   // Write some data
///   pw_VariableLengthEntryQueue_PushOverwrite(queue, "123", 3);
///   pw_VariableLengthEntryQueue_PushOverwrite(queue, "456", 3);
///
///   assert(pw_VariableLengthEntryQueue_Size(queue) == 2u);
///
///   // Remove the entries
///   pw_VariableLengthEntryQueue_Pop(queue);
///   pw_VariableLengthEntryQueue_Pop(queue);
///
/// @endcode
///
/// Alternately, a `VariableLengthEntryQueue` may also be initialized in an
/// existing ``uint32_t`` array.
///
/// @code{c}
///
///   // Initialize a VariableLengthEntryQueue.
///   uint32_t buffer[32];
///   pw_VariableLengthEntryQueue_Init(buffer, 32);
///
///   // Largest supported entry works out to 114 B (13 B overhead + 1 B prefix)
///   assert(pw_VariableLengthEntryQueue_MaxEntrySizeBytes(buffer) == 114u);
///
///   // Write some data
///   pw_VariableLengthEntryQueue_PushOverwrite(buffer, "123", 3);
///
/// @endcode

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// @defgroup variable_length_entry_queue_c_api VariableLengthEntryQueue C API
/// @{

/// Handle that refers to a `VariableLengthEntryQueue`. In memory, the queue
/// is a `uint32_t` array.
typedef uint32_t* pw_VariableLengthEntryQueue_Handle;
typedef const uint32_t* pw_VariableLengthEntryQueue_ConstHandle;

/// Declares and initializes a `VariableLengthEntryQueue` that can hold up to
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

/// The size of the `VariableLengthEntryQueue` header, in `uint32_t` elements.
/// This header stores the buffer length and head and tail offsets.
///
/// The underlying `uint32_t` array of a `VariableLengthEntryQueue` must be
/// larger than this size.
#define PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 (3)

/// Initializes a `VariableLengthEntryQueue` in place in a `uint32_t` array. The
/// array MUST be larger than
/// @c_macro{PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32} (3) elements.
static inline void pw_VariableLengthEntryQueue_Init(uint32_t array[],
                                                    size_t array_size_uint32);

/// Empties the queue.
static inline void pw_VariableLengthEntryQueue_Clear(
    pw_VariableLengthEntryQueue_Handle queue);

/// Appends an entry to the end of the queue.
///
/// @pre The entry MUST NOT be larger than `max_size_bytes()`.
void pw_VariableLengthEntryQueue_Push(pw_VariableLengthEntryQueue_Handle queue,
                                      const void* data,
                                      uint32_t data_size_bytes);

/// Appends an entry to the end of the queue, removing entries with `Pop`
/// as necessary to make room.
///
/// @pre The entry MUST NOT be larger than `max_size_bytes()`.
void pw_VariableLengthEntryQueue_PushOverwrite(
    pw_VariableLengthEntryQueue_Handle queue,
    const void* data,
    uint32_t data_size_bytes);

/// Removes the first entry from queue.
///
/// @pre The queue MUST have at least one entry.
void pw_VariableLengthEntryQueue_Pop(pw_VariableLengthEntryQueue_Handle queue);

/// Iterator object for a `VariableLengthEntryQueue`. Iterators are checked for
/// equality with
/// @cpp_func{pw_VariableLengthEntryQueue_Iterator_Equal}.
///
/// Iterators are invalidated by any operations that change the container or
/// its underlying data (push/pop/init).
typedef struct {
  // Private: do not access these fields directly!
  pw_VariableLengthEntryQueue_ConstHandle _pw_queue;
  uint32_t _pw_offset;
} pw_VariableLengthEntryQueue_Iterator;

/// An entry in the queue. Entries may be stored in up to two segments, so this
/// struct includes pointers to both portions of the entry.
typedef struct {
  const uint8_t* data_1;
  uint32_t size_1;
  const uint8_t* data_2;
  uint32_t size_2;
} pw_VariableLengthEntryQueue_Entry;

/// Returns an iterator to the start of the `VariableLengthEntryQueue`.
static inline pw_VariableLengthEntryQueue_Iterator
pw_VariableLengthEntryQueue_Begin(
    pw_VariableLengthEntryQueue_ConstHandle queue);

/// Returns an iterator that points past the end of the queue.
static inline pw_VariableLengthEntryQueue_Iterator
pw_VariableLengthEntryQueue_End(pw_VariableLengthEntryQueue_ConstHandle queue);

/// Advances an iterator to point to the next entry in the queue. It is
/// invalid to call `Advance` on an iterator equal to the `End` iterator.
void pw_VariableLengthEntryQueue_Iterator_Advance(
    pw_VariableLengthEntryQueue_Iterator* iterator);

/// Compares two iterators for equality.
static inline bool pw_VariableLengthEntryQueue_Iterator_Equal(
    const pw_VariableLengthEntryQueue_Iterator* lhs,
    const pw_VariableLengthEntryQueue_Iterator* rhs);

/// Dereferences an iterator, loading the entry it points to.
pw_VariableLengthEntryQueue_Entry pw_VariableLengthEntryQueue_GetEntry(
    const pw_VariableLengthEntryQueue_Iterator* iterator);

/// Copies the contents of the entry to the provided buffer. The entry may be
/// split into two regions; this serializes it into one buffer.
///
/// @param entry The entry whose contents to copy
/// @param dest The buffer into which to copy the serialized entry
/// @param count Copy up to this many bytes; must not be larger than the `dest`
///     buffer, but may be larger than the entry
uint32_t pw_VariableLengthEntryQueue_Entry_Copy(
    const pw_VariableLengthEntryQueue_Entry* entry, void* dest, uint32_t count);

/// Returns the byte at the specified index in the entry. Asserts if index is
/// out-of-bounds.
static inline uint8_t pw_VariableLengthEntryQueue_Entry_At(
    const pw_VariableLengthEntryQueue_Entry* entry, size_t index);

/// Returns the number of variable-length entries in the queue. This is O(n) in
/// the number of entries in the queue.
uint32_t pw_VariableLengthEntryQueue_Size(
    pw_VariableLengthEntryQueue_ConstHandle queue);

/// Returns the combined size in bytes of all entries in the queue, excluding
/// metadata. This is O(n) in the number of entries in the queue.
uint32_t pw_VariableLengthEntryQueue_SizeBytes(
    pw_VariableLengthEntryQueue_ConstHandle queue);

/// Returns the the maximum number of bytes that can be stored in the queue.
/// This is largest possible value of `size_bytes()`, and the size of the
/// largest single entry that can be stored in this queue. Attempting to store a
/// larger entry is invalid and results in a crash.
static inline uint32_t pw_VariableLengthEntryQueue_MaxSizeBytes(
    pw_VariableLengthEntryQueue_ConstHandle queue);

/// Returns the size of the raw underlying `VariableLengthEntryQueue` storage.
/// This size may be used to copy a `VariableLengthEntryQueue` into another
/// 32-bit aligned memory location.
static inline uint32_t pw_VariableLengthEntryQueue_RawStorageSizeBytes(
    pw_VariableLengthEntryQueue_ConstHandle queue);

/// Returns true if the `VariableLengthEntryQueue` is empty, false if it has at
/// least one entry.
static inline bool pw_VariableLengthEntryQueue_Empty(
    pw_VariableLengthEntryQueue_ConstHandle queue);

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

static inline void pw_VariableLengthEntryQueue_Init(uint32_t array[],
                                                    size_t array_size_uint32) {
  array[0] = _PW_VAR_QUEUE_GET_ARRAY_SIZE_BYTES(array_size_uint32);
  array[1] = 0;  // head
  array[2] = 0;  // tail
}

static inline void pw_VariableLengthEntryQueue_Clear(
    pw_VariableLengthEntryQueue_Handle queue) {
  _PW_VAR_QUEUE_HEAD = 0;  // head
  _PW_VAR_QUEUE_TAIL = 0;  // tail
}

static inline pw_VariableLengthEntryQueue_Iterator
pw_VariableLengthEntryQueue_Begin(
    pw_VariableLengthEntryQueue_ConstHandle queue) {
  pw_VariableLengthEntryQueue_Iterator begin = {queue, _PW_VAR_QUEUE_HEAD};
  return begin;
}

static inline pw_VariableLengthEntryQueue_Iterator
pw_VariableLengthEntryQueue_End(pw_VariableLengthEntryQueue_ConstHandle queue) {
  pw_VariableLengthEntryQueue_Iterator end = {queue, _PW_VAR_QUEUE_TAIL};
  return end;
}

static inline bool pw_VariableLengthEntryQueue_Iterator_Equal(
    const pw_VariableLengthEntryQueue_Iterator* lhs,
    const pw_VariableLengthEntryQueue_Iterator* rhs) {
  return lhs->_pw_offset == rhs->_pw_offset && lhs->_pw_queue == rhs->_pw_queue;
}

// Private function that returns a pointer to the specified index in the Entry.
static inline const uint8_t* _pw_VariableLengthEntryQueue_Entry_GetPointer(
    const pw_VariableLengthEntryQueue_Entry* entry, size_t index) {
  if (index < entry->size_1) {
    return &entry->data_1[index];
  }
  return &entry->data_2[index - entry->size_1];
}

const uint8_t* _pw_VariableLengthEntryQueue_Entry_GetPointerChecked(
    const pw_VariableLengthEntryQueue_Entry* entry, size_t index);

static inline uint8_t pw_VariableLengthEntryQueue_Entry_At(
    const pw_VariableLengthEntryQueue_Entry* entry, size_t index) {
  return *_pw_VariableLengthEntryQueue_Entry_GetPointerChecked(entry, index);
}

static inline uint32_t pw_VariableLengthEntryQueue_RawStorageSizeBytes(
    pw_VariableLengthEntryQueue_ConstHandle queue) {
  return PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 * sizeof(uint32_t) +
         _PW_VAR_QUEUE_ARRAY_SIZE_BYTES;
}

static inline uint32_t pw_VariableLengthEntryQueue_MaxSizeBytes(
    pw_VariableLengthEntryQueue_ConstHandle queue) {
  return _PW_VAR_QUEUE_ARRAY_SIZE_BYTES - 1 -
         (uint32_t)pw_varint_EncodedSizeBytes(_PW_VAR_QUEUE_ARRAY_SIZE_BYTES -
                                              1);
}

static inline bool pw_VariableLengthEntryQueue_Empty(
    pw_VariableLengthEntryQueue_ConstHandle queue) {
  return _PW_VAR_QUEUE_HEAD == _PW_VAR_QUEUE_TAIL;
}

// These macros are not part of the public API, so undefine them.
#undef _PW_VAR_QUEUE_ARRAY_SIZE_BYTES
#undef _PW_VAR_QUEUE_HEAD
#undef _PW_VAR_QUEUE_TAIL
#undef _PW_VAR_QUEUE_DATA

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
