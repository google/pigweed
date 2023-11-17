// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/common/retire_log.h"

#include <limits>

namespace bt::internal {

RetireLog::RetireLog(size_t min_depth, size_t max_depth)
    : min_depth_(min_depth), max_depth_(max_depth) {
  BT_ASSERT(min_depth_ > 0);
  BT_ASSERT(min_depth_ <= max_depth_);

  // For simplicity, log indexes are computed with doubles, so limit the depth
  // to 2**53 in which precision is preserved, assuming IEEE-754 DPFPs.
  BT_ASSERT(max_depth_ <=
            (decltype(max_depth_){1} << std::numeric_limits<double>::digits));
  buffer_.reserve(max_depth_);
  std::apply(
      [this](auto&... scratchpad) { (scratchpad.reserve(max_depth_), ...); },
      quantile_scratchpads_);
}

void RetireLog::Retire(size_t byte_count,
                       pw::chrono::SystemClock::duration age) {
  if (depth() < max_depth_) {
    buffer_.push_back({byte_count, age});
    return;
  }
  buffer_[next_insertion_index_] = {byte_count, age};
  next_insertion_index_ = (next_insertion_index_ + 1) % depth();
}

}  // namespace bt::internal
