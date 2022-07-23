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

#include <span>

#include "gtest/gtest.h"
#include "pw_protobuf/encoder.h"
#include "pw_snapshot_protos/snapshot.pwpb.h"

namespace pw::snapshot {
namespace {

TEST(Status, CompileTest) {
  constexpr size_t kMaxProtoSize = 256;
  std::byte encode_buffer[kMaxProtoSize];
  std::byte submessage_buffer[kMaxProtoSize];

  stream::MemoryWriter writer(encode_buffer);
  Snapshot::StreamEncoder snapshot_encoder(writer, submessage_buffer);
  {
    Metadata::StreamEncoder metadata_encoder =
        snapshot_encoder.GetMetadataEncoder();
    metadata_encoder
        .WriteReason(
            std::as_bytes(std::span("It just died, I didn't do anything")))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    metadata_encoder.WriteFatal(true)
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    metadata_encoder.WriteProjectName(std::as_bytes(std::span("smart-shoe")))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    metadata_encoder.WriteDeviceName(std::as_bytes(std::span("smart-shoe-p1")))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  }
  ASSERT_TRUE(snapshot_encoder.status().ok());
}

}  // namespace
}  // namespace pw::snapshot
