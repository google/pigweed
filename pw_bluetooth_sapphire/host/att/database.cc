// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "database.h"

#include <algorithm>

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace att {
namespace {

bool StartLessThan(const AttributeGrouping& grp, const Handle handle) {
  return grp.start_handle() < handle;
}

bool EndLessThan(const AttributeGrouping& grp, const Handle handle) {
  return grp.end_handle() < handle;
}

}  // namespace

Database::Iterator::Iterator(GroupingList* list,
                             Handle start,
                             Handle end,
                             const common::UUID* type,
                             bool groups_only)
    : start_(start), end_(end), grp_only_(groups_only), attr_offset_(0u) {
  ZX_DEBUG_ASSERT(list);
  grp_end_ = list->end();

  if (type)
    type_filter_ = *type;

  // Initialize the iterator by performing a binary search over the attributes.
  // If we were asked to iterate over groupings only, then look strictly within
  // the range. Otherwise we allow the first grouping to partially overlap the
  // range.
  grp_iter_ = std::lower_bound(list->begin(), grp_end_, start_,
                               grp_only_ ? StartLessThan : EndLessThan);

  if (AtEnd())
    return;

  // If the first grouping is out of range then the iterator is done.
  if (grp_iter_->start_handle() > end) {
    MarkEnd();
    return;
  }

  if (start_ > grp_iter_->start_handle()) {
    attr_offset_ = start_ - grp_iter_->start_handle();
  }

  // If the first is inactive or if it doesn't match the current filter then
  // skip ahead.
  if (!grp_iter_->active() ||
      (type_filter_ &&
       grp_iter_->attributes()[attr_offset_].type() != *type_filter_)) {
    Advance();
  }
}

const Attribute* Database::Iterator::get() const {
  if (AtEnd() || !grp_iter_->active())
    return nullptr;

  ZX_DEBUG_ASSERT(attr_offset_ < grp_iter_->attributes().size());
  return &grp_iter_->attributes()[attr_offset_];
}

void Database::Iterator::Advance() {
  if (AtEnd())
    return;

  do {
    if (!grp_only_ && grp_iter_->active()) {
      // If this grouping has more attributes to look at.
      if (attr_offset_ < grp_iter_->attributes().size() - 1) {
        size_t end_offset = grp_iter_->end_handle() - grp_iter_->start_handle();
        ZX_DEBUG_ASSERT(end_offset < grp_iter_->attributes().size());

        // Advance.
        attr_offset_++;

        for (; attr_offset_ <= end_offset; ++attr_offset_) {
          const auto& attr = grp_iter_->attributes()[attr_offset_];

          // If |end_| is within this grouping and we go past it, the iterator
          // is done.
          if (attr.handle() > end_) {
            MarkEnd();
            return;
          }

          // If there is no filter then we're done. Otherwise, loop until an
          // attribute is found that matches the filter.
          if (!type_filter_ || attr.type() == *type_filter_)
            return;
        }
      }

      // We are done with the current grouping. Fall through and move to the
      // next group below.
      attr_offset_ = 0u;
    } else {
      ZX_DEBUG_ASSERT(attr_offset_ == 0u);
    }

    // Advance the group.
    grp_iter_++;
    if (AtEnd())
      return;

    if (grp_iter_->start_handle() > end_) {
      MarkEnd();
      return;
    }

    if (!grp_iter_->active() || !grp_iter_->complete())
      continue;

    // If there is no filter then we're done. Otherwise, loop until an
    // attribute is found that matches the filter. (NOTE: the group type is the
    // type of the first attribute).
    if (!type_filter_ || (*type_filter_ == grp_iter_->group_type()))
      return;
  } while (true);
}

Database::Database(Handle range_start, Handle range_end)
    : range_start_(range_start), range_end_(range_end) {
  ZX_DEBUG_ASSERT(range_start_ < range_end_);
  ZX_DEBUG_ASSERT(range_start_ >= kHandleMin);
  ZX_DEBUG_ASSERT(range_end_ <= kHandleMax);
}

Database::Iterator Database::GetIterator(Handle start,
                                         Handle end,
                                         const common::UUID* type,
                                         bool groups_only) {
  ZX_DEBUG_ASSERT(start >= range_start_);
  ZX_DEBUG_ASSERT(end <= range_end_);
  ZX_DEBUG_ASSERT(start <= end);

  return Iterator(&groupings_, start, end, type, groups_only);
}

AttributeGrouping* Database::NewGrouping(const common::UUID& group_type,
                                         size_t attr_count,
                                         const common::ByteBuffer& decl_value) {
  // This method looks for a |pos| before which to insert the new grouping.
  Handle start_handle;
  decltype(groupings_)::iterator pos;

  if (groupings_.empty()) {
    if (range_end_ - range_start_ < attr_count)
      return nullptr;

    start_handle = range_start_;
    pos = groupings_.end();
  } else if (groupings_.front().start_handle() - range_start_ > attr_count) {
    // There is room at the head of the list.
    start_handle = range_start_;
    pos = groupings_.begin();
  } else if (range_end_ - groupings_.back().end_handle() > attr_count) {
    // There is room at the tail end of the list.
    start_handle = groupings_.back().end_handle() + 1;
    pos = groupings_.end();
  } else {
    // Linearly search for a gap that fits the new grouping.
    // TODO(armansito): This is suboptimal for long running cases where the
    // database is fragmented. Think about using a better algorithm.

    auto prev = groupings_.begin();
    pos = prev;
    pos++;

    for (; pos != groupings_.end(); ++pos, ++prev) {
      size_t next_avail = pos->start_handle() - prev->end_handle() - 1;
      if (attr_count < next_avail)
        break;
    }

    if (pos == groupings_.end()) {
      bt_log(TRACE, "att", "attribute database is out of space!");
      return nullptr;
    }

    start_handle = prev->end_handle() + 1;
  }

  auto iter =
      groupings_.emplace(pos, group_type, start_handle, attr_count, decl_value);
  ZX_DEBUG_ASSERT(iter != groupings_.end());

  return &*iter;
}

bool Database::RemoveGrouping(Handle start_handle) {
  auto iter = std::lower_bound(groupings_.begin(), groupings_.end(),
                               start_handle, StartLessThan);

  if (iter == groupings_.end() || iter->start_handle() != start_handle)
    return false;

  groupings_.erase(iter);
  return true;
}

const Attribute* Database::FindAttribute(Handle handle) {
  if (handle == kInvalidHandle)
    return nullptr;

  // Do a binary search to find the grouping that this handle is in.
  auto iter = std::lower_bound(groupings_.begin(), groupings_.end(), handle,
                               EndLessThan);
  if (iter == groupings_.end() || iter->start_handle() > handle)
    return nullptr;

  if (!iter->active() || !iter->complete())
    return nullptr;

  size_t index = handle - iter->start_handle();
  ZX_DEBUG_ASSERT(index < iter->attributes().size());

  return &iter->attributes()[index];
}

}  // namespace att
}  // namespace bt
