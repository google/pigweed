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

#include "pw_stream_shmem_mcuxpresso/stream.h"

#include "pw_unit_test/framework.h"

namespace pw::stream {
namespace {

std::array<std::byte, 20> read_buffer = {};
std::array<std::byte, 20> write_buffer = {};

TEST(ShmemMcuxpressoStream, CompilesOk) {
  auto stream = ShmemMcuxpressoStream{MUA, read_buffer, write_buffer};
  stream.Enable();
}

}  // namespace
}  // namespace pw::stream
