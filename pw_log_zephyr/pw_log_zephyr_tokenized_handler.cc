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

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_msg.h>

#include "pw_log_tokenized/handler.h"

namespace pw::log_tokenized {

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                           const uint8_t log_buffer[],
                                           size_t size_bytes) {
  ARG_UNUSED(metadata);
  ARG_UNUSED(log_buffer);
  ARG_UNUSED(size_bytes);
  // TODO(asemjonovs): implement this function
}

}  // namespace pw::log_tokenized
