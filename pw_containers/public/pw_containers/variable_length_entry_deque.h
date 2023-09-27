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

/// @file pw_containers/variable_length_entry_deque.h
///
/// A `VariableLengthEntryDeque` is a double-ended queue of variable-length
/// binary entries. It is implemented as a ring (circular) buffer and supports
/// operations to append entries and overwrite if necessary. Entries may be zero
/// bytes up to the maximum size supported by the deque.
///
/// The `VariableLengthEntryDeque` a few interesting properties.
///
/// - Data and metadata are stored inline in a contiguous block of
///   `uint32_t`-aligned memory.
/// - All data structure state changes are accomplished with a single update to
///   a `uint32_t`. The memory is always in a valid state and may be parsed
///   offline.
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
/// A `VariableLengthEntryDeque` may be declared and initialized in C with the
/// @c_macro{PW_VARIABLE_LENGTH_ENTRY_DEQUE_DECLARE} macro.
///
/// @code{c}
///
///   // Declares a deque with a maximum entry size of 10 bytes.
///   PW_VARIABLE_LENGTH_ENTRY_DEQUE_DECLARE(deque, 10);
///
///   // Write some data
///   pw_VariableLengthEntryDeque_PushBackOverwrite(deque, "123", 3);
///   pw_VariableLengthEntryDeque_PushBackOverwrite(deque, "456", 3);
///
///   assert(pw_VariableLengthEntryDeque_Size(deque) == 2u);
///
///   // Remove the entries
///   pw_VariableLengthEntryDeque_PopFront(deque);
///   pw_VariableLengthEntryDeque_PopFront(deque);
///
/// @endcode
///
/// Alternately, a `VariableLengthEntryDeque` may also be initialized in an
/// existing ``uint32_t`` array.
///
/// @code{c}
///
///   // Initialize a VariableLengthEntryDeque.
///   uint32_t buffer[32];
///   pw_VariableLengthEntryDeque_Init(buffer, 32);
///
///   // Largest supported entry works out to 114 B (13 B overhead + 1 B prefix)
///   assert(pw_VariableLengthEntryDeque_MaxEntrySizeBytes(buffer) == 114u);
///
///   // Write some data
///   pw_VariableLengthEntryDeque_PushBackOverwrite(buffer, "123", 3);
///
/// @endcode

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// @defgroup variable_length_entry_deque_c_api VariableLengthEntryDeque C API
/// @{

/// Handle that refers to a `VariableLengthEntryDeque`. In memory, the deque
/// is a `uint32_t` array.
typedef uint32_t* pw_VariableLengthEntryDeque_Handle;
typedef const uint32_t* pw_VariableLengthEntryDeque_ConstHandle;

/// Declares and initializes a `VariableLengthEntryDeque` that can hold an entry
/// of up to `max_entry_size_bytes`. Attempting to store larger entries is
/// invalid and will fail an assertion.
#define PW_VARIABLE_LENGTH_ENTRY_DEQUE_DECLARE(variable, max_entry_size_bytes) \
  uint32_t variable[PW_VARIABLE_LENGTH_ENTRY_DEQUE_HEADER_SIZE_UINT32 +        \
                    _PW_VAR_DEQUE_DATA_SIZE_UINT32(max_entry_size_bytes)] = {  \
      _PW_VAR_DEQUE_DATA_SIZE_BYTES(max_entry_size_bytes),                     \
      /*head=*/0u,                                                             \
      /*tail=*/0u}

/// The size of the `VariableLengthEntryDeque` header, in `uint32_t` elements.
/// This header stores the buffer length and head and tail offsets.
///
/// The underlying `uint32_t` array of a `VariableLengthEntryDeque` must be
/// larger than this size.
#define PW_VARIABLE_LENGTH_ENTRY_DEQUE_HEADER_SIZE_UINT32 (3)

/// Initializes a `VariableLengthEntryDeque` in place in a `uint32_t` array. The
/// array MUST be larger than
/// @c_macro{PW_VARIABLE_LENGTH_ENTRY_DEQUE_HEADER_SIZE_UINT32} (3) elements.
static inline void pw_VariableLengthEntryDeque_Init(uint32_t array[],
                                                    size_t array_size_uint32);

/// Appends an entry to the end of the deque.
///
/// @pre The entry MUST not be larger than
/// @cpp_func{pw_VariableLengthEntryDeque_MaxEntrySizeBytes}; asserts if it is.
void pw_VariableLengthEntryDeque_PushBack(
    pw_VariableLengthEntryDeque_Handle deque,
    const void* data,
    uint32_t data_size_bytes);

/// Appends an entry to the end of the deque, removing entries with `PopFront`
/// as necessary to make room.
///
/// @pre The entry MUST not be larger than
/// @cpp_func{pw_VariableLengthEntryDeque_MaxEntrySizeBytes}; asserts if it is.
void pw_VariableLengthEntryDeque_PushBackOverwrite(
    pw_VariableLengthEntryDeque_Handle deque,
    const void* data,
    uint32_t data_size_bytes);

/// Removes the first entry from the ring buffer.
///
/// @pre The deque MUST have at least one entry.
void pw_VariableLengthEntryDeque_PopFront(
    pw_VariableLengthEntryDeque_Handle deque);

/// Iterator object for a `VariableLengthEntryDeque`. Entries may be stored in
/// up to two segments, so this iterator includes pointers to both portions of
/// the entry. Iterators are checked for equality with
/// @cpp_func{pw_VariableLengthEntryDeque_Iterator_Equals}.
///
/// Iterators are invalidated by any operations that change the container or
/// its underlying data (push/pop/init).
typedef struct {
  const uint8_t* data_1;
  uint32_t size_1;
  const uint8_t* data_2;
  uint32_t size_2;

  // Private: do not access this field directly!
  pw_VariableLengthEntryDeque_ConstHandle _pw_deque;
} pw_VariableLengthEntryDeque_Iterator;

/// Returns an iterator to the start of the `VariableLengthEntryDeque`.
pw_VariableLengthEntryDeque_Iterator pw_VariableLengthEntryDeque_Begin(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns an iterator to entry following the last entry, which is not valid.
static inline pw_VariableLengthEntryDeque_Iterator
pw_VariableLengthEntryDeque_End(pw_VariableLengthEntryDeque_ConstHandle deque);

/// Advances an iterator to point to the next entry in the deque. It is
/// invalid to call `Advance` on an iterator equal to the `End` iterator.
void pw_VariableLengthEntryDeque_Iterator_Advance(
    pw_VariableLengthEntryDeque_Iterator* iterator);

/// Compares two iterators for equality.
static inline bool pw_VariableLengthEntryDeque_Iterator_Equals(
    const pw_VariableLengthEntryDeque_Iterator* lhs,
    const pw_VariableLengthEntryDeque_Iterator* rhs);

/// Returns the number of variable-length entries in the deque. This is O(n) in
/// the number of entries in the deque.
uint32_t pw_VariableLengthEntryDeque_Size(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns the number of bytes stored in the buffer, including entry metadata.
/// This can be used with `RawCapacityBytes` to gauge available space for
/// entries.
static inline uint32_t pw_VariableLengthEntryDeque_RawSizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns the maximum number of bytes that can be stored in the buffer,
/// including per-entry metadata. This can be used with `RawSizeBytes` to gauge
/// available space for entries.
static inline uint32_t pw_VariableLengthEntryDeque_RawCapacityBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns the size of the raw underlying `VariableLengthEntryDeque` storage.
/// This size may be used to copy a `VariableLengthEntryDeque` into another
/// 32-bit aligned memory location.
static inline uint32_t pw_VariableLengthEntryDeque_RawStorageSizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns the size of the largest entry this `VariableLengthEntryDeque` can
/// hold. Attempting to store a larger entry is invalid and fails an assert.
static inline uint32_t pw_VariableLengthEntryDeque_MaxEntrySizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// Returns true if the `VariableLengthEntryDeque` is empty, false if it has at
/// least one entry.
static inline bool pw_VariableLengthEntryDeque_Empty(
    pw_VariableLengthEntryDeque_ConstHandle deque);

/// @}

// Implementation details.

#define _PW_VAR_DEQUE_DATA_SIZE_UINT32(max_entry_size_bytes) \
  ((_PW_VAR_DEQUE_DATA_SIZE_BYTES(max_entry_size_bytes) + 3 /* round up */) / 4)

#define _PW_VAR_DEQUE_DATA_SIZE_BYTES(max_entry_size_bytes)                    \
  (PW_VARINT_ENCODED_SIZE_BYTES(max_entry_size_bytes) + max_entry_size_bytes + \
   1 /*end byte*/)

#define _PW_VAR_DEQUE_ARRAY_SIZE_BYTES deque[0]
#define _PW_VAR_DEQUE_HEAD deque[1]
#define _PW_VAR_DEQUE_TAIL deque[2]  // points after the last byte
#define _PW_VAR_DEQUE_DATA ((const uint8_t*)&deque[3])

#define _PW_VAR_DEQUE_GET_ARRAY_SIZE_BYTES(array_size_uint32)     \
  (uint32_t)(array_size_uint32 -                                  \
             PW_VARIABLE_LENGTH_ENTRY_DEQUE_HEADER_SIZE_UINT32) * \
      sizeof(uint32_t)

static inline void pw_VariableLengthEntryDeque_Init(uint32_t array[],
                                                    size_t array_size_uint32) {
  array[0] = _PW_VAR_DEQUE_GET_ARRAY_SIZE_BYTES(array_size_uint32);
  array[1] = 0;  // head
  array[2] = 0;  // tail
}

static inline pw_VariableLengthEntryDeque_Iterator
pw_VariableLengthEntryDeque_End(pw_VariableLengthEntryDeque_ConstHandle deque) {
  pw_VariableLengthEntryDeque_Iterator end = {
      &_PW_VAR_DEQUE_DATA[_PW_VAR_DEQUE_TAIL], 0, NULL, 0, deque};  // NOLINT
  return end;
}

static inline bool pw_VariableLengthEntryDeque_Iterator_Equals(
    const pw_VariableLengthEntryDeque_Iterator* lhs,
    const pw_VariableLengthEntryDeque_Iterator* rhs) {
  return lhs->data_1 == rhs->data_1;
}

static inline uint32_t pw_VariableLengthEntryDeque_RawSizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  uint32_t tail = _PW_VAR_DEQUE_TAIL;
  if (tail < _PW_VAR_DEQUE_HEAD) {
    tail += _PW_VAR_DEQUE_ARRAY_SIZE_BYTES;
  }
  return tail - _PW_VAR_DEQUE_HEAD;
}

static inline uint32_t pw_VariableLengthEntryDeque_RawCapacityBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  return _PW_VAR_DEQUE_ARRAY_SIZE_BYTES - 1;
}

static inline uint32_t pw_VariableLengthEntryDeque_RawStorageSizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  return PW_VARIABLE_LENGTH_ENTRY_DEQUE_HEADER_SIZE_UINT32 * sizeof(uint32_t) +
         _PW_VAR_DEQUE_ARRAY_SIZE_BYTES;
}

static inline uint32_t pw_VariableLengthEntryDeque_MaxEntrySizeBytes(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  return _PW_VAR_DEQUE_ARRAY_SIZE_BYTES - 1 -
         (uint32_t)pw_varint_EncodedSizeBytes(_PW_VAR_DEQUE_ARRAY_SIZE_BYTES -
                                              1);
}

static inline bool pw_VariableLengthEntryDeque_Empty(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  return _PW_VAR_DEQUE_HEAD == _PW_VAR_DEQUE_TAIL;
}

// These macros are not part of the public API, so undefine them.
#undef _PW_VAR_DEQUE_ARRAY_SIZE_BYTES
#undef _PW_VAR_DEQUE_HEAD
#undef _PW_VAR_DEQUE_TAIL
#undef _PW_VAR_DEQUE_DATA

#ifdef __cplusplus
}       // extern "C"
#endif  // __cplusplus
