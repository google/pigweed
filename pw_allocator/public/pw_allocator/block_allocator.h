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

// This file exists merely to allow block allocator users to migrate to
// including specific block allocator header files directly. New users should
// not use this header, and it will eventually be deprecated.

// TODO(b/326509341): Remove when all customers import the correct files.
#include "pw_allocator/best_fit_block_allocator.h"
#include "pw_allocator/dual_first_fit_block_allocator.h"
#include "pw_allocator/first_fit_block_allocator.h"
#include "pw_allocator/last_fit_block_allocator.h"
#include "pw_allocator/worst_fit_block_allocator.h"
