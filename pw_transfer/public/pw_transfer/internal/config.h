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

// Configuration macros for the transfer module.
#pragma once

#include <cinttypes>
#include <limits>

#include "pw_chrono/system_clock.h"

// The default maximum number of times a transfer should retry sending a chunk
// when no response is received. This can later be configured per-transfer.
#ifndef PW_TRANSFER_DEFAULT_MAX_RETRIES
#define PW_TRANSFER_DEFAULT_MAX_RETRIES 3
#endif  // PW_TRANSFER_DEFAULT_MAX_RETRIES

// The default amount of time, in milliseconds, to wait for a chunk to arrive
// before retrying. This can later be configured per-transfer.
#ifndef PW_TRANSFER_DEFAULT_TIMEOUT_MS
#define PW_TRANSFER_DEFAULT_TIMEOUT_MS 2000
#endif  // PW_TRANSFER_DEFAULT_TIMEOUT_MS

static_assert(PW_TRANSFER_DEFAULT_MAX_RETRIES > 0 &&
              PW_TRANSFER_DEFAULT_MAX_RETRIES <=
                  std::numeric_limits<uint8_t>::max());

static_assert(PW_TRANSFER_DEFAULT_TIMEOUT_MS > 0);

namespace pw::transfer::cfg {

inline constexpr uint8_t kDefaultMaxRetries = PW_TRANSFER_DEFAULT_MAX_RETRIES;
inline constexpr chrono::SystemClock::duration kDefaultChunkTimeout =
    std::chrono::milliseconds(PW_TRANSFER_DEFAULT_TIMEOUT_MS);

}  // namespace pw::transfer::cfg
