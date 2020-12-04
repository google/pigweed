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
 private:
  Status DoWrite(ConstByteSpan) final { return Status::Ok(); }
};

// Stream reader which never reads any bytes. Always returns OUT_OF_RANGE, which
// indicates there is no more data to read.
class NullReader final : public Reader {
 private:
  StatusWithSize DoRead(ByteSpan) final { return StatusWithSize::OutOfRange(); }
};

}  // namespace pw::stream
