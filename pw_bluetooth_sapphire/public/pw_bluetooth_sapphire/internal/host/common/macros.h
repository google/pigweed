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
