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

#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "pw_assert/check.h"

namespace bt {

// BidirectionalMultimap provides a 1:many bidirectional mapping between two
// types, One and Many, such that both types can be used as keys during a
// lookup. Removing the One key from the map removes all of its Many mappings.
// Removing a single Many mapping will only remove the corresponding One if
// there was a 1:1 mapping between them at the time.
//
// NOTE: The Many to One relationship must be onto but not necessarily one to
// one. That is, each element of the Many set must map to at most a single One.
template <typename One,
          typename Many,
          class OneHasher = std::hash<One>,
          class ManyHasher = std::hash<Many>>
class BidirectionalMultimap {
 public:
  // Due to C++ method overloading rules, Left and Right cannot be the same
  // type. If we enable support for the same type, we need to check both
  // directions in get, put, remove, contains, etc. The semantics of
  // a bidirectional map become unclear in such a situation. For simplicity, we
  // disable Left and Right being the same type.
  //
  // If you must use the same Left and Right types, you can use type aliasing to
  // create two new psuedotypes in the type system.
  static_assert(!std::is_same_v<One, Many>,
                "Template types One and Many must be different.");

  BidirectionalMultimap() = default;

  // Returns the Many set for a given One key.
  std::optional<
      std::reference_wrapper<const std::unordered_set<Many, ManyHasher>>>
  get(const One& one) const {
    if (auto itr = one_to_many_.find(one); itr != one_to_many_.end()) {
      return std::cref(itr->second);
    }

    return std::nullopt;
  }

  // Returns the One key for a given Many key.
  std::optional<std::reference_wrapper<const One>> get(const Many& many) const {
    if (auto itr = many_to_one_.find(many); itr != many_to_one_.end()) {
      return std::cref(itr->second);
    }

    return std::nullopt;
  }

  // Create a mapping between one and many. The implicit mapping in the other
  // direction is also created.
  void put(const One& one, const Many& many) {
    PW_CHECK(many_to_one_.count(many) == 0,
             "many to one relationship must be a surjection (onto)");

    one_to_many_[one].insert(many);
    many_to_one_[many] = one;
  }

  // Removes the mapping from one to all the many elements it maps to in both
  // directions.
  void remove(const One& one) {
    if (!contains(one)) {
      return;
    }

    for (const auto& many : one_to_many_[one]) {
      many_to_one_.erase(many);
    }

    one_to_many_.erase(one);
  }

  // Removes the mapping from many to the one it maps to in both directions.
  void remove(const Many& many) {
    if (!contains(many)) {
      return;
    }

    const auto& one = many_to_one_[many];
    one_to_many_[one].erase(many);
    if (one_to_many_[one].empty()) {
      one_to_many_.erase(one);
    }

    many_to_one_.erase(many);
  }

  // Returns true if key is present in the container, false otherwise.
  bool contains(const One& one) const { return one_to_many_.count(one) != 0; }

  // Returns true if key is present in the container, false otherwise.
  bool contains(const Many& many) const {
    return many_to_one_.count(many) != 0;
  }

  // Returns the number of One elements there are in the container
  std::size_t size_one() const { return one_to_many_.size(); }

  // Returns the number of Many elements there are in the container
  std::size_t size_many() const { return many_to_one_.size(); }

  // Returns true if there are no mappings in the container, false otherwise
  bool empty() const { return one_to_many_.empty(); }

  // Remove all mappings
  void clear() {
    one_to_many_.clear();
    many_to_one_.clear();
  }

 private:
  std::unordered_map<One, std::unordered_set<Many, ManyHasher>, OneHasher>
      one_to_many_;
  std::unordered_map<Many, One, ManyHasher> many_to_one_;
};
}  // namespace bt
