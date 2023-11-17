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
#include "pw_assert/check.h"

#define BT_PANIC(msg, ...) PW_CRASH(msg, ##__VA_ARGS__)
#define BT_ASSERT(x) PW_CHECK(x)
#define BT_ASSERT_MSG(x, msg, ...) PW_CHECK(x, msg, ##__VA_ARGS__)
#define BT_DEBUG_ASSERT(x) PW_DCHECK(x)
#define BT_DEBUG_ASSERT_MSG(x, msg, ...) PW_DCHECK(x, msg, ##__VA_ARGS__)
