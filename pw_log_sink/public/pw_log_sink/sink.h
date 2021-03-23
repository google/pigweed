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

// TODO(pwbug.dev/351): This file is very similar to pw_log_multisink/sink.h,
// which will be deprecated in a later change.

#include "pw_bytes/span.h"
#include "pw_containers/intrusive_list.h"

namespace pw {
namespace log_sink {

class Sink : public IntrusiveList<Sink>::Item {
 public:
  virtual ~Sink() = default;

  // Write an entry to the sink. This is a best-effort attempt to send data to
  // the sink, so failures are ignored.
  virtual void HandleEntry(ConstByteSpan entry) = 0;

  // Notifies the sink of messages dropped before ingress. The writer may use
  // this to signal to sinks that an entry (or entries) was lost before sending
  // to the sink (e.g. the log sink failed to encode the message).
  virtual void HandleDropped(uint32_t drop_count) = 0;
};

}  // namespace log_sink
}  // namespace pw
