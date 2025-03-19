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

#include "pw_bluetooth_proxy/internal/recombiner.h"

#include <optional>

#include "pw_assert/check.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

pw::Status Recombiner::StartRecombination(
    uint16_t local_cid,
    multibuf::MultiBufAllocator& multibuf_allocator,
    size_t size) {
  if (IsActive()) {
    return Status::FailedPrecondition();
  }

  is_active_ = true;
  local_cid_ = local_cid;
  expected_size_ = size;
  recombined_size_ = 0;

  buf_ = multibuf_allocator.AllocateContiguous(size);
  if (!buf_) {
    return Status::ResourceExhausted();
  }

  return pw::OkStatus();
}

pw::Status Recombiner::RecombineFragment(pw::span<const uint8_t> data) {
  if (!IsActive()) {
    return Status::FailedPrecondition();
  }

  PW_CHECK(buf_.has_value());
  PW_TRY(buf_->CopyFrom(as_bytes(data), write_offset()));

  recombined_size_ += data.size();

  return pw::OkStatus();
}

multibuf::MultiBuf Recombiner::TakeAndEnd() {
  PW_CHECK(IsActive());
  PW_CHECK(IsComplete());

  PW_CHECK(buf_);
  multibuf::MultiBuf mbuf_buf = std::exchange(buf_, std::nullopt).value();

  // We expect MultiBufWriter to have used `AllocateContiguous()` to create the
  // MultiBuf.
  PW_CHECK(mbuf_buf.IsContiguous());

  EndRecombination();

  return mbuf_buf;
}

void Recombiner::EndRecombination() {
  is_active_ = false;
  buf_ = std::nullopt;
}

}  // namespace pw::bluetooth::proxy
