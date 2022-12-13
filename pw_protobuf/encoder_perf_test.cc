// Copyright 2022 The Pigweed Authors
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

#include "pw_bytes/span.h"
#include "pw_perf_test/perf_test.h"
#include "pw_protobuf/encoder.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_stream/memory_stream.h"

namespace pw::protobuf {
namespace {

void BasicIntegerPerformance(pw::perf_test::State& state, int64_t value) {
  std::byte encode_buffer[30];

  while (state.KeepRunning()) {
    MemoryEncoder encoder(encode_buffer);
    encoder.WriteUint32(1, value).IgnoreError();
  }
}

PW_PERF_TEST(SmallIntegerEncoding, BasicIntegerPerformance, 1);
PW_PERF_TEST(LargerIntegerEncoding, BasicIntegerPerformance, 4000000000);

}  // namespace
}  // namespace pw::protobuf
