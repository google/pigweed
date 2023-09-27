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

#include "pw_containers/variable_length_entry_deque.h"

#include <string.h>

#include "pw_assert/check.h"
#include "pw_varint/varint.h"

// Access the underlying buffer size and capacity (one less than size).
static uint32_t BufferSize(const uint32_t* deque) { return deque[0]; }
static uint32_t Capacity(const uint32_t* deque) {
  return BufferSize(deque) - 1;
}

// Access the head and tail offsets.
#define HEAD(deque) deque[1]
#define TAIL(deque) deque[2]

// Access the data. Strict aliasing rules do not apply to byte pointers.
static uint8_t* WritableData(uint32_t* deque) { return (uint8_t*)&deque[3]; }
static const uint8_t* Data(const uint32_t* deque) {
  return (const uint8_t*)&deque[3];
}

static uint32_t WrapIndex(pw_VariableLengthEntryDeque_ConstHandle deque,
                          uint32_t offset) {
  if (offset >= BufferSize(deque)) {
    offset -= BufferSize(deque);
  }
  return offset;
}

typedef struct {
  uint32_t prefix;
  uint32_t data;
} EntrySize;

// Returns the size of an entry, including both the prefix length and data size.
static EntrySize ReadEntrySize(pw_VariableLengthEntryDeque_ConstHandle deque,
                               uint32_t offset) {
  EntrySize size = {0, 0};

  bool keep_going;
  do {
    PW_DCHECK_UINT_NE(size.prefix, PW_VARINT_MAX_INT32_SIZE_BYTES);

    keep_going = pw_varint_DecodeOneByte32(
        Data(deque)[offset], size.prefix++, &size.data);
    offset = WrapIndex(deque, offset + 1);
  } while (keep_going);

  return size;
}

static uint32_t EncodePrefix(pw_VariableLengthEntryDeque_ConstHandle deque,
                             uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES],
                             uint32_t data_size_bytes) {
  const uint32_t prefix_size = (uint32_t)pw_varint_Encode32(
      data_size_bytes, prefix, PW_VARINT_MAX_INT32_SIZE_BYTES);

  // Check that the ring buffer is capable of holding entries of this size.
  PW_CHECK_UINT_LE(prefix_size + data_size_bytes, Capacity(deque));
  return prefix_size;
}

// Returns the total encoded size of an entry.
static uint32_t ReadEncodedEntrySize(
    pw_VariableLengthEntryDeque_ConstHandle deque, uint32_t offset) {
  const EntrySize entry_size = ReadEntrySize(deque, offset);
  return entry_size.prefix + entry_size.data;
}

static uint32_t PopFrontNonEmpty(pw_VariableLengthEntryDeque_Handle deque) {
  const uint32_t entry_size = ReadEncodedEntrySize(deque, HEAD(deque));
  HEAD(deque) = WrapIndex(deque, HEAD(deque) + entry_size);
  return entry_size;
}

// Copies data to the buffer, wrapping around the end if needed.
static uint32_t CopyAndWrap(pw_VariableLengthEntryDeque_Handle deque,
                            uint32_t tail,
                            const uint8_t* data,
                            uint32_t size) {
  // Copy the new data in one or two chunks. The first chunk is copied to the
  // byte after the tail, the second from the beginning of the buffer. Both may
  // be zero bytes.
  uint32_t first_chunk = BufferSize(deque) - tail;
  if (first_chunk >= size) {
    first_chunk = size;
  } else {  // Copy 2nd chunk from the beginning of the buffer (may be 0 bytes).
    memcpy(WritableData(deque),
           (const uint8_t*)data + first_chunk,
           size - first_chunk);
  }
  memcpy(&WritableData(deque)[tail], data, first_chunk);
  return WrapIndex(deque, tail + size);
}

static void AppendEntryKnownToFit(pw_VariableLengthEntryDeque_Handle deque,
                                  const uint8_t* prefix,
                                  uint32_t prefix_size,
                                  const void* data,
                                  uint32_t size) {
  // Calculate the new tail offset. Don't update it until the copy is complete.
  uint32_t tail = TAIL(deque);
  tail = CopyAndWrap(deque, tail, prefix, prefix_size);
  TAIL(deque) = CopyAndWrap(deque, tail, data, size);
}

void pw_VariableLengthEntryDeque_PushBack(
    pw_VariableLengthEntryDeque_Handle deque,
    const void* data,
    const uint32_t data_size_bytes) {
  uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES];
  uint32_t prefix_size = EncodePrefix(deque, prefix, data_size_bytes);

  PW_CHECK(prefix_size + data_size_bytes <=
           Capacity(deque) - pw_VariableLengthEntryDeque_RawSizeBytes(deque));

  AppendEntryKnownToFit(deque, prefix, prefix_size, data, data_size_bytes);
}

void pw_VariableLengthEntryDeque_PushBackOverwrite(
    pw_VariableLengthEntryDeque_Handle deque,
    const void* data,
    const uint32_t data_size_bytes) {
  uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES];
  uint32_t prefix_size = EncodePrefix(deque, prefix, data_size_bytes);

  uint32_t available_bytes =
      Capacity(deque) - pw_VariableLengthEntryDeque_RawSizeBytes(deque);
  while (data_size_bytes + prefix_size > available_bytes) {
    available_bytes += PopFrontNonEmpty(deque);
  }

  AppendEntryKnownToFit(deque, prefix, prefix_size, data, data_size_bytes);
}

void pw_VariableLengthEntryDeque_PopFront(
    pw_VariableLengthEntryDeque_Handle deque) {
  PW_CHECK(!pw_VariableLengthEntryDeque_Empty(deque));
  PopFrontNonEmpty(deque);
}

static pw_VariableLengthEntryDeque_Iterator GetIterator(
    pw_VariableLengthEntryDeque_ConstHandle deque, uint32_t offset) {
  if (offset == TAIL(deque)) {
    return pw_VariableLengthEntryDeque_End(deque);
  }

  pw_VariableLengthEntryDeque_Iterator iterator;
  EntrySize size = ReadEntrySize(deque, offset);
  uint32_t offset_1 = WrapIndex(deque, offset + size.prefix);

  const uint32_t first_chunk = BufferSize(deque) - offset_1;

  if (size.data <= first_chunk) {
    iterator.size_1 = size.data;
    iterator.size_2 = 0;
  } else {
    iterator.size_1 = first_chunk;
    iterator.size_2 = size.data - first_chunk;
  }

  iterator.data_1 = Data(deque) + offset_1;
  iterator.data_2 = Data(deque) + WrapIndex(deque, offset_1 + iterator.size_1);

  iterator._pw_deque = deque;
  return iterator;
}

pw_VariableLengthEntryDeque_Iterator pw_VariableLengthEntryDeque_Begin(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  return GetIterator(deque, HEAD(deque));
}

void pw_VariableLengthEntryDeque_Iterator_Advance(
    pw_VariableLengthEntryDeque_Iterator* iterator) {
  *iterator =
      GetIterator(iterator->_pw_deque,
                  WrapIndex(iterator->_pw_deque,
                            (uint32_t)(iterator->data_2 + iterator->size_2 -
                                       Data(iterator->_pw_deque))));
}

uint32_t pw_VariableLengthEntryDeque_Size(
    pw_VariableLengthEntryDeque_ConstHandle deque) {
  uint32_t entry_count = 0;
  uint32_t offset = HEAD(deque);

  while (offset != TAIL(deque)) {
    offset = WrapIndex(deque, offset + ReadEncodedEntrySize(deque, offset));
    entry_count += 1;
  }
  return entry_count;
}
