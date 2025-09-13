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

#include "pw_metric_private/metric_walker.h"

#include <cinttypes>
#include <optional>

#include "pw_assert/check.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_metric/metric.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::metric::internal {

// Private implementation of ScopedName. Exists to safely push/pop parent groups
// from the explicit stack during a metric walk.
struct ResumableMetricWalker::ScopedName {
  ScopedName(Token name, ResumableMetricWalker& rhs) : walker(rhs) {
    // Ensure the metric path depth doesn't exceed the allocated capacity. If
    // this assert fails, the path_ vector's capacity should be increased.
    PW_CHECK_INT_LT(walker.path_.size(),
                    walker.path_.capacity(),
                    "The path_ vector's capacity should be increased.");
    walker.path_.push_back(name);
  }
  ~ScopedName() { walker.path_.pop_back(); }
  ResumableMetricWalker& walker;
};

ResumableMetricWalker::ResumableMetricWalker(UnaryMetricWriter& writer)
    : writer_(writer) {}

Result<uint64_t> ResumableMetricWalker::Walk(
    const IntrusiveList<Metric>& metrics,
    const IntrusiveList<Group>& groups,
    std::optional<uint64_t> cursor) {
  start_cursor_ = cursor.value_or(0);
  writing_phase_ = (start_cursor_ == 0);
  next_cursor_ = 0;

  Status walk_status = RecursiveWalkHelper(metrics, groups);

  // If the client provided a cursor but the walk ended (for any reason)
  // without finding it, the cursor is invalid. This check must happen BEFORE
  // checking for other statuses like RESOURCE_EXHAUSTED.
  if (start_cursor_ != 0 && !writing_phase_) {
    return Status::NotFound();
  }

  if (walk_status.IsResourceExhausted()) {
    // A page was filled. Before returning the cursor for the next page, check
    // if progress is being made. If the next cursor is the same as the start
    // cursor, it means the metric at that address is too large to fit into an
    // empty response buffer. This would cause an infinite loop.
    if (start_cursor_ != 0 && start_cursor_ == next_cursor_) {
      PW_LOG_ERROR("Walker stalled: metric at cursor 0x%llx is too large.",
                   (unsigned long long)start_cursor_);
      return Status::ResourceExhausted();
    }
    return walk_status;
  }

  PW_TRY(walk_status);

  // The walk completed successfully.
  return 0u;
}

Status ResumableMetricWalker::RecursiveWalkHelper(
    const IntrusiveList<Metric>& metrics, const IntrusiveList<Group>& groups) {
  for (const auto& metric : metrics) {
    ScopedName scoped_name(metric.name(), *this);

    if (!writing_phase_) {
      if (reinterpret_cast<uint64_t>(&metric) == start_cursor_) {
        writing_phase_ = true;
        // Fall through to write the metric on this iteration.
      } else {
        continue;  // Still searching for the cursor.
      }
    }

    Status status = writer_.Write(metric, path_);
    if (status.IsResourceExhausted()) {
      // The page is full. The current metric could not be written.
      // Its address becomes the cursor for the next request.
      next_cursor_ = reinterpret_cast<uint64_t>(&metric);
      return status;
    }
    PW_TRY(status);
  }

  for (const auto& group : groups) {
    ScopedName scoped_name(group.name(), *this);
    PW_TRY(RecursiveWalkHelper(group.metrics(), group.children()));
  }

  return OkStatus();
}

}  // namespace pw::metric::internal
