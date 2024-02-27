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

#include "pw_multibuf/multibuf.h"

#include "pw_assert/check.h"

namespace pw::multibuf {

void MultiBuf::Release() noexcept {
  while (first_ != nullptr) {
    Chunk* removed = first_;
    first_ = first_->next_in_buf_;
    removed->Free();
  }
}

size_t MultiBuf::size() const {
  size_t len = 0;
  for (const auto& chunk : Chunks()) {
    len += chunk.size();
  }
  return len;
}

bool MultiBuf::ClaimPrefix(size_t bytes_to_claim) {
  if (first_ == nullptr) {
    return false;
  }
  return first_->ClaimPrefix(bytes_to_claim);
}

bool MultiBuf::ClaimSuffix(size_t bytes_to_claim) {
  if (first_ == nullptr) {
    return false;
  }
  return Chunks().back().ClaimSuffix(bytes_to_claim);
}

void MultiBuf::DiscardPrefix(size_t bytes_to_discard) {
  PW_DCHECK(bytes_to_discard <= size());
  while (bytes_to_discard != 0) {
    if (ChunkBegin()->size() > bytes_to_discard) {
      ChunkBegin()->DiscardPrefix(bytes_to_discard);
      return;
    }
    OwnedChunk front_chunk = TakeFrontChunk();
    bytes_to_discard -= front_chunk.size();
  }
}

void MultiBuf::Slice(size_t begin, size_t end) {
  PW_DCHECK(end >= begin);
  DiscardPrefix(begin);
  Truncate(end - begin);
}

void MultiBuf::Truncate(size_t len) {
  PW_DCHECK(len <= size());
  if (len == 0) {
    Release();
  }
  Chunk* new_last_chunk = first_;
  size_t len_from_chunk_start = len;
  while (len_from_chunk_start > new_last_chunk->size()) {
    len_from_chunk_start -= new_last_chunk->size();
    new_last_chunk = new_last_chunk->next_in_buf_;
  }
  new_last_chunk->Truncate(len_from_chunk_start);
  Chunk* remainder = new_last_chunk->next_in_buf_;
  new_last_chunk->next_in_buf_ = nullptr;
  MultiBuf discard;
  discard.first_ = remainder;
}

void MultiBuf::PushPrefix(MultiBuf&& front) {
  front.PushSuffix(std::move(*this));
  *this = std::move(front);
}

void MultiBuf::PushSuffix(MultiBuf&& tail) {
  if (first_ == nullptr) {
    first_ = tail.first_;
    tail.first_ = nullptr;
    return;
  }
  Chunks().back().next_in_buf_ = tail.first_;
  tail.first_ = nullptr;
}

std::optional<MultiBuf> MultiBuf::TakePrefix(size_t bytes_to_take) {
  PW_DCHECK(bytes_to_take <= size());
  MultiBuf front;
  if (bytes_to_take == 0) {
    return front;
  }
  // Pointer to the last element of `front`, allowing constant-time appending.
  Chunk* last_front_chunk = nullptr;
  while (bytes_to_take > ChunkBegin()->size()) {
    OwnedChunk new_chunk = TakeFrontChunk().Take();
    Chunk* new_chunk_ptr = &*new_chunk;
    bytes_to_take -= new_chunk.size();
    if (last_front_chunk == nullptr) {
      front.PushFrontChunk(std::move(new_chunk));
    } else {
      last_front_chunk->next_in_buf_ = std::move(new_chunk).Take();
    }
    last_front_chunk = new_chunk_ptr;
  }
  if (bytes_to_take == 0) {
    return front;
  }
  std::optional<OwnedChunk> last_front_bit = first_->TakePrefix(bytes_to_take);
  if (last_front_bit.has_value()) {
    if (last_front_chunk != nullptr) {
      Chunk* last_front_bit_ptr = std::move(*last_front_bit).Take();
      last_front_chunk->next_in_buf_ = last_front_bit_ptr;
    } else {
      front.PushFrontChunk(std::move(*last_front_bit));
    }
    return front;
  }
  // The front chunk could not be split, so we must put the front back on.
  PushPrefix(std::move(front));
  return std::nullopt;
}

std::optional<MultiBuf> MultiBuf::TakeSuffix(size_t bytes_to_take) {
  size_t front_size = size() - bytes_to_take;
  std::optional<MultiBuf> front_opt = TakePrefix(front_size);
  if (!front_opt.has_value()) {
    return std::nullopt;
  }
  MultiBuf front_then_back = std::move(*front_opt);
  std::swap(front_then_back, *this);
  return front_then_back;
}

void MultiBuf::PushFrontChunk(OwnedChunk&& chunk) {
  PW_DCHECK(chunk->next_in_buf_ == nullptr);
  Chunk* new_chunk = std::move(chunk).Take();
  Chunk* old_first = first_;
  new_chunk->next_in_buf_ = old_first;
  first_ = new_chunk;
}

void MultiBuf::PushBackChunk(OwnedChunk&& chunk) {
  PW_DCHECK(chunk->next_in_buf_ == nullptr);
  Chunk* new_chunk = std::move(chunk).Take();
  if (first_ == nullptr) {
    first_ = new_chunk;
    return;
  }
  Chunk* cur = first_;
  while (cur->next_in_buf_ != nullptr) {
    cur = cur->next_in_buf_;
  }
  cur->next_in_buf_ = new_chunk;
}

MultiBuf::ChunkIterator MultiBuf::InsertChunk(ChunkIterator position,
                                              OwnedChunk&& chunk) {
  // Note: this also catches the cases where ``first_ == nullptr``
  PW_DCHECK(chunk->next_in_buf_ == nullptr);
  if (position == ChunkBegin()) {
    PushFrontChunk(std::move(chunk));
    return ChunkIterator(first_);
  }
  Chunk* previous = Previous(position.chunk_);
  Chunk* old_next = previous->next_in_buf_;
  Chunk* new_chunk = std::move(chunk).Take();
  new_chunk->next_in_buf_ = old_next;
  previous->next_in_buf_ = new_chunk;
  return ChunkIterator(new_chunk);
}

OwnedChunk MultiBuf::TakeFrontChunk() {
  Chunk* old_first = first_;
  first_ = old_first->next_in_buf_;
  old_first->next_in_buf_ = nullptr;
  return OwnedChunk(old_first);
}

std::tuple<MultiBuf::ChunkIterator, OwnedChunk> MultiBuf::TakeChunk(
    ChunkIterator position) {
  Chunk* chunk = position.chunk_;
  if (position == ChunkBegin()) {
    OwnedChunk old_first = TakeFrontChunk();
    return std::make_tuple(ChunkIterator(first_), std::move(old_first));
  }
  Chunk* previous = Previous(chunk);
  previous->next_in_buf_ = chunk->next_in_buf_;
  chunk->next_in_buf_ = nullptr;
  return std::make_tuple(ChunkIterator(previous->next_in_buf_),
                         OwnedChunk(chunk));
}

Chunk* MultiBuf::Previous(Chunk* chunk) const {
  Chunk* previous = first_;
  while (previous != nullptr && previous->next_in_buf_ != chunk) {
    previous = previous->next_in_buf_;
  }
  return previous;
}

MultiBuf::iterator& MultiBuf::iterator::operator++() {
  if (byte_index_ + 1 == chunk_->size()) {
    chunk_ = chunk_->next_in_buf_;
    byte_index_ = 0;
    AdvanceToData();
  } else {
    ++byte_index_;
  }
  return *this;
}

MultiBuf::iterator& MultiBuf::iterator::operator+=(size_t advance) {
  if (advance == 0) {
    return *this;
  }
  while (advance >= (chunk_->size() - byte_index_)) {
    advance -= (chunk_->size() - byte_index_);
    chunk_ = chunk_->next_in_buf_;
    byte_index_ = 0;
  }
  byte_index_ += advance;
  return *this;
}

void MultiBuf::iterator::AdvanceToData() {
  while (chunk_ != nullptr && chunk_->size() == 0) {
    chunk_ = chunk_->next_in_buf_;
  }
}

MultiBuf::const_iterator& MultiBuf::const_iterator::operator++() {
  if (byte_index_ + 1 == chunk_->size()) {
    chunk_ = chunk_->next_in_buf_;
    byte_index_ = 0;
    AdvanceToData();
  } else {
    ++byte_index_;
  }
  return *this;
}

MultiBuf::const_iterator& MultiBuf::const_iterator::operator+=(size_t advance) {
  if (advance == 0) {
    return *this;
  }
  while (advance >= (chunk_->size() - byte_index_)) {
    advance -= (chunk_->size() - byte_index_);
    chunk_ = chunk_->next_in_buf_;
    byte_index_ = 0;
  }
  byte_index_ += advance;
  return *this;
}

void MultiBuf::const_iterator::AdvanceToData() {
  while (chunk_ != nullptr && chunk_->size() == 0) {
    chunk_ = chunk_->next_in_buf_;
  }
}

Chunk& MultiBuf::ChunkIterable::back() {
  Chunk* current = first_;
  while (current->next_in_buf_ != nullptr) {
    current = current->next_in_buf_;
  }
  return *current;
}

size_t MultiBuf::ChunkIterable::size() const {
  Chunk* current = first_;
  size_t i = 0;
  while (current != nullptr) {
    ++i;
    current = current->next_in_buf_;
  }
  return i;
}

}  // namespace pw::multibuf
