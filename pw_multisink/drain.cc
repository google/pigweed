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
#include "pw_multisink/drain.h"

#include "pw_assert/light.h"

namespace pw {
namespace multisink {

Result<ConstByteSpan> Drain::GetEntry(ByteSpan entry,
                                      uint32_t& drop_count_out) {
  uint32_t entry_sequence_id = 0;
  drop_count_out = 0;
  const Result<ConstByteSpan> result =
      MultiSink::GetEntry(*this, entry, entry_sequence_id);

  // Exit immediately if the result isn't OK or OUT_OF_RANGE, as the
  // entry_sequence_id cannot be used for computation. Later invocations to
  // GetEntry will permit readers to determine how far the sequence ID moved
  // forward.
  if (!result.ok() && !result.status().IsOutOfRange()) {
    return result;
  }

  // Compute the drop count delta by comparing this entry's sequence ID with the
  // last sequence ID this drain successfully read.
  //
  // The drop count calculation simply computes the difference between the
  // current and last sequence IDs. Consecutive successful reads will always
  // differ by one at least, so it is subtracted out. If the read was not
  // successful, the difference is not adjusted.
  drop_count_out =
      entry_sequence_id - last_handled_sequence_id_ - (result.ok() ? 1 : 0);

  last_handled_sequence_id_ = entry_sequence_id;
  return result;
}

}  // namespace multisink
}  // namespace pw
