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

// Configuration macros for the unit test module.
#pragma once

#include <cstddef>

#include "pw_polyfill/language_feature_macros.h"

#ifndef PW_UNIT_TEST_CONFIG_EVENT_BUFFER_SIZE
/// The size of the event buffer that the ``UnitTestService`` contains.
/// This buffer is used to encode events.  By default this is set to
/// 128 bytes.
#define PW_UNIT_TEST_CONFIG_EVENT_BUFFER_SIZE 128
#endif  // PW_UNIT_TEST_CONFIG_EVENT_BUFFER_SIZE

#ifndef PW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE
/// The size of the memory pool to use for test fixture instances. By default
/// this is set to 16K.
#define PW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE 16384
#endif  // PW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE

namespace pw {
namespace unit_test {
namespace config {

PW_INLINE_VARIABLE constexpr size_t kEventBufferSize =
    PW_UNIT_TEST_CONFIG_EVENT_BUFFER_SIZE;

PW_INLINE_VARIABLE constexpr size_t kMemoryPoolSize =
    PW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE;

}  // namespace config
}  // namespace unit_test
}  // namespace pw
