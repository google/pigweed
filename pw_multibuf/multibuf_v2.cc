// Copyright 2025 The Pigweed Authors
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

#include "pw_multibuf/multibuf_v2.h"

#include <cstring>
#include <utility>

#include "public/pw_multibuf/byte_iterator.h"
#include "public/pw_multibuf/multibuf_v2.h"
#include "pw_assert/check.h"
#include "pw_status/try.h"

namespace pw::multibuf::internal {

GenericMultiBuf& GenericMultiBuf::operator=(GenericMultiBuf&& other) {
  deque_ = std::move(other.deque_);
  depth_ = std::exchange(other.depth_, 2);
  CopyMemoryContext(other);
  other.ClearMemoryContext();
  observer_ = std::exchange(other.observer_, nullptr);
  return *this;
}

bool GenericMultiBuf::TryReserveChunks(size_type num_chunks) {
  size_type current_chunks = deque_.size() / depth_;
  if (num_chunks <= current_chunks) {
    return true;
  }
  size_type num_entries = num_chunks - current_chunks;
  PW_CHECK_MUL(num_entries, depth_, &num_entries);
  return TryReserveEntries(num_entries);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          const GenericMultiBuf& mb) {
  PW_CHECK(IsCompatible(mb));
  size_type depth = depth_;
  size_type width = mb.deque_.size() / mb.depth_;
  while (depth_ < mb.depth_) {
    if (!AddLayer(0)) {
      break;
    }
  }
  if (depth_ >= mb.depth_ && TryReserveEntries(pos, depth_ * width)) {
    return true;
  }
  while (depth_ > depth) {
    std::ignore = PopLayer();
  }
  return false;
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos, size_t size) {
  PW_CHECK_UINT_LE(size, Entry::kMaxSize);
  return TryReserveEntries(pos, depth_);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          size_t size,
                                          const Deallocator* deallocator) {
  PW_CHECK(IsCompatible(deallocator));
  return TryReserveForInsert(pos, size);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          size_t size,
                                          const ControlBlock* control_block) {
  PW_CHECK(IsCompatible(control_block));
  return TryReserveForInsert(pos, size);
}

void GenericMultiBuf::Insert(const_iterator pos, GenericMultiBuf&& mb) {
  PW_CHECK(TryReserveForInsert(pos, mb));
  if (!has_deallocator()) {
    CopyMemoryContext(mb);
  }

  // Make room for the other object's entries.
  size_type mb_width = mb.deque_.size() / mb.depth_;
  size_type index = InsertEntries(pos, mb_width * depth_);

  // Merge the entries into this object.
  size_t size = 0;
  while (!mb.empty()) {
    size_type i = 0;
    size_type offset = mb.GetOffset(0);
    size_type length = mb.GetLength(0);
    for (; i < mb.depth_; ++i) {
      deque_[index + i] = mb.deque_.front();
      mb.deque_.pop_front();
    }

    // If this object is deeper than `mb`, pad it with extra entries.
    for (; i < depth_; ++i) {
      deque_[index + i].view = {
          .offset = offset,
          .sealed = false,
          .length = length,
          .boundary = true,
      };
    }
    size += size_t{length};
    index += depth_;
  }
  if (mb.observer_ != nullptr) {
    mb.observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesAdded, size);
  }
}

void GenericMultiBuf::Insert(const_iterator pos, ConstByteSpan bytes) {
  Insert(pos, bytes, 0, bytes.size());
}

void GenericMultiBuf::Insert(const_iterator pos,
                             ConstByteSpan bytes,
                             size_t offset,
                             size_t length,
                             Deallocator* deallocator) {
  PW_CHECK(TryReserveForInsert(pos, bytes.size(), deallocator));
  if (!has_deallocator()) {
    SetDeallocator(deallocator);
  }
  size_type index = Insert(pos, bytes, offset, length);
  deque_[index + 1].base_view.owned = true;
}

void GenericMultiBuf::Insert(const_iterator pos,
                             ConstByteSpan bytes,
                             size_t offset,
                             size_t length,
                             ControlBlock* control_block) {
  PW_CHECK(TryReserveForInsert(pos, bytes.size(), control_block));
  if (!has_control_block()) {
    SetControlBlock(control_block);
  }
  size_type index = Insert(pos, bytes, offset, length);
  deque_[index + 1].base_view.shared = true;
}

bool GenericMultiBuf::IsRemovable(const_iterator pos, size_t size) const {
  PW_CHECK(pos != cend());
  PW_CHECK_UINT_NE(size, 0u);
  if (static_cast<size_t>(cend() - pos) < size) {
    return false;
  }
  auto [index, offset] = GetIndexAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_index, end_offset] = GetIndexAndOffset(end);
  return (offset == 0 || !IsOwned(index)) &&
         (end_offset == 0 || !IsOwned(end_index));
}

Result<GenericMultiBuf> GenericMultiBuf::Remove(const_iterator pos,
                                                size_t size) {
  PW_CHECK(IsRemovable(pos, size));
  GenericMultiBuf out(deque_.get_allocator());
  if (!TryReserveForRemove(pos, size, &out)) {
    return Status::ResourceExhausted();
  }
  CopyRange(pos, size, out);
  EraseRange(pos, size);
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  return Result<GenericMultiBuf>(std::move(out));
}

Result<GenericMultiBuf> GenericMultiBuf::PopFrontFragment() {
  PW_CHECK(!empty());
  size_t size = 0;
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    size_type length = GetLength(index);
    if (length == 0) {
      continue;
    }
    size += size_t{length};
    if (IsBoundary(index)) {
      break;
    }
  }
  return Remove(begin(), size);
}

Result<GenericMultiBuf::const_iterator> GenericMultiBuf::Discard(
    const_iterator pos, size_t size) {
  PW_CHECK_UINT_NE(size, 0u);
  difference_type out_offset = pos - begin();
  if (!TryReserveForRemove(pos, size, nullptr)) {
    return Status::ResourceExhausted();
  }
  ClearRange(pos, size);
  EraseRange(pos, size);
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  return cbegin() + out_offset;
}

bool GenericMultiBuf::IsReleasable(const_iterator pos) const {
  PW_CHECK(pos != cend());
  auto [index, offset] = GetIndexAndOffset(pos);
  return IsOwned(index);
}

UniquePtr<std::byte[]> GenericMultiBuf::Release(const_iterator pos) {
  PW_CHECK(IsReleasable(pos));
  auto [index, offset] = GetIndexAndOffset(pos);
  ByteSpan bytes(GetData(index) + deque_[index + 1].base_view.offset,
                 deque_[index + 1].base_view.length);
  auto* deallocator = GetDeallocator();
  EraseRange(pos - offset, size_t{GetLength(index)});
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, bytes.size());
  }
  return UniquePtr<std::byte[]>(bytes.data(), bytes.size(), *deallocator);
}

bool GenericMultiBuf::IsShareable(const_iterator pos) const {
  PW_CHECK(pos != cend());
  auto [index, offset] = GetIndexAndOffset(pos);
  return !IsOwned(index) && IsShared(index);
}

std::byte* GenericMultiBuf::Share(const_iterator pos) {
  PW_CHECK(IsShareable(pos));
  auto [index, offset] = GetIndexAndOffset(pos);
  GetControlBlock()->IncrementShared();
  return GetData(index);
}

size_t GenericMultiBuf::CopyTo(ByteSpan dst, size_t offset) const {
  return CopyToImpl(dst, offset, 0);
}

size_t GenericMultiBuf::CopyFrom(ConstByteSpan src, size_t offset) {
  size_t total = 0;
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    if (src.empty()) {
      break;
    }
    ByteSpan view = GetView(index);
    if (offset < view.size()) {
      size_t size = std::min(view.size() - offset, src.size());
      std::memcpy(view.data() + offset, src.data(), size);
      src = src.subspan(size);
      offset = 0;
      total += size;
    } else {
      offset -= view.size();
    }
  }
  return total;
}

ConstByteSpan GenericMultiBuf::Get(ByteSpan copy, size_t offset) const {
  ByteSpan buffer;
  std::optional<size_type> start;
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    ByteSpan view = GetView(index);
    if (buffer.empty() && offset >= view.size()) {
      // Still looking for start of data.
      offset -= view.size();
    } else if (buffer.empty()) {
      // Found the start of data.
      buffer = view.subspan(offset);
      start = index;
    } else if (buffer.data() + buffer.size() == view.data()) {
      // Current view is contiguous with previous; append.
      buffer = ByteSpan(buffer.data(), buffer.size() + view.size());
    } else {
      // Span is discontiguous and needs to be copied.
      size_t copied = CopyToImpl(copy, offset, start.value());
      return copy.subspan(0, copied);
    }
  }
  // Requested span is contiguous and can be directly passed to the visitor.
  return buffer.size() <= copy.size() ? buffer : buffer.subspan(0, copy.size());
}

void GenericMultiBuf::Clear() {
  while (depth_ > 2) {
    if (!PopLayer()) {
      UnsealTopLayer();
    }
  }
  // Free any owned chunks.
  Deallocator* deallocator = has_deallocator() ? GetDeallocator() : nullptr;
  size_t num_bytes = 0;
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    num_bytes += size_t{GetLength(index)};
    if (!IsOwned(index)) {
      continue;
    }
    deallocator->Deallocate(GetData(index));
    if (!IsShared(index)) {
      continue;
    }
    for (size_type shared = FindShared(index, index + depth_);
         shared != deque_.size();
         shared = FindShared(index, shared + depth_)) {
      deque_[shared + 1].base_view.owned = false;
      deque_[shared + 1].base_view.shared = false;
    }
  }
  deque_.clear();
  ClearMemoryContext();
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, num_bytes);
    observer_ = nullptr;
  }
}

bool GenericMultiBuf::AddLayer(size_t offset, size_t length) {
  CheckRange(offset, length, size());
  size_t num_fragments = NumFragments();

  // Given entries with layers A and B, to which we want to add layer C:
  //     A1 B1 A2 B2 A3 B3 A4 B4
  // 1). Add `width` empty buffers:
  //     A1 B1 A2 B2 A3 B3 A4 B4 -- -- -- --
  size_type width = deque_.size() / depth_;
  if (!TryReserveEntries(width)) {
    return false;
  }
  ++depth_;
  for (size_t i = 0; i < width; ++i) {
    deque_.push_back({.data = nullptr});
  }

  // 2). Shift the existing layers over. This is expensive, but slicing usually
  //     happens with `width == 1`:
  for (size_type i = deque_.size(); i != 0; --i) {
    if (i % depth_ == 0) {
      --width;
      deque_[i - 1].view = {
          .offset = 0,
          .sealed = false,
          .length = 0,
          .boundary = false,
      };
    } else {
      deque_[i - 1] = deque_[i - 1 - width];
    }
  }

  // 3). Fill in the new layer C with subspans of layer B:
  //     A1 B1 C1 A2 B2 C2 A3 B3 C3 A4 B4 C4
  SetLayer(offset, length);

  // 4). Mark the end of the new layer.
  if (!deque_.empty()) {
    deque_.back().view.boundary = true;
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kLayerAdded, num_fragments);
  }
  return true;
}

void GenericMultiBuf::SealTopLayer() {
  PW_CHECK_UINT_GT(depth_, 2u);
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    deque_[index + depth_ - 1].view.sealed = true;
  }
}

void GenericMultiBuf::UnsealTopLayer() {
  PW_CHECK_UINT_GT(depth_, 2u);
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    deque_[index + depth_ - 1].view.sealed = false;
  }
}

bool GenericMultiBuf::ResizeTopLayer(size_t offset, size_t length) {
  PW_CHECK_UINT_GT(depth_, 2u);
  CheckRange(offset, length, size());
  if (IsTopLayerSealed()) {
    return false;
  }
  SetLayer(offset, length);
  return true;
}

bool GenericMultiBuf::PopLayer() {
  PW_CHECK_UINT_GT(depth_, 2u);
  if (IsTopLayerSealed()) {
    return false;
  }
  size_t num_fragments = NumFragments();

  // Given entries with layers A, B, and C, to remove layer C:
  //     A1 B1 C1 A2 B2 C2 A3 B3 C3 A4 B4 C4
  // 1). Check that the layer is not sealed.
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    PW_CHECK(!IsSealed(index));
  }

  // 2). Compress lower layers backward.
  //     -- -- -- -- A1 B1 A2 B2 A3 B3 A4 B4
  size_type shift = 0;
  size_type discard = deque_.size() / depth_;
  size_type keep = deque_.size() - discard;
  --depth_;
  for (size_type i = 1; i <= keep; ++i) {
    size_type j = deque_.size() - i;
    if ((i - 1) % depth_ == 0) {
      ++shift;
    }
    deque_[j] = deque_[j - shift];
    if ((j - discard) % depth_ != depth_ - 1) {
      continue;
    }
  }

  // 3). Discard the first elements
  //     A1 B1 A2 B2 A3 B3 A4 B4
  for (size_type i = 0; i < discard; ++i) {
    deque_.pop_front();
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kLayerRemoved, num_fragments);
  }
  return true;
}

// Implementation methods

size_t GenericMultiBuf::CheckRange(size_t offset, size_t length, size_t size) {
  PW_CHECK_UINT_LE(offset, size);
  if (length == dynamic_extent) {
    return size - offset;
  }
  PW_CHECK_UINT_LE(length, size - offset);
  return length;
}

Deallocator* GenericMultiBuf::GetDeallocator() const {
  switch (memory_tag_) {
    case MemoryTag::kDeallocator:
      return memory_context_.deallocator;
    case MemoryTag::kControlBlock:
      return memory_context_.control_block->allocator();
    case MemoryTag::kEmpty:
      PW_CRASH("Invalid memory tag");
  }
  PW_UNREACHABLE;
}

void GenericMultiBuf::SetDeallocator(Deallocator* deallocator) {
  memory_tag_ = MemoryTag::kDeallocator;
  memory_context_.deallocator = deallocator;
}

GenericMultiBuf::ControlBlock* GenericMultiBuf::GetControlBlock() const {
  PW_DCHECK_UINT_EQ(memory_tag_, MemoryTag::kControlBlock);
  return memory_context_.control_block;
}

void GenericMultiBuf::SetControlBlock(ControlBlock* control_block) {
  memory_tag_ = MemoryTag::kControlBlock;
  memory_context_.control_block = control_block;
  memory_context_.control_block->IncrementShared();
}

void GenericMultiBuf::CopyMemoryContext(const GenericMultiBuf& other) {
  memory_tag_ = other.memory_tag_;
  memory_context_ = other.memory_context_;
}

void GenericMultiBuf::ClearMemoryContext() {
  if (memory_tag_ == MemoryTag::kControlBlock) {
    memory_context_.control_block->DecrementShared();
  }
  memory_tag_ = MemoryTag::kEmpty;
  memory_context_.deallocator = nullptr;
}

bool GenericMultiBuf::IsCompatible(const GenericMultiBuf& other) const {
  if (other.has_control_block()) {
    return IsCompatible(other.GetControlBlock());
  }
  if (other.has_deallocator()) {
    return IsCompatible(other.GetDeallocator());
  }
  return true;
}

bool GenericMultiBuf::IsCompatible(const Deallocator* other) const {
  return !has_deallocator() || GetDeallocator() == other;
}

bool GenericMultiBuf::IsCompatible(const ControlBlock* other) const {
  return has_control_block() ? (GetControlBlock() == other)
                             : IsCompatible(other->allocator());
}

GenericMultiBuf::size_type GenericMultiBuf::NumFragments() const {
  size_type num_fragments = 0;
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    if (GetLength(index) != 0 && IsBoundary(index)) {
      ++num_fragments;
    }
  }
  return num_fragments;
}

std::pair<GenericMultiBuf::size_type, GenericMultiBuf::size_type>
GenericMultiBuf::GetIndexAndOffset(const_iterator pos) const {
  size_type index = pos.chunk_index();
  size_type offset = 0;
  size_t left = pos.offset_;
  while (left != 0 && index < deque_.size()) {
    size_t length = size_t{GetLength(index)};
    if (left < length) {
      offset = static_cast<size_type>(left);
      left = 0;
      break;
    }
    left -= length;
    index += depth_;
  }
  PW_CHECK_UINT_EQ(left, 0u);
  return std::make_pair(index, offset);
}

bool GenericMultiBuf::TryReserveEntries(const_iterator pos,
                                        size_type num_entries) {
  auto [index, offset] = GetIndexAndOffset(pos);
  return TryReserveEntries(num_entries, offset != 0);
}

bool GenericMultiBuf::TryReserveEntries(size_type num_entries, bool split) {
  if (split) {
    PW_CHECK_ADD(num_entries, depth_, &num_entries);
  }
  PW_CHECK_ADD(num_entries, deque_.size(), &num_entries);
  return deque_.try_reserve_exact(num_entries);
}

GenericMultiBuf::size_type GenericMultiBuf::InsertEntries(
    const_iterator pos, size_type num_entries) {
  auto [index, offset] = GetIndexAndOffset(pos);
  if (offset != 0) {
    PW_CHECK_ADD(num_entries, depth_, &num_entries);
  }
  Entry entry;
  entry.data = nullptr;
  for (size_type i = 0; i < num_entries; ++i) {
    deque_.push_back(entry);
  }
  for (size_type i = deque_.size() - 1; i >= index + num_entries; --i) {
    deque_[i] = deque_[i - num_entries];
  }

  if (offset == 0) {
    // New chunk falls between existing chunks.
    return index;
  }
  // New chunk within an existing chunk, which must be split.
  SplitAfter(index, offset, deque_, index + num_entries);
  SplitBefore(index, offset);
  return index + depth_;
}

GenericMultiBuf::size_type GenericMultiBuf::Insert(const_iterator pos,
                                                   ConstByteSpan bytes,
                                                   size_t offset,
                                                   size_t length) {
  length = CheckRange(offset, length, bytes.size());
  size_type index = InsertEntries(pos, depth_);
  deque_[index].data = const_cast<std::byte*>(bytes.data());
  auto offset_ = static_cast<Entry::size_type>(offset);
  auto length_ = static_cast<Entry::size_type>(length);
  deque_[index + 1].base_view = {
      .offset = offset_,
      .owned = false,
      .length = length_,
      .shared = false,
  };
  for (size_type i = 2; i < depth_; ++i) {
    deque_[index + i].view = {
        .offset = offset_,
        .sealed = false,
        .length = length_,
        .boundary = true,
    };
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesAdded, length);
  }
  return index;
}

void GenericMultiBuf::SplitBase(size_type index,
                                Deque& out_deque,
                                size_type out_index) {
  if (IsOwned(index)) {
    PW_CHECK_PTR_EQ(&deque_, &out_deque);
    deque_[index + 1].base_view.shared = true;
  }
  if (&deque_ == &out_deque && index == out_index) {
    return;
  }
  for (size_type i = 0; i < depth_; ++i) {
    out_deque[out_index + i] = deque_[index + i];
  }
}

void GenericMultiBuf::SplitBefore(size_type index,
                                  size_type split,
                                  Deque& out_deque,
                                  size_type out_index) {
  SplitBase(index, out_deque, out_index);
  split += GetOffset(index);
  for (size_type i = 1; i < depth_; ++i) {
    Entry src = deque_[index + i];
    Entry& dst = out_deque[out_index + i];
    if (i == 1) {
      dst.base_view.offset = src.base_view.offset;
      dst.base_view.length = split - src.base_view.offset;
    } else {
      dst.view.offset = src.view.offset;
      dst.view.length = split - src.view.offset;
    }
  }
}

void GenericMultiBuf::SplitBefore(size_type index, size_type split) {
  SplitBefore(index, split, deque_, index);
}

void GenericMultiBuf::SplitAfter(size_type index,
                                 size_type split,
                                 Deque& out_deque,
                                 size_type out_index) {
  SplitBase(index, out_deque, out_index);
  split += GetOffset(index);
  for (size_type i = 1; i < depth_; ++i) {
    Entry src = deque_[index + i];
    Entry& dst = out_deque[out_index + i];
    if (i == 1) {
      dst.base_view.offset = split;
      dst.base_view.length =
          src.base_view.offset + src.base_view.length - split;
    } else {
      dst.view.offset = split;
      dst.view.length = src.view.offset + src.view.length - split;
    }
  }
}

void GenericMultiBuf::SplitAfter(size_type index, size_type split) {
  SplitAfter(index, split, deque_, index);
}

bool GenericMultiBuf::TryReserveForRemove(const_iterator pos,
                                          size_t size,
                                          GenericMultiBuf* out) {
  auto [index, offset] = GetIndexAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_index, end_offset] = GetIndexAndOffset(end);
  size_type shift = end_index - index;
  if (shift == 0 && offset != 0) {
    return (out == nullptr || out->TryReserveEntries(depth_)) &&
           TryReserveEntries(0, offset != 0);
  }
  if (out == nullptr) {
    return true;
  }
  if (shift == 0 && offset == 0) {
    return out->TryReserveEntries(depth_);
  }
  size_type reserve = end_offset == 0 ? shift : shift + depth_;
  return out == nullptr || out->TryReserveEntries(reserve);
}

void GenericMultiBuf::CopyRange(const_iterator pos,
                                size_t size,
                                GenericMultiBuf& out) {
  out.depth_ = depth_;
  out.CopyMemoryContext(*this);

  auto [index, offset] = GetIndexAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_index, end_offset] = GetIndexAndOffset(end);

  // Determine how many entries needs to be moved.
  size_type shift = end_index - index;

  // Are we removing the prefix of a single chunk?
  if (shift == 0 && offset == 0) {
    out.InsertEntries(begin(), depth_);
    SplitBefore(index, end_offset, out.deque_, 0);
    return;
  }

  // Are we removing a sub-chunk? If so, split the chunk in two.
  if (shift == 0) {
    out.InsertEntries(begin(), depth_);
    SplitBefore(end_index, end_offset, out.deque_, 0);
    out.SplitAfter(0, offset);
    return;
  }

  // Otherwise, start by copying entries to the new deque, if provided.
  size_type out_index = 0;
  size_type reserve = end_offset == 0 ? shift : shift + depth_;
  out.InsertEntries(cend(), reserve);

  // Copy the suffix of the first chunk.
  if (offset != 0) {
    SplitAfter(index, offset, out.deque_, out_index);
    index += depth_;
    shift -= depth_;
    out_index += depth_;
  }

  // Copy the complete chunks.
  std::memcpy(&out.deque_[out_index], &deque_[index], shift * sizeof(Entry));
  index += shift;
  out_index += shift;
  if (has_control_block()) {
    GetControlBlock()->IncrementShared();
  }

  // Copy the prefix of the last chunk.
  if (end_offset != 0) {
    SplitBefore(end_index, end_offset, out.deque_, out_index);
  }
}

void GenericMultiBuf::ClearRange(const_iterator pos, size_t size) {
  auto [index, offset] = GetIndexAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_index, end_offset] = GetIndexAndOffset(end);

  // Deallocate any owned memory that was not moved.
  if (!has_deallocator()) {
    return;
  }
  Deallocator* deallocator = GetDeallocator();
  if (offset != 0) {
    index += depth_;
  }
  for (; index < end_index; index += depth_) {
    if (!IsOwned(index)) {
      continue;
    }
    if (IsShared(index) && (FindShared(index, 0) != index ||
                            FindShared(index, end_index) != deque_.size())) {
      // There is at least one shared reference being kept.
      continue;
    }
    deallocator->Deallocate(GetData(index));
  }
}

void GenericMultiBuf::EraseRange(const_iterator pos, size_t size) {
  auto [index, offset] = GetIndexAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_index, end_offset] = GetIndexAndOffset(end);

  // Are we removing a sub-chunk? If so, split the chunk in two.
  if (index == end_index && offset != 0) {
    SplitAfter(InsertEntries(pos, 0), end_offset - offset);
    return;
  }

  // Discard suffix of first chunk.
  if (offset != 0) {
    SplitBefore(index, offset);
    index += depth_;
  }

  // Discard prefix of last chunk.
  if (end_offset != 0) {
    SplitAfter(end_index, end_offset);
  }

  // Dicard complete chunks.
  if (index < end_index) {
    deque_.erase(deque_.begin() + index, deque_.begin() + end_index);
  }

  // Check if the memory context is still needed.
  if (!has_deallocator()) {
    return;
  }
  Deallocator* deallocator = GetDeallocator();
  bool needs_deallocator = false;
  for (index = 0; index < deque_.size(); index += depth_) {
    if (IsOwned(index)) {
      needs_deallocator = true;
    } else if (IsShared(index)) {
      return;
    }
  }
  ClearMemoryContext();
  if (needs_deallocator) {
    SetDeallocator(deallocator);
  }
}

GenericMultiBuf::size_type GenericMultiBuf::FindShared(size_type index,
                                                       size_type start) {
  std::byte* data = GetData(index);
  for (index = start; index < deque_.size(); index += depth_) {
    if (IsShared(index) && data == GetData(index)) {
      break;
    }
  }
  return index;
}

size_t GenericMultiBuf::CopyToImpl(ByteSpan dst,
                                   size_t offset,
                                   size_type start) const {
  size_t total = 0;
  for (size_type index = start; index < deque_.size(); index += depth_) {
    if (dst.empty()) {
      break;
    }
    ConstByteSpan view = GetView(index);
    if (offset < view.size()) {
      size_t size = std::min(view.size() - offset, dst.size());
      std::memcpy(dst.data(), view.data() + offset, size);
      dst = dst.subspan(size);
      offset = 0;
      total += size;
    } else {
      offset -= view.size();
    }
  }
  return total;
}

bool GenericMultiBuf::IsTopLayerSealed() const {
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    if (deque_[index + depth_ - 1].view.sealed) {
      return true;
    }
  }
  return false;
}

void GenericMultiBuf::SetLayer(size_t offset, size_t length) {
  for (size_type index = 0; index < deque_.size(); index += depth_) {
    Entry& lower = deque_[index + depth_ - 2];
    size_type lower_offset, lower_length;
    if (depth_ == 3) {
      lower_offset = lower.base_view.offset;
      lower_length = lower.base_view.length;
    } else {
      lower_offset = lower.view.offset;
      lower_length = lower.view.length;
    }

    // Skip over entries until we reach `offset`.
    Entry& entry = deque_[index + depth_ - 1];
    if (offset >= lower_length) {
      offset -= size_t{lower_length};
      entry.view.offset = 0;
      entry.view.length = 0;
      continue;
    }
    entry.view.offset = lower_offset + static_cast<size_type>(offset);
    lower_length -= static_cast<size_type>(offset);

    if (length == dynamic_extent) {
      entry.view.length = lower_length;
    } else {
      entry.view.length =
          static_cast<size_type>(std::min(size_t{lower_length}, length));
      length -= size_t{entry.view.length};
    }
    offset = 0;
  }
}

}  // namespace pw::multibuf::internal
