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
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

pw::Status Recombiner::StartRecombination(LockedL2capChannel& channel,
                                          size_t size) {
  if (IsActive()) {
    return Status::FailedPrecondition();
  }

  is_active_ = true;
  local_cid_ = channel.channel().local_cid();
  expected_size_ = size;
  recombined_size_ = 0;

  return channel.channel().StartRecombinationBuf(direction_, size);
}

pw::Status Recombiner::RecombineFragment(
    std::optional<LockedL2capChannel>& channel, pw::span<const uint8_t> data) {
  if (!IsActive()) {
    return Status::FailedPrecondition();
  }

  if (!channel.has_value()) {
    // Channel was destroyed before recombination ended. We still
    // need to complete recombination to read the fragments from ACL connection,
    // but Recombiner won't be storing the data (just tracking sizes).

    PW_LOG_INFO(
        "Channel %#x that was receiving the recombined PDU %s is no longer "
        "acquired. Will complete reading the recombined PDU, but result will "
        "be dropped.",
        local_cid(),
        DirectionToString(direction_));

    recombined_size_ += data.size();

    return pw::OkStatus();
  }

  PW_CHECK_INT_EQ(channel->channel().local_cid(), local_cid_);

  PW_CHECK(channel->channel().HasRecombinationBuf(direction_));

  PW_TRY(channel->channel().CopyToRecombinationBuf(
      direction_, as_bytes(data), write_offset()));

  recombined_size_ += data.size();

  return pw::OkStatus();
}

multibuf::MultiBuf Recombiner::TakeAndEnd(
    std::optional<LockedL2capChannel>& channel) {
  PW_CHECK(IsActive());
  PW_CHECK(IsComplete());
  PW_CHECK(channel.has_value());

  PW_CHECK_INT_EQ(channel->channel().local_cid(), local_cid_);

  // TODO: https://pwbug.dev/402457004 - It's actually possible for old channel
  // to be dtor'd and new channel with same id created since we started
  // recombination. That case would trigger this CHECK. Will handle more
  // appropriately in later CL.
  PW_CHECK(channel->channel().HasRecombinationBuf(direction_));

  multibuf::MultiBuf return_buf =
      channel->channel().TakeRecombinationBuf(direction_);
  // Should always be true since use `AllocateContiguous()` to create the
  // MultiBuf.
  PW_CHECK(return_buf.IsContiguous());

  EndRecombination(channel);

  return return_buf;
}

void Recombiner::EndRecombination(std::optional<LockedL2capChannel>& channel) {
  is_active_ = false;

  if (!channel.has_value()) {
    // Channel and its recombination writer has already been destroyed.
    return;
  }

  PW_CHECK_INT_EQ(channel->channel().local_cid(), local_cid_);

  channel->channel().EndRecombinationBuf(direction_);
}

}  // namespace pw::bluetooth::proxy
