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

#pragma once

#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/internal/locked_l2cap_channel.h"
#include "pw_multibuf/multibuf.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

// Recombiner recombines a fragmented ACL payload for a channel into a single
// payload.
//
// Its functions are passed the locked target channel which it uses to
// provide storage for the payload as it is recombined. Currently the locked
// channel needs to be passed for each call because client can dtor it from
// under us. Passing the locked version of the channel helps to ensure we
// actually have the lock.
//
// TODO: https://pwbug.dev/402454277 - Once we have channel ref ptrs the
// Recombiner can hold on to its channel's ref ptr for the duration of
// recombination. So will only need it passed at start of recombination.
class Recombiner {
 public:
  Recombiner(Direction direction) : direction_(direction) {}

  // Starts a new recombination session.
  //
  // Precondition: Recombination must not already be active.
  //
  // Returns:
  // * FAILED_PRECONDITION if recombination is already active.
  // * Any error from MultiBufWriter::Create(), namely RESOURCE_EXHAUSTED.
  // * OK if recombination is started.
  pw::Status StartRecombination(LockedL2capChannel& channel, size_t size);

  // Adds a fragment of data to the recombination buffer.
  //
  // Precondition: Recombination must be active
  //
  // Returns:
  // * FAILED_PRECONDITION if recombination is not active.
  // * Any error from MultiBufWriter::Write(), namely RESOURCE_EXHAUSTED.
  // * OK if the data was written
  pw::Status RecombineFragment(std::optional<LockedL2capChannel>& channel,
                               pw::span<const uint8_t> data);

  // Returns the recombined MultiBuf and ends recombination.
  //
  // The MultiBuf will be non-empty and contiguous.
  //
  // Preconditions: `IsActive()` and `IsComplete()` are both true.
  multibuf::MultiBuf TakeAndEnd(std::optional<LockedL2capChannel>& channel);

  // Ends recombination.
  // Frees the MultiBuf held in the channel (if any).
  void EndRecombination(std::optional<LockedL2capChannel>& channel);

  // Returns true if recombined size matches specified size.
  bool IsComplete() const {
    PW_CHECK(is_active_);

    return recombined_size_ == expected_size_;
  }

  // Returns true if recombination is active.
  // (currently receiving and recombining fragments).
  uint16_t IsActive() { return is_active_; }

  // Returns local_cid of channel being recombined. Should only be called
  // when recombination is active.
  uint16_t local_cid() {
    PW_CHECK(IsActive());
    return local_cid_;
  }

 private:
  size_t write_offset() const { return recombined_size_; }

  bool is_active_ = false;
  Direction direction_;
  uint16_t local_cid_ = 0;
  size_t expected_size_ = 0;
  size_t recombined_size_ = 0;
};

}  // namespace pw::bluetooth::proxy
