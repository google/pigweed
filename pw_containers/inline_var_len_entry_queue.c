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

#include "pw_containers/inline_var_len_entry_queue.h"

#include <string.h>

#include "pw_assert/check.h"
#include "pw_varint/varint.h"

// Access the underlying buffer size and capacity (one less than size).
static uint32_t BufferSize(const uint32_t* queue) { return queue[0]; }
static uint32_t Capacity(const uint32_t* queue) {
  return BufferSize(queue) - 1;
}

// Access the head and tail offsets.
#define HEAD(queue) queue[1]
#define TAIL(queue) queue[2]

// Access the data. Strict aliasing rules do not apply to byte pointers.
static uint8_t* WritableData(uint32_t* queue) { return (uint8_t*)&queue[3]; }
static const uint8_t* Data(const uint32_t* queue) {
  return (const uint8_t*)&queue[3];
}

static uint32_t WrapIndex(pw_InlineVarLenEntryQueue_ConstHandle queue,
                          uint32_t offset) {
  if (offset >= BufferSize(queue)) {
    offset -= BufferSize(queue);
  }
  return offset;
}

typedef struct {
  uint32_t prefix;
  uint32_t data;
} EntrySize;

// Returns the size of an entry, including both the prefix length and data size.
static EntrySize ReadEntrySize(pw_InlineVarLenEntryQueue_ConstHandle queue,
                               uint32_t offset) {
  EntrySize size = {0, 0};

  bool keep_going;
  do {
    PW_DCHECK_UINT_NE(size.prefix, PW_VARINT_MAX_INT32_SIZE_BYTES);

    keep_going = pw_varint_DecodeOneByte32(
        Data(queue)[offset], size.prefix++, &size.data);
    offset = WrapIndex(queue, offset + 1);
  } while (keep_going);

  return size;
}

static uint32_t EncodePrefix(pw_InlineVarLenEntryQueue_ConstHandle queue,
                             uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES],
                             uint32_t data_size_bytes) {
  const uint32_t prefix_size = (uint32_t)pw_varint_Encode32(
      data_size_bytes, prefix, PW_VARINT_MAX_INT32_SIZE_BYTES);

  // Check that the ring buffer is capable of holding entries of this size.
  PW_CHECK_UINT_LE(prefix_size + data_size_bytes,
                   Capacity(queue),
                   "Entry is too large for this InlineVarLenEntryQueue");
  return prefix_size;
}

// Returns the total encoded size of an entry.
static uint32_t ReadEncodedEntrySize(
    pw_InlineVarLenEntryQueue_ConstHandle queue, uint32_t offset) {
  const EntrySize entry_size = ReadEntrySize(queue, offset);
  return entry_size.prefix + entry_size.data;
}

static uint32_t PopNonEmpty(pw_InlineVarLenEntryQueue_Handle queue) {
  const uint32_t entry_size = ReadEncodedEntrySize(queue, HEAD(queue));
  HEAD(queue) = WrapIndex(queue, HEAD(queue) + entry_size);
  return entry_size;
}

// Copies data to the buffer, wrapping around the end if needed.
static uint32_t CopyAndWrap(pw_InlineVarLenEntryQueue_Handle queue,
                            uint32_t tail,
                            const uint8_t* data,
                            uint32_t size) {
  // Copy the new data in one or two chunks. The first chunk is copied to the
  // byte after the tail, the second from the beginning of the buffer. Both may
  // be zero bytes.
  uint32_t first_chunk = BufferSize(queue) - tail;
  if (first_chunk >= size) {
    first_chunk = size;
  } else {  // Copy 2nd chunk from the beginning of the buffer (may be 0 bytes).
    memcpy(WritableData(queue),
           (const uint8_t*)data + first_chunk,
           size - first_chunk);
  }
  memcpy(&WritableData(queue)[tail], data, first_chunk);
  return WrapIndex(queue, tail + size);
}

static void AppendEntryKnownToFit(pw_InlineVarLenEntryQueue_Handle queue,
                                  const uint8_t* prefix,
                                  uint32_t prefix_size,
                                  const void* data,
                                  uint32_t size) {
  // Calculate the new tail offset. Don't update it until the copy is complete.
  uint32_t tail = TAIL(queue);
  tail = CopyAndWrap(queue, tail, prefix, prefix_size);
  TAIL(queue) = CopyAndWrap(queue, tail, data, size);
}

static inline uint32_t AvailableBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  uint32_t tail = TAIL(queue);
  if (tail < HEAD(queue)) {
    tail += BufferSize(queue);
  }
  return Capacity(queue) - (tail - HEAD(queue));
}

void pw_InlineVarLenEntryQueue_Push(pw_InlineVarLenEntryQueue_Handle queue,
                                    const void* data,
                                    const uint32_t data_size_bytes) {
  uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES];
  uint32_t prefix_size = EncodePrefix(queue, prefix, data_size_bytes);

  PW_CHECK(prefix_size + data_size_bytes <= AvailableBytes(queue),
           "Insufficient remaining space for entry");

  AppendEntryKnownToFit(queue, prefix, prefix_size, data, data_size_bytes);
}

void pw_InlineVarLenEntryQueue_PushOverwrite(
    pw_InlineVarLenEntryQueue_Handle queue,
    const void* data,
    const uint32_t data_size_bytes) {
  uint8_t prefix[PW_VARINT_MAX_INT32_SIZE_BYTES];
  uint32_t prefix_size = EncodePrefix(queue, prefix, data_size_bytes);

  uint32_t available_bytes = AvailableBytes(queue);
  while (data_size_bytes + prefix_size > available_bytes) {
    available_bytes += PopNonEmpty(queue);
  }

  AppendEntryKnownToFit(queue, prefix, prefix_size, data, data_size_bytes);
}

void pw_InlineVarLenEntryQueue_Pop(pw_InlineVarLenEntryQueue_Handle queue) {
  PW_CHECK(!pw_InlineVarLenEntryQueue_Empty(queue));
  PopNonEmpty(queue);
}

void pw_InlineVarLenEntryQueue_Iterator_Advance(
    pw_InlineVarLenEntryQueue_Iterator* iterator) {
  iterator->_pw_offset = WrapIndex(
      iterator->_pw_queue,
      iterator->_pw_offset +
          ReadEncodedEntrySize(iterator->_pw_queue, iterator->_pw_offset));
}

pw_InlineVarLenEntryQueue_Entry pw_InlineVarLenEntryQueue_GetEntry(
    const pw_InlineVarLenEntryQueue_Iterator* iterator) {
  pw_InlineVarLenEntryQueue_ConstHandle queue = iterator->_pw_queue;

  pw_InlineVarLenEntryQueue_Entry entry;
  EntrySize size = ReadEntrySize(queue, iterator->_pw_offset);
  uint32_t offset_1 = WrapIndex(queue, iterator->_pw_offset + size.prefix);

  const uint32_t first_chunk = BufferSize(queue) - offset_1;

  if (size.data <= first_chunk) {
    entry.size_1 = size.data;
    entry.size_2 = 0;
  } else {
    entry.size_1 = first_chunk;
    entry.size_2 = size.data - first_chunk;
  }

  entry.data_1 = Data(queue) + offset_1;
  entry.data_2 = Data(queue) + WrapIndex(queue, offset_1 + entry.size_1);
  return entry;
}

uint32_t pw_InlineVarLenEntryQueue_Entry_Copy(
    const pw_InlineVarLenEntryQueue_Entry* entry, void* dest, uint32_t count) {
  PW_DCHECK(dest != NULL || count == 0u);

  const uint32_t entry_size = entry->size_1 + entry->size_2;
  const uint32_t to_copy = count < entry_size ? count : entry_size;

  if (to_copy == 0u) {
    return 0u;
  }

  memcpy(dest, entry->data_1, entry->size_1);

  const uint32_t remaining = to_copy - entry->size_1;
  if (remaining != 0u) {
    memcpy((uint8_t*)dest + entry->size_1, entry->data_2, remaining);
  }

  return to_copy;
}

const uint8_t* _pw_InlineVarLenEntryQueue_Entry_GetPointerChecked(
    const pw_InlineVarLenEntryQueue_Entry* entry, size_t index) {
  PW_CHECK_UINT_LT(index, entry->size_1 + entry->size_2);
  return _pw_InlineVarLenEntryQueue_Entry_GetPointer(entry, index);
}

uint32_t pw_InlineVarLenEntryQueue_Size(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  uint32_t entry_count = 0;
  uint32_t offset = HEAD(queue);

  while (offset != TAIL(queue)) {
    offset = WrapIndex(queue, offset + ReadEncodedEntrySize(queue, offset));
    entry_count += 1;
  }
  return entry_count;
}

uint32_t pw_InlineVarLenEntryQueue_SizeBytes(
    pw_InlineVarLenEntryQueue_ConstHandle queue) {
  uint32_t total_entry_size_bytes = 0;
  uint32_t offset = HEAD(queue);

  while (offset != TAIL(queue)) {
    const EntrySize size = ReadEntrySize(queue, offset);
    offset = WrapIndex(queue, offset + size.prefix + size.data);
    total_entry_size_bytes += size.data;
  }
  return total_entry_size_bytes;
}
