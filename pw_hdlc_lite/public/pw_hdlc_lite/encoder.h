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

#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::hdlc_lite {

// Function used to encode 0-kMaxPayloadSize bytes and write it to our
// pw::stream::writer. This function is safe to call multiple times in
// succession since it automatically writes a delimiter byte at the
// beginning and the end. This enables successive encoding of multiple
// data frames.
Status EncodeAndWritePayload(ConstByteSpan payload, stream::Writer& writer);

}  // namespace pw::hdlc_lite
