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

#include "pw_assert/light.h"
#include "pw_multisink/drain.h"
#include "pw_status/try.h"
#include "pw_varint/varint.h"

namespace pw {
namespace multisink {

Result<ConstByteSpan> MultiSink::GetEntry(Drain& drain,
                                          ByteSpan buffer,
                                          uint32_t& sequence_id_out) {
  size_t bytes_read = 0;

  // Exit immediately if there's no multisink attached to this drain.
  if (drain.multisink_ == nullptr) {
    return Status::FailedPrecondition();
  }

  const Status status =
      drain.reader_.PeekFrontWithPreamble(buffer, sequence_id_out, bytes_read);
  if (status.IsOutOfRange()) {
    // If the drain has caught up, report the last handled sequence ID so that
    // it can still process any dropped entries.
    sequence_id_out = drain.multisink_->sequence_id_ - 1;
    return status;
  }
  PW_CHECK(drain.reader_.PopFront().ok());
  return std::as_bytes(buffer.first(bytes_read));
}

Status MultiSink::AttachDrain(Drain& drain) {
  PW_DCHECK(drain.multisink_ == nullptr);
  drain.multisink_ = this;
  drain.last_handled_sequence_id_ = sequence_id_ - 1;
  return ring_buffer_.AttachReader(drain.reader_);
}

Status MultiSink::DetachDrain(Drain& drain) {
  PW_DCHECK(drain.multisink_ == this);
  drain.multisink_ = nullptr;
  return ring_buffer_.DetachReader(drain.reader_);
}

}  // namespace multisink
}  // namespace pw
