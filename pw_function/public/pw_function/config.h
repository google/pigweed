// Copyright 2021 The Pigweed Authors
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

// Configuration macros for the function module.
#pragma once

#include <cstddef>

#include "lib/fit/function.h"

// The maximum size of a callable that can be inlined within a function.
// Callables larger than this are stored externally to the function (if dynamic
// allocation is enabled).
//
// This defaults to the size of 1 pointer, which is capable of storing common
// callables such as function pointers and lambdas with a single pointer's size
// of captured data.
#ifndef PW_FUNCTION_INLINE_CALLABLE_SIZE
#define PW_FUNCTION_INLINE_CALLABLE_SIZE (sizeof(void*))
#endif  // PW_FUNCTION_INLINE_CALLABLE_SIZE

static_assert(PW_FUNCTION_INLINE_CALLABLE_SIZE > 0 &&
              PW_FUNCTION_INLINE_CALLABLE_SIZE % alignof(void*) == 0);

// Whether functions should allocate memory dynamically if a callable is larger
// than the inline size.
//
// Enabling this allows functions to support callables larger than
// `PW_FUNCTION_INLINE_CALLABLE_SIZE` by dynamically allocating storage space
// for them. The Allocator type used can be provided as a template argument, and
// the default type can be specified by overriding
// `PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE`. The Allocator must satisfy the C++
// named requirements for Allocator:
// https://en.cppreference.com/w/cpp/named_req/Allocator
#ifndef PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION
#define PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION 0
#endif  // PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION

// The default Allocator used to dynamically allocate the callable, if dynamic
// allocation is enabled. Its `value_type` is irrelevant, since it must support
// rebinding.
//
// This definition is useful to ensure that a project can specify an Allocator
// that will be used even by Pigweed source code.
#ifndef PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE
#define PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE fit::default_callable_allocator
#endif  // PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE

namespace pw::function_internal::config {

inline constexpr size_t kInlineCallableSize = PW_FUNCTION_INLINE_CALLABLE_SIZE;
inline constexpr bool kEnableDynamicAllocation =
    PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION;

}  // namespace pw::function_internal::config
