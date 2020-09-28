// Copyright 2020 The Pigweed Authors
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

#include <cstddef>
#include <span>

#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_stream/stream.h"

namespace pw::stream {

// Stream writer which quietly drops all of the data, similar to /dev/null.
class NullWriter final : public Writer {
 public:
  size_t ConservativeWriteLimit() const override {
    // In theory this can sink as much as is addressable, however this way it is
    // compliant with pw::StatusWithSize.
    return StatusWithSize::max_size();
  }

 private:
  Status DoWrite(ConstByteSpan data) override { return Status::Ok(); }
};

}  // namespace pw::stream
