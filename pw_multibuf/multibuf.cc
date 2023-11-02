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

void MultiBuf::PushFrontChunk(OwnedChunk chunk) {
  PW_DCHECK(chunk->next_in_buf_ == nullptr);
  Chunk* new_chunk = std::move(chunk).Take();
  Chunk* old_first = first_;
  new_chunk->next_in_buf_ = old_first;
  first_ = new_chunk;
}

MultiBuf::ChunkIterator MultiBuf::InsertChunk(ChunkIterator position,
                                              OwnedChunk chunk) {
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

std::tuple<MultiBuf::ChunkIterator, OwnedChunk> MultiBuf::TakeChunk(
    ChunkIterator position) {
  Chunk* chunk = position.chunk_;
  if (position == ChunkBegin()) {
    Chunk* old_first = first_;
    first_ = old_first->next_in_buf_;
    old_first->next_in_buf_ = nullptr;
    return std::make_tuple(ChunkIterator(first_), OwnedChunk(old_first));
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

void MultiBuf::const_iterator::AdvanceToData() {
  while (chunk_ != nullptr && chunk_->size() == 0) {
    chunk_ = chunk_->next_in_buf_;
  }
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
