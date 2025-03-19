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

#include <optional>

#include "pw_assert/check.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

// Recombiner recombines a fragmented ACL payload into a single payload.
class Recombiner {
 public:
  Recombiner() = default;

  // Starts a new recombination session.
  //
  // Precondition: Recombination must not already be active.
  //
  // Returns:
  // * FAILED_PRECONDITION if recombination is already active.
  // * Any error from MultiBufWriter::Create(), namely RESOURCE_EXHAUSTED.
  // * OK if recombination is started.
  pw::Status StartRecombination(uint16_t local_cid,
                                multibuf::MultiBufAllocator& multibuf_allocator,
                                size_t size);

  // Adds a fragment of data to the recombination buffer.
  //
  // Precondition: Recombination must be active
  //
  // Returns:
  // * FAILED_PRECONDITION if recombination is not active.
  // * Any error from MultiBufWriter::Write(), namely RESOURCE_EXHAUSTED.
  // * OK if the data was written
  pw::Status RecombineFragment(pw::span<const uint8_t> data);

  // Returns the recombined MultiBuf and ends recombination.
  //
  // The MultiBuf will be non-empty and contiguous.
  //
  // Preconditions: `IsActive()` and `IsComplete()` are both true.
  multibuf::MultiBuf TakeAndEnd();

  // Ends recombination.
  // Frees any MultiBuf held.
  void EndRecombination();

  // Returns true if recombined size matches specified size.
  bool IsComplete() const {
    PW_CHECK(is_active_);
    PW_CHECK(buf_.has_value());

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
  uint16_t local_cid_ = 0;
  size_t expected_size_ = 0;
  size_t recombined_size_ = 0;
  std::optional<multibuf::MultiBuf> buf_ = std::nullopt;
};

}  // namespace pw::bluetooth::proxy
