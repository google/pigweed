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
#pragma once

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"
#include "pw_status/status.h"

namespace pw {
namespace multisink {
class Drain;

// An asynchronous single-writer multi-reader queue that ensures readers can
// poll for dropped message counts, which is useful for logging or similar
// scenarios where readers need to be aware of the input message sequence.
// TODO(pwbug/342): Support notifying readers when the queue is readable,
// rather than requiring them to poll to check for new entries.
// TODO(pwbug/343): Add thread-safety, separate from the thread-safety work
// planned for the underlying ring buffer.
class MultiSink {
 public:
  // Constructs a multisink using a ring buffer backed by the provided buffer.
  MultiSink(ByteSpan buffer) : ring_buffer_(true), sequence_id_(0) {
    ring_buffer_.SetBuffer(buffer);
  }

  // Write an entry to the multisink. If available space is less than the
  // size of the entry, the internal ring buffer will push the oldest entries
  // out to make space, so long as the entry is not larger than the buffer.
  // The sequence ID of the multisink will always increment as a result of
  // calling HandleEntry, regardless of whether pushing the entry succeeds.
  //
  // Return values:
  // Ok - Entry was successfully pushed to the ring buffer.
  // InvalidArgument - Size of data to write is zero bytes.
  // OutOfRange - Size of data is greater than buffer size.
  // FailedPrecondition - Buffer was not initialized.
  Status HandleEntry(ConstByteSpan entry) {
    return ring_buffer_.PushBack(entry, sequence_id_++);
  }

  // Notifies the multisink of messages dropped before ingress. The writer
  // may use this to signal to readers that an entry (or entries) failed
  // before being sent to the multisink (e.g. the writer failed to encode
  // the message). This API increments the sequence ID of the multisink by
  // the provided `drop_count`.
  void HandleDropped(uint32_t drop_count = 1) { sequence_id_ += drop_count; }

  // Attach a drain to the multisink. Drains may not be associated with more
  // than one multisink at a time. Entries pushed before the drain was attached
  // are not seen by the drain, so drains should be attached before entries
  // are pushed.
  //
  // Return values:
  // Ok - Drain was successfully attached.
  // InvalidArgument - Drain is currently associated with another multisink.
  Status AttachDrain(Drain& drain);

  // Detaches a drain from the multisink. Drains may only be detached if they
  // were previously attached to this multisink.
  //
  // Return values:
  // Ok - Drain was successfully detached.
  // InvalidArgument - Drain is not currently associated with this multisink.
  Status DetachDrain(Drain& drain);

  // Removes all data from the internal buffer. The multisink's sequence ID is
  // not modified, so readers may interpret this event as droppping entries.
  void Clear() { ring_buffer_.Clear(); }

 protected:
  friend Drain;
  // Gets an entry from the provided drain and unpacks sequence ID information.
  // Drains use this API to strip away sequence ID information for drop
  // calculation.
  //
  // Returns:
  // Ok - An entry was successfully read from the multisink. The `sequence_id`
  // is set to the ID encoded in the oldest entry.
  // FailedPrecondition - The drain is not attached to a multisink.
  // ResourceExhausted - The provided buffer was not large enough to store
  // the next available entry.
  // DataLoss - An entry was read from the multisink, but did not contains an
  // encoded sequence ID.
  static Result<ConstByteSpan> GetEntry(Drain& drain,
                                        ByteSpan buffer,
                                        uint32_t& sequence_id_out);

 private:
  ring_buffer::PrefixedEntryRingBufferMulti ring_buffer_;
  uint32_t sequence_id_ = 0;
};

}  // namespace multisink
}  // namespace pw
