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

#include "pw_multisink/multisink.h"
#include "pw_status/status.h"

namespace pw {
namespace multisink {

// An asynchronous reader which is attached to a MultiSink via AttachDrain.
// Each Drain holds a PrefixedEntryRingBufferMulti::Reader and abstracts away
// entry sequence information for clients.
class Drain {
 public:
  constexpr Drain() : last_handled_sequence_id_(0), multisink_(nullptr) {}

  // Returns the next available entry if it exists and acquires the latest drop
  // count in parallel.
  //
  // The `drop_count_out` is set to the number of entries that were dropped
  // since the last call to GetEntry, if the read operation was successful or
  // indicated that no entries were available to read. If the read operation
  // fails otherwise, the `drop_count_out` is set to zero.
  //
  // Drop counts are internally maintained with a 32-bit counter. If UINT32_MAX
  // entries have been handled by the attached multisink between subsequent
  // calls to GetEntry, the drop count will overflow and will report a lower
  // count erroneously. Users should ensure that sinks call GetEntry
  // at least once every UINT32_MAX entries.
  //
  // Return values:
  // Ok - An entry was successfully read from the multisink. The drop_count_out
  // is set to the count of entries that were dropped since the last call
  // to GetEntry.
  // FailedPrecondition - The drain must be attached to a sink.
  // OutOfRange - No entries were available, the drop_count_out is set to the
  // number of entries that were dropped since the last call to GetEntry.
  // ResourceExhausted - The provided buffer was not large enough to store the
  // next available entry.
  // DataLoss - An entry was read from the multisink, but did not match the
  // expected format (i.e. failed to decode).
  // InvalidArgument - The drain is not currently associated with a multisink.
  Result<ConstByteSpan> GetEntry(ByteSpan entry, uint32_t& drop_count_out);

 protected:
  friend MultiSink;
  ring_buffer::PrefixedEntryRingBufferMulti::Reader reader_;
  uint32_t last_handled_sequence_id_;
  MultiSink* multisink_;
};

}  // namespace multisink
}  // namespace pw
