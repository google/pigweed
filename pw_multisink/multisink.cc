// Copyright 2021 The Pigweed Authors
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
#include "pw_multisink/multisink.h"

#include <cstring>

#include "pw_assert/check.h"
#include "pw_status/try.h"
#include "pw_varint/varint.h"

namespace pw {
namespace multisink {

void MultiSink::HandleEntry(ConstByteSpan entry) {
  std::lock_guard lock(lock_);
  PW_DCHECK_OK(ring_buffer_.PushBack(entry, sequence_id_++));
  NotifyListeners();
}

void MultiSink::HandleDropped(uint32_t drop_count) {
  std::lock_guard lock(lock_);
  sequence_id_ += drop_count;
  NotifyListeners();
}

Result<ConstByteSpan> MultiSink::GetEntry(Drain& drain,
                                          ByteSpan buffer,
                                          uint32_t& drop_count_out) {
  size_t bytes_read = 0;
  uint32_t entry_sequence_id = 0;
  drop_count_out = 0;

  std::lock_guard lock(lock_);
  PW_DCHECK_PTR_EQ(drain.multisink_, this);

  const Status peek_status = drain.reader_.PeekFrontWithPreamble(
      buffer, entry_sequence_id, bytes_read);
  if (peek_status.IsOutOfRange()) {
    // If the drain has caught up, report the last handled sequence ID so that
    // it can still process any dropped entries.
    entry_sequence_id = sequence_id_ - 1;
  } else if (!peek_status.ok()) {
    // Exit immediately if the result isn't OK or OUT_OF_RANGE, as the
    // entry_entry_sequence_id cannot be used for computation. Later invocations
    // to GetEntry will permit readers to determine how far the sequence ID
    // moved forward.
    return peek_status;
  }

  // Compute the drop count delta by comparing this entry's sequence ID with the
  // last sequence ID this drain successfully read.
  //
  // The drop count calculation simply computes the difference between the
  // current and last sequence IDs. Consecutive successful reads will always
  // differ by one at least, so it is subtracted out. If the read was not
  // successful, the difference is not adjusted.
  drop_count_out = entry_sequence_id - drain.last_handled_sequence_id_ -
                   (peek_status.ok() ? 1 : 0);
  drain.last_handled_sequence_id_ = entry_sequence_id;

  // The Peek above may have failed due to OutOfRange, now that we've set the
  // drop count see if we should return before attempting to pop.
  if (peek_status.IsOutOfRange()) {
    return peek_status;
  }

  // Success, pop the oldest entry!
  PW_CHECK(drain.reader_.PopFront().ok());
  return std::as_bytes(buffer.first(bytes_read));
}

void MultiSink::AttachDrain(Drain& drain) {
  std::lock_guard lock(lock_);
  PW_DCHECK_PTR_EQ(drain.multisink_, nullptr);
  drain.multisink_ = this;
  drain.last_handled_sequence_id_ = sequence_id_ - 1;
  PW_CHECK_OK(ring_buffer_.AttachReader(drain.reader_));
}

void MultiSink::DetachDrain(Drain& drain) {
  std::lock_guard lock(lock_);
  PW_DCHECK_PTR_EQ(drain.multisink_, this);
  drain.multisink_ = nullptr;
  PW_CHECK_OK(ring_buffer_.DetachReader(drain.reader_),
              "The drain wasn't already attached.");
}

void MultiSink::AttachListener(Listener& listener) {
  std::lock_guard lock(lock_);
  listeners_.push_back(listener);
}

void MultiSink::DetachListener(Listener& listener) {
  std::lock_guard lock(lock_);
  [[maybe_unused]] bool was_detached = listeners_.remove(listener);
  PW_DCHECK(was_detached, "The listener was already attached.");
}

void MultiSink::Clear() {
  std::lock_guard lock(lock_);
  ring_buffer_.Clear();
}

void MultiSink::NotifyListeners() {
  for (auto& listener : listeners_) {
    listener.OnNewEntryAvailable();
  }
}

Result<ConstByteSpan> MultiSink::Drain::GetEntry(ByteSpan buffer,
                                                 uint32_t& drop_count_out) {
  PW_DCHECK_NOTNULL(multisink_);
  return multisink_->GetEntry(*this, buffer, drop_count_out);
}

}  // namespace multisink
}  // namespace pw
