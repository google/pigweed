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

#pragma once
#include <cstddef>
#include <queue>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"

namespace bt {

// This class is intended to represent a list node in Inspect, which doesn't
// support lists natively. Furthermore, it makes sure that the number of list
// items doesn't exceed |capacity|.
//
// Each item in BoundedInspectListNode is represented as a child node with name
// as index. This index is always increasing and does not wrap around. For
// example, if capacity is 3, then the children names are `[0, 1, 2]` on the
// first three additions. When a new node is added, `0` is popped, and the
// children names are `[1, 2, 3]`.
//
// Example Usage:
//    BoundedInspectListNode list(/*capacity=*/2);
//    list.AttachInspect(parent_node);
//
//    auto& item_0 = list.CreateItem();
//    item_0.node.RecordInt("property_0", 0);
//
//    auto& item_1 = list.CreateItem();
//    item_1.node.RecordInt("property_A", 1);
//    item_1.node.RecordInt("property_B", 2);
//
// Inspect Tree:
//     example_list:
//         0:
//             property_0: 0
//         1:
//             property_A: 1
//             property_B: 2
class BoundedInspectListNode {
 public:
  struct Item {
    // The list child node with the index as it's name (0, 1, 2...).
    inspect::Node node;
  };

  explicit BoundedInspectListNode(size_t capacity) : capacity_(capacity) {
    BT_ASSERT(capacity_ > 0u);
  }
  ~BoundedInspectListNode() = default;

  // Attach this node as a child of |parent| with the name |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  // Add an item to the list, removing a previous item if the list is at
  // capacity. The returned item reference is valid until the next item is added
  // (or this list node is destroyed).
  Item& CreateItem();

 private:
  inspect::Node list_node_;
  size_t next_index_ = 0;
  const size_t capacity_;
  std::queue<Item> items_;
};

}  // namespace bt
