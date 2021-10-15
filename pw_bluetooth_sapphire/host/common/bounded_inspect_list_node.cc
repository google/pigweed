// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bounded_inspect_list_node.h"

#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt {

void BoundedInspectListNode::AttachInspect(inspect::Node& parent, std::string name) {
  list_node_ = parent.CreateChild(name);
}

BoundedInspectListNode::Item& BoundedInspectListNode::CreateItem() {
  if (items_.size() == capacity_) {
    items_.pop();
  }

  std::string index = bt_lib_cpp_string::StringPrintf("%zu", next_index_);
  next_index_++;

  items_.push({.node = list_node_.CreateChild(index)});
  return items_.back();
}

}  // namespace bt
