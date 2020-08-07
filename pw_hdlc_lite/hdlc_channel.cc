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
#include "pw_hdlc_lite/hdlc_channel.h"

#include "pw_hdlc_lite/encoder.h"
#include "pw_log/log.h"

namespace pw::rpc {

void HdlcChannelOutput::SendAndReleaseBuffer(size_t size) {
  Status status =
      hdlc_lite::EncodeAndWritePayload(buffer_.first(size), *writer_);
  if (!status.ok()) {
    PW_LOG_ERROR("Failed writing to %s: %s", name(), status.str());
  }
}

}  // namespace pw::rpc
