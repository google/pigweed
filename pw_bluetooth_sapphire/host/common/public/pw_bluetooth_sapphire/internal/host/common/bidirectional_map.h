// Copyright 2024 The Pigweed Authors
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

#include <functional>
#include <optional>
#include <unordered_map>

#include "pw_assert/check.h"

namespace bt {

// UnorderedBiMap provides a 1:1 bidirectional mapping between two types, Left
// and Right, such that both types can be used as keys during a lookup. Removing
// the mapping from one direction removes the mapping from the other direction
// as well.
template <typename Left,
          typename Right,
          class LeftHasher = std::hash<Left>,
          class RightHasher = std::hash<Right>>
class BidirectionalMap {
 public:
  // Due to C++ method overloading rules, Left and Right cannot be the same
  // type. If we enable support for the same type, we need to check both
  // directions in get, put, remove, contains, etc. The semantics of
  // a bidirectional map become unclear in such a situation. For simplicity, we
  // disable Left and Right being the same type.
  //
  // If you must use the same Left and Right types, you can use type aliasing to
  // create two new pseudotypes in the type system.
  static_assert(!std::is_same_v<Left, Right>,
                "Template types Left and Right must be different.");

  BidirectionalMap() = default;

  // Returns the other side of a Left key's mapping.
  std::optional<std::reference_wrapper<const Right>> get(
      const Left& left) const {
    if (auto itr = left_to_right_.find(left); itr != left_to_right_.end()) {
      return std::cref(itr->second);
    }

    return std::nullopt;
  }

  // Returns the other side of a Right key's mapping.
  std::optional<std::reference_wrapper<const Left>> get(
      const Right& right) const {
    if (auto itr = right_to_left_.find(right); itr != right_to_left_.end()) {
      return std::cref(itr->second);
    }

    return std::nullopt;
  }

  // Create a mapping between left to right. The implicit mapping
  // in the other direction is also created.
  //
  // This function must handle a special class of bug when dealing with
  // bidirectional maps. Consider the following insert sequence:
  //
  // Operation:         Left to Right       Right to Left
  // map(1, 2)          {1: 2}              {2: 1}
  // map(3, 1)          {1: 2, 3: 1}        {1: 3, 2: 1}
  //
  // The correct operation would have been to realize that the {1: 2} mapping
  // already exists and update it.
  void insert(const Left& left, const Right& right) {
    if (auto itr = left_to_right_.find(left);
        itr != left_to_right_.end() && itr->second != right) {
      right_to_left_.erase(itr->second);
    }

    if (auto itr = right_to_left_.find(right);
        itr != right_to_left_.end() && itr->second != left) {
      left_to_right_.erase(itr->second);
    }

    left_to_right_[left] = right;
    right_to_left_[right] = left;
  }

  // Create a mapping between right to left. The implicit mapping
  // in the other direction is also created.
  void insert(const Right& right, const Left& left) {
    if (auto itr = left_to_right_.find(left);
        itr != left_to_right_.end() && itr->second != right) {
      right_to_left_.erase(itr->second);
    }

    if (auto itr = right_to_left_.find(right);
        itr != right_to_left_.end() && itr->second != left) {
      left_to_right_.erase(itr->second);
    }

    left_to_right_[left] = right;
    right_to_left_[right] = left;
  }

  // Removes the mapping from left to right. The implicit mapping in the other
  // direction is also removed.
  void erase(const Left& left) {
    auto itr = left_to_right_.find(left);
    if (itr == left_to_right_.end()) {
      return;
    }

    right_to_left_.erase(itr->second);
    left_to_right_.erase(itr);
  }

  // Removes the mapping from right to left. The implicit mapping in the other
  // direction is also removed.
  void erase(const Right& right) {
    auto itr = right_to_left_.find(right);
    if (itr == right_to_left_.end()) {
      return;
    }

    left_to_right_.erase(itr->second);
    right_to_left_.erase(itr);
  }

  // Returns true if key is present in the container, false otherwise
  bool contains(const Left& left) const {
    return left_to_right_.count(left) != 0;
  }

  // Returns true if key is present in the container, false otherwise
  bool contains(const Right& right) const {
    return right_to_left_.count(right) != 0;
  }

  // Get the number of unique mappings in the container
  std::size_t size() const {
    PW_DCHECK(left_to_right_.size() == right_to_left_.size());
    return left_to_right_.size();
  }

  // Returns true if there are no mappings in the container, false otherwise
  bool empty() const {
    PW_DCHECK(left_to_right_.empty() == right_to_left_.empty());
    return left_to_right_.empty();
  }

  // Remove all mappings
  void clear() {
    left_to_right_.clear();
    right_to_left_.clear();
  }

 private:
  std::unordered_map<Left, Right, LeftHasher> left_to_right_;
  std::unordered_map<Right, Left, RightHasher> right_to_left_;
};

}  // namespace bt
