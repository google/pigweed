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
#pragma once

#include "pw_log_sink/sink.h"
#include "pw_multisink/multisink.h"

namespace pw {
namespace log_sink {

class MultiSinkAdapter final : public Sink {
 public:
  MultiSinkAdapter(multisink::MultiSink& multisink) : multisink_(multisink) {}

  // Write an entry to the sink.
  void HandleEntry(ConstByteSpan entry) final {
    // Best-effort attempt to send data to the sink, so status is ignored. The
    // multisink handles failures internally and propagates them to its drains.
    multisink_.HandleEntry(entry);
  }

  // Notifies the sink of messages dropped before ingress. The writer may use
  // this to signal to sinks that an entry (or entries) was lost before sending
  // to the sink (e.g. the log sink failed to encode the message).
  void HandleDropped(uint32_t drop_count) final {
    multisink_.HandleDropped(drop_count);
  }

 private:
  multisink::MultiSink& multisink_;
};

}  // namespace log_sink
}  // namespace pw
