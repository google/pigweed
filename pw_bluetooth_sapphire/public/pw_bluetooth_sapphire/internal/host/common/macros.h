// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_MACROS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_MACROS_H_

// Macro used to simplify the task of deleting all of the default copy
// constructors and assignment operators.
#define BT_DISALLOW_COPY_ASSIGN_AND_MOVE(_class_name)  \
  _class_name(const _class_name&) = delete;            \
  _class_name(_class_name&&) = delete;                 \
  _class_name& operator=(const _class_name&) = delete; \
  _class_name& operator=(_class_name&&) = delete

// Macro used to simplify the task of deleting the non rvalue reference copy
// constructors and assignment operators.  (IOW - forcing move semantics)
#define BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(_class_name) \
  _class_name(const _class_name&) = delete;                 \
  _class_name& operator=(const _class_name&) = delete

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_MACROS_H_
