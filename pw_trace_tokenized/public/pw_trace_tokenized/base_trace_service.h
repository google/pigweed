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
#pragma once

#include <cstdint>
#include <optional>

#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_trace_tokenized/trace_tokenized.h"

namespace pw::trace {
class BaseTraceService {
 public:
  BaseTraceService(TokenizedTracer& tokenized_tracer,
                   stream::Writer& trace_writer);

  void SetTransferId(uint32_t id) { transfer_id_ = id; }

 protected:
  Status Start();
  Status Stop();

 private:
  TokenizedTracer& tokenized_tracer_;
  stream::Writer& trace_writer_;

 protected:
  std::optional<uint32_t> transfer_id_;
};

}  // namespace pw::trace
