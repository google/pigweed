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

#include <pw_log_tokenized/handler.h>
#include <pw_log_tokenized/metadata.h>
#include <pw_span/span.h>
#include <pw_tokenizer/base64.h>
#include <zephyr/logging/log_core.h>

namespace pw::log_tokenized {

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                           const uint8_t log_buffer[],
                                           size_t size_bytes) {
  pw::log_tokenized::Metadata meta(metadata);

  // Encode the tokenized message as Base64.
  char base64_buffer[tokenizer::kDefaultBase64EncodedBufferSize];

  const size_t bytes = tokenizer::PrefixedBase64Encode(
      span(log_buffer, size_bytes), base64_buffer);

  if (bytes == 0) {
    return;
  }

  // _is_raw is set to 0 here because the print string is required to be a
  // string literal if _is_raw is set to 1.
  Z_LOG_PRINTK(/*_is_raw=*/0, "%s", base64_buffer);
}

}  // namespace pw::log_tokenized
