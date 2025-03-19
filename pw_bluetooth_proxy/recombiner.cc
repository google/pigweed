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

  Result<MultiBufWriter> mbufw =
      MultiBufWriter::Create(multibuf_allocator, size);
  if (!mbufw.ok()) {
    return mbufw.status();
  }

  mbufw_.emplace(std::move(*mbufw));

  return pw::OkStatus();
}

pw::Status Recombiner::RecombineFragment(pw::span<const uint8_t> data) {
  if (!IsActive()) {
    return Status::FailedPrecondition();
  }

  PW_CHECK(mbufw_.has_value());
  pw::Status status = mbufw_->Write(pw::as_bytes(data));

  if (status == pw::OkStatus()) {
    recombined_size_ += data.size();
  }

  PW_CHECK_INT_EQ(mbufw_->U8Span().size(), recombined_size_);

  return status;
}

multibuf::MultiBuf Recombiner::TakeAndEnd() {
  PW_CHECK(IsActive());
  PW_CHECK(IsComplete());

  multibuf::MultiBuf mbuf = mbufw_->TakeMultiBuf();
  // We expect MultiBufWriter to have used `AllocateContiguous()` to create the
  // MultiBuf.
  PW_CHECK(mbuf.IsContiguous());

  EndRecombination();
  return mbuf;
}

void Recombiner::EndRecombination() {
  is_active_ = false;
  mbufw_ = std::nullopt;
}

}  // namespace pw::bluetooth::proxy
